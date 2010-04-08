# Central Services (light)
# Copyright (c) 2010 Eric Van Hensbergen <ericvh@gmail.com>
#
# This code explicitly manages a mount table for a cluster of service
# providers.
#
# based heavily on mntgen code
#
# usage:
# 	mount {csrv} /csrv
#

implement Csrvlite;
include "sys.m";
	sys: Sys;
include "draw.m";
include "styx.m";
	styx: Styx;
	Rmsg, Tmsg: import styx;
include "styxservers.m";
	styxservers: Styxservers;
	Ebadfid, Enotopen, Enotfound, Eopen, Einuse: import Styxservers;
	Styxserver, readbytes, Navigator, Fid: import styxservers;

	nametree: Nametree;
	Tree: import nametree;
include "string.m";
	str: String;

Csrvlite: module {
	init: fn(nil: ref Draw->Context, argv: list of string);
};

csrvroot := "/csrv/"; # primary mount point for csrv

Qroot: con big 16rfffffff;
Qctl: big;			# path to ctl file

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

uniq: int;

mntgeninit(fspipe: ref Sys->FD)
{
	styx = load Styx Styx->PATH;
	if (styx == nil)
		badmodule(Styx->PATH);
	styx->init();
	styxservers = load Styxservers Styxservers->PATH;
	if (styxservers == nil)
		badmodule(Styxservers->PATH);
	styxservers->init(styx);
 
	nametree = load Nametree Nametree->PATH;
	if (nametree == nil)
		badmodule(Nametree->PATH);
	nametree->init();

	navop: chan of ref Styxservers->Navop;
	(tree, navop) = nametree->start();
	nav = Navigator.new(navop);
	(tchan, srv) := Styxserver.new(fspipe, nav, Qroot);

	tree.create(Qroot, dir(".", Sys->DMDIR | 8r555, Qroot));
	
	ctld := addentry("ctl", 8r222);
	Qctl = ctld.qid.path;
	
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

walk1(c: ref Fid, name: string): string
{
	if (name == ".."){
		if (c.path != Qroot)
			decref(c.path);
		c.walk(Sys->Qid(Qroot, 0, Sys->QTDIR));
	} else if (c.path == Qroot) {
		(d, nil) := nav.walk(c.path, name);
		if (d == nil)
			return Enotfound;
		else
			incref(d.qid.path);
		c.walk(d.qid);
	} else
		return Enotfound;
	return nil;
}

handlemsg(gm: ref Styx->Tmsg, srv: ref Styxserver, nil: ref Tree): string
{
	pick m := gm {
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
	Write =>
		c := srv.getfid(m.fid);
		if(c == nil)
			return Ebadfid;
		if(!c.isopen)
			return Enotopen;
		if(c.path == Qctl) {
			error := ctlworker(m.data);
			if(error==nil) {
				#sys->print("sending reply: %d\n", len m.data);
				srv.reply(ref Rmsg.Write(m.tag, len m.data));
				return nil;
			} else
				return error;
		}
		srv.default(gm);
	Clunk =>
		c := srv.clunk(m);
		if (c != nil && c.path != Qroot && c.path != Qctl)
			decref(c.path);
	* =>
		srv.default(gm);
	}
	return nil;
}

addentry(name: string, mode: int): ref Sys->Dir
{
	for (i := 0; i < len refcounts; i++)
		if (refcounts[i].refcount == 0)
			break;
	if (i == len refcounts) {
		refcounts = (array[len refcounts * 2] of Entry)[0:] = refcounts;
		for (j := i; j < len refcounts; j++)
			refcounts[j].refcount = 0;
	}
	d := dir(name, mode, big i | (big uniq++ << 32));
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

#
# sloppy: wrapper to allow spawn
# TODO: need to rethink this at some point
# and do this properly so we can return error values
#
bindit(s, on: string, flags: int)
{
	if(sys->bind(s, on, flags) < 0)
		sys->fprint(sys->fildes(2), "csrv: bind: %r");
}

ctlworker(buf: array of byte) :string
{
	n:= len buf;
	#sys->print("read %d: %s\n", n, string buf[0:n]);
	cmdlist := str->unquoted(string buf[0:n]);
	cmdstr := hd cmdlist;
	cmdargs := tl cmdlist;
	case cmdstr {
		"quit" =>
			sys->unmount(nil, "/csrv");
			exit;
		"bind" =>	# bind <path> <name>
			path: string;
			name: string;
			
			if(cmdargs != nil) {
				path = hd cmdargs;
				cmdargs = tl cmdargs;
				if(cmdargs != nil) {
					name = hd cmdargs;
					cmdargs = tl cmdargs;
				}
			}
			
			if((path == "") || (name == "")){
				sys->print("csrv: usage: bind <path> <name>\n");
				sys->print("csrv: command received: %s\n", string buf[0:n]);
				return "ctl: usage: bind <path> <name>";
			}
			
			addentry(name, Sys->DMDIR|8r555);
			#sys->print("bind %s %s%s\n", path, csrvroot, name);
			spawn bindit(path, csrvroot+name, sys->MREPL);				
		"mount" =>	# mount <ip> <name>
			dialstr: string;
			name: string;
			
			if(cmdargs != nil) {
				dialstr = hd cmdargs;
				cmdargs = tl cmdargs;
				if(cmdargs != nil) {
					name = hd cmdargs;
					cmdargs = tl cmdargs;
				}
			}
			
			if((dialstr == "") || (name == "")){
				sys->print("csrv: usage: bind <dial-string> <name>\n");
				sys->print("csrv: command received: %s\n", string buf[0:n]);
				return "ctl: usage: mount <ip> <name>";
			}
			
			addentry(name, Sys->DMDIR|8r555);	
			#sys->print("mount %s /n/%s\n", dialstr, name);
			(ok, dest) := sys->dial(dialstr, nil);
			if(ok < 0){
				sys->print("csrv: can't dial %s: %r\n", dialstr);
				return "csrv: couldn't dial dest";
			}
			if(sys->mount(dest.dfd, nil, "/n/"+name, sys->MREPL, nil) < 0) {
				sys->print("csrv: can't mount %s %s: %r\n", dialstr, csrvroot+name);		
				return "csrv: couldn't mount dest";
			}
			
			spawn bindit("/n/"+name+"/csrv", csrvroot+name, sys->MREPL);			
		* =>
			sys->print("csrv: unknown command received: %s\n", string buf[0:n]);
			return "unknown command received";
	}	
	
	return nil;	
}

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	str = load String String->PATH;
	if (str == nil)
		badmodule(String->PATH);	
	
	fspipe := array[2] of ref Sys->FD;
	
	#setup pipe for fs
	if(sys->pipe(fspipe) < 0)
		raise("fail: pipe: couldn't initialize");
	
	# spawn mntgen thread to handle tree
	# and feed it one end of a pipe to boot
	spawn mntgeninit(fspipe[0]);
	sys->mount(fspipe[1], nil, "/csrv", Sys->MAFTER, nil);
}