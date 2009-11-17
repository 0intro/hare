#!/bin/sh

NAME=boot9
VERSION=0.3
CONSOLE=mmcs_db_console
LOGPATH=/bgsys/logs/BGP/

function cleanup {
	echo "Cleaning up block $BLOCKID"
	rm -f $MARKER
	kill %1 
	cat | $CONSOLE <<- STOP
	free $BLOCKID
	quit
STOP
	echo "Done."
	END=`date`
	echo "----- $END"
	exit
}

function boot {
echo "Booting $BLOCKID ($MARKER)"
cat | $CONSOLE <<- STOP
	free_block $BLOCKID
	allocate_block $BLOCKID
	redirect $BLOCKID on
	boot_block
	! while [ -f $MARKER ]; do sleep 5; done
	free $BLOCKID
	quit
STOP
echo "Done with boot."
}

function testboot {
	echo "Test Booting $BLOCKID ($MARKER)"
	while [ -f $MARKER ]; do sleep 5; done
	echo "Done with test boot."
}


hostname
pwd
whoami
COBALT_JOBID=$1
MARKER=$HOME/.plan9-$COBALT_JOBID
echo "echo jobid $1"
BLOCKID=`qstat --header Location $COBALT_JOBID | tail -n 1`
touch $MARKER
# should validate blockid somehow
echo "Plan 9 boot script (version $VERSION) for $COBALT_JOBID - block $BLOCKID"
START=`date`
echo "----- $START"

trap cleanup TERM

NODECARD=`echo $BLOCKID | tail -c+5 | head -c10`
echo "if you like: tail -f $LOGPATH$NODECARD-J00.log"
boot &
#testboot &
echo "waiting...."
wait
echo "Exiting..."
