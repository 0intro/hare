#!/bin/rc

echo Hello, squidboy

# bind our shit for now
bind -a /n/root /
bind -b /n/root/power/bin /bin
bind -b /n/root/rc/bin /bin

# start brasil
@{
	rfork
	brasil csrvlite
} &

# wait for miniciod to do its thing, need better sync	
echo -n waiting for brasil to come up
sleep 10
times=0
while(! ~ $#times 20 && test ! -e '/srv/csrv') {
	echo -n '.'
	times=($times 1)
	sleep 10 
}
echo done

mount /srv/csrv /csrv

# this version only connects front end and io nodes

# first stage
if(~ $cpunode '0'){
	echo bind /n/local/n/brasil/csrv parent > /csrv/ctl
}
if(~ $cpunode '1'){
	echo bind /n/local/n/io/csrv parent > /csrv/ctl
}

# second stage (needs to run after we setup our services)
@{
	rfork
	sleep 10
	if(~ $cpunode '0') {
		echo mount tcp!$ip(1)^!564 $sysname > /csrv/parent/ctl
	}	
	if(~ $cpunode '1') {
		echo mount tcp!11.$treeip^!564 $sysname > /csrv/parent/ctl
	}
} &

