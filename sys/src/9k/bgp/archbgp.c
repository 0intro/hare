/*
 *
 * Copyright (C) 2007-2009, IBM Corporation, 
 *                     Eric Van Hensbergen (bergevan@us.ibm.com)
 *
 * Based on arch440gx.c Copyright (C) 2006-2009 Alcatel-Lucent
 *
 * Description: platform specific support routines for BG/P
 *
 * All rights reserved
 *
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "../port/netif.h"
#include "../ip/ip.h"

#include "bgp_personality.h"
#include "bgcns.h"
#include "io.h"

#include "etherif.h"

/*static*/ BGP_Personality_t* personality;

static void
ipaddrfix(uchar ipaddr[IPaddrlen], u8int octet[16])
{
	int i;
	u8int *p;

	/*
	 * Seem to be some malformed IPv6 addresses.
	 * Make sure that, if it is a v4 address in the v6 space
	 * (octets 0-9 should be 0 and octets 10 and 11 should
	 * be 0xff), it is correctly formed.
	assert(IPaddrlen == 16);
	 */
	p = octet;
	for(i = 0; i < 12; i++){
		if((ipaddr[i] = *p) != 0)
			break;
		p++;
	}
	if(i == 12){
		ipaddr[10] = 0xff;
		ipaddr[11] = 0xff;
	}
	while(i < IPaddrlen)
		ipaddr[i++] = *p++;
}

void
archreset(void)
{
	if(m->machno != 0)
		return;

	/*
	 * To do:
	 * this is a bit confused with squidboy()/machinit(), etc;
	 * should do all the personality parsing and munging here,
	 * instead of it being scattered about below, in devarch,
	 * and in drivers.
	 */
	personality = CNS(getPersonalityData, BGP_Personality_t*, 0);

	sys->ncores = CNS(getNumberOfCores, int, 0);
	if(sys->ncores > MAXMACH)
		sys->ncores = MAXMACH;
	sys->coremask = 1;

	sys->mhz = personality->Kernel_Config.FreqMHz;
	sys->mb = personality->DDR_Config.DDRSizeMB;
	sys->mb = 1024;
	sys->block = personality->Network_Config.BlockID;
	sys->config = personality->Kernel_Config.NodeConfig;

	sys->ionode = CNS(isIONode, int, 0);

	m->cpumhz = sys->mhz;
	m->cpuhz = m->cpumhz*1000000;
	m->clockgen = m->cpuhz;

	sys->x = personality->Network_Config.Xcoord;
	sys->y = personality->Network_Config.Ycoord;
	sys->z = personality->Network_Config.Zcoord;

	sys->nx = personality->Network_Config.Xnodes;
	sys->ny = personality->Network_Config.Ynodes;

	sys->yspan = sys->nx;
	sys->zspan = sys->ny*sys->yspan;
	sys->nxyz = sys->nz*sys->zspan;

	sys->iop2paddr = sys->nxyz + personality->Network_Config.PSetNum;
	sys->myp2paddr = xyztop2p(sys->x, sys->y, sys->z);
	sys->iox = ((sys->iop2paddr>>14) & 0x3f) + 128;
	sys->ioy = (sys->iop2paddr>>7) & 0x3f;
	sys->ioz = (sys->iop2paddr & 0x3f) + 1;

	assert(sizeof(personality->Ethernet_Config.EmacID) == Eaddrlen);
	assert(sizeof(BGP_IP_Addr_t) == IPaddrlen);

	memmove(sys->ea, personality->Ethernet_Config.EmacID, sizeof(sys->ea));
	ipaddrfix(sys->ipaddr, personality->Ethernet_Config.IPAddress.octet);
	ipaddrfix(sys->ipmask, personality->Ethernet_Config.IPNetmask.octet);
	ipaddrfix(sys->ipbcast, personality->Ethernet_Config.IPBroadcast.octet);
	ipaddrfix(sys->ipgw, personality->Ethernet_Config.IPGateway.octet);

	/*
	 * Only let I/O nodes and compute node 1.0.0 ({1}.0)
	 * print on the console.
	 * Cuts down on the noise until things are stable.
	 */
	if(sys->ionode || (sys->x == 1 && sys->y == 0 && sys->z == 0))
		m->chatty = 1;
		
	sys->nz = personality->Network_Config.Znodes;
	
	sys->rank = personality->Network_Config.Rank;
	sys->pset = personality->Network_Config.PSetNum;
	sys->psetsz = personality->Network_Config.PSetSize;
	sys->prank = personality->Network_Config.RankInPSet;
	sys->iorank = personality->Network_Config.IOnodeRank;

	fmtinstall('E', eipfmt);
	fmtinstall('I', eipfmt);
	fmtinstall('M', eipfmt);
	fmtinstall('V', eipfmt);
}

void
archpersonalityfmt(char* s, char* e)
{
	int i;
	char *p;
	BGP_Personality_Kernel_t *kp;
	BGP_Personality_DDR_t *dp;
	BGP_Personality_Networks_t *np;
	BGP_Personality_Ethernet_t *ep;
	u8int buf[MAX(IPaddrlen, BGP_PERSONALITY_LEN_SECKEY)];

	kp = &personality->Kernel_Config;
	p = seprint(s, e, "UCI %#ux\n", kp->UniversalComponentIdentifier);
	p = seprint(p, e, "FreqMHz %ud\n", kp->FreqMHz);
	p = seprint(p, e, "RASPolicy %#ux\n", kp->RASPolicy);
	p = seprint(p, e, "ProcessConfig %#ux\n", kp->ProcessConfig);
	p = seprint(p, e, "TraceConfig %#ux\n", kp->TraceConfig);
	p = seprint(p, e, "NodeConfig %#ux\n", kp->NodeConfig);
	p = seprint(p, e, "L1Config %#ux\n", kp->L1Config);
	p = seprint(p, e, "L2Config %#ux\n", kp->L2Config);
	p = seprint(p, e, "L3Config %#ux\n", kp->L3Config);
	p = seprint(p, e, "L3Select %#ux\n", kp->L3Select);
	p = seprint(p, e, "SharedMemMB %ud\n", kp->SharedMemMB);
	p = seprint(p, e, "ClockStop0 %#ux\n", kp->ClockStop0);
	p = seprint(p, e, "ClockStop1 %#ux\n", kp->ClockStop1);

	/*
	 * Most of the DDR configuration is uninteresting.
	 */
	dp = &personality->DDR_Config;
	p = seprint(p, e, "DDRSizeMB %ud\n", dp->DDRSizeMB);

	np = &personality->Network_Config;
	p = seprint(p, e, "BlockID %#ux\n", np->BlockID);
	p = seprint(p, e, "Xnodes %ud\n", np->Xnodes);
	p = seprint(p, e, "Ynodes %ud\n", np->Ynodes);
	p = seprint(p, e, "Znodes %ud\n", np->Znodes);
	p = seprint(p, e, "Xcoord %ud\n", np->Xcoord);
	p = seprint(p, e, "Ycoord %ud\n", np->Ycoord);
	p = seprint(p, e, "Zcoord %ud\n", np->Zcoord);
	p = seprint(p, e, "PSetNum %ud\n", np->PSetNum);
	p = seprint(p, e, "PSetSize %ud\n", np->PSetSize);
	p = seprint(p, e, "RankInPSet %ud\n", np->RankInPSet);
	p = seprint(p, e, "IOnodes %ud\n", np->IOnodes);
	p = seprint(p, e, "Rank %ud\n", np->Rank);
	p = seprint(p, e, "IOnodeRank %ud\n", np->IOnodeRank);
	for(i = 0; i < nelem(np->TreeRoutes); i++){
		p = seprint(p, e, "TreeRoutes[%d] %#ux\n",
			i, np->TreeRoutes[i]);
	}

	ep = &personality->Ethernet_Config;
	p = seprint(p, e, "MTU: %ud\n", ep->MTU);
	p = seprint(p, e, "EmacID: %E\n", ep->EmacID);
	ipaddrfix(buf, ep->IPAddress.octet);
	p = seprint(p, e, "IPAddress: %I\n", buf);
	ipaddrfix(buf, ep->IPNetmask.octet);
	p = seprint(p, e, "IPNetmask: %M\n", buf);
	ipaddrfix(buf, ep->IPBroadcast.octet);
	p = seprint(p, e, "IPBroadcast: %I\n", buf);
	ipaddrfix(buf, ep->IPGateway.octet);
	p = seprint(p, e, "IPGateway: %I\n", buf);
	ipaddrfix(buf, ep->NFSServer.octet);
	p = seprint(p, e, "NFSServer: %I\n", buf);
	ipaddrfix(buf, ep->serviceNode.octet);
	p = seprint(p, e, "serviceNode: %V\n", buf);
	p = seprint(p, e, "NFSExportDir: %#.*s\n",
		BGP_PERSONALITY_LEN_NFSDIR, ep->NFSExportDir);
	p = seprint(p, e, "NFSMountDir: %#.*s\n",
		BGP_PERSONALITY_LEN_NFSDIR, ep->NFSExportDir);
	p = seprint(p, e, "SecurityKey: ");
	memset(buf, 0, BGP_PERSONALITY_LEN_SECKEY);
	if(memcmp(buf, ep->SecurityKey, BGP_PERSONALITY_LEN_SECKEY)){
		for(i = 0; i < BGP_PERSONALITY_LEN_SECKEY; i++)
			p = seprint(p, e, "%2.2ux", ep->SecurityKey[i]);
		seprint(p, e, "\n");
	}
	else
		seprint(p, e, "0\n");
}

u32int
xyztop2p(u8int x, u8int y, u8int z)
{
	/*
	 * Convert an x.y.z address to a P2P address.
	 * If the destination address is that of an I/O node
	 * return the P2P address of this Pset (MSb of x is set,
	 * either by convention for addresses passed from the
	 * IP layer or by x == 0xff meaning this call was from
	 * archbgpreset() on an I/O node.
	 * Otherwise, calculate a compute node P2P address.
	 */
	if(x & 128)
		return sys->iop2paddr;

	return (x + y*sys->yspan + z*sys->zspan) & 0xffffff;
}

void
archprint(void)
{
	print("\n\nBG/P Personality\n");
	print("Clock: %udMHz\n", sys->mhz);
	print("Memory Size: %udMB\n", sys->mb);
	print("Block: %#8.8ux\n", sys->block);
	print("Config: %#ux\n", sys->config);
	if(!sys->ionode){
		print("Compute Node Addr: x %d y %d z %d\n",
			sys->x, sys->y, sys->z);
	}
	else{
		print("I/O Node MAC Address: %E\n", sys->ea);
		print("Assigned IP Address: %I\n", sys->ipaddr);
	}
}

/*
	handshake is 0x01, 0x1e, 0x0, 0,0,0
	node exit is 0x01, 0x1e, 0x15, 0,0,0
*/

void
sendRAS(int a1, int a2, int a3, int x1, int x2, int x3)
{
	Mreg s;
	if(!sys->ionode)
		return;

	s = splhi();
 	CNS(writeRASEvent, int, a1, a2, a3, x1, x2, x3);
 	splx(s);
}

void
meminit(unsigned cnsbase)
{
	unsigned n;

	/*
	 * BG/P has 2 or 4 GiB. Low-level boot only sets up a
	 * single 48MiB of TLB entries, let's stick with that for now.
	 * For each GiB of memory, it takes almost 16MiB to allocate
	 * the Page structures. There must be a better way.
	 *
	 * Should check here that CNS isn't in this space but assume
	 * it is up at the top of the physical memory.
	 *
	 * Will need work if there is more than 4GiB physical memory.
	 */
	n = sys->mb*MiB;
	if(n > cnsbase)
		n = cnsbase;

	sys->tom = n/MiB;

	/* was 48, try 48 */
	conf.mem[0].base = PGROUND(PADDR(sys->memstart));
	conf.mem[0].npage = (48*MiB - conf.mem[0].base)/BY2PG;
	conf.mem[1].base = 48*MiB;
	conf.mem[1].npage = (n - 48*MiB)/BY2PG;
}

int
archether(int ctlrno, Ether* edev)
{
	if(ctlrno != 0)
		return -1;

	if(!sys->ionode)
		return -1;

	edev->type = "XEMAC";
	edev->port = ctlrno;
	edev->pa = PHYSXEMAC;
	edev->irq = VectorXEMAC0;

	assert(sizeof(sys->ea) == Eaddrlen);
	memmove(edev->ea, sys->ea, Eaddrlen);

	edev->minmtu = 60;
	edev->maxmtu = 9014;

	return 1;
}

/*
 * wire the mii for the given port
 * return the mask of PHY numbers to probe
 */
uint
archmiiport(int port)
{
	if(!sys->ionode || port != 0)
		return 0;

	/*
	 * XGXS (PHY ID#4),
	 * PMA/PMD functions (PHY ID#1)
	 * and PCS (PHY ID#3).
	 */
	return 0x0000001a;
}
