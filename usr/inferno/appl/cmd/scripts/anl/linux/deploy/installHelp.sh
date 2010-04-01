#!/bin/bash
#
# installHelp.sh : Script to help installation of  hare binaries to  proper location
# 
# USAGE: 
# ./installHelp.sh [-l hare_location]
#
# WARNING : 
#	This script *MUST* be executed from the same directory where is is found,
#		as it depends on "pwd" for locating other files.
# 
#
# ARGUMENTS :
#    -l: path to the location where hare binaries will be installed.
#
# ENVIRONMENT VARIABLES: (Can be used to override defaults in the place of cmdline)
#   HARE_LOCATION: path to the location where hare binaries will be installed. 
# 
# 
set -e
# ENVIRONMENTAL DEFAULTS
lflag=$HARE_LOCATION

while getopts 'l:' OPTION
do
	case $OPTION in
	l)	lflag="$OPTARG"
		;;
	?)	printf "Usage: %s: [-l hare_location]\n" $(basename $0) >&2
		exit 2
		;;
	esac
done
shift $((OPTIND - 1))

# SET DEFAULTS IF USER DOESNT SPECIFY
if [ -z $pflag ]
then
	lflag="/bgsys/argonne-utils/profiles/plan9/LATEST/bin/"
fi
mkdir -p $lflag

hare_src_dir="../../../../../../../../" 

# Find brasil executable
brasil_location=`find "$hare_src_dir/usr/brasil/Linux/" -name brasil`
echo "brasil exec location is $brasil_location"

# making sure that it is executable
if [ ! -x $brasil_location ]
then
	echo "ERROR: Could not find brasil executable at $brasil_location"
	echo "Make sure that path is correct and code is compiled."
	exit 1
fi

# copy it to destination
cp -f $brasil_location $lflag
echo "Copied brasil"


deploy_script_location="/usr/inferno/appl/cmd/scripts/anl/linux/deploy/"

# copy the deployBrasil.sh script.
script_name="deployBrasil.sh"
src="$hare_src_dir/$deploy_script_location/$script_name"

# making sure that it is executable
if [ ! -x $src ]
then
	echo "ERROR: Could not find $script_name executable at $src."
	echo "Make sure that path is correct."
	exit 1
fi

# copy it to destination
cp -f $src $lflag
echo "Copied $script_name"


# copy the runJob.py script.
script_name="runJob.py"
src="$hare_src_dir/$deploy_script_location/$script_name"

# making sure that it is executable
if [ ! -x $src ]
then
	echo "ERROR: Could not find $script_name executable at $src."
	echo "Make sure that path is correct."
	exit 1
fi

# copy it to destination
cp -f $src $lflag
echo "Copied $script_name"


# copy the py9p directory.
dir_name="py9p"
src="$hare_src_dir/$deploy_script_location/$dir_name"

# making sure that it is a directory 
if [ ! -d $src ]
then
	echo "ERROR: Could not find $dir_name directory at $src."
	echo "Make sure that path is correct."
	exit 1
fi

# copy it to destination
cp -rf $src $lflag
echo "Copied $dir_name"

echo "Done..."
