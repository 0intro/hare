#!/bin/bash
ROOT="$HOME/src/inferno-os"
EMUBIN=$ROOT/Linux/386/bin
NINEROOT=/n/local/home/$USER/9
MYPORT=1127

exec "$EMUBIN/emu" -s -r "$ROOT" "$@" /dis/sh.dis -c "{
        load std
        or {ftest -e /net/cs} {ndb/cs}
        bind -c '#U*' /n/local
	bind -a '#C' /
	mkdir -p /tmp/lpipe
        bind '#|' /tmp/lpipe
        os ssh login2.surveyor.alcf.anl.gov /home/ericvh/inferno/Linux/power/bin/emu -I -r /home/ericvh/inferno /dis/sshexport  </tmp/lpipe/data1 >/tmp/lpipe/data1 &
        mount -A /tmp/lpipe/data /n/anl
	/dis/styxlisten -A tcp!*!6666 {export /} &
	/dis/styxlisten -A tcp!*!1127 {export /n/anl} &
	while {} {date;sleep 60;}
}"
