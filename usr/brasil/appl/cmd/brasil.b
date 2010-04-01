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

logfd: ref Sys->FD;
fsaddr: string;		# file system export
debugaddr: string;	# brasil debug export
csrvaddr: string;	# central services export
backaddr: string;	# back mount address (for Linux)

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

init()
{
	sys = load Sys Sys->PATH;
	sh = load Sh Sh->PATH;
    {	
	if (sh == nil)
		sys->fprint(sys->fildes(2), "Couldn't load shell module\n");
	args := getenv("emuargs");
	arg = load Arg Arg->PATH;
	mode: string;
	
	fsaddr = "tcp!*!5670";		#  file system export
	backaddr = "tcp!127.0.0.1!5640";# csrv backmount export
	
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
			mode = "Server";
		} else {	
			# pull mode from arguments
			mode = hd args;
	
			# next parse our options
			arg->init(args);
			while((c = arg->opt()) != 0)
				case c {
				'h' =>			# fs export addr
					fsaddr = arg->arg();
				'd' =>			# brasil debug addr
					debugaddr = arg->arg();
				'c' =>			# central services addr
					csrvaddr = arg->arg();
				'b' =>			# backmount address
					backaddr = arg->arg();
				* =>
					sys->print("detected option: %c\n", c);
	                  	}		
			args = arg->argv();
		}
		if(args == nil) {
			args = list of {"none"};
		}
	}
	sys->bind("#U*/tmp/", "/tmp", sys->MBEFORE|sys->MCREATE);
	
	# bind log file over stdio
	if(mode == "gateway") {
		logfd = sys->create("/tmp/brasil.log", Sys->OWRITE, 8r666);
		if(logfd == nil)
			sys->print("couldn't open log file: %r\n");
		else {
			sys->dup(logfd.fd, 2);	# override stderr? 
			sys->dup(logfd.fd, 1);	# override stdout? 
		}
	} else
		logfd = sys->fildes(2);
	
	sys->bind("/tmp/brasil.log", "/dev/cons", sys->MREPL);
		
	sh->system(nil, "mount -c {/dis/mntgen.dis} /n"); # setup tmp for us
	sys->bind("#C", "/", sys->MAFTER);
	sys->bind("#e", "/env", sys->MREPL|sys->MCREATE);
	sys->bind("#U*", "/n/local", sys->MREPL|sys->MCREATE);
	if(sys->bind("#I", "/net", sys->MREPL) < 0) {
		# no net might mean we are on Plan 9
		if(sys->bind("/n/local/net", "/net", sys->MREPL) < 0) {
			sys->fprint(logfd, "WARNING: brasil: unable to bind network\n");
		}
	} else {
		sh->system(nil, "/dis/ndb/cs.dis");
	}
	
	sh->system(nil, "mount -c {mntgen} /n/csrv"); # shadow csrv
	sys->bind("#T", "/csrv", sys->MBEFORE);
	sys->bind("#U*", "/csrv/local/fs", sys->MREPL|sys->MCREATE);
	sys->bind("/net", "/csrv/local/net", sys->MREPL);
	
	case mode {
		#"terminal" =>
		#	# initiate a remote ssh and 27b-6 over it
		#	sys->print("terminal");
		"gateway" =>
			# TODO: pass csrv port number
			sh->system(nil, "mount -ca {csrv gw} /csrv");
		* => # server: provide a backmount
			if(csrvaddr != nil) {
				# TODO: pass csrv addr to csrv
				sh->system(nil, "mount -ca {csrv} /csrv");
			}
			if(sys->bind("#₪", "/srv", sys->MREPL|sys->MCREATE) < 0) {
				sh->system(nil, "/dis/styxlisten.dis -A "+backaddr+" export /csrv");
			} else {
				sys->fprint(logfd, "creating srv export\n");
				fd := sys->create("/srv/brasil", Sys->ORDWR, 8r600);
				if(fd == nil)
					sys->fprint(logfd, "creation of srv export failed: %r\n");
				sys->export(fd, "/csrv", Sys->EXPASYNC);
			}		
	}	
	
	# TODO: Get better synchronization
	sys->sleep(5);
	sh->system(nil, "/dis/styxlisten.dis -A "+fsaddr+" export /");
	if(debugaddr != nil){
		sys->fprint(logfd, "exporting to %s\n", debugaddr);
		sh->system(nil, "/dis/styxlisten.dis -A "+debugaddr+" export /");
	}
    } exception e { # abuse exceptions
	"*" =>
    	sys->fprint(logfd, "unexpected exception: %s\n", e);
    	return;
    }
}
