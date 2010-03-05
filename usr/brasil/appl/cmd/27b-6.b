implement Twenty7bstroke6;

#
# 27b-6 - paperwork for ducts which encodes hostname in prefix to stream
# and uses it as a mount point on the remote side
# Copyright (c) 2009 Eric Van Hensbergen <ericvh@gmail.com>
#

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "styx.m";
include "duct.m";
	duct: Duct;

Twenty7bstroke6: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

recvhostname(infd: ref Sys->FD): string
{
	buf := array[Styx->MAXRPC] of byte;
	
	count := sys->readn(infd, buf, 4);
	if(count < 4) {
		raise "short read of header packet size";
	}
		
	size := (int buf[1] << 8) | int buf[0];
	size |= ((int buf[3] << 8) | int buf[2]) << 16;
	if(size > Styx->MAXRPC)
		raise "oversized packet";

	count = sys->readn(infd, buf, size);
	if(count <= 0)
		return nil;		
	if(count < size-4) {
		raise "rcvhostname: short read of packet";
	}		
	
	return string buf[0:count];
}

sendhostname(outfd: ref Sys->FD)
{
	buf := array[Styx->MAXRPC] of byte;

	fd := sys->open("/dev/sysname", sys->OREAD);
	if(fd == nil) {
		sys->fprint(sys->fildes(2), "couldn't open /dev/sysname\n");
		return;
	}
	
	n := sys->read(fd, buf[4:], (len buf) - 4);
	if(n < 0) {
		sys->fprint(sys->fildes(2), "short read /dev/sysname\n");
		return;
	}	
 
	buf[0] = byte n;
	buf[1] = byte (n>>8);
	buf[2] = byte (n>>16);
	buf[3] = byte (n>>24);		

	sys->write(outfd, buf, n+4);
}

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	duct = load Duct Duct->PATH;
	infd: ref Sys->FD;
	outfd: ref Sys->FD;
	logfd := sys->create("/tmp/27b-6.log", Sys->OWRITE, 8r666);

	if(logfd == nil)
		raise("fail:YOU SUCK");
	sys->fprint(logfd, "attempting uplink\n");
	
	if (duct == nil)
		sys->fprint(logfd, "couldn't load duct.dis :%r\n");
	
	if(len args != 5) {
		sys->fprint(logfd, "usage: duct <infile> <outfile> <in> <out>\n");
		return;
	}
	
	args = tl args; 
	infile := hd args; args = tl args;
	outfile := hd args; args = tl args;
	in := hd args; args = tl args;
	out := hd args;
	
	if(infile == "-")
		infd = sys->fildes(0);
	else
		infd = sys->open(infile, Sys->OREAD);
		
	if(infd == nil) {
		sys->fprint(logfd, "can't open infile (%s)\n", infile);
		return;
	}
		
	if(outfile == "-")
		outfd = sys->fildes(1);
	else
		outfd = sys->open(outfile, Sys->OWRITE);
		
	if(outfd == nil) {
		sys->fprint(logfd, "can't open outfile (%s)\n", outfile);
		return;
	}	

	sendhostname(outfd);
	inother := recvhostname(infd);
	sys->fprint(logfd, "got me some (%s)\n", inother);
	# NOTE: this requires either csrv to be running or mntgen on the in port
	spawn duct->uplink(infd, outfd, out, in+"/"+inother);
	
	sys->fprint(logfd, "uplinked (%s) (%s)\n", out, in+"/"+inother);
}