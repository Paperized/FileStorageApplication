#!/bin/bash

################################################################################################
# This script runs the first test case, it generates some random data and a couple of files    #
# copied by the current project folder code to carry out the test.                             #
# Each file will call the run_client.sh script which will create automatically all folder      #
# needed to save all files during this test, in the end this will generate a log file inside   #
# the test folder which can be readed easily with the statistiche.sh script                    #
################################################################################################


MAINDIR=$(pwd)
TESTDIR=$MAINDIR/tests
SCRIPTDIR=$MAINDIR/script
DATADIR=$TESTDIR/test2/test_data

rm -rf $DATADIR && mkdir -p $DATADIR

# those files might be a little too big but due to the character filter they are mostly reduced to 1/4 or 1/5
for i in {1..10}
do
   head -c ${i}00KB /dev/urandom | tr -dc 'A-Za-z0-9' > $DATADIR/file$i.txt
done

runTest() {
    POLICY=$1

    rm -rf $TESTDIR/test2/$POLICY && mkdir -p $TESTDIR/test2/$POLICY
    cd $TESTDIR/test2/$POLICY

    SERVER_PATH="../../../server/bin"
    TMP_SOCKET="./tmp_socket.sk"

    echo "SERVER_SOCKET_NAME=$TMP_SOCKET
SERVER_THREAD_WORKERS=4
SERVER_BYTE_STORAGE_AVAILABLE=1MB
SERVER_MAX_FILES_NUM=10
POLICY_NAME=$POLICY
SERVER_BACKLOG_NUM=10
SERVER_LOG_NAME=$POLICY-logs.log" > $POLICY-config.txt

    $SERVER_PATH/server.out ./$POLICY-config.txt &
    SERVER_PID=$!
    sleep 5s

    chmod +x $SCRIPTDIR/utils/run_client.sh

    # Write all files multiple times to test the replacement
    # I read random files to change the frenquency of some files (for LRU test)
    $SCRIPTDIR/utils/run_client.sh 1 $TMP_SOCKET "-p -w $DATADIR,0 -r $DATADIR/file1.txt,$DATADIR/file2.txt,$DATADIR/file9.txt,$DATADIR/file1.txt"
    $SCRIPTDIR/utils/run_client.sh 2 $TMP_SOCKET "-p -w $DATADIR,0 -r $DATADIR/file1.txt,$DATADIR/file5.txt,$DATADIR/file9.txt,$DATADIR/file4.txt"
    $SCRIPTDIR/utils/run_client.sh 3 $TMP_SOCKET "-p -w $DATADIR,0"
    
    kill -s HUP "$SERVER_PID"
    wait $SERVER_PID

    if [ -f "$TMP_SOCKET" ] ; then
        rm "$TMP_SOCKET"
    fi
    cd $MAINDIR
}

echo "------------- RUNNING TEST2 FIFO -------------"
runTest FIFO

echo "------------- RUNNING TEST2  LRU -------------"
runTest LRU

echo "------------- RUNNING TEST2  LFU -------------"
runTest LFU