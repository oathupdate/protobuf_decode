# protobuf decoder
decode the protobuf field value without the proto file.

# decode
input: 
```
the proto data file
```

output:
```
key:   pb_${field_num}_${sub_field_num}, when field type is array or assambled, the key same as parent field number.
value: decoded value
```

# buid & run
```
cmake .
make
./decoder intput_file
```

# test
```
test deps: pip install protobuf
sh make.sh : generating protobuf python code by test.proto
python gen.py : generating the name of test_data_pb test proto data
sh cish.sh : run '../decoder test_data_pb' and output decoded fields value
```
