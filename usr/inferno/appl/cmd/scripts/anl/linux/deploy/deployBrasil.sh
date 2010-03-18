#!/bin/bash
set -e
# This script should be executed from the directory where this file is found.
BrasilPath="../../../../../../../brasil/"
port=5544

if [ "$#" -eq 1 ]
then
port=$1
fi
echo "using local port $port for listening"
cd $BrasilPath
arg=`echo "'"tcp!*!$port"'"`
./Linux/386/bin/brasil server -d $arg 
