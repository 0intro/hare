#!/bin/sh

NAME=console.sh
VERSION=0.1
CONSOLE=mmcs_db_console
LOGPATH=/bgsys/logs/BGP/

function cleanup {
	echo "Stopping debug for block $BLOCKID"
	rm -f $MARKER
	echo "Done."
	END=`date`
	echo "----- $END"
	exit
}

function debug {
echo "Debugging $BLOCKID ($MARKER)"
cat | $CONSOLE <<- STOP
	select_block $BLOCKID
	redirect $BLOCKID on
	! while [ -f $MARKER ]; do sleep 5; done
	quit
STOP
echo "Done with debug."
}

#
# Add a test, if we are not on the service-node
#
HOST=`hostname`
trap cleanup EXIT
COBALT_JOBID=$1
MARKER=$HOME/.plan9-debug-$COBALT_JOBID
if [ $HOST = sn1 ]; then
	echo "echo jobid $1"
	BLOCKID=`qstat --header Location $COBALT_JOBID | tail -n 1`
	touch $MARKER
	# should validate blockid somehow
	echo "Plan 9 debug script (version $VERSION) for $COBALT_JOBID - block $BLOCKID"

	NODECARD=`echo $BLOCKID | tail -c+5 | head -c10`
	debug
else
	ssh sn1 console.sh $@
fi
