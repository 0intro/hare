#!/bin/bash
#
usage () {
printf """
deployBrasil.sh : Script to launch the brasil instance

USAGE: 
deployBrasil.sh [-p brasil_port] [-l hare_location] [-f]

ARGUMENTS :
   -p: port running brasil daemon (should be unique per user)
   -l: path to the location of hare installation
   -f: runs brasil in foreground (by default it runs in background)

ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
	HARE_LOCATION: Location of hare installation 
			(default location [/bgsys/argonne-utils/profiles/plan9/LATEST/bin/])
	BRASIL_PORT - port running brasil daemon (should be unique) (default 5670)

By default, Inferno root filesystem will be exported on port 5670.
User can give different port by passing command line option -p.

EXAMPLES
  deployBrasil.sh 
  deployBrasil.sh -p 4433
  deployBrasil.sh -l /path/to/hare/installation/bin/ -p 4433

""" >&2

}

# Loading ENVIRONMENTAL DEFAULTS
pflag=$BRASIL_PORT
lflag=$HARE_LOCATION

while getopts 'fbp:l:' OPTION
do
	case $OPTION in
	p)	pflag="$OPTARG"
		;;
	l)	lflag="$OPTARG"
		;;
	b)	echo "Running without any arguments"	
		bflag="true"
		;;
	f)	echo "Running in forground"	
		fflag="true"
		;;
	?)	usage 
		exit 2
		;;
	esac
done
shift $((OPTIND - 1))

# SET DEFAULTS IF USER DOESNT SPECIFY
if [ -z $pflag ]
then
	printf "\nWARNING: using default port (5670)\nPlease set using BRASIL_PORT environment variable or -p command line argument\n\n"
	pflag=5670
fi

# checking if the port is already in use
if netstat -ntap 2> /dev/null | grep $pflag
then
	printf "ERROR: port $pflag is already in use.\nPlease use another port\n\n"
	exit 2
fi

if [ -z $lflag ]
then
	lflag="/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
fi

# Making sure that executable exists
brasil_exec="$lflag/brasil"
if  [ ! -x "$brasil_exec" ]
then
	echo "Could not find the brasil executable at [$brasil_exec]"
	exit 2
fi

# Launch the brasil
arg=`echo "'"tcp!*!$pflag"'"`


if [ ! -z $bflag ]
then
	$brasil_exec 
fi
if [ -z $fflag ]
then
	$brasil_exec server -h $arg < /dev/null > /dev/null &
else
	$brasil_exec server -h $arg 
fi
#$brasil_exec server -d $arg 

