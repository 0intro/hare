#
# brasil - startup code for brasil mode
# Copyright 2010 Eric Van Hensbergen
# 
# NOTE: This code is awful and needs to be cleaned up and/or re-written
#

implement Emuinit;
include "sys.m";
	sys: Sys;
include "draw.m";
include "sh.m";
	sh: Sh;
include "arg.m";
	arg: Arg;

Emuinit: module
{
	init: fn();
};

# isnt this in env.m?
fsaddr: string;		# file system export

unquoted(s: string): list of string
{
	args: list of string;
	word: string;
	inquote := 0;
	for(j := len s; j > 0;){
		c := s[j-1];
		if(c == ' ' || c == '\t' || c == '\n'){
			j--;
			continue;
		}
		for(i := j-1; i >= 0 && ((c = s[i]) != ' ' && c != '\t' && c != '\n' || inquote); i--){	# collect word
			if(c == '\''){
				word = s[i+1:j] + word;
				j = i;
				if(!inquote || i == 0 || s[i-1] != '\'')
					inquote = !inquote;
				else
					i--;
			}
		}
		args = (s[i+1:j]+word) :: args;
		word = nil;
		j = i;
	}
	# if quotes were unbalanced, balance them and try again.
	if(inquote)
		return unquoted(s + "'");
	return args;
}

getenv(v: string): list of string
{
	fd := sys->open("#e/"+v, Sys->OREAD);
	if (fd == nil)
		return nil;
	(ok, d) := sys->fstat(fd);
	if(ok == -1)
		return nil;
	buf := array[int d.length] of byte;
	n := sys->read(fd, buf, len buf);
	if (n <= 0)
		return nil;
	return unquoted(string buf[0:n]);
}

# redirect standard output and what not to the log file
redirectoutput(s : chan of int)
{
	# this was for log files (only?)
	sys->bind("#U*/tmp/", "/tmp", sys->MBEFORE|sys->MCREATE);

	userfd := sys->open("/dev/user", Sys->OREAD);
	userbuf := array[255] of byte;
	n := sys->read(userfd, userbuf, 255);
	logfd := sys->create("/tmp/"+string userbuf[0:n]+"-brasil.log", Sys->OWRITE, 8r666);
	if(logfd == nil) {
		sys->print("couldn't open log file: %r\n");
		s <-= 1;
	}
	
	kpfd := sys->open("/dev/kprint", Sys->OREAD);
	s <-= 0;
	while(1) {
		n = sys->read(kpfd, userbuf, 255);
		sys->write(logfd, userbuf, n);
	}
}

# Establish a backmount for the host
# Also serves as an external export right now
# and blocks?
backmountexport(srvpath: string, mon: int, debug: int)
{
	# export result to host (via srv on Plan 9 or port on UNIX)
	if(sys->bind("#₪", "/srv", sys->MREPL|sys->MCREATE) < 0) {
		sh->system(nil, "/dis/styxlisten.dis -A "+fsaddr+" export /");
	} else {
		if(debug)
			sh->system(nil, "/dis/styxlisten.dis -A "+fsaddr+" export / &");
		#sys->fprint(sys->fildes(2), "creating srv export %s\n", srvpath);
		fd := sys->create(srvpath, Sys->ORDWR, 8r600);
		if(fd == nil)
			sys->fprint(sys->fildes(2), "creation of srv export failed: %r\n");
		if(mon) {
			sh->system(nil, "/dis/mount.dis {/dis/styxmon.dis {/dis/export /csrv}} /n/remote");
			sys->export(fd, "/n/remote", Sys->EXPWAIT);
		} else
			sys->export(fd, "/csrv", Sys->EXPWAIT);
	}			
}

init()
{
	sys = load Sys Sys->PATH;
	sh = load Sh Sh->PATH;
	
	gateway := 0;
	logging := 0;
	mon := 0;
	debug := 0;
	srvpath := "/srv/csrv";
 
	if (sh == nil)
		sys->fprint(sys->fildes(2), "Couldn't load shell module\n");
	args := getenv("emuargs");
	arg = load Arg Arg->PATH;
	mode: string;
	
	fsaddr = "tcp!*!5670";		#  file system export
	
	if (arg == nil)
		sys->fprint(sys->fildes(2), "emuinit: cannot load %s: %r\n", Arg->PATH);
	else{
		arg->init(args);
		# first parse emu options away
		while((c := arg->opt()) != 0)
			case c {
			'g' or 'c' or 'C' or 'm' or 'p' or 'f' or 'r' or 'd' =>
				arg->arg();
	                  }
		args = arg->argv();
	
		if(args==nil) {
			mode = "simple";
		} else {	
			# pull mode from arguments
			mode = hd args;
	
			# next parse our options
			arg->init(args);
			while((c = arg->opt()) != 0)
				case c {
				'L' =>			# Use Log for output
					logging = 1;
				'D' =>			# do a debug export even on plan9
					debug = 1;
				'9' =>			# turn on 9P monitor
					mon = 1;
				'h' =>			# fs export addr
					fsaddr = arg->arg();
				'M' =>
					srvpath = arg->arg();
				'g' =>			# setup an inbound ssh gateway duct
					gateway++;	
				* =>
					sys->print("detected option: %c\n", c);
	                  	}		
			args = arg->argv();
		}
		if(args == nil) {
			args = list of {"none"};
		}
	}
	
	if(logging) {
		sync := chan of int;
		spawn redirectoutput(sync);
		<- sync;			# technically a return value
	}
	
	sh->system(nil, "mount -c {/dis/mntgen.dis} /n"); # setup tmp for us
	sys->bind("#C", "/", sys->MAFTER);
	sys->bind("#e", "/env", sys->MREPL|sys->MCREATE);
	sys->bind("#U*", "/n/local", sys->MREPL|sys->MCREATE);
	sys->bind("#U*/home", "/n/home", sys->MREPL|sys->MCREATE);
	if(sys->bind("#I", "/net", sys->MREPL) < 0) {
		# no net might mean we are on Plan 9
		if(sys->bind("/n/local/net", "/net", sys->MREPL) < 0) {
			sys->fprint(sys->fildes(2), "WARNING: brasil: unable to bind network\n");
		}
	} else {
		sh->system(nil, "/dis/ndb/cs.dis");
	}
	
	sys->bind("#T", "/csrv", sys->MBEFORE);
	sys->bind("#U*", "/csrv/local/fs", sys->MREPL|sys->MCREATE);
	sys->bind("/net", "/csrv/local/net", sys->MREPL);
	
	case mode {
		"simple" =>
			sh->system(nil, "/dis/styxlisten.dis -A "+fsaddr+" export /");
		"csrvlite" =>
			#sys->fprint(sys->fildes(2), "csrvlite\n");
			sh->system(nil, "/dis/csrv-lite.dis");
			backmountexport(srvpath, mon, debug);
		"terminal" =>
			# TODO: initiate ssh duct to remote system
			backmountexport(srvpath, mon, debug);
		* => # unknown mode
			sys->fprint(sys->fildes(2), "unrecognized mode \n");
			sys->sleep(30);
			getout := sys->open("/dev/sysctl", Sys->OWRITE);
			sys->fprint(getout, "halt\n");
	}	
}
