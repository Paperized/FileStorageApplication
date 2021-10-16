#!/bin/bash
LOG_PATH=$1

if [ ! -f "$LOG_PATH" ] ; then
    echo "No valid log path provided!";
    exit 1
fi

avgOfSum() {
  SUM=0
  N=0
  while read NUM
  do
    ((SUM+=10#$NUM))
    ((N+=1))
  done

  if [[ $N -eq 0 ]] ; then
    echo "0"
  else
    echo "scale=2; $SUM/$N" | bc
  fi
}

avgSumBytesInMB() {
  SUM=0
  N=0
  while read NUM
  do
    ((SUM+=10#$NUM))
    ((N+=1))
  done

  if [[ $N -eq 0 ]] ; then
    echo "0"
  else
    echo "scale=2; $SUM/(1000000*$N)" | bc
  fi
}

NUM_LOGS=$(grep "\- START \-" -c $LOG_PATH)

N_READ=$(grep "OP_READ_FILE" -c $LOG_PATH)
AVG_READ=$(grep "OP_READ_FILE" $LOG_PATH | grep -o "data read [[:digit:]]*" | grep -o "[[:digit:]]*" | avgSumBytesInMB)
N_WRITE=$(grep "OP_WRITE_FILE" -c $LOG_PATH)
AVG_WRITE=$(grep "OP_WRITE_FILE" $LOG_PATH | grep -o "data written [[:digit:]]*" | grep -o "[[:digit:]]*" | avgSumBytesInMB)
N_LOCK=$(grep "OP_LOCK_FILE" -c $LOG_PATH)
N_OPENLOCK=$(grep "OP_OPEN_FILE" $LOG_PATH | grep -c "flags 2\|flags 3")
N_UNLOCK=$(grep "OP_UNLOCK_FILE" -c $LOG_PATH)
N_CLOSE=$(grep "OP_CLOSE_FILE" -c $LOG_PATH)
N_REPLACEMENT=$(grep "OP_REPLACEMENT" -c $LOG_PATH)
MAX_SIZE=$(grep "FINAL_METRICS Max storage size" $LOG_PATH | grep -o "[[:digit:]]*" | avgSumBytesInMB)
MAX_COUNT=$(grep "FINAL_METRICS Max file count" $LOG_PATH | grep -o "[[:digit:]]*" | avgOfSum)
N_MAX_CONN=$(grep "FINAL_METRICS Max clients connected alltogether" $LOG_PATH | grep -o "[[:digit:]]*" | avgOfSum)

echo "> AVG METRICS BASED ON $NUM_LOGS execution of the server (using this log file)"

echo "----- SERVER OPERATIONS ----------"
echo "Number of read: $N_READ"
echo "Average size read: ${AVG_READ}MB"

echo "Number of write: $N_WRITE"
echo "Average size write: ${AVG_WRITE}MB"

echo "Number of lock: $N_LOCK"
echo "Number of open with lock: $N_OPENLOCK"
echo "Number of unlock: $N_UNLOCK"
echo "Number of cache replacement: $N_REPLACEMENT"
echo -e "Number of close: $N_CLOSE\n"

echo "----- SERVER METRICS -----------"
echo "Max clients connected alltogether: $N_MAX_CONN"
echo "Server max size: ${MAX_SIZE}MB"
echo "Server max file count: $MAX_COUNT"
