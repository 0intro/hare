#!/dis/sh.dis

ANLLOGIN=surveyor.alcf.anl.gov
ANLEMU=/bgsys/argonne-utils/profiles/plan9/inferno

ANLMNT=/n/anl
ANLPORT=1127

echo cleaning previous state from ANL
os ssh $ANLLOGIN rm -f ~/.plan9-*
os ssh $ANLLOGIN killall5

load std expr string

mount {mntgen} /n

mkdir -p /tmp/lpipe
bind '#|' /tmp/lpipe
os ssh $ANLLOGIN $ANLEMU/Linux/power/bin/emu -I -r $ANLEMU /dis/sshexport  </tmp/lpipe/data1 >/tmp/lpipe/data1 &
mount -A /tmp/lpipe/data $ANLMNT
# re-export it
/dis/styxlisten -A tcp!*!$ANLPORT {export $ANLMNT} > /dev/null 2> /dev/null
# hoping the polling on the tail keeps the mount open
echo Argonne Uplink Established > $ANLMNT/README.survmount
tail -f $ANLMNT/README.survmount
# if this fails, we will exit and restart
echo Argonne Uplink Interrupted
echo halt > /dev/sysctl
