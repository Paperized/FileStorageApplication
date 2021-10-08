runTest() {
    POLICY=$1
    MAINDIR=$(pwd)
    TESTDIR=$MAINDIR/tests
    SCRIPTDIR=$MAINDIR/script

    rm -rf tests/test2/$POLICY && mkdir -p tests/test2/$POLICY
    cd ./tests/test2/$POLICY

    SERVER_PATH="../../../server/bin"
    TMP_SOCKET="tmp_socket.sk"

    echo "SERVER_SOCKET_NAME=tmp_socket.sk
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

    # Write the entire shared_lib folder OK
    # Write current config and log file OK
    # Lock the log file OK -> automatic unlock on exit
    $SCRIPTDIR/utils/run_client.sh 1 $SERVER_SOCKET_PATH "-w $MAINDIR/shared_lib,0 -W ./$POLICY-config.txt,./$POLICY-logs.log -l ./$POLICY-logs.log"
    # Read log file and config OK
    # Remove config file ERROR -> no lock
    # Obtain config lock OK
    # Remove again config OK
    $SCRIPTDIR/utils/run_client.sh 2 $SERVER_SOCKET_PATH "-r ./$POLICY-logs.log,./$POLICY-config.txt -c ./$POLICY-config.txt -l ./$POLICY-config.txt -c ./$POLICY-config.txt"
    # Lock three files OK
    # Remove .keep OK
    # Lock .keep file ERROR
    # Unlock .keep file ERROR
    # Read all files OK -> Files should match the result log file at the end
    $SCRIPTDIR/utils/run_client.sh 3 $SERVER_SOCKET_PATH "-l ./$POLICY-logs.log,$MAINDIR/shared_lib/bin/shared_lib.a,$MAINDIR/shared_lib/bin/.keep -c $MAINDIR/shared_lib/bin/.keep -l $MAINDIR/shared_lib/bin/.keep -u $MAINDIR/shared_lib/bin/.keep -R 0"
    
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