#! /bin/sh
#
# deploy_package.sh
#
checkFile() {
    fileSize=`ls -l $2  | awk '{print $5}'`
    result=`curl -I $1$2`
    code=`echo $result | awk '{print $2}'`
    if [ $code -ne 200 ]; then
        return 1
    fi
    size=`echo $result | awk -F '\r' '{print $5}' | awk -F ':' '{print $2}' | awk '$1=$1'`
    if [ $fileSize -eq $size ]; then
        return 0
    else
        return 1
    fi
}

checkFileExist() {
    result=`curl -I $1$2`
    code=`echo $result | awk '{print $2}'`
    if [ $code -eq 404 ]; then
        return 0
    else
        return 1
    fi
}
VERSION=`date +"%Y-%m-%d"`-`git rev-parse --short HEAD`
sh -x steps/package_fedb.sh $VERSION || exit 1
sh -x steps/package_fedb_pysdk.sh $VERSION || exit 1
sh -x steps/package_fedb_javasdk.sh $VERSION || exit 1

URL="http://pkg.4paradigm.com:81/fedb/dailybuild/"
CHECKURL="http://pkg.4paradigm.com/fedb/dailybuild/"
FILE=fedb-cluster-$VERSION.tar.gz
sh -x steps/upload_to_pkg.sh $URL $FILE
checkFile $CHECKURL $FILE
if [ $? -ne 0 ]; then
    echo "upload package failed"
    exit 1
fi
FILE_PYSDK=fedb-pysdk-$VERSION.tar.gz

sh -x steps/upload_to_pkg.sh $URL $FILE_PYSDK
checkFile $CHECKURL $FILE_PYSDK
if [ $? -ne 0 ]; then
    echo "upload package failed"
    exit 1
fi

