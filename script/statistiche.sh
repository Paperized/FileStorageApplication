LOG_PATH=$1
for i in {1..9}
do 
    head -c ${i}0MB /dev/urandom | tr -dc 'A-Za-z0-9' > ./file${i}.txt
done

if [ ! -f "$LOG_PATH" ] ; then
    echo "No valid log path provided!";
    exit 1
fi

