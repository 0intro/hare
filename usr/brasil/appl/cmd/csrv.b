# Central Services
# Copyright (c) 2009 Eric Van Hensbergen <ericvh@gmail.com>
#
# This module will take care of auto-uplinking a duct to a referenced 
# node.
#
# Based heavily on mntgen
#

implement Csrv;
include "sys.m";
	sys: Sys;
include "draw.m";
include "styx.m";
	styx: Styx;
	Rmsg, Tmsg: import styx;
include "styxservers.m";
	styxservers: Styxservers;
	Ebadfid, Enotfound, Eopen, Einuse: import Styxservers;
	Styxserver, readbytes, Navigator, Fid: import styxservers;

	nametree: Nametree;
	Tree: import nametree;
	
include "duct.m";
	duct: Duct;

Csrv: module {
	init: fn(nil: ref Draw->Context, argv: list of string);
};

Qroot: con big 16rfffffff;
Csrv_port := 1127;
Csrv_prefix := "/csrv";

badmodule(p: string)
{
	sys->fprint(sys->fildes(2), "cannot load %s: %r\n", p);
	raise "fail:bad module";
}
DEBUG: con 0;

Entry: adt {
	refcount: int;
	path: big;
};
refcounts := array[10] of Entry;
tree: ref Tree;
nav: ref Navigator;

rootfid: int;	# single unique fid for root mount point for now
mypid: int;	# mypid which will be used to cleanup pgrp
uniq: int;

logfile: ref Sys->FD;

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	styx = load Styx Styx->PATH;
	if (styx == nil)
		badmodule(Styx->PATH);
		
	mypid = sys->pctl(Sys->NEWPGRP, nil);	
		
	styx->init();
	styxservers = load Styxservers Styxservers->PATH;
	if (styxservers == nil)
		badmodule(Styxservers->PATH);
	styxservers->init(styx);
	
	nametree = load Nametree Nametree->PATH;
	if (nametree == nil)
		badmodule(Nametree->PATH);
	nametree->init();
	
	duct = load Duct Duct->PATH;
	if (duct == nil)
		badmodule(Duct->PATH);

	navop: chan of ref Styxservers->Navop;
	(tree, navop) = nametree->start();
	nav = Navigator.new(navop);
	(tchan, srv) := Styxserver.new(sys->fildes(0), nav, Qroot);

	tree.create(Qroot, dir(".", Sys->DMDIR | 8r555, Qroot));
	
	logfile = sys->create("/tmp/csrv.log", Sys->OWRITE, 8r666);
	spawn waitforit("tcp!*!1127");
		
	if(len args > 1) {
		sys->fprint(logfile, "gateway mode detected\n");
		if(hd tl args == "gw")
			spawn gwconnect();
	}

	for (;;) {
		gm := <-tchan;
		if (gm == nil) {
			tree.quit();
			exit;
		}
		e := handlemsg(gm, srv, tree);
		if (e != nil)
			srv.reply(ref Rmsg.Error(gm.tag, e));
	}
}

killgrp()
{
	fd := sys->open("/prog/"+string mypid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "killgrp\n");
	exit;
}


walk1(c: ref Fid, name: string): string
{
	if (name == ".."){
		if (c.path != Qroot)
			decref(c.path);
		c.walk(Sys->Qid(Qroot, 0, Sys->QTDIR));
	} else if (c.path == Qroot) {
		(d, nil) := nav.walk(c.path, name);
		if (d == nil) {
			d = addentry(name, 1);
			if (d == nil)
				return Enotfound;
		} else
			incref(d.qid.path);
		c.walk(d.qid);
	} else
		return Enotfound;
	return nil;
}

handlemsg(gm: ref Styx->Tmsg, srv: ref Styxserver, nil: ref Tree): string
{
	pick m := gm {
	Attach =>
		rootfid = m.fid;
		srv.default(gm);
	Walk =>
		c := srv.getfid(m.fid);
		if(c == nil)
			return Ebadfid;
		if(c.isopen)
			return Eopen;
		if(m.newfid != m.fid){
			nc := srv.newfid(m.newfid);
			if(nc == nil)
				return Einuse;
			c = c.clone(nc);
			incref(c.path);
		}
		qids := array[len m.names] of Sys->Qid;
		oldpath := c.path;
		oldqtype := c.qtype;
		incref(oldpath);
		for (i := 0; i < len m.names; i++){
			err := walk1(c, m.names[i]);
			if (err != nil){
				if(m.newfid != m.fid){
					decref(c.path);
					srv.delfid(c);
				}
				c.path = oldpath;
				c.qtype = oldqtype;
				if(i == 0)
					return err;
				srv.reply(ref Rmsg.Walk(m.tag, qids[0:i]));
				return nil;
			}
			qids[i] = Sys->Qid(c.path, 0, c.qtype);
		}
		decref(oldpath);
		srv.reply(ref Rmsg.Walk(m.tag, qids));
	Clunk =>
		c := srv.clunk(m);
		if(m.fid == rootfid) {
			killgrp();		
		}
			
		if (c != nil && c.path != Qroot)
			decref(c.path);
	* =>
		srv.default(gm);
	}
	return nil;
}

#
# conn = 1 to establish a duct, conn = 0 to just create mnt
#

addentry(name: string, conn: int): ref Sys->Dir
{
	# new - establish 27b-6 duct to remote specified by name
	if((conn) && (netconnect(name) < 0))
		return nil;

	for (i := 0; i < len refcounts; i++)
		if (refcounts[i].refcount == 0)
			break;
	if (i == len refcounts) {
		refcounts = (array[len refcounts * 2] of Entry)[0:] = refcounts;
		for (j := i; j < len refcounts; j++)
			refcounts[j].refcount = 0;
	}
	d := dir(name, Sys->DMDIR|8r555, big i | (big uniq++ << 32));
	tree.create(Qroot, d);
	refcounts[i] = (1, d.qid.path);
		
	return ref d;
}

incref(q: big)
{
	id := int q;
	if (id >= 0 && id < len refcounts){
		refcounts[id].refcount++;
	}
}

decref(q: big)
{
	id := int q;
	if (id >= 0 && id < len refcounts){
		if (--refcounts[id].refcount == 0)
			tree.remove(refcounts[id].path);
	}
}

Blankdir: Sys->Dir;
dir(name: string, perm: int, qid: big): Sys->Dir
{
	d := Blankdir;
	d.name = name;
	d.uid = "me";
	d.gid = "me";
	d.qid.path = qid;
	if (perm & Sys->DMDIR)
		d.qid.qtype = Sys->QTDIR;
	else
		d.qid.qtype = Sys->QTFILE;
	d.mode = perm;
	return d;
}

recvhostname(infd: ref Sys->FD): string
{
	buf := array[Styx->MAXRPC] of byte;
	
	count := sys->readn(infd, buf, 4);
	if(count < 4) {
		sys->fprint(sys->fildes(2), "readn returned: %d %r\n", count);
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

#listener
waitforit(addr: string)
{
	(err, c) := sys->announce(addr);
	if(err < 0) {
		sys->fprint(sys->fildes(2), "csrv: can't announce %s: %r\n", addr);
		raise "fail:dial";
	}
	
	while(1) {
		(err2, dest) := sys->listen(c);
		if(err2 < 0) {
			sys->fprint(sys->fildes(2), "csrv: can't listen %s: %r\n", addr);
			raise "fail:listen";	
		} else {
			dest.dfd = sys->open(dest.dir + "/data", Sys->ORDWR);
			if (dest.dfd == nil) {
				sys->fprint(sys->fildes(2), "listen: cannot open %s: %r\n", dest.dir + "/data");
				raise "fail: stupidity";
			} else {
				if(dest.cfd != nil)
					sys->fprint(dest.cfd, "keepalive");
			}		
			out := Csrv_prefix;
			sendhostname(dest.dfd);
			in := recvhostname(dest.dfd);
			foo := Csrv_prefix+"/"+in;
			addentry(in, 0);
			spawn duct->uplink(dest.dfd, dest.dfd, out, foo);
		}
	}
}

# passthe hostname we are trying to connect to
netconnect(name: string) :int
{
	addr := "tcp!"+name+"!"+string Csrv_port;
	(ok, dest) := sys->dial(addr, nil);
	if(ok < 0){
		sys->fprint(sys->fildes(2), "cserv: can't dial %s: %r\n", addr);
		return -1;
	}
	return uplink(dest.dfd, dest.dfd, name);
}

gwconnect()
{
	{
	infd := sys->open("/dev/hoststdin", Sys->OREAD);
	if(infd == nil)
		sys->fprint(logfile, "bogus infile\n");	
	outfd := sys->open("/dev/hoststdout", Sys->OWRITE);
	if(outfd == nil)
		sys->fprint(logfile, "bogus outfile\n");
		
	sys->sleep(5); # TODO: need better sync

	uplink(infd, outfd, nil);
	} exception e {
	"*" =>
    		sys->fprint(logfile, "unexpected exception: %s\n", e);
    		return;
	}
}

uplink(infd: ref Sys->FD, outfd: ref Sys->FD, name: string) :int
{
	out := Csrv_prefix;
	sendhostname(outfd);
	in := recvhostname(infd);
	if(name == nil) {			# gw mode hack
		name = in;			# TODO: need to prefix with /csrv douchebag
		sys->fprint(logfile, "making a new point for dir %s == %s\n", in, name);
		addentry(in, 0);		
	}
		
	fullpath := Csrv_prefix+"/"+name;

	sys->fprint(logfile, "uplink %s %s\n", out, fullpath);	
	spawn duct->uplink(infd, outfd, out, fullpath);
	
	# need some kind of better sync
	sys->sleep(5); # TODO: need better sync
	
	return 0;
}
