#!/bin/bash
MAINDIR=$(pwd)
NCLIENTS=$1
TESTDIR=$MAINDIR/tests
SCRIPTDIR=$MAINDIR/script
DATADIR=$TESTDIR/test3/test_data

rm -rf $DATADIR && mkdir -p $DATADIR

# those files might be a little too big but due to the character filter they are mostly reduced to 1/4 or 1/5
for i in $(seq 1 $NCLIENTS)
do
   head -c ${i}0MB /dev/urandom | tr -dc 'A-Za-z0-9' > $DATADIR/file$i.txt
done

cp -r $MAINDIR/shared_lib $DATADIR/shared_lib
cp -r $MAINDIR/client $DATADIR/client
cp -r $MAINDIR/server $DATADIR/server
other_folders=($DATADIR/shared_lib $DATADIR/client $DATADIR/server)

runTest() {
    keepExecuting() {
        ID=$1
        SK=$2
        PARAMS=$3
        while true
        do
            $SCRIPTDIR/utils/run_client.sh $ID $SK "$PARAMS"
        done
    }

    POLICY=$1

    rm -rf $TESTDIR/test3/$POLICY && mkdir -p $TESTDIR/test3/$POLICY
    cd ./tests/test3/$POLICY

    SERVER_PATH="../../../server/bin"
    TMP_SOCKET="./tmp_socket.sk"

    echo "SERVER_SOCKET_NAME=tmp_socket.sk
SERVER_THREAD_WORKERS=8
SERVER_BYTE_STORAGE_AVAILABLE=32MB
SERVER_MAX_FILES_NUM=100
POLICY_NAME=$POLICY
SERVER_BACKLOG_NUM=10
SERVER_LOG_NAME=$POLICY-logs.log" > $POLICY-config.txt

    #gdb -ex "run ./$POLICY-config.txt" -ex bt -ex quit $SERVER_PATH/server.out &
    valgrind --leak-check=full $SERVER_PATH/server.out ./$POLICY-config.txt &
    SERVER_PID=$!
    sleep 5s

    sleep 30 && kill -s INT "$SERVER_PID" &
    chmod +x $SCRIPTDIR/utils/run_client.sh

    clients=()
    for i in $(seq 1 $NCLIENTS)
    do
        keepExecuting $i $TMP_SOCKET "-t 0 -W $DATADIR/file${i}.txt -w ${other_folders[$(($RANDOM % 3))]},0 -R 0" &
        clients+=($!)
    done
    
    wait $SERVER_PID
    for i in ${clients[@]}
    do
        kill $i
        wait $i
    done

    killall -q client/bin/client.out

    if [ -f "$TMP_SOCKET" ] ; then
        rm "$TMP_SOCKET"
    fi
    cd $MAINDIR
}

echo "------------- RUNNING TEST3 FIFO -------------"
runTest FIFO

#echo "------------- RUNNING TEST3  LRU -------------"
#runTest LRU

#echo "------------- RUNNING TEST3  LFU -------------"
#runTest LFU