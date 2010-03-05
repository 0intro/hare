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
	if (arg == nil)
		sys->fprint(sys->fildes(2), "emuinit: cannot load %s: %r\n", Arg->PATH);
	else{
		arg->init(args);
		while((c := arg->opt()) != 0)
			case c {
			'g' or 'c' or 'C' or 'm' or 'p' or 'f' or 'r' or 'd' =>
				arg->arg();
	                  }
		args = arg->argv();
		if(args == nil) {
			args = list of {"none"};
		}
	}
	
	sys->bind("#U*/tmp/", "/tmp", sys->MBEFORE|sys->MCREATE);
	
	# bind log file over stdio
	logfd = sys->create("/tmp/brasil.log", Sys->OWRITE, 8r666);
	if(logfd == nil)
		sys->print("couldn't open log file: %r\n");
	else {
		sys->dup(logfd.fd, 2);	# override stderr? 
		sys->dup(logfd.fd, 1);	# override stdout? 
	}
	
	sys->bind("/tmp/brasil.log", "/dev/cons", sys->MREPL);
	
	sh->system(nil, "/dis/echo,dis test\n");
	
	sh->system(nil, "mount -c {/dis/mntgen.dis} /n"); # setup tmp for us
	
	sys->bind("#e", "/env", sys->MREPL|sys->MCREATE);
	sys->bind("#T", "/task", sys->MREPL|sys->MCREATE);	
	sys->bind("#U*", "/n/local", sys->MREPL|sys->MCREATE);
	if(sys->bind("#I", "/net", sys->MREPL) < 0) {
		# no net might mean we are on Plan 9
		if(sys->bind("/n/local/net", "/net", sys->MREPL) < 0) {
			sys->fprint(sys->fildes(2), "WARNING: brasil: unable to bind network\n");
		}
	} else {
		sh->system(nil, "/dis/ndb/cs.dis");
	}
	
	sh->system(nil, "mount -c {mntgen} /n/csrv"); # shadow csrv
	sys->bind("#2", "/csrv", sys->MBEFORE);
	sys->bind("#U*", "/csrv/local/fs", sys->MREPL|sys->MCREATE);
	sys->bind("/net", "/csrv/local/net", sys->MREPL);
	
	mode := hd args;
	case mode {
		#"terminal" =>
		#	# initiate a remote ssh and 27b-6 over it
		#	sys->print("terminal");
		"gateway" =>
			sh->system(nil, "mount -ca {csrv gw} /csrv");
			#sh->system(nil, "/dis/27b-6.dis /dev/hoststdin /dev/hoststdout /csrv /csrv >>  /tmp/brasil.log &");
		* =>
			sh->system(nil, "mount -ca {csrv} /csrv");
			if(sys->bind("#₪", "/srv", sys->MREPL|sys->MCREATE) < 0) {
				sys->fprint(logfd,"mounting 9srv failed: %r\n");
				sh->system(nil, "/dis/styxlisten.dis -A tcp!127.0.0.1!6666 export /csrv");
			} else {
				sys->fprint(logfd, "creating srv export\n");
				fd := sys->create("/srv/brasil", Sys->ORDWR, 8r600);
				if(fd == nil)
					sys->fprint(logfd, "creation of srv export failed: %r\n");
				sys->export(fd, "/csrv", Sys->EXPWAIT);
			}		
	}	
	
	#debug (TODO: This should be optional)
	sys->sleep(5);
	sh->system(nil, "/dis/styxlisten.dis -A tcp!*!5670 export /");
    } exception e { # abuse exceptions
	"*" =>
    	sys->fprint(logfd, "unexpected exception: %s\n", e);
    	return;
    }
}
