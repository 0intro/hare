#!/bin/bash
#
# deployBrasil.sh : Script to launch the brasil instance
#
# USAGE: 
# deployBrasil.sh [-p brasil_port] [-l hare_location]
#
# ARGUMENTS :
#    -p: port running brasil daemon (should be unique per user)
#    -l: path to the location of hare installation
#
# ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
#   HARE_LOCATION: Location of hare installation 
#				(default location [/bgsys/argonne-utils/profiles/plan9/LATEST/bin/])
#	BRASIL_PORT - port running brasil daemon (should be unique) (default 5670)
#
# By default, Inferno root filesystem will be exported on port 5670.
# User can give different port by passing command line option -p.
#
# EXAMPLES
#   deployBrasil.sh 
#   deployBrasil.sh -p 4433
#   deployBrasil.sh -l /path/to/hare/installation/bin/ -p 4433
#
#

# Loading ENVIRONMENTAL DEFAULTS
pflag=$BRASIL_PORT
lflag=$HARE_LOCATION

while getopts 'p:l:' OPTION
do
	case $OPTION in
	p)	pflag="$OPTARG"
		;;
	l)	lflag="$OPTARG"
		;;
	?)	printf "Usage: %s: [-p brasil_port] [-l hare_location]\n" $(basename $0) >&2
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

if [ -z $lflag ]
then
	lflag="/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
fi
brasil_exec="$lflag/brasil"

if  [ ! -x "$brasil_exec" ]
then
	echo "Could not find the brasil executable at [$brasil_exec]"
	exit 2
fi
arg=`echo "'"tcp!*!$pflag"'"`
$brasil_exec server -h $arg 
#$brasil_exec server -d $arg 

