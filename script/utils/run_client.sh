#!/bin/bash
CURRENT_CLIENT=client$1
SOCKET_PATH=$2
PARAMS=$3
POLICYDIR=$(pwd)
CURRENT_CLIENT_PATH=$POLICYDIR/$CURRENT_CLIENT
CLIENTBIN_PATH=$POLICYDIR/../../../client/bin

mkdir -p $CURRENT_CLIENT_PATH
mkdir -p $CURRENT_CLIENT_PATH/readed
mkdir -p $CURRENT_CLIENT_PATH/replaced
$CLIENTBIN_PATH/client.out -f $SOCKET_PATH -d $CURRENT_CLIENT_PATH/readed -D $CURRENT_CLIENT_PATH/replaced $PARAMS