#!/dis/sh.dis
#
# Argonne National Labs BGP Script Liibraries
# Copyright 2009 Eric Van Hensbergen <ericvh@gmail.com>
#

#
# Read in private configuration if it exists, otherwise set defaults
#

CFG=$home/lib/anl.cfg

(if {ftest -e $CFG } {
	run $CFG
}
{
	echo You need to setup your personal ANL configuration
	echo Please edit $CFG before running this again.
	echo '#!/dis/sh.dis' >$CFG
	echo '#ANL Scripts Configuration File' >>$CFG
	echo	'# contains the mapping of Node ID to IP address of component IO Nodes' >>$CFG
	echo 'IOIPDB=$home/src/hare/usr/inferno/lib/surveyor.table' >>$CFG
	echo '# ANL Login Machine' >>$CFG
	echo 'ANLLOGIN=login2.surveyor.alcf.anl.gov' >>$CFG
	echo '# PATH to Inferno on ANL Machine' >>$CFG
	echo 'ANLEMU=/home/ericvh/inferno' >>$CFG
	echo '# path to where you mount your connection to surveyor' >>$CFG	
	echo 'ANLMNT=/n/anl' >>$CFG
	echo '# your username on surveyor' >>$CFG
	echo 'ANLUSR=$user' >>$CFG
	echo '# path to your profile directory on login node' >>$CFG	
	echo 'PROFILEDIR=/home/$ANLUSR/profiles' >>$CFG
	echo '# where you want your session info stored' >>$CFG
	echo 'BGSESSION=$home/lib/bgsession' >>$CFG
}
)

#
# Get a date string broken apart
# (YEAR MONTH DAY) = ${getdate}
#
subfn getdate {
	result =`{os date -u +'%Y %m %d'}
}

#
# IP Address Mapping
#

#
# IO IP Address Mapping
#

subfn ioip {
	IONODEID=$1

	result=${tl `{grep $IONODEID $IOIPDB}}
}

#
# CPU Address Mapping
#

#
# Session Management
# 

#
# Make sure ANL link is active
#

fn check_link {
	if {! ftest -e $ANLMNT/cmd/clone } {
		echo No Uplink Established to Argnonne
		raise failed
	}	
}

#
# Pull Info from qstat or raise an error if too many or too few jobs are running
#

subfn check_jobs {
	# Grab runnig jobs
	# 108470 ericvh running ANL-R00-M1-08-64
	result=`{os -m $ANLMNT/cmd qstat --header JobId:User:State:Location | grep $ANLUSR}

	if {~ $#result 0} {
		echo No Jobs Currently Being Run By $user
		rm -f $home/lib/bgsession/*
		raise failed
	}
}

# Right now setup to handle only a single session on the machine.
# The appropriate bits are squirreled away in the /usr/$USER/lib/bgsession
# directory with subfiles (JobID, KernelScore, NodeID)
#

fn update_session {
	echo Scanning Session Info From Cobalt
	
	# Grab kernel md5sum
	# f9b6c7d3f8.... /home/ericvh/profiles/9bgp.elf
	SCORE=${hd `{os -m $ANLMNT/cmd md5sum $PROFILEDIR/9bgp.elf}}
	CNSCORE = ${hd `{os -m $ANLMNT/cmd md5sum $PROFILEDIR/CNS}}
	ULOADSCORE = ${hd `{os -m $ANLMNT/cmd md5sum $PROFILEDIR/uloader}}
	(JOBID UID STATE NODEID) = ${check_jobs}

	mkdir -p $BGSESSION
	echo -n $SCORE > $BGSESSION/KernelScore
	echo -n $JOBID > $BGSESSION/JobID
	echo -n $NODEID > $BGSESSION/NodeID
	echo -n $CNSCORE > $BGSESSION/CNSScore
	echo -n $ULOADSCORE > $BGSESSION/ULOADScore
}

#
# load session from local file system or scan cobalt if necessary
#

fn load_session {
	check_link

	if {ftest -e $BGSESSION/JobID} {
		JOBID = `{cat $BGSESSION/JobID}
		SCORE = `{cat $BGSESSION/KernelScore}
		NODEID = `{cat $BGSESSION/NodeID}
	}
	(
	if {~ $#JOBID 0} {
		update_session
	}
	{
		if {! ftest -e $ANLMNT/n/local/home/$ANLUSR/.plan9-$JOBID}{
			if {ftest -e $BGSESSION/JobID} {rm -f $BGSESSION/*}
			update_session
		}
	}	
	)
}

fn remove_session {
	rm $BGSESSION/*
}

#
# CPU Helpers
#

#
# return a list of IO IP addresses based on a list of indicies
#

subfn mapio {
	load_session
	load expr
	
	ionodes := ${ioip $NODEID}
	result =
	ifs := ','
	indicies := ${split $*}
	
	for n in $indicies {
		if {~ ${expr $n 0 le} 1} {
			echo Error: IO Node Indicies Starts At 1
			raise failed
		}
		if {~ ${expr $n $#ionodes gt} 1} {
			echo Error: Requested IO Node Out Of Range ($#ionodes IO Nodes present)
			raise failed
		}
		
		
		result = $result ${index $n $ionodes}
	}
}

# Get to an IO Node 
#
# usage: bgpio [-a] [-i n1 [,n2,n3,...] ] [-h <ipaddr>] [cmd]
#   -a: run on All IO Nodes
#   -i n1 [,n2,n3] : specify nodes to run on (by index starting with 1)
#   -r n1 n2 : run command on node indicies between n1 and n2
#   -h ipaddr : specify IP address to connect to
#   [cmd] : command to run on node
#
# When specifying multiple nodes (or all nodes), you must specify a command
# to run as we do not currently manage input multiplexing
#


fn bgpio {
	load_session
	load arg
	load expr
	
	ionodes := ${ioip $NODEID}
	iotargets := ${hd $ionodes}
	iocmd := /bin/rc
	
	args := $*
	(arg
		h+ 	{iotargets=$arg}
		i+ 	{iotargets=${mapio $arg}}
		a 	{iotargets=$ionodes }
		r++	{
				if {! ~ $#arg 2} {
					echo wrong number of arguments
					raise failed
				}
				n1 = ${index 1 $arg}
				n2 = ${index 2 $arg}
				iotargets = ${mapio ${join , ${expr $n1 $n2 seq}}}
			}
				
		'*' 	{echo unknown option $opt}
		- $args
	)
	
	(
	if {! ~ $#* 0 } {
		iocmd = $*
	}
	{
		if {! ~ $#iotargets 1} {
			echo ERROR: No commands specified for multiple targets
			raise failed
		}
	}
	)

	@ {
		bind $ANLMNT/net /net
		for target in $iotargets {
			(
			if {~ $target ${index $#iotargets $iotargets}} {
				9cpu -h $target -c $iocmd -a none
			}
			{
				9cpu -h $target -c $iocmd -a none &
			}
			)
		}
	}
}

# Establish Uplink to an IO Node
#
# (NOTE: external caller should be in private namespace)
# Will bind resulting filesystem to /n/bgpio

fn iouplink {
	load_session
	
	if {! ftest -e /n/bgpio/dev/cons } {
		bind $ANLMNT/net /net
		IONODE=${hd ${ioip $NODEID}}
		mount -A tcp!$IONODE!564 /n/bgpio
	}
}

# Grab personality bits from IO Node

fn bgpinfo {
	load_session
	if {! ftest -e $BGSESSION/personality} {
		@ {
			iouplink
			cat /n/bgpio/dev/personality > $BGSESSION/personality
			cat /n/bgpio/dev/version > $BGSESSION/version
		}
	}
	
	if {! ftest -e $BGSESSION/version} {
		@ {
			iouplink
			cat /n/bgpio/dev/version > $BGSESSION/version
		}
	}
	
	XNODES = ${tl `{grep Xnodes $BGSESSION/personality}}
	YNODES = ${tl `{grep Ynodes $BGSESSION/personality}}
	ZNODES = ${tl `{grep Znodes $BGSESSION/personality}}	
}

#
# Map CPU Coordinate (x, y, z) to IP address
#

subfn cpuip {
	load expr
	
	z = ${expr 1 $3 +}
	result=11.$1.$2.$z
}


# Map an index to a CPU node
#
# Pass an index to a cpu get an IP address
# index starts at 1

subfn mapcpu {
	load expr
	
	index = ${expr $1 1 -}
	if {~ ${expr $index 0 lt} 1} {
		echo Error: Index must start at 1
		raise failed
	}
	plane = ${expr $XNODES $YNODES x}
	
	z = ${expr $index $plane /}
	if {~ ${expr $z $ZNODES gt} 1} {
		echo Error: requested node out of range
		raise failed
	}		
	zrem = ${expr $1 $plane %}
	y = ${expr $zrem $XNODES /}
	if {~ ${expr $y $YNODES gt} 1} {
		echo Error: requested node out of range
		raise failed
	}
	x = ${expr $zrem $XNODES %}
	if {~ ${expr $x $XNODES gt} 1} {
		echo Error: requested node out of range
		raise failed
	}	
	result = ${cpuip $x $y $z}
}

subfn mapcpus {
	load expr 
	
	# TODO: for each argument, process with mapcpu and add to list
	results :=
	ifs := ','
	indicies := ${split $*}

	for n in $indicies {
		if {~ ${expr $n 0 le} 1} {
			echo Error: CPU Node Indicies Starts At 1
			raise failed
		}
		if {~ ${expr $n $cpunodes gt} 1} {
			echo Error: Requested IO Node Out Of Range ($#ionodes IO Nodes present)
			raise failed
		}
		
		result = $result ${mapcpu $n}
	}
}

# Get to a CPU node

fn bgpcpu {
	@ {
		load_session
		load arg
		load expr
	
		iouplink
		bgpinfo

		cpunodes = ${expr $XNODES $YNODES $ZNODES x x}
		cputargets := 11.0.0.1
		cpucmd := /bin/rc
	
		args := $*
		(arg
			h+ 	{cputargets=$arg}
			i+ 	{cputargets=${mapcpus $arg}}
			a 	{cputargets=${mapcpus ${join , ${expr 1 $cpunodes seq}}}}
			r++	{
				if {! ~ $#arg 2} {
					echo wrong number of arguments
					raise failed
				}
				n1 = ${index 1 $arg}
				n2 = ${index 2 $arg}
				cputargets = ${mapcpus ${join , ${expr $n1 $n2 seq}}}
			}			
			'*' {echo unknown option $opt}
			- $args
		)
		
		(
		if {! ~ $#* 0 } {
			cpucmd = $*
		}
		{
			if {! ~ $#cputargets 1} {
				echo ERROR: No commands specified for multiple targets
				raise failed
			}
		}
		)
		
		bind -a /net /n/bgpio/net
		bind /n/bgpio/net /net
		
		for target in $cputargets {
			(
			if {~ $target ${index $#cputargets $cputargets}} {
				9cpu -h $target -c $cpucmd -a none
			}
			{
				9cpu -h $target -c $cpucmd -a none &
			}
			)			
		}
	} $*
}

#
# Blow away everything on our login node
#

fn anl-cleanup {
	os ssh $ANLLOGIN rm -f /home/$ANLUSR/.plan9-*
	os ssh $ANLLOGIN killall5
}

#
# Establish link with surveyor
#
fn anl-uplink {
	anl-cleanup
	
	load std expr string
	args := $*
	s := sh -c ${quote $"args}
	mkdir -p /tmp/lpipe
	bind '#|' /tmp/lpipe
	os ssh $ANLLOGIN $ANLEMU/Linux/power/bin/emu -I -r $ANLEMU /dis/sshexport  </tmp/lpipe/data1 >/tmp/lpipe/data1 &
	mount -A /tmp/lpipe/data $ANLMNT
	# hoping the polling on the tail keeps the mount open
	echo Argonne Uplink Established > $ANLMNT/README.survmount
	tail -f $ANLMNT/README.survmount
}

#
# New Model
#

fn anl-uplink-new {
	#anl-cleanup
	
	# TODO: Make sure csrv is running (?)
	ANLBRASIL=/home/ericvh-laptop/src/ericvh-brasil
	#ANLNEWLOGIN=9.3.45.4
	ANLNEWLOGIN=192.168.51.129
	
	load std expr string
	args := $*
	s := sh -c ${quote $"args}
	bind '#U*' /n/local
	mkdir -p /tmp/lpipe
	bind '#|' /tmp/lpipe
	os ssh $ANLNEWLOGIN $ANLBRASIL/Linux/386/bin/brasil -I gateway  </tmp/lpipe/data1 >/tmp/lpipe/data1 &
	# for now use mntgen
	mount -c {mntgen} /csrv
	/dis/27b-6.dis /tmp/lpipe/data /tmp/lpipe/data /csrv /n/local
	# hoping the polling on the tail keeps the mount open
	#echo Argonne Uplink Established > $ANLMNT/README.survmount
	#tail -f $ANLMNT/README.survmount	
}

#
# Kick off execution
# specify number of nodes as the only argument
#

fn anl-run {
	check_link
	
       (if {~ $#* 0} {
 		numnodes = 2
 	}
	{
		numnodes = $1
 	}
 	)	
	
	os -m $ANLMNT/cmd -d /home/$ANLUSR /home/$ANLUSR/bin/runhare $numnodes
	
	sleep 5
	update_session

	tail -f $ANLMNT/n/local/home/$ANLUSR/$JOBID.output
}

fn anl-regress {
	check_link
	
       (if {~ $#* 0} {
 		numnodes = 2
 	}
	{
		numnodes = $1
 	}
 	)	
	
	os -m $ANLMNT/cmd -d /home/$ANLUSR /home/$ANLUSR/bin/runhare $numnodes
	
	sleep 30
	update_session
}

fn anl-del {
	check_link
	
	load_session
	os -m $ANLMNT/cmd -d /home/$ANLUSR qdel $JOBID
	mkdir -p $ANLMNT/n/local/home/$ANLUSR/old-sessions
	mv $ANLMNT/n/local/home/$ANLUSR/$JOBID.* $ANLMNT/n/local/home/$ANLUSR/old-sessions
	remove_session
}

fn anl-qstat {
	check_link
	os -m $ANLMNT/cmd -d /home/$ANLUSR qstat
}

# BUG: can't do it from inside inferno apparently without a terminal
fn anl-debug {	
	echo Doesnt currently work because jtagdebug wants a real terminal
	raise failed
	check_link
	load_session
	os ssh $ANLLOGIN jtagdebug $JOBID
}

