#!/bin/bash
MAINDIR=$(pwd)
TESTDIR=$MAINDIR/tests
SCRIPTDIR=$MAINDIR/script
DATADIR=$TESTDIR/test1/test_data

rm -rf $DATADIR && mkdir -p $DATADIR

# those files might be a little too big but due to the character filter they are mostly reduced to 1/4 or 1/5
for i in {1..15}
do
   head -c ${i}0KB /dev/urandom | tr -dc 'A-Za-z0-9' > $DATADIR/file$i.txt
done
cp -r $MAINDIR/shared_lib $DATADIR/shared_lib

runTest() {
    POLICY=$1

    rm -rf $TESTDIR/test1/$POLICY && mkdir -p $TESTDIR/test1/$POLICY
    cd $TESTDIR/test1/$POLICY

    SERVER_PATH="../../../server/bin"
    TMP_SOCKET="./tmp_socket.sk"

    echo "SERVER_SOCKET_NAME=$TMP_SOCKET
SERVER_THREAD_WORKERS=4
SERVER_BYTE_STORAGE_AVAILABLE=1MB
SERVER_MAX_FILES_NUM=10
POLICY_NAME=$POLICY
SERVER_BACKLOG_NUM=10
SERVER_LOG_NAME=$POLICY-logs.log" > $POLICY-config.txt

    valgrind --leak-check=full $SERVER_PATH/server.out ./$POLICY-config.txt &
    SERVER_PID=$!
    sleep 5s

    chmod +x $SCRIPTDIR/utils/run_client.sh

    # Write shared lib amd half of the files
    $SCRIPTDIR/utils/run_client.sh 1 $TMP_SOCKET "-p -t 200 -w $DATADIR/shared_lib,0 -W $DATADIR/file1.txt,$DATADIR/file2.txt,$DATADIR/file3.txt,$DATADIR/file4.txt,$DATADIR/file5.txt,$DATADIR/file6.txt,$DATADIR/file7.txt,$DATADIR/file8.txt"
    # Write the other half of the files
    # Read linked_list.h file OK
    # Remove linked_list.h file ERROR -> no lock
    # Obtain linked_list.h lock OK
    # Remove again linked_list.h OK
    $SCRIPTDIR/utils/run_client.sh 2 $TMP_SOCKET "-p -t 200 -W $DATADIR/file9.txt,$DATADIR/file10.txt,$DATADIR/file11.txt,$DATADIR/file12.txt,$DATADIR/file13.txt,$DATADIR/file14.txt,$DATADIR/file15.txt -r $DATADIR/shared_lib/includes/linked_list.h,$DATADIR/shared_lib/includes/packet.h -c $DATADIR/shared_lib/includes/linked_list.h -l $DATADIR/shared_lib/includes/linked_list.h -c $DATADIR/shared_lib/includes/linked_list.h"
    # Lock three files OK
    # Remove queue.h OK
    # Lock queue.h file ERROR
    # Unlock queue.h file ERROR
    # Read all files OK -> Files should match the result log file at the end
    $SCRIPTDIR/utils/run_client.sh 3 $TMP_SOCKET "-p -t 200 -l $DATADIR/shared_lib/includes/linked_list.h,$DATADIR/shared_lib/includes/queue.h,$DATADIR/shared_lib/includes/utils.h -c $DATADIR/shared_lib/includes/queue.h -l $DATADIR/shared_lib/includes/queue.h -u $DATADIR/shared_lib/includes/queue.h -R 0"
    
    kill -s HUP "$SERVER_PID"
    wait $SERVER_PID

    if [ -f "$TMP_SOCKET" ] ; then
        rm "$TMP_SOCKET"
    fi
    cd $MAINDIR
}

echo "------------- RUNNING TEST1 FIFO -------------"
runTest FIFO

echo "------------- RUNNING TEST1  LRU -------------"
runTest LRU

echo "------------- RUNNING TEST1  LFU -------------"
runTest LFU