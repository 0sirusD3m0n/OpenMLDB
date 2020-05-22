#! /bin/sh
#
# compile.sh

WORK_DIR=`pwd`
if [ "$1" = "DEBUG" ]
then
    sed -i '/set(CMAKE_BUILD_TYPE/c\set(CMAKE_BUILD_TYPE Debug)' CMakeLists.txt 
else
    sed -i '/set(CMAKE_BUILD_TYPE/c\set(CMAKE_BUILD_TYPE RelWithDebInfo)' CMakeLists.txt 
fi

sh steps/gen_code.sh
mkdir -p $WORK_DIR/build 
cd $WORK_DIR/build && cmake .. && make -j10 rtidb
code=$?
cd $WORK_DIR
exit $code
