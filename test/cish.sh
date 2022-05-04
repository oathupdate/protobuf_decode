#!/bin/bash

dir=$(cd $(dirname $0);pwd)
cd $dir/..
cmake .
make
cd -

../protobuf/decoder test_data_pb

