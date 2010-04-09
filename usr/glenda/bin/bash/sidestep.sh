#!/bin/bash

MARKER=$HOME/.plan9-$COBALT_JOBID

function cleanup {
        echo "Cleaning up block $BLOCKID"
        rm -f $MARKER
        kill %1
        quit
STOP
        echo "Done."
        END=`date`
        echo "----- $END"
        exit
}

trap cleanup TERM

ssh sn1-mgmt /home/$USER/bin/myjob.sh $COBALT_JOBID &

wait
