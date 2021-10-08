runTest() {
    POLICY=$1
    MAINDIR=$(pwd)
    TESTDIR=$MAINDIR/tests
    SCRIPTDIR=$MAINDIR/script

    rm -rf tests/test1/$POLICY && mkdir -p tests/test1/$POLICY
    cd ./tests/test1/$POLICY

    SERVER_PATH="../../../server/bin"
    TMP_SOCKET="./tmp_socket.sk"

    echo "SERVER_SOCKET_NAME=$TMP_SOCKET
SERVER_THREAD_WORKERS=1
SERVER_BYTE_STORAGE_AVAILABLE=128MB
SERVER_MAX_FILES_NUM=10000
POLICY_NAME=$POLICY
SERVER_BACKLOG_NUM=10
SERVER_LOG_NAME=$POLICY-logs.log" > $POLICY-config.txt

    valgrind --leak-check=full $SERVER_PATH/server.out ./$POLICY-config.txt &
    SERVER_PID=$!
    sleep 5s

    chmod +x $SCRIPTDIR/utils/run_client.sh

    # Write the entire shared_lib folder OK
    # Write current config and log file OK
    # Lock the log file OK -> automatic unlock on exit
    $SCRIPTDIR/utils/run_client.sh 1 $TMP_SOCKET "-p -t 200 -w $MAINDIR/shared_lib,0 -W ./$POLICY-config.txt,./$POLICY-logs.log -l ./$POLICY-logs.log"
    # Read log file and config OK
    # Remove config file ERROR -> no lock
    # Obtain config lock OK
    # Remove again config OK
    $SCRIPTDIR/utils/run_client.sh 2 $TMP_SOCKET "-p -t 200 -r ./$POLICY-logs.log,./$POLICY-config.txt -c ./$POLICY-config.txt -l ./$POLICY-config.txt -c ./$POLICY-config.txt"
    # Lock three files OK
    # Remove .keep OK
    # Lock .keep file ERROR
    # Unlock .keep file ERROR
    # Read all files OK -> Files should match the result log file at the end
    $SCRIPTDIR/utils/run_client.sh 3 $TMP_SOCKET "-p -t 200 -l ./$POLICY-logs.log,$MAINDIR/shared_lib/bin/shared_lib.a,$MAINDIR/shared_lib/bin/.keep -c $MAINDIR/shared_lib/bin/.keep -l $MAINDIR/shared_lib/bin/.keep -u $MAINDIR/shared_lib/bin/.keep -R 0"
    
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