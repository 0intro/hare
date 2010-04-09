#!/bin/bash
#
usage () {
printf """
multipleRun.sh : Script to test brasil by running LARGE number of jobs 

USAGE: 
 multipleRun.sh [-h brasil_host] [-p brasil_port] [-l hare_location] [-n no_of_jobs_per_run] [-r no_of_runs] [-u unique_run_id]

ARGUMENTS :
	-h: hostname running brasil (default localhost)
	-p: port running brasil daemon (default 5670)
	-l: path to the location of hare installation
			(default [/bgsys/argonne-utils/profiles/plan9/LATEST/bin/])
	-n: number of jobs per run (default 4)
	-r: number of runs (default 200)

ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
	BRASIL_HOST - host running the brasil daemon (default localhost)
	BRASIL_PORT - port running brasil daemon (default 5670)
	HARE_LOCATION: Location of hare installation 
			(default [/bgsys/argonne-utils/profiles/plan9/LATEST/bin/])

EXAMPLES
  multipleRun.sh 
  multipleRun.sh -r 400 -n 2
  multipleRun.sh -h otherBrasil -p 4433
  multipleRun.sh -l /path/to/hare/installation/bin/ -p 4433

""" >&2

}

#

# Loading ENVIRONMENTAL DEFAULTS

set -e

hflag=$BRASIL_HOST
pflag=$BRASIL_PORT
lflag=$HARE_LOCATION

while getopts 'u:h:p:l:i:c:r:n:' OPTION
do
	case $OPTION in
	h)	hflag="$OPTARG"
		;;
	p)	pflag="$OPTARG"
		;;
	l)	lflag="$OPTARG"
		;;
	u)	uflag="$OPTARG"
		;;
	c)	cflag="$OPTARG"
		;;
	i)	iflag="$OPTARG"
		;;
	n)	nflag="$OPTARG"
		;;
	r)	rflag="$OPTARG"
		;;
	?)	usage
		exit 2
		;;
	esac
done
shift $((OPTIND - 1))

if [ -z $hflag ]
then
	hflag="localhost"
fi

if [ -z $pflag ]
then
	pflag=5670
fi

if [ -z $lflag ]
then
	lflag="/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
fi

if [ -z $rflag ]
then
	rflag=200
fi

if [ -z $nflag ]
then
	nflag=4
fi

if [ -z $cflag ]
then
	cflag="wc -l"
	iflag="$lflag/runJob.py"
fi

if [ -z $uflag ]
then
	uflag=`date +%s`
fi

if [ ! -z $iflag ]
then
	if [ ! -f $iflag ]
	then
		echo "input file $iflag is not present, give correct inpfilename"
		exit 1
	fi
fi

goodoutput="goodoutput$uflag.txt"
output="output$uflag.txt"
if [ -f $goodoutput ]
then
	echo "run number $uflag already running, give another one"
	echo "or file $goodoutput is present from previous crashed run, delete it and re-run"
	exit 1
fi
if [ -f $output ]
then
	echo "run number $uflag already running, give another one"
	echo "or file $output is present from previous crashed run, delete it and re-run"
	exit 1
fi

$lflag/runJob.py -h $hflag -p $pflag -n $nflag "wc -l" $iflag > $goodoutput
cat $goodoutput 

i=1
while [ $i -le $rflag ];
do
	echo $i
$lflag/runJob.py -h $hflag -p $pflag -n $nflag "wc -l" $iflag > $output
	diff $output $goodoutput
	i=`expr $i + 1`
done
rm $output
rm $goodoutput

echo "[$rflag * $nflag] commands executed sucessfully"
