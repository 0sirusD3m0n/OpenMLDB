#! /bin/sh
#
# ut.sh
#
WORK_DIR=`pwd`
ulimit -c unlimited
test -d reports && rm -rf reports
mkdir -p reports
cp steps/zoo.cfg thirdsrc/zookeeper-3.4.14/conf
cd thirdsrc/zookeeper-3.4.14 && ./bin/zkServer.sh start && cd $WORK_DIR
sleep 5
TMPFILE="code.tmp"
echo 0 > $TMPFILE
mkdir -p core-dumps
ls  build/bin/ | grep test | grep -v grep | while read line 
do 
    ./build/bin/$line --gtest_output=xml:./reports/$line.xml
    RET=$?
    echo "$line result code is: $RET"
    if [ $RET -ne 0 ];then 
        echo $RET > $TMPFILE
    fi 
done
code=`cat $TMPFILE`
echo "code result: $code"
rm $TMPFILE
cd thirdsrc/zookeeper-3.4.14 && ./bin/zkServer.sh stop
cd -
exit $code
