#!/bin/rc

# fire up a cache
if(~ $cpunode '0') {
	ramcfs -a $fsaddr -n /n/cache
	bind /n/cache/n/local /n/frontend
	bind -c /n/frontend^$fshome /n/home
	bind -c /n/frontend^$plan9root /n/root
}

# bind our shit for now
bind -a /n/root /
bind -b /n/root/power/bin /bin
bind -b /n/root/rc/bin /bin
bind -a /n/root/sys /sys

# start brasil
@{
	rfork
	brasil csrvlite
} &

#need a better sync here
# wait for miniciod to do its thing, need better sync	
sleep 10
times=0
while(! ~ $#times 20 && test ! -e '/srv/csrv') {
	times=($times 1)
	sleep 10 
}

mount /srv/csrv /csrv
bind /csrv /n/csrv
#echo debug 9 > /csrv/local/ctl
#newxlog
# this version only connects front end and io nodes

# first stage
#echo first stage bind
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
	#if(~ $cpunode '0') {
	#	echo mount tcp!$ip(1)^!564 $sysname > /n/brasil/csrv/ctl
	#}
	if(~ $cpunode '0') {
		echo mount tcp!$ip(1)^!564 $sysname > /csrv/parent/ctl
	}	
	if(~ $cpunode '1') {
		echo mount tcp!11.$treeip^!564 $sysname > /csrv/parent/ctl
	}
} &

