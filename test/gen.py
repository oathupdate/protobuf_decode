#! /usr/bin/env python

import test_pb2 as test

message = test.Test()
message.test_int32_1 = 1
message.test_int32_2 = -1024
message.test_int32_3 = 123456789
message.test_string_1 = 'test_string_1'
message.test_string_3 = ''
message.test_array.append('array1')
message.test_array.append('array2')
struct = message.struct.add()
struct.struct_string = 'struct_string'
struct.struct_int32 = 520

file = open('test_data_pb', 'w')
file.write(message.SerializeToString())
file.close()
