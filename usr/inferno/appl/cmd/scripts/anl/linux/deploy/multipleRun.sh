#!/bin/bash
set -e

host="localhost"
port=5544
cmd='"pwd"'

if [ "$#" -eq 3 ]
then
sid=$1
count=$2
ifile=$3
if [ ! -f $ifile ]
then
	echo "input file $ifile is not present, give correct input filename"
	exit 1
fi
goodoutput="goodoutput$sid.txt"
output="output$sid.txt"
if [ -f $goodoutput ]
then
	echo "run number $sid already running, give another one"
	echo "or file $goodoutput is present from previous crashed run, delete it and re-run"
	exit 1
fi
if [ -f $output ]
then
	echo "run number $sid already running, give another one"
	echo "or file $output is present from previous crashed run, delete it and re-run"
	exit 1
fi


else
	echo "USAGE: $0 <run.no> <no.of.resources> <input.file>"
	exit 1
fi

#./runJob.py $host $port $count "pwd" > $goodoutput
./runJob.py $host $port $count "wc -l" $ifile > $goodoutput
cat $goodoutput 
for i in {1..200}
do
echo $i
#./runJob.py $host $port $count "pwd" > $output
./runJob.py $host $port $count "wc -l" $ifile > $output
diff $output $goodoutput
done
echo "commplete"
rm $output
rm $goodoutput

