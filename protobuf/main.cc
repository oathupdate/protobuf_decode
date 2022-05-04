// Copyright (cc) 2022 oathupdate. All rights reserved.

#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

using std::string;

extern void Decode(u_char *data, size_t len);

bool ReadFile(const string &file, string *content) {
  if (file.empty() || !content) {
    return false;
  }
  struct stat st;
  int fd = open(file.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  if (fstat(fd, &st) != 0) {
    close(fd);
    return false;
  }

  if (!st.st_size) {
    close(fd);
    return true;
  }
  content->resize(st.st_size);
  if (read(fd, const_cast<char*>(content->data()), content->size())
      != static_cast<ssize_t>(content->size())) {
    close(fd);
    return false;
  }

  close(fd);
  return true;
}
int main(int argc, char **argv) {
  string content;
  if (argc < 2) {
    printf("ERROR: please input source data file\n");
    return 1;
  }
  string file_name = argv[1];
  if (!ReadFile(file_name.c_str(), &content)) {
    printf("ERROR: read file failed\n");
    return 1;
  }

  Decode(reinterpret_cast<u_char*>(const_cast<char*>(content.data())),
         content.size());

  return 0;
}
