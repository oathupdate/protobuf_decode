// Copyright (cc) 2022 oathupdate. All rights reserved.

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <iostream>
#include <vector>

using std::vector;
using std::string;

#define MAX_FIELD_NUM 512
#define PROTOBUF_PREDICT_TRUE(x) (__builtin_expect(!!(x), 1))

#define DECODE_ERROR -1
#define DECODE_OK    1

#define TAG_TYPE_BITS 3
#define TAG_TYPE_MASK ((1 << TAG_TYPE_BITS) - 1)
#define MAX_VARINT_BYTES 10
#define MAX_VARINT_32BYTES 5

struct PbBuf {
  u_char *pos;
  u_char *last;
  u_char *start;
  u_char *end;
};

struct Field {
  uint32_t  tag;
  uint32_t  wire_type;
  uint32_t  field_num;
  uint32_t  depth;
  int64_t   number_value;  // the value of type number
  PbBuf     value;         // the value of length_delimited
  vector<Field> sub_fields;
};

static void InitField(Field *field, PbBuf *in);
static int TryDecodeSubField(Field *field);
static int DecodeField(Field *field, PbBuf *in);
static void FieldOutput(const Field *field, PbBuf *trace);
const uint8_t *ReadVarint32FromArray(uint32_t first_byte, const uint8_t *buffer,
    uint32_t* value);

enum WireType {
  // int32, int64, uint32
  WIRETYPE_VARINT = 0,
  // fixed64, sfixed64, double uint64, sint32, sint64, bool
  WIRETYPE_FIXED64 = 1,
  // string, bytes, embedded messages, packed repeated
  WIRETYPE_LENGTH_DELIMITED = 2,
  WIRETYPE_START_GROUP = 3,
  WIRETYPE_END_GROUP = 4,
  WIRETYPE_FIXED32 = 5,
};

static inline int32_t PbBufSize(const PbBuf *b) {
  return b->last - b->pos;
}

static inline int32_t PbBufCapacity(const PbBuf *b) {
  return b->end - b->start;
}

static inline uint32_t GetTag(PbBuf *b) {
  if (!b->pos || b->pos >= b->last) {
    return 0;
  }
  uint32_t tag, tag2;
  tag = (uint32_t)b->pos[0];
  if (tag < 0x80) {
    b->pos++;
    return tag;
  }

  const uint8_t * ptr = ReadVarint32FromArray(tag, b->pos, &tag2);
  if (!ptr) {
    return 0;
  }

  b->pos = const_cast<uint8_t*>(ptr);

  return tag2;
}

static inline int GetTagWireType(uint32_t tag) {
  return tag & TAG_TYPE_MASK;
}


static inline int GetTagFieldNumber(uint32_t tag) {
  return tag >> TAG_TYPE_BITS;
}

const uint8_t* ReadVarint32FromArray(uint32_t first_byte, const uint8_t *buffer,
  uint32_t *value) {
  const uint8_t *ptr = buffer;
  uint32_t b, i;
  uint32_t result = first_byte - 0x80;
  ++ptr;  // We just processed the first byte. Move on to the second.
  b = *(ptr++);
  result += b << 7;
  if (!(b & 0x80)) {
    goto done;
  }
  result -= 0x80 << 7;
  b = *(ptr++);
  result += b << 14;
  if (!(b & 0x80)) {
    goto done;
  }
  result -= 0x80 << 14;
  b = *(ptr++);
  result += b << 21;
  if (!(b & 0x80)) {
    goto done;
  }
  result -= 0x80 << 21;
  b = *(ptr++);
  result += b << 28;
  if (!(b & 0x80)) {
    goto done;
  }
  for (i = 0; i < MAX_VARINT_BYTES - MAX_VARINT_32BYTES; i++) {
    b = *(ptr++);
    if (!(b & 0x80)) {
      goto done;
    }
  }

  return NULL;;
done:
  *value = result;

  return ptr;
}

int64_t ReadVarint32Fallback(PbBuf *b, uint32_t first_byte) {
  uint32_t temp;
  if (PbBufSize(b) <= MAX_VARINT_BYTES) {
    return DECODE_ERROR;
  }
  const uint8_t *ptr = ReadVarint32FromArray(first_byte, b->pos, &temp);
  if (!ptr) {
    return DECODE_ERROR;
  }

  b->pos = const_cast<uint8_t*>(ptr);

  return temp;
}

static inline int ReadVarint32(PbBuf *b, uint32_t *value) {
  uint32_t v = 0;
  if (b->pos < b->last) {
    v = *b->pos;
    if (v < 0x80) {
      *value = v;
      b->pos++;
      return DECODE_OK;
    }
  }

  int64_t result = ReadVarint32Fallback(b, v);
  if (result < 0) {
    return DECODE_ERROR;
  }
  *value = result;

  return DECODE_OK;
}

const char* VarintParse(PbBuf *b, const char *p, int64_t *out) {
  int i;
  int64_t res = 0;
  int64_t extra = 0;
  for (i = 0; i < 10; i++) {
    int64_t byte = (uint8_t)p[i];
    res += byte << (i * 7);
    int j = i + 1;
    if (PROTOBUF_PREDICT_TRUE(byte < 128)) {
      *out = res - extra;
      return p + j;
    }
    extra += 128ull << (i * 7);
  }

  *out = 0;

  return NULL;
}

static inline int DecodeNumber(Field *field, PbBuf *in) {
  u_char *p;
  int64_t n;
  if (in->pos >= in->last) {
    return DECODE_ERROR;
  }

  p = (u_char*)VarintParse(in, (char*)(in->pos), &n);
  if (!p) {
    return DECODE_ERROR;
  }

  in->pos = p;
  field->number_value = n;

  return DECODE_OK;
}

static inline int DecodeFixed64(Field *field, PbBuf *in) {
  int64_t n;
  if (PbBufSize(in) < static_cast<int>(sizeof(int64_t))) {
    return DECODE_ERROR;
  }
  memcpy(&n, in->pos, sizeof(int64_t));
  in->pos += sizeof(int64_t);
  field->number_value = n;

  return DECODE_OK;
}

static inline int DecodeFixed32(Field *field, PbBuf *in) {
  int32_t n;
  if (PbBufSize(in) < static_cast<int>(sizeof(int32_t))) {
    return DECODE_ERROR;
  }
  memcpy(&n, in->pos, sizeof(int32_t));
  in->pos += sizeof(int32_t);
  field->number_value = n;

  return DECODE_OK;
}

static inline int DecodeString(Field *field, PbBuf *in) {
  uint32_t length;
  int ret = ReadVarint32(in, &length);
  if (ret != DECODE_OK || static_cast<int>(length) > PbBufSize(in)) {
    return DECODE_ERROR;
  }

  field->value.pos = in->pos;
  field->value.last = in->pos + length;
  in->pos = field->value.last;

  return DECODE_OK;
}

static inline int TryDecodeSubField(Field *field) {
  PbBuf b;
  b.pos = field->value.pos;
  b.last = field->value.last;

  while (b.pos < b.last) {
    Field sub_field;
    InitField(&sub_field, &b);
    sub_field.depth = field->depth + 1;
    if (!sub_field.tag || !sub_field.field_num) {
      field->sub_fields.clear();
      return DECODE_ERROR;
    }

    if (DECODE_OK != DecodeField(&sub_field, &b)) {
      field->sub_fields.clear();
      return DECODE_ERROR;
    }

    field->sub_fields.push_back(sub_field);
  }

  return DECODE_OK;
}

static inline void InitField(Field *field, PbBuf *in) {
  memset(field, 0, sizeof(Field));
  field->tag = GetTag(in);
  field->field_num = GetTagFieldNumber(field->tag);
  field->wire_type = GetTagWireType(field->tag);
}

static void FieldOutput(const Field *field, PbBuf *trace) {
  u_char *parent_last, *raw_last = trace->last;
  trace->last += snprintf(reinterpret_cast<char*>(trace->last),
      PbBufCapacity(trace), "_%d", field->field_num);
  if (!field->sub_fields.empty()) {
    parent_last = trace->last;
    for (const auto &f : field->sub_fields) {
      FieldOutput(&f, trace);
      trace->last = parent_last;
    }
    trace->last = raw_last;
    return;
  }

  string val;
  if (field->wire_type <= WIRETYPE_FIXED64 ||
      field->wire_type == WIRETYPE_FIXED32) {
    val = std::to_string((int64_t)field->number_value);
  } else {
    val.assign((const char*)field->value.pos, PbBufSize(&field->value));
  }

  printf("%.*s : %s\n", PbBufSize(trace), trace->pos, val.c_str());
  trace->last = raw_last;
}

static inline int DecodeField(Field *field, PbBuf *in) {
  switch (field->wire_type) {
    case WIRETYPE_VARINT:
      return DecodeNumber(field, in);

    case WIRETYPE_FIXED64:
      return DecodeFixed64(field, in);

    case WIRETYPE_LENGTH_DELIMITED:
      if (DECODE_OK != DecodeString(field, in)) {
        return DECODE_ERROR;
      }
      // when try decode sub_field failed, then type is string
      TryDecodeSubField(field);
      return DECODE_OK;

    case WIRETYPE_FIXED32:
      return DecodeFixed32(field, in);

    default:
      return DECODE_ERROR;
  }

  return DECODE_ERROR;
}

void Decode(u_char *data, size_t len) {
  PbBuf b, *in;
  b.pos = data;
  b.last = data + len;
  in = &b;
  u_char tmp[1024];
  PbBuf trace;
  trace.pos = trace.start = tmp;
  trace.end = trace.start + 1024;
  trace.pos[0] = 'p';
  trace.pos[1] = 'b';
  trace.last = trace.pos + 2;

  while (PbBufSize(in) > 0) {
    Field field;
    InitField(&field, in);
    if (field.field_num == 0 || field.field_num > MAX_FIELD_NUM) {
      return;
    }

    if (!DecodeField(&field, in)) {
      return;
    }
    if (PbBufSize(in) < 0) {
      return;
    }
    FieldOutput(&field, &trace);
  }
}
