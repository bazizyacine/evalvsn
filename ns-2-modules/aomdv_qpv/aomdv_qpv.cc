/*
 * Copyright (c) 2008, Marcello Caleffi, <marcello.caleffi@unina.it>,
 * http://wpage.unina.it/marcello.caleffi
 *
 * The AOMDV_QPV code has been developed at DIET, Department of Electronic
 * and Telecommunication Engineering, University of Naples "Federico II"
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

/*
 Copyright (c) 1997, 1998 Carnegie Mellon University.  All Rights
 Reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 3. The name of the author may not be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The AODV code developed by the CMU/MONARCH group was optimized and tuned by Samir Das and Mahesh Marina, University of Cincinnati. The work was partially done in Sun Microsystems.

 */

// #include <ip.h>

#include <queue/priqueueEvalVSN.h>
#include <mac/mac-802_11EvalVSN.h>
#include <aomdv_qpv/aomdv_qpv.h>
#include <aomdv_qpv/aomdv_qpv_packet.h>
#include <random.h>
// #include <energy-model.h>
#include <iostream>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define CURRENT_TIME Scheduler::instance().clock()

// AOMDV_QPV
#define DROP_RTR_RTEXPIRE "REXP"
#define DROP_RTR_HELLO "HELO"

// #define DEBUG
// #define ERROR

#ifdef DEBUG
// static int extra_route_reply = 0;
// static int limit_route_request = 0;
static int route_request = 0;
#endif

/// Map : adresse du nexthop → pointeur vers l'IFQ associé
map<nsaddr_t, priqueueEvalVSN *> nexthop_ifq_map_;

/*
 TCL Hooks
 */

int hdr_aomdv_qpv::offset_;
static class AOMDV_QPVHeaderClass : public PacketHeaderClass
{
public:
	AOMDV_QPVHeaderClass() : PacketHeaderClass("PacketHeader/AOMDV_QPV",
											   sizeof(hdr_all_aomdv_qpv))
	{
		bind_offset(&hdr_aomdv_qpv::offset_);
	}
} class_rtProtoAOMDV_QPV_hdr;

static class AOMDV_QPVclass : public TclClass
{
public:
	AOMDV_QPVclass() : TclClass("Agent/AOMDV_QPV") {}
	TclObject *create(int argc, const char *const *argv)
	{
		assert(argc == 5);
		// return (new AODV((nsaddr_t) atoi(argv[4])));
		return (new AOMDV_QPV((nsaddr_t)Address::instance().str2addr(argv[4])));
	}
} class_rtProtoAOMDV_QPV;

/**
 * @brief Handles Tcl command interface for the AOMDV_QPV agent.
 *
 * This method processes commands passed from the Tcl interpreter to configure or query
 * the AOMDV_QPV routing agent. It supports commands with 2, 3, or 4 arguments, handling
 * various agent operations such as retrieving the node index, dumping the routing table,
 * starting timers, setting internal parameters, and registering interface queues.
 *
 * Supported commands include:
 * - "id": Returns the node index.
 * - "dump-table": Prints the routing table for the node.
 * - "start": Initializes and starts agent timers.
 * - "index <value>": Sets the node index.
 * - "log-target" or "tracetarget <object>": Sets the logging or trace target.
 * - "drop-target <object>": Sets the drop target for the routing queue.
 * - "if-queue <object>": Sets the interface queue.
 * - "port-dmux <object>": Sets the port demultiplexer.
 * - "setMac <object>": Sets the MAC layer object.
 * - "set-rtq-max-len <value>": Sets the maximum length of the routing queue.
 * - "register_ifq_for_nexthop <nexthop> <ifq>": Registers an interface queue for a specific next hop.
 *
 * For unrecognized commands, the method delegates to the base Agent::command().
 *
 * @param argc Number of command arguments.
 * @param argv Array of command argument strings.
 * @return int TCL_OK on success, TCL_ERROR on failure, or result of base class command handler.
 */
int AOMDV_QPV::command(int argc, const char *const *argv)
{
	if (argc == 2)
	{
		Tcl &tcl = Tcl::instance();

		if (strncasecmp(argv[1], "id", 2) == 0)
		{
			tcl.resultf("%d", index);
			return TCL_OK;
		}
		// AOMDV_QPV code - should it be removed?
		if (strncasecmp(argv[1], "dump-table", 10) == 0)
		{
			printf("Node %d: Route table:\n", index);
			rtable.rt_dumptable();
			return TCL_OK;
		}
		if (strncasecmp(argv[1], "start", 2) == 0)
		{
			btimer.handle((Event *)0);

#ifndef AOMDV_QPV_LINK_LAYER_DETECTION
			htimer.handle((Event *)0);
			ntimer.handle((Event *)0);
#endif // LINK LAYER DETECTION

			rtimer.handle((Event *)0);
			return TCL_OK;
		}
	}
	else if (argc == 3)
	{
		if (strcmp(argv[1], "index") == 0)
		{
			index = atoi(argv[2]);
			return TCL_OK;
		}
		else if (strcmp(argv[1], "log-target") == 0 || strcmp(argv[1], "tracetarget") == 0)
		{
			logtarget = (Trace *)TclObject::lookup(argv[2]);
			if (logtarget == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		else if (strcmp(argv[1], "drop-target") == 0)
		{
			int stat = rqueue.command(argc, argv);
			if (stat != TCL_OK)
				return stat;
			return Agent::command(argc, argv);
		}
		else if (strcmp(argv[1], "if-queue") == 0)
		{
			AOMDV_QPVifqueue = (PriQueue *)TclObject::lookup(argv[2]);

			if (AOMDV_QPVifqueue == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		// AODV ns-2.31 code
		else if (strcmp(argv[1], "port-dmux") == 0)
		{
			dmux_ = (PortClassifier *)TclObject::lookup(argv[2]);
			if (dmux_ == 0)
			{
				fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__,
						argv[1], argv[2]);
				return TCL_ERROR;
			}
			return TCL_OK;
		}
		else if (strcmp(argv[1], "setMac") == 0)
		{
			mac_ = (Mac802_11EvalVSN *)TclObject::lookup(argv[2]);

			if (mac_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		else if (strcmp(argv[1], "set-rtq-max-len") == 0)
		{
			rqueue.setLimit(atoi(argv[2]));
			return TCL_OK;
		}
	}
	else if (argc == 4)
	{
		if (strcmp(argv[1], "register_ifq_for_nexthop") == 0)
		{
			nsaddr_t nexthop = atoi(argv[2]);
			priqueueEvalVSN *ifq = (priqueueEvalVSN *)TclObject::lookup(argv[3]);

			if (!ifq)
				return TCL_ERROR;

			register_ifq_for_nexthop(nexthop, ifq);

			return TCL_OK;
		}
	}

	return Agent::command(argc, argv);
}

/*
Constructor
 */
AOMDV_QPV::AOMDV_QPV(nsaddr_t id) : Agent(PT_AOMDV_QPV),
									btimer(this), htimer(this), ntimer(this),
									rtimer(this), lrtimer(this), rqueue(), mac_(NULL)
{
	// AOMDV_QPV code
	aomdv_qpv_max_paths_ = 3;
	bind("aomdv_qpv_max_paths_", &aomdv_qpv_max_paths_);

	aomdv_qpv_prim_alt_path_len_diff_ = 1;
	bind("aomdv_qpv_prim_alt_path_len_diff_", &aomdv_qpv_prim_alt_path_len_diff_);

	index = id;
	seqno = 2;
	bid = 1;

	LIST_INIT(&nbhead);
	LIST_INIT(&bihead);

	logtarget = 0;
	AOMDV_QPVifqueue = 0;

	AOMDV_QPV_DBG = 0;
	bind("AOMDV_QPV_DBG", &AOMDV_QPV_DBG);

	rqueue.setAgent(this); // Lien entre la file et l'agent
}

/*
 Timers
 */

void AOMDV_QPVBroadcastTimer::handle(Event *)
{
	agent->id_purge();
	Scheduler::instance().schedule(this, &intr, BCAST_ID_SAVE);
}

void AOMDV_QPVHelloTimer::handle(Event *)
{
	// AOMDV_QPV code - could it be removed?
	// agent->sendHello();
	/* Do not send a HELLO message unless we have a valid route entry. */
	if (agent->rtable.rt_has_active_route())
		agent->sendHello();
	// AODV ns-2.31 code
	// double interval = HELLO_INTERVAL + 0.01*Random::uniform();
	double interval = MinHelloInterval + ((MaxHelloInterval - MinHelloInterval) * Random::uniform());
	assert(interval >= 0);
	Scheduler::instance().schedule(this, &intr, interval);
}

void AOMDV_QPVNeighborTimer::handle(Event *)
{
	agent->nb_purge();
	Scheduler::instance().schedule(this, &intr, HELLO_INTERVAL);
}

void AOMDV_QPVRouteCacheTimer::handle(Event *)
{
	agent->rt_purge();
#define FREQUENCY 0.5 // sec
	Scheduler::instance().schedule(this, &intr, FREQUENCY);
}

void AOMDV_QPVLocalRepairTimer::handle(Event *p)
{ // SRD: 5/4/99
	aomdv_qpv_rt_entry *rt;
	struct hdr_ip *ih = HDR_IP((Packet *)p);

	/* you get here after the timeout in a local repair attempt */
	/* fprintf(stderr, "%s\n", __FUNCTION__); */

	rt = agent->rtable.rt_lookup(ih->daddr());

	if (rt && rt->rt_flags != RTF_UP)
	{
		// route is yet to be repaired
		// I will be conservative and bring down the route
		// and send route errors upstream.
		/* The following assert fails, not sure why */
		/* assert (rt->rt_flags == RTF_IN_REPAIR); */

		// rt->rt_seqno++;
		agent->rt_down(rt);
		// send RERR
#ifdef DEBUG
//		fprintf(stderr,"Node %d: Dst - %d, failed local repair\n",index, rt->rt_dst);
#endif
	}
	Packet::free((Packet *)p);
}

/*
 Broadcast ID Management  Functions
 */

// AODV ns-2.31 code
void AOMDV_QPV::id_insert(nsaddr_t id, u_int32_t bid)
{
	AOMDV_QPVBroadcastID *b = new AOMDV_QPVBroadcastID(id, bid);

	assert(b);
	b->expire = CURRENT_TIME + BCAST_ID_SAVE;
	LIST_INSERT_HEAD(&bihead, b, link);
}

// AODV ns-2.31 code
/* SRD */
bool AOMDV_QPV::id_lookup(nsaddr_t id, u_int32_t bid)
{
	AOMDV_QPVBroadcastID *b = bihead.lh_first;

	// Search the list for a match of source and bid
	for (; b; b = b->link.le_next)
	{
		if ((b->src == id) && (b->id == bid))
			return true;
	}
	return false;
}

// AOMDV_QPV ns-2.31 code
AOMDV_QPVBroadcastID *
AOMDV_QPV::id_get(nsaddr_t id, u_int32_t bid)
{
	AOMDV_QPVBroadcastID *b = bihead.lh_first;

	// Search the list for a match of source and bid
	for (; b; b = b->link.le_next)
	{
		if ((b->src == id) && (b->id == bid))
			return b;
	}
	return NULL;
}

void AOMDV_QPV::id_purge()
{
	AOMDV_QPVBroadcastID *b = bihead.lh_first;
	AOMDV_QPVBroadcastID *bn;
	double now = CURRENT_TIME;

	for (; b; b = bn)
	{
		bn = b->link.le_next;
		if (b->expire <= now)
		{
			LIST_REMOVE(b, link);
			delete b;
		}
	}
}

/*
 Helper Functions
 */

double
AOMDV_QPV::PerHopTime(aomdv_qpv_rt_entry *rt)
{
	int num_non_zero = 0, i;
	double total_latency = 0.0;

	if (!rt)
		return ((double)NODE_TRAVERSAL_TIME);

	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (rt->rt_disc_latency[i] > 0.0)
		{
			num_non_zero++;
			total_latency += rt->rt_disc_latency[i];
		}
	}
	if (num_non_zero > 0)
		return (total_latency / (double)num_non_zero);
	else
		return ((double)NODE_TRAVERSAL_TIME);
}

/*
 Link Failure Management Functions
 */

static void aomdv_qpv_rt_failed_callback(Packet *p, void *arg)
{
	((AOMDV_QPV *)arg)->rt_ll_failed(p);
}

/*
 * This routine is invoked when the link-layer reports a route failed.
 */
void AOMDV_QPV::rt_ll_failed(Packet *p)
{
// AOMDV_QPV ns-2.31 code
#ifndef AOMDV_QPV_LINK_LAYER_DETECTION
	drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
#else

	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	aomdv_qpv_rt_entry *rt;
	nsaddr_t broken_nbr = ch->next_hop_;

	/*
	 * Non-data packets and Broadcast Packets can be dropped.
	 */
	if (!DATA_PACKET(ch->ptype()) ||
		(u_int32_t)ih->daddr() == IP_BROADCAST)
	{
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		return;
	}
	log_link_broke(p);
	if ((rt = rtable.rt_lookup(ih->daddr())) == 0)
	{
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		return;
	}
	log_link_del(ch->next_hop_);

#ifdef AOMDV_QPV_LOCAL_REPAIR
	/* if the broken link is closer to the dest than source,
		attempt a local repair. Otherwise, bring down the route. */

	if (ch->num_forwards() > rt->rt_hops)
	{
		local_rt_repair(rt, p); // local repair
								// retrieve all the packets in the ifq using this link,
								// queue the packets for which local repair is done,
		return;
	}
#endif // LOCAL REPAIR

	{
		// AOMDV_QPV code - could it be removed?
		handle_link_failure(broken_nbr);
		// AOMDV_QPV code
#ifdef AOMDV_QPV_PACKET_SALVAGING

		if (!DATA_PACKET(ch->ptype()))
			drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		else
		{
			// salvage the packet using an alternate path if available.
			aomdv_qpv_rt_entry *rt = rtable.rt_lookup(ih->daddr());
			if (rt && (rt->rt_flags == RTF_UP) && (ch->aomdv_qpv_salvage_count_ < AOMDV_QPV_MAX_SALVAGE_COUNT))
			{
				ch->aomdv_qpv_salvage_count_ += 1;
				forward(rt, p, NO_AOMDV_QPV_DELAY);
			}
			else
				drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		}

		while ((p = AOMDV_QPVifqueue->filter(broken_nbr)))
		{
			struct hdr_cmn *ch = HDR_CMN(p);
			struct hdr_ip *ih = HDR_IP(p);
			if (!DATA_PACKET(ch->ptype()))
				drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
			else
			{
				// salvage the packet using an alternate path if available.
				aomdv_qpv_rt_entry *rt = rtable.rt_lookup(ih->daddr());
				if (rt && (rt->rt_flags == RTF_UP) && (ch->aomdv_qpv_salvage_count_ < AOMDV_QPV_MAX_SALVAGE_COUNT))
				{
					ch->aomdv_qpv_salvage_count_ += 1;
					forward(rt, p, NO_AOMDV_QPV_DELAY);
				}
				else
					drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
			}
		}
#else  // NO PACKET SALVAGING
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		// Do the same thing for other packets in the interface queue using the
		// broken link -Mahesh
		while ((p = AOMDV_QPVifqueue->filter(broken_nbr)))
		{
			drop(p, mac_->phy_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		}
		nb_delete(broken_nbr);
		// AOMDV_QPV code
#endif // NO PACKET SALVAGING
	}

#endif // LINK LAYER DETECTION
}

// AOMDV_QPV code
void AOMDV_QPV::handle_link_failure(nsaddr_t id)
{
	bool error = true;
	aomdv_qpv_rt_entry *rt, *rtn;
	Packet *rerr = Packet::alloc();
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(rerr);
#ifdef DEBUG
	fprintf(stderr, "%s: multipath version\n", __FUNCTION__);
#endif // DEBUG
	re->DestCount = 0;
	for (rt = rtable.head(); rt; rt = rtn)
	{ // for each rt entry
		AOMDV_QPV_Path *path;
		rtn = rt->rt_link.le_next;
		if ((rt->rt_flags == RTF_UP) && (path = rt->path_lookup(id)))
		{
			assert((rt->rt_seqno % 2) == 0);

			rt->path_delete(id);
			if (rt->path_empty())
			{
				rt->rt_seqno++;
				rt->rt_seqno = max(rt->rt_seqno, rt->rt_highest_seqno_heard);
				// CHANGE
				if (rt->rt_error)
				{
					re->unreachable_dst[re->DestCount] = rt->rt_dst;
					re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
#ifdef DEBUG
					fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\n", __FUNCTION__, CURRENT_TIME,
							index, re->unreachable_dst[re->DestCount],
							re->unreachable_dst_seqno[re->DestCount], id);
#endif // DEBUG
					re->DestCount += 1;
					rt->rt_error = false;
				}
				// CHANGE
				rt_down(rt);
			}
		}
	}

	if ((re->DestCount > 0) && (error))
	{
#ifdef DEBUG
		fprintf(stdout, "%s(%f): %d\tsending RERR...\n", __FUNCTION__, CURRENT_TIME, index);
#endif // DEBUG
		sendError(rerr, false);
	}
	else
	{
		Packet::free(rerr);
	}
}

void AOMDV_QPV::local_rt_repair(aomdv_qpv_rt_entry *rt, Packet *p)
{
#ifdef DEBUG
	fprintf(stderr, "%s: Dst - %d\n", __FUNCTION__, rt->rt_dst);
#endif
	// Buffer the packet
	rqueue.enque(p);

	// mark the route as under repair
	rt->rt_flags = RTF_IN_REPAIR;

	sendRequest(rt->rt_dst);

	// set up a timer interrupt
	Scheduler::instance().schedule(&lrtimer, p->copy(), rt->rt_req_timeout);
}

void AOMDV_QPV::rt_down(aomdv_qpv_rt_entry *rt)
{
	/*
	 *  Make sure that you don't "down" a route more than once.
	 */

	// AOMDV_QPV code
#ifdef DEBUG
	fprintf(stderr, "%s: multipath version\n", __FUNCTION__);
#endif // DEBUG

	if (rt->rt_flags == RTF_DOWN)
	{
		return;
	}

	// AODV ns-2.31 code
	// assert (rt->rt_seqno%2); // is the seqno odd?
	// AOMDV_QPV code
	rt->rt_flags = RTF_DOWN;
	rt->rt_advertised_hops = INFINITY;
	rt->path_delete();
	rt->rt_expire = 0;

} /* rt_down function */

/*
 Route Handling Functions
 */

void AOMDV_QPV::rt_resolve(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	aomdv_qpv_rt_entry *rt;

	/*
	 *  Set the transmit failure callback.  That
	 *  won't change.
	 */
	ch->xmit_failure_ = aomdv_qpv_rt_failed_callback;
	ch->xmit_failure_data_ = (void *)this;
	rt = rtable.rt_lookup(ih->daddr());
	if (rt == 0)
	{
		rt = rtable.rt_add(ih->daddr());
	}

	/*
	 * If the route is up, forward the packet
	 */

	if (rt->rt_flags == RTF_UP)
	{
		assert(rt->rt_hops != INFINITY2);
		forward(rt, p, NO_AOMDV_QPV_DELAY);
	}
	/*
	 *  if I am the source of the packet, then do a Route Request.
	 */
	else if (ih->saddr() == index)
	{
		rqueue.enque(p);
		sendRequest(rt->rt_dst);
	}
	/*
	 *   A local repair is in progress. Buffer the packet.
	 */
	else if (rt->rt_flags == RTF_IN_REPAIR)
	{
		rqueue.enque(p);
	}

	/*
	 * I am trying to forward a packet for someone else to which
	 * I don't have a route.
	 */
	else
	{
		Packet *rerr = Packet::alloc();
		struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(rerr);
		/*
		 * For now, drop the packet and send error upstream.
		 * Now the route errors are broadcast to upstream
		 * neighbors - Mahesh 09/11/99
		 */

		assert(rt->rt_flags == RTF_DOWN);
		re->DestCount = 0;
		re->unreachable_dst[re->DestCount] = rt->rt_dst;
		re->unreachable_dst_seqno[re->DestCount] = rt->rt_seqno;
		re->DestCount += 1;
#ifdef DEBUG
		fprintf(stderr, "%s: sending RERR...\n", __FUNCTION__);
#endif
		sendError(rerr, false);

		drop(p, mac_->phy_->dropReason(p, DROP_RTR_NO_ROUTE));
	}
}

void AOMDV_QPV::rt_purge()
{
	aomdv_qpv_rt_entry *rt, *rtn;
	double now = CURRENT_TIME;
	double delay = 0.0;
	Packet *p;

	for (rt = rtable.head(); rt; rt = rtn)
	{ // for each rt entry
		rtn = rt->rt_link.le_next;
		// AOMDV_QPV code
		// MODIFIED BY US! Added '&& rt-> ...' in if-statement
		if (rt->rt_flags == RTF_UP && (rt->rt_expire < now))
		{
			rt->path_purge();

			if (rt->path_empty())
			{
				while ((p = rqueue.deque(rt->rt_dst)))
				{
					drop(p, mac_->phy_->dropReason(p, DROP_RTR_RTEXPIRE));
				}

				rt->rt_seqno++;
				rt->rt_seqno = max(rt->rt_seqno, rt->rt_highest_seqno_heard);
				if (rt->rt_seqno % 2 == 0)
					rt->rt_seqno += 1;
				// assert (rt->rt_seqno%2);
				rt_down(rt);
			}
		}
		else if (rt->rt_flags == RTF_UP)
		{
			// If the route is not expired,
			// and there are packets in the sendbuffer waiting,
			// forward them. This should not be needed, but this extra
			// check does no harm.
			assert(rt->rt_hops != INFINITY2);
			while ((p = rqueue.deque(rt->rt_dst)))
			{
				forward(rt, p, delay);
				delay += ARP_DELAY;
			}
		}
		else if (rqueue.find(rt->rt_dst))
			// If the route is down and
			// if there is a packet for this destination waiting in
			// the sendbuffer, then send out route request. sendRequest
			// will check whether it is time to really send out request
			// or not.
			// This may not be crucial to do it here, as each generated
			// packet will do a sendRequest anyway.

			sendRequest(rt->rt_dst);
	}
}

/*
 Packet Reception Routines
 */

/**
 * @brief Handles the reception of a packet in the AOMDV_QPV protocol.
 *
 * This function processes incoming packets, updates link metrics if a MAC layer is present,
 * and determines the appropriate action based on the packet type, source address, and TTL.
 * It supports special handling for AOMDV_QPV control packets, video packets, and broadcast packets.
 * The function also manages packet forwarding, loop detection, and TTL expiration.
 *
 * @param p Pointer to the received Packet.
 * @param h Unused handler parameter.
 *
 * Packet processing steps:
 * - Updates link quality and SNR metrics for the neighbor if MAC is present.
 * - Logs debug information for video and non-video packets if debugging is enabled.
 * - Handles AOMDV_QPV control packets by decrementing TTL and invoking recvAOMDV_QPV().
 * - For originated packets, adds IP header, initializes salvage count, and sets TTL.
 * - Detects and drops packets involved in routing loops.
 * - Forwards packets, dropping them if TTL expires.
 * - Resolves routes for unicast packets or forwards broadcast packets.
 */
void AOMDV_QPV::recv(Packet *p, Handler *)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;
	struct hdr_ip *ih = HDR_IP(p);

	assert(initialized());
	// assert(p->incoming == 0);
	//  XXXXX NOTE: use of incoming flag has been depracated; In order to track direction of pkt flow, direction_ in hdr_cmn is used instead. see packet.h for details.

	if (mac_)
	{
		double lqi = ch->linkQualityIndicator_;
		double snr = ch->signalToNoiseRatio_;

		// debug log
		if (AOMDV_QPV_DBG)
		{
			char msg[1000];

			if (packetType == PT_VIDEO)
			{
				sprintf(msg, "using: neighbor=%d LQI=%.2f SNR=%.2f for VIDEO packetType=%s, frameType='%c', priority=%d",
						ih->saddr(), lqi, snr, packet_info.name(packetType), frameType, priority);
			}
			else
			{
				sprintf(msg, "using: neighbor=%d LQI=%.2f SNR=%.2f for NON-VIDEO packetType=%s",
						ih->saddr(), lqi, snr, packet_info.name(packetType));
			}

			log("recv (cross-layer)", msg);
		}

		mac_->update_link_metrics(ih->saddr(), lqi, snr);
	}

	if (ch->ptype() == PT_AOMDV_QPV)
	{
		ih->ttl_ -= 1;
		recvAOMDV_QPV(p);
		return;
	}

	/*
	 *  Must be a packet I'm originating...
	 */
	if ((ih->saddr() == index) && (ch->num_forwards() == 0))
	{
		/*
		 * Add the IP Header
		 */
		ch->size() += IP_HDR_LEN;
		// AOMDV_QPV code
		ch->aomdv_qpv_salvage_count_ = 0;
		// Added by Parag Dadhania && John Novatnack to handle broadcasting
		if ((u_int32_t)ih->daddr() != IP_BROADCAST)
			ih->ttl_ = NETWORK_DIAMETER;
	}
	/*
	 *  I received a packet that I sent.  Probably
	 *  a routing loop.
	 */
	else if (ih->saddr() == index)
	{
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_ROUTE_LOOP));
		return;
	}
	/*
	 *  Packet I'm forwarding...
	 */
	else
	{
		/*
		 *  Check the TTL.  If it is zero, then discard.
		 */
		if (--ih->ttl_ == 0)
		{
			drop(p, mac_->phy_->dropReason(p, DROP_RTR_TTL));
			return;
		}
	}
	// Added by Parag Dadhania && John Novatnack to handle broadcasting
	if ((u_int32_t)ih->daddr() != IP_BROADCAST)
		rt_resolve(p);
	else
		forward((aomdv_qpv_rt_entry *)0, p, NO_AOMDV_QPV_DELAY);
}

void AOMDV_QPV::recvAOMDV_QPV(Packet *p)
{
	struct hdr_aomdv_qpv *ah = HDR_AOMDV_QPV(p);
	// AODV ns-2.31 code
	// struct hdr_ip *ih = HDR_IP(p);
	assert(HDR_IP(p)->sport() == RT_PORT);
	assert(HDR_IP(p)->dport() == RT_PORT);

	/*
	 * Incoming Packets.
	 */
	switch (ah->ah_type)
	{

	case AOMDV_QPVTYPE_RREQ:
		recvRequest(p);
		break;

	case AOMDV_QPVTYPE_RREP:
		recvReply(p);
		break;

	case AOMDV_QPVTYPE_RERR:
		recvError(p);
		break;

	case AOMDV_QPVTYPE_HELLO:
		recvHello(p);
		break;

	default:
		fprintf(stderr, "Invalid AOMDV_QPV type (%x)\n", ah->ah_type);
		exit(1);
	}
}

// AOMDV_QPV
/**
 * @brief Handles the reception of a Route Request (RREQ) packet in the AOMDV_QPV protocol.
 *
 * This function processes incoming RREQ packets as part of the AOMDV_QPV (Ad hoc On-demand Multipath Distance Vector with Quality Path Variation) routing protocol.
 * It performs the following main tasks:
 *   - Updates link quality metrics if cross-layer MAC information is available.
 *   - Drops the packet if it is a duplicate or originated by this node.
 *   - Caches the broadcast ID to prevent processing duplicate RREQs.
 *   - Updates or creates a reverse route entry to the RREQ source in the routing table.
 *   - Maintains and updates path metrics such as hop count and cumulative cost.
 *   - Handles insertion of new disjoint reverse paths if appropriate.
 *   - If the node is the destination, or has a fresh enough route to the destination, generates and sends a Route Reply (RREP).
 *   - Otherwise, forwards the RREQ to neighboring nodes, updating relevant fields.
 *
 * The function ensures loop freedom, multipath disjointness, and efficient route discovery by managing path and sequence number information.
 *
 * @param p Pointer to the received Packet containing the RREQ.
 */
void AOMDV_QPV::recvRequest(Packet *p)
{
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_request *rq = HDR_AOMDV_QPV_REQUEST(p);
	aomdv_qpv_rt_entry *rt;
	AOMDV_QPVBroadcastID *b = NULL;
	bool kill_request_propagation = false;
	AOMDV_QPV_Path *reverse_path = NULL;

	if (mac_)
	{
		// --- MAJ PRR à chaque réception de RREQ ---
		updatePRR_onRecv(p, ih->saddr());
		// --- MAJ PRR à chaque réception de RREQ ---
	}

	/*
	 * Drop if:
	 *      - I'm the source
	 *      - I recently heard this request.
	 */

	if (rq->rq_src == index)
	{
#ifdef DEBUG
		fprintf(stderr, "%s: got my own REQUEST\n", __FUNCTION__);
#endif // DEBUG
		Packet::free(p);
		return;
	}

	/* If RREQ has already been received - drop it, else remember "RREQ id" <src IP, bcast ID>. */
	if ((b = id_get(rq->rq_src, rq->rq_bcast_id)) == NULL)
	{
		// Cache the broadcast ID
		id_insert(rq->rq_src, rq->rq_bcast_id);
		b = id_get(rq->rq_src, rq->rq_bcast_id);
	}
	else
		kill_request_propagation = true;

	/* If I am a neighbor to the RREQ source, make myself first hop on path from source to dest. */
	if (rq->rq_hop_count == 0)
		rq->rq_first_hop = index;

	/*
	 * We are either going to forward the REQUEST or generate a
	 * REPLY. Before we do anything, we make sure that the REVERSE
	 * route is in the route table.
	 */
	aomdv_qpv_rt_entry *rt0; // rt0 is the reverse route

	rt0 = rtable.rt_lookup(rq->rq_src);
	if (rt0 == 0)
	{ /* if not in the route table */
		// create an entry for the reverse route.
		rt0 = rtable.rt_add(rq->rq_src);
	}

	if (mac_)
	{
		// --- Cross-layer: coût cumulé sur le reverse path ---
		// Calcule la métrique composite pour le saut précédent
		nsaddr_t prev_hop = ih->saddr();
		LinkQuality q = getLinkQuality(p, prev_hop);
		double cost = linkCost(p, q);

		// Cumulez le coût sur le chemin
		if (rq->rq_hop_count == 0)
			rq->rq_cost_cumule = cost;
		else
			rq->rq_cost_cumule += cost;
		// --- Cross-layer: coût cumulé sur le reverse path ---
	}

	/*
	 * Create/update reverse path (i.e. path back to RREQ source)
	 * If RREQ contains more recent seq number than route table entry - update route entry to source.
	 */
	if (rt0->rt_seqno < rq->rq_src_seqno)
	{
		rt0->rt_seqno = rq->rq_src_seqno;
		rt0->rt_advertised_hops = INFINITY;
		rt0->path_delete(); // Delete all previous paths to RREQ source
		rt0->rt_flags = RTF_UP;
		/* Insert new path for route entry to source of RREQ.
			(src addr, hop count + 1, lifetime, last hop (first hop for RREQ)) */
		reverse_path = rt0->path_insert(ih->saddr(), rq->rq_hop_count + 1, CURRENT_TIME + REV_ROUTE_LIFE, rq->rq_first_hop);
		// CHANGE
		rt0->rt_last_hop_count = rt0->path_get_max_hopcount();
		// CHANGE

		if (mac_)
		{
			// --- Cross-layer: stocke le coût cumulé dans le reverse path ---
			if (reverse_path)
				reverse_path->path_cost = rq->rq_cost_cumule;
			// --- Cross-layer: stocke le coût cumulé dans le reverse path ---
		}
	}
	/* If a new path with smaller hop count is received
	(same seqno, better hop count) - try to insert new path in route table. */
	else if ((rt0->rt_seqno == rq->rq_src_seqno) &&
			 (rt0->rt_advertised_hops > rq->rq_hop_count))
	{
		AOMDV_QPV_Path *erp = NULL;

		assert(rt0->rt_flags == RTF_UP); // Make sure path is up

		/*
		 * If path already exists - adjust the lifetime of the path.
		 */
		if ((reverse_path = rt0->disjoint_path_lookup(ih->saddr(), rq->rq_first_hop)))
		{
			assert(reverse_path->hopcount == (rq->rq_hop_count + 1));
			reverse_path->expire = max(reverse_path->expire, (CURRENT_TIME + REV_ROUTE_LIFE));
		}
		/*
		 * Got a new alternate disjoint reverse path - so insert it.
		 * I.e. no path exists which has RREQ source as next hop and no
		 * path with RREQ first hop as last hop exists for this route entry.
		 * Simply stated: no path with the same last hop exists already.
		 */
		else if (rt0->new_disjoint_path(ih->saddr(), rq->rq_first_hop))
		{
			/* Only insert new path if not too many paths exists for this destination
			and new path does not differ too much in length compared to previous paths */
			if ((rt0->rt_num_paths_ < aomdv_qpv_max_paths_) &&
				(((rq->rq_hop_count + 1) - rt0->path_get_min_hopcount()) <= aomdv_qpv_prim_alt_path_len_diff_))
			{
				/* Insert new (disjoint) reverse path */
				reverse_path = rt0->path_insert(ih->saddr(), rq->rq_hop_count + 1, CURRENT_TIME + REV_ROUTE_LIFE, rq->rq_first_hop);
				// CHANGE
				rt0->rt_last_hop_count = rt0->path_get_max_hopcount();
				// CHANGE
			}
			/* If new path differs too much in length compared to previous paths - drop packet. */
			if (((rq->rq_hop_count + 1) - rt0->path_get_min_hopcount()) > aomdv_qpv_prim_alt_path_len_diff_)
			{
				Packet::free(p);
				return;
			}
		}
		/* (RREQ was intended for me) AND
			((Path with RREQ first hop as last hop does not exist) OR
			 (The path exists and has less hop count than RREQ)) - drop packet.
			Don't know what this case is for... */
		else if ((rq->rq_dst == index) &&
				 (((erp = rt0->path_lookup_lasthop(rq->rq_first_hop)) == NULL) ||
				  ((rq->rq_hop_count + 1) > erp->hopcount)))
		{
			Packet::free(p);
			return;
		}
	}
	/* Older seqno (or same seqno with higher hopcount), i.e. I have a
	more recent route entry - so drop packet.*/
	else
	{
		Packet::free(p);
		return;
	}

	/* If route is up */
	if (rt0->rt_flags == RTF_UP)
	{
		// Reset the soft state
		rt0->rt_req_timeout = 0.0;
		rt0->rt_req_last_ttl = 0;
		rt0->rt_req_cnt = 0;

		/*
		 * Find out whether any buffered packet can benefit from the
		 * reverse route.
		 */
		Packet *buffered_pkt;
		while ((buffered_pkt = rqueue.deque(rt0->rt_dst)))
		{
			if (rt0 && (rt0->rt_flags == RTF_UP))
			{
				forward(rt0, buffered_pkt, NO_AOMDV_QPV_DELAY);
			}
		}
	}
	/* Check route entry for RREQ destination */
	rt = rtable.rt_lookup(rq->rq_dst);

	/* I am the intended receiver of the RREQ - so send a RREP */
	if (rq->rq_dst == index)
	{

		if (seqno < rq->rq_dst_seqno)
		{
			// seqno = max(seqno, rq->rq_dst_seqno)+1;
			seqno = rq->rq_dst_seqno + 1; // CHANGE (replaced above line with this one)
		}
		/* Make sure seq number is even (why?) */
		if (seqno % 2)
			seqno++;

		sendReply(rq->rq_src,		// IP Destination
				  0,				// Hop Count
				  index,			// (RREQ) Dest IP Address
				  seqno,			// Dest Sequence Num
				  MY_ROUTE_TIMEOUT, // Lifetime
				  rq->rq_timestamp, // timestamp
				  ih->saddr(),		// nexthop
				  rq->rq_bcast_id,	// broadcast id to identify this route discovery
				  ih->saddr());

		Packet::free(p);
	}
	/* I have a fresh route entry for RREQ destination - so send RREP */
	else if (rt &&
			 (rt->rt_flags == RTF_UP) &&
			 (rt->rt_seqno >= rq->rq_dst_seqno))
	{

		assert((rt->rt_seqno % 2) == 0); // is the seqno even?
		/* Reverse path exists */
		if (reverse_path)
		{
#ifdef AOMDV_QPV_NODE_DISJOINT_PATHS
			if (b->count == 0)
			{
				b->count = 1;

				// route advertisement
				if (rt->rt_advertised_hops == INFINITY)
					rt->rt_advertised_hops = rt->path_get_max_hopcount();

				AOMDV_QPV_Path *forward_path = rt->path_find();
				// CHANGE
				rt->rt_error = true;
				// CHANGE
				sendReply(rq->rq_src,
						  rt->rt_advertised_hops,
						  rq->rq_dst,
						  rt->rt_seqno,
						  forward_path->expire - CURRENT_TIME,
						  rq->rq_timestamp,
						  ih->saddr(),
						  rq->rq_bcast_id,
						  forward_path->lasthop);
			}
#endif // AOMDV_QPV_NODE_DISJOINT_PATHS
#ifdef AOMDV_QPV_LINK_DISJOINT_PATHS
			AOMDV_QPV_Path *forward_path = NULL;
			AOMDV_QPV_Path *r = rt->rt_path_list.lh_first; // Get first path for RREQ destination
			/* Make sure we don't answer with the same forward path twice in response
				to a certain RREQ (received more than once). E.g. "middle node"
				in "double diamond". */
			for (; r; r = r->path_link.le_next)
			{
				if (b->forward_path_lookup(r->nexthop, r->lasthop) == NULL)
				{
					forward_path = r;
					break;
				}
			}
			/* If an unused forward path is found and we have not answered
				along this reverse path (for this RREQ) - send a RREP back. */
			if (forward_path &&
				(b->reverse_path_lookup(reverse_path->nexthop, reverse_path->lasthop) == NULL))
			{
				/* Mark the reverse and forward path as used (for this RREQ). */
				// Cache the broadcast ID
				b->reverse_path_insert(reverse_path->nexthop, reverse_path->lasthop);
				b->forward_path_insert(forward_path->nexthop, forward_path->lasthop);

				// route advertisement
				if (rt->rt_advertised_hops == INFINITY)
					rt->rt_advertised_hops = rt->path_get_max_hopcount();

				// CHANGE
				rt->rt_error = true;
				// CHANGE
				sendReply(rq->rq_src,
						  rt->rt_advertised_hops,
						  rq->rq_dst,
						  rt->rt_seqno,
						  forward_path->expire - CURRENT_TIME,
						  rq->rq_timestamp,
						  ih->saddr(),
						  rq->rq_bcast_id,
						  forward_path->lasthop);
			}
#endif // AOMDV_QPV_LINK_DISJOINT_PATHS
		}
		Packet::free(p);
	}
	/* RREQ not intended for me and I don't have a fresh
	enough entry for RREQ dest - so forward the RREQ */
	else
	{

		if (kill_request_propagation)
		{
			// do not propagate a duplicate RREQ
			Packet::free(p);
			return;
		}
		else
		{
			ih->saddr() = index;

			// Maximum sequence number seen en route
			if (rt)
				rq->rq_dst_seqno = max(rt->rt_seqno, rq->rq_dst_seqno);

			// route advertisement
			if (rt0->rt_advertised_hops == INFINITY)
				rt0->rt_advertised_hops = rt0->path_get_max_hopcount();
			rq->rq_hop_count = rt0->rt_advertised_hops;
#ifdef AOMDV_QPV_NODE_DISJOINT_PATHS
			rq->rq_first_hop = (rt0->path_find())->lasthop;
#endif // AOMDV_QPV_NODE_DISJOINT_PATHS

			forward((aomdv_qpv_rt_entry *)0, p, AOMDV_QPV_DELAY);
		}
	}
}

// AOMDV_QPV
/**
 * @brief Handles the reception of a Route Reply (RREP) packet in the AOMDV_QPV protocol.
 *
 * This function processes incoming RREP packets, updates the routing table and path information,
 * manages cross-layer metrics (such as link quality and cumulative cost), and forwards the RREP
 * towards the source of the corresponding Route Request (RREQ) as needed. It also manages
 * duplicate suppression and path disjointness (node- or link-disjoint) according to protocol settings.
 *
 * Main steps:
 * - Drops RREP if received by the original source (should not occur).
 * - Looks up or creates a routing table entry for the RREP destination.
 * - Updates route and path information if the RREP contains fresher or better information.
 * - Updates cross-layer metrics (e.g., link quality, cumulative path cost) if enabled.
 * - Forwards any buffered data packets if the route is now available.
 * - Forwards the RREP towards the RREQ source if not the final destination, ensuring
 *   node- or link-disjointness as configured.
 * - Drops the RREP if it does not provide new or useful information.
 *
 * @param p Pointer to the received RREP packet.
 */
void AOMDV_QPV::recvReply(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_reply *rp = HDR_AOMDV_QPV_REPLY(p);
	aomdv_qpv_rt_entry *rt0, *rt;
	AOMDV_QPVBroadcastID *b = NULL;
	AOMDV_QPV_Path *forward_path = NULL;

#ifdef DEBUG
	fprintf(stderr, "%d - %s: received a REPLY\n", index, __FUNCTION__);
#endif // DEBUG

	if (mac_)
	{
		// --- MAJ PRR à chaque réception de RREP ---
		updatePRR_onRecv(p, ih->saddr());
		// --- MAJ PRR à chaque réception de RREP ---
	}

	/* If I receive a RREP with myself as source - drop packet (should not occur).
 Comment: rp_dst is the source of the RREP, or rather the destination of the RREQ. */
	if (rp->rp_dst == index)
	{
		Packet::free(p);
		return;
	}

	/*
	 *  Got a reply. So reset the "soft state" maintained for
	 *  route requests in the request table. We don't really have
	 *  have a separate request table. It is just a part of the
	 *  routing table itself.
	 */
	// Note that rp_dst is the dest of the data packets, not the
	// the dest of the reply, which is the src of the data packets.

	rt = rtable.rt_lookup(rp->rp_dst);

	/*
	 *  If I don't have a rt entry to this host... adding
	 */
	if (rt == 0)
	{
		rt = rtable.rt_add(rp->rp_dst);
	}

	/* If RREP contains more recent seqno for (RREQ) destination -
	   delete all old paths and add the new forward path to (RREQ) destination */
	if (rt->rt_seqno < rp->rp_dst_seqno)
	{
		rt->rt_seqno = rp->rp_dst_seqno;
		rt->rt_advertised_hops = INFINITY;
		rt->path_delete();
		rt->rt_flags = RTF_UP;
		/* Insert forward path to RREQ destination. */
		forward_path = rt->path_insert(rp->rp_src, rp->rp_hop_count + 1, CURRENT_TIME + rp->rp_lifetime, rp->rp_first_hop);
		// CHANGE
		rt->rt_last_hop_count = rt->path_get_max_hopcount();
		// CHANGE
	}
	/* If the sequence number in the RREP is the same as for route entry but
	   with a smaller hop count - try to insert new forward path to (RREQ) dest. */
	else if ((rt->rt_seqno == rp->rp_dst_seqno) &&
			 (rt->rt_advertised_hops > rp->rp_hop_count))
	{
		assert(rt->rt_flags == RTF_UP);
		/* If the path already exists - increase path lifetime */
		if ((forward_path = rt->disjoint_path_lookup(rp->rp_src, rp->rp_first_hop)))
		{
			assert(forward_path->hopcount == (rp->rp_hop_count + 1));
			forward_path->expire = max(forward_path->expire, CURRENT_TIME + rp->rp_lifetime);
		}
		/* If the path does not already exist, there is room for it and it
			does not differ too much in length - we add the path */
		else if (rt->new_disjoint_path(rp->rp_src, rp->rp_first_hop) &&
				 (rt->rt_num_paths_ < aomdv_qpv_max_paths_) &&
				 ((rp->rp_hop_count + 1) - rt->path_get_min_hopcount() <= aomdv_qpv_prim_alt_path_len_diff_))
		{
			/* Insert forward path to RREQ destination. */
			forward_path = rt->path_insert(rp->rp_src, rp->rp_hop_count + 1, CURRENT_TIME + rp->rp_lifetime, rp->rp_first_hop);
			// CHANGE
			rt->rt_last_hop_count = rt->path_get_max_hopcount();
			// CHANGE
		}
		/* Path did not exist nor could it be added - just drop packet. */
		else
		{
			Packet::free(p);
			return;
		}
	}
	/* The received RREP did not contain more recent information
	   than route table - so drop packet */
	else
	{
		Packet::free(p);
		return;
	}

	if (mac_)
	{
		// --- Cross-layer: coût cumulé sur le chemin ---
		if (forward_path)
		{
			nsaddr_t prev_hop = rp->rp_src;
			LinkQuality q = getLinkQuality(p, prev_hop);
			double cost = linkCost(p, q);

			// Cumulez le coût sur le chemin
			if (rp->rp_hop_count == 0)
				forward_path->path_cost = cost;
			else
				forward_path->path_cost = cost + rp->rp_cost_cumule;

			// Propagation du coût cumulé pour le prochain saut
			rp->rp_cost_cumule = forward_path->path_cost;
		}
		// --- Fin cross-layer ---
	}

	/* If route is up */
	if (rt->rt_flags == RTF_UP)
	{
		// Reset the soft state
		rt->rt_req_timeout = 0.0;
		rt->rt_req_last_ttl = 0;
		rt->rt_req_cnt = 0;

		if (ih->daddr() == index)
		{
			// I am the RREP destination

#ifdef DYNAMIC_RREQ_RETRY_TIMEOUT // This macro does not seem to be set.
			if (rp->rp_type == AOMDV_QPVTYPE_RREP)
			{
				rt->rt_disc_latency[rt->hist_indx] = (CURRENT_TIME - rp->rp_timestamp) / (double)(rp->rp_hop_count + 1);
				// increment indx for next time
				rt->hist_indx = (rt->hist_indx + 1) % MAX_HISTORY;
			}
#endif // DYNAMIC_RREQ_RETRY_TIMEOUT
		}

		/*
		 * Find out whether any buffered packet can benefit from the
		 * forward route.
		 */
		Packet *buffered_pkt;
		while ((buffered_pkt = rqueue.deque(rt->rt_dst)))
		{
			if (rt && (rt->rt_flags == RTF_UP))
			{
				forward(rt, buffered_pkt, NO_AOMDV_QPV_DELAY);
			}
		}
	}
	/* If I am the intended receipient of the RREP nothing more needs
	   to be done - so drop packet. */
	if (ih->daddr() == index)
	{
		Packet::free(p);
		return;
	}
	/* If I am not the intended receipient of the RREP - check route
	   table for a path to the RREP dest (i.e. the RREQ source). */
	rt0 = rtable.rt_lookup(ih->daddr());
	b = id_get(ih->daddr(), rp->rp_bcast_id); // Check for <RREQ src IP, bcast ID> tuple

#ifdef AOMDV_QPV_NODE_DISJOINT_PATHS

	if ((rt0 == NULL) || (rt0->rt_flags != RTF_UP) || (b == NULL) || (b->count))
	{
		Packet::free(p);
		return;
	}

	b->count = 1;
	AOMDV_QPV_Path *reverse_path = rt0->path_find();

	ch->addr_type() = AF_INET;
	ch->next_hop_ = reverse_path->nexthop;
	ch->xmit_failure_ = aomdv_qpv_rt_failed_callback;
	ch->xmit_failure_data_ = (void *)this;

	// route advertisement
	rp->rp_src = index;
	if (rt->rt_advertised_hops == INFINITY)
		rt->rt_advertised_hops = rt->path_get_max_hopcount();
	rp->rp_hop_count = rt->rt_advertised_hops;
	rp->rp_first_hop = (rt->path_find())->lasthop;

	reverse_path->expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;

	if (mac_)
	{
		// --- Cross-layer: propagation du coût cumulé ---
		// rp->rp_cost_cumule déjà mis à jour ci-dessus
		// MAJ PRR pour chaque envoi de RREP (reverse)
		updatePRR_onSend(p, reverse_path->nexthop);
		// --- Fin cross-layer ---
	}

	// CHANGE
	rt->rt_error = true;
	// CHANGE
	forward(rt0, p, NO_AOMDV_QPV_DELAY);
//   Scheduler::instance().schedule(target_, p, 0.);
#endif // AOMDV_QPV_NODE_DISJOINT_PATHS
#ifdef AOMDV_QPV_LINK_DISJOINT_PATHS
	/* Drop the RREP packet if we do not have a path back to the source,
  or the route is marked as down, or if we never received the original RREQ. */
	if ((rt0 == NULL) || (rt0->rt_flags != RTF_UP) || (b == NULL))
	{
		Packet::free(p);
		return;
	}
	/* Make sure we don't answer along the same path twice in response
	   to a certain RREQ. Try to find an unused (reverse) path to forward the RREP. */
	AOMDV_QPV_Path *reverse_path = NULL;
	AOMDV_QPV_Path *r = rt0->rt_path_list.lh_first;
	for (; r; r = r->path_link.le_next)
	{
		if (b->reverse_path_lookup(r->nexthop, r->lasthop) == NULL)
		{
			fprintf(stderr, "\tcycle cycle\n");
			reverse_path = r;
			break;
		}
	}
	/* If an unused reverse path is found and the forward path (for
	   this RREP) has not already been replied - forward the RREP. */
	if (reverse_path &&
		(b->forward_path_lookup(forward_path->nexthop, forward_path->lasthop) == NULL))
	{
		assert(forward_path->nexthop == rp->rp_src);
		assert(forward_path->lasthop == rp->rp_first_hop);
		/* Mark the forward and reverse path used to answer this RREQ as used. */
		b->reverse_path_insert(reverse_path->nexthop, reverse_path->lasthop);
		b->forward_path_insert(forward_path->nexthop, forward_path->lasthop);

		ch->addr_type() = AF_INET;
		ch->next_hop_ = reverse_path->nexthop;
		ch->xmit_failure_ = aomdv_qpv_rt_failed_callback;
		ch->xmit_failure_data_ = (void *)this;

		// route advertisement
		if (rt->rt_advertised_hops == INFINITY)
			rt->rt_advertised_hops = rt->path_get_max_hopcount();
		rp->rp_hop_count = rt->rt_advertised_hops;
		rp->rp_src = index;

		reverse_path->expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;

		if (mac_)
		{
			// --- Cross-layer: propagation du coût cumulé ---
			// rp->rp_cost_cumule déjà mis à jour ci-dessus
			// MAJ PRR pour chaque envoi de RREP (reverse)
			updatePRR_onSend(p, reverse_path->nexthop);
			// --- Fin cross-layer ---
		}

		// CHANGE
		rt->rt_error = true;
		// CHANGE
		forwardReply(rt0, p, NO_AOMDV_QPV_DELAY); // CHANGE (previously used forward())
												  //      Scheduler::instance().schedule(target_, p, 0.);
	}
	else
	{
		Packet::free(p);
		return;
	}
#endif // AOMDV_QPV_LINK_DISJOINT_PATHS
}

// AOMDV_QPV code
void AOMDV_QPV::recvError(Packet *p)
{
#ifdef DEBUG
	fprintf(stderr, "%s: node=%d\n", __FUNCTION__, index);
#endif // DEBUG
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(p);
	aomdv_qpv_rt_entry *rt;
	u_int8_t i;
	Packet *rerr = Packet::alloc();
	struct hdr_aomdv_qpv_error *nre = HDR_AOMDV_QPV_ERROR(rerr);

#ifdef DEBUG
	fprintf(stderr, "%s: multipath version\n", __FUNCTION__);
#endif // DEBUG

	nre->DestCount = 0;

	for (i = 0; i < re->DestCount; i++)
	{
		// For each unreachable destination
		AOMDV_QPV_Path *path;
		rt = rtable.rt_lookup(re->unreachable_dst[i]);
		/* If route entry exists, route is up, a path to the unreachable
			destination exists through the neigbor from which RERR was
			received, and my sequence number is not more recent - delete
			path and add it to the RERR message I will send. */
		if (rt && (rt->rt_flags == RTF_UP) &&
			(path = rt->path_lookup(ih->saddr())) &&
			(rt->rt_seqno <= re->unreachable_dst_seqno[i]))
		{
			assert((rt->rt_seqno % 2) == 0); // is the seqno even?
#ifdef DEBUG
			fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\t(%d\t%u\t%d)\n",
					__FUNCTION__, CURRENT_TIME,
					index, rt->rt_dst, rt->rt_seqno, ih->src_.addr_,
					re->unreachable_dst[i], re->unreachable_dst_seqno[i],
					ih->src_.addr_);
#endif // DEBUG

			rt->path_delete(ih->saddr());
			rt->rt_highest_seqno_heard = max(rt->rt_highest_seqno_heard, re->unreachable_dst_seqno[i]);
			if (rt->path_empty())
			{
				rt->rt_seqno = rt->rt_highest_seqno_heard;
				rt_down(rt);
				// CHANGE
				if (rt->rt_error)
				{
					nre->unreachable_dst[nre->DestCount] = rt->rt_dst;
					nre->unreachable_dst_seqno[nre->DestCount] = rt->rt_seqno;
					nre->DestCount += 1;
					rt->rt_error = false;
				}
				// CHANGE
			}
		}
	}

	if (nre->DestCount > 0)
	{
#ifdef DEBUG
		fprintf(stderr, "%s(%f): %d\t sending RERR...\n", __FUNCTION__, CURRENT_TIME, index);
#endif // DEBUG
		sendError(rerr);
	}
	else
	{
		Packet::free(rerr);
	}

	Packet::free(p);
}

/*
 Packet Transmission Routines
 */

/**
 * @brief Forwards a packet according to the AOMDV_QPV routing protocol, selecting the best path based on congestion and path cost.
 *
 * This function is responsible for forwarding packets in the AOMDV_QPV protocol implementation.
 * It performs the following key operations:
 *   - Drops the packet if its TTL has expired.
 *   - Delivers the packet locally if it is destined for the current node or is a broadcast.
 *   - If a valid route exists, selects the best path among multiple available paths by considering:
 *       - Path cost.
 *       - Congestion status of the interface queue (IFQ) for each next hop.
 *       - Composite metrics combining path cost and congestion penalty.
 *       - Prefers non-congested paths, but if all are congested, selects the least congested one.
 *   - Updates routing and packet headers as necessary, including next hop, address type, and direction.
 *   - Schedules the packet for transmission, applying jitter for broadcast packets and optional delay for unicast.
 *   - Logs detailed information for debugging and congestion analysis if debugging is enabled.
 *
 * @param rt Pointer to the routing table entry for the destination.
 * @param p Pointer to the packet to be forwarded.
 * @param delay Optional delay before forwarding the packet (in seconds).
 *
 * @note This function includes cross-layer logic to evaluate IFQ congestion and select the optimal path.
 * @note Broadcast packets are jittered to avoid collisions.
 * @note Updates per-path statistics such as PRR (Packet Reception Ratio) on each send.
 */
void AOMDV_QPV::forward(aomdv_qpv_rt_entry *rt, Packet *p, double delay)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	struct hdr_ip *ih = HDR_IP(p);

	if (ih->ttl_ == 0)
	{

#ifdef DEBUG
		fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
#endif // DEBUG

		drop(p, mac_->phy_->dropReason(p, DROP_RTR_TTL));
		return;
	}

	// AODV ns-2.31 code
	if (((ch->ptype() != PT_AOMDV_QPV && ch->direction() == hdr_cmn::UP) &&
		 ((u_int32_t)ih->daddr() == IP_BROADCAST)) ||
		(ih->daddr() == here_.addr_))
	{
		dmux_->recv(p, 0);
		return;
	}

	if (rt)
	{
		if (mac_)
		{
			assert(rt->rt_flags == RTF_UP);

			// --- Cross-layer: Sélection du chemin en tenant compte de la congestion IFQ ---
			// Cette section sélectionne le meilleur chemin en évitant les chemins congestionnés,
			// sauf si tous les chemins sont congestionnés (dans ce cas, on prend le moins mauvais).

			AOMDV_QPV_Path *best = NULL;
			double min_cost = 1e9;
			int next_path = 0;
			int not_congested_paths = 0;

			// Première passe : compter les chemins non congestionnés
			for (AOMDV_QPV_Path *tmp = rt->rt_path_list.lh_first; tmp; tmp = tmp->path_link.le_next)
			{
				priqueueEvalVSN *nh_ifq = get_ifq_for_nexthop(tmp->nexthop);

				bool congestedIFQ = nh_ifq ? notifyCongestionIFQ(nh_ifq) : false;
				int qlen = nh_ifq ? nh_ifq->getQueueLength() : 0;
				int congestionThreshold = nh_ifq ? nh_ifq->computeCongestionThreshold(p) : 0;

				if (!congestedIFQ && qlen <= congestionThreshold)
					not_congested_paths++;
			}

			// Deuxième passe : sélection du meilleur chemin
			for (AOMDV_QPV_Path *path_iter = rt->rt_path_list.lh_first; path_iter; path_iter = path_iter->path_link.le_next)
			{
				if (path_iter->expire < CURRENT_TIME)
					continue;

				double total_cost = path_iter->path_cost;

				// Récupérer l’IFQ associée à ce nexthop (cross-layer)
				priqueueEvalVSN *nh_ifq = get_ifq_for_nexthop(path_iter->nexthop);

				PacketCountStats stats = nh_ifq->countPackets();
				bool congestedIFQ = nh_ifq ? notifyCongestionIFQ(nh_ifq) : false;
				int congestionThreshold = nh_ifq ? nh_ifq->computeCongestionThreshold(p) : 0;
				int qlen = nh_ifq ? nh_ifq->getQueueLength() : 0;

				// N'ignorer le chemin congestionné que s'il existe au moins un chemin non congestionné
				if (not_congested_paths > 0)
				{
					if (congestedIFQ)
						continue;
					if (qlen > congestionThreshold)
						continue;
				}

				// Métrique composite : coût du chemin + pénalité de congestion
				if (nh_ifq && qlen > 0)
				{
					double occupancy = double(qlen) / double(nh_ifq->getQlim());
					total_cost += congestionThreshold * occupancy;
				}

				if (total_cost < min_cost)
				{
					min_cost = total_cost;
					best = path_iter;
				}

				next_path++;

				// Debug log pour chaque chemin
				if (AOMDV_QPV_DBG)
				{
					char msg[1000];
					sprintf(msg, "PATH %d: total_cost=%.4f path_cost=%.4f qlen=%d nexthop=%d",
							next_path, total_cost, path_iter->path_cost, qlen, path_iter->nexthop);
					log("forward_path_iter (cross-layer)", msg);
				}

				// Log de congestion détaillé si besoin
				if (logtarget && AOMDV_QPV_DBG)
				{
					sprintf(logtarget->pt_->buffer(),
							"AOMDV_QPV (cross-layer): Route congested for video=%d (seuil=%d), total=%d, non_video=%d, video_M=%d, video_D=%d",
							stats.video, congestionThreshold, stats.total, stats.non_video, stats.video_M, stats.video_D);
					logtarget->pt_->dump();

					for (map<int, int>::iterator it = stats.prio_count.begin(); it != stats.prio_count.end(); ++it)
					{
						char prio_msg[1000];
						sprintf(prio_msg, "AOMDV_QPV (cross-layer): IFQ prio=%d count=%d", it->first, it->second);
						sprintf(logtarget->pt_->buffer(), "%s", prio_msg);
						logtarget->pt_->dump();
					}
				}
			}
			// --- Fin cross-layer

			if (best)
			{
				ch->addr_type() = NS_AF_INET;
				ch->next_hop() = best->nexthop;
				best->expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;

				// MAJ PRR pour chaque envoi de DATA ou routage
				updatePRR_onSend(p, best->nexthop);

				// Debug log for selected best path
				if (AOMDV_QPV_DBG)
				{
					char msg[1000];

					if (packetType == PT_VIDEO)
					{
						sprintf(msg, "SELECTED: min_cost=%.4f best->path_cost=%.4f best->nexthop=%d for VIDEO packetType=%s, frameType='%c', priority=%d",
								min_cost, best->path_cost, best->nexthop, packet_info.name(packetType), frameType, priority);
					}
					else
					{
						sprintf(msg, "SELECTED: min_cost=%.4f best->path_cost=%.4f best->nexthop=%d for NON-VIDEO packetType=%s",
								min_cost, best->path_cost, best->nexthop, packet_info.name(packetType));
					}

					log("forward_best_path (cross-layer)", msg);
				}

				// CHANGE
				if ((ih->saddr() != index) && DATA_PACKET(ch->ptype()))
				{
					rt->rt_error = true;
				}

				// CHANGE
				ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
			}
		}
		else
		{
			assert(rt->rt_flags == RTF_UP);
			// AOMDV_QPV code
			ch->addr_type() = NS_AF_INET;
			AOMDV_QPV_Path *path = rt->path_find();
			ch->next_hop() = path->nexthop;
			path->expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
			// CHANGE
			if ((ih->saddr() != index) && DATA_PACKET(ch->ptype()))
			{
				rt->rt_error = true;
			}
			// CHANGE
			ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
		}
	}
	else
	{ // if it is a broadcast packet
	  // assert(ch->ptype() == PT_AODV); // maybe a diff pkt type like gaf
		assert(ih->daddr() == (nsaddr_t)IP_BROADCAST);
		ch->addr_type() = NS_AF_NONE;
		ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
	}

	if (ih->daddr() == (nsaddr_t)IP_BROADCAST)
	{
		// If it is a broadcast packet
		assert(rt == 0);
		/*
		 *  Jitter the sending of broadcast packets by 10ms
		 */
		Scheduler::instance().schedule(target_, p,
									   0.01 * Random::uniform());
	}
	else
	{ // Not a broadcast packet
		if (delay > 0.0)
		{
			Scheduler::instance().schedule(target_, p, delay);
		}
		else
		{
			// Not a broadcast packet, no delay, send immediately
			Scheduler::instance().schedule(target_, p, 0.);
		}
	}
}

/**
 * @brief Sends a Route Request (RREQ) packet for the specified destination.
 *
 * This function initiates the route discovery process by broadcasting a RREQ packet
 * for the given destination address. It implements rate limiting, expanding ring search,
 * and manages route request retries and timeouts according to AOMDV_QPV protocol logic.
 *
 * Key operations performed:
 * - Allocates and initializes a RREQ packet.
 * - Implements cross-layer cost initialization if MAC layer is present.
 * - Checks if a valid route already exists or if a request is rate-limited.
 * - Handles maximum retry logic and drops queued packets if no route is found.
 * - Dynamically determines the TTL for the RREQ using expanding ring search.
 * - Calculates and sets appropriate timeouts for the route request.
 * - Fills all required fields in the RREQ packet header.
 * - Updates cross-layer metrics (e.g., PRR) if applicable.
 * - Schedules the packet for transmission.
 *
 * @param dst The destination address for which the route is being requested.
 */
void AOMDV_QPV::sendRequest(nsaddr_t dst)
{
	// Allocate a RREQ packet
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_request *rq = HDR_AOMDV_QPV_REQUEST(p);
	aomdv_qpv_rt_entry *rt = rtable.rt_lookup(dst);
	assert(rt);

	if (mac_)
	{
		// Initialisation du coût cumulé (cross-layer)
		rq->rq_cost_cumule = 0.0; // Premier saut, coût nul
	}

	/*
	 *  Rate limit sending of Route Requests. We are very conservative
	 *  about sending out route requests.
	 */

	if (rt->rt_flags == RTF_UP)
	{
		assert(rt->rt_hops != INFINITY2);
		Packet::free((Packet *)p);
		return;
	}

	if (rt->rt_req_timeout > CURRENT_TIME)
	{
		Packet::free((Packet *)p);
		return;
	}

	// rt_req_cnt is the no. of times we did network-wide broadcast
	// RREQ_RETRIES is the maximum number we will allow before
	// going to a long timeout.

	if (rt->rt_req_cnt > RREQ_RETRIES)
	{
		rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
		rt->rt_req_cnt = 0;
		Packet *buf_pkt;
		while ((buf_pkt = rqueue.deque(rt->rt_dst)))
		{
			drop(buf_pkt, mac_->phy_->dropReason(buf_pkt, DROP_RTR_NO_ROUTE));
		}
		Packet::free((Packet *)p);
		return;
	}

#ifdef DEBUG
	fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d\n",
			++route_request, index, rt->rt_dst);
#endif // DEBUG

	// Determine the TTL to be used this time.
	// Dynamic TTL evaluation - SRD

	rt->rt_req_last_ttl = max(rt->rt_req_last_ttl, rt->rt_last_hop_count);

	if (0 == rt->rt_req_last_ttl)
	{
		// first time query broadcast
		ih->ttl_ = TTL_START;
	}
	else
	{
		// Expanding ring search.
		if (rt->rt_req_last_ttl < TTL_THRESHOLD)
			ih->ttl_ = rt->rt_req_last_ttl + TTL_INCREMENT;
		else
		{
			// network-wide broadcast
			ih->ttl_ = NETWORK_DIAMETER;
			rt->rt_req_cnt += 1;
		}
	}

	// remember the TTL used  for the next time
	rt->rt_req_last_ttl = ih->ttl_;

	// PerHopTime is the roundtrip time per hop for route requests.
	// The factor 2.0 is just to be safe .. SRD 5/22/99
	// Also note that we are making timeouts to be larger if we have
	// done network wide broadcast before.

	rt->rt_req_timeout = 2.0 * (double)ih->ttl_ * PerHopTime(rt);
	if (rt->rt_req_cnt > 0)
		rt->rt_req_timeout *= rt->rt_req_cnt;
	rt->rt_req_timeout += CURRENT_TIME;

	// Don't let the timeout to be too large, however .. SRD 6/8/99
	if (rt->rt_req_timeout > CURRENT_TIME + MAX_RREQ_TIMEOUT)
		rt->rt_req_timeout = CURRENT_TIME + MAX_RREQ_TIMEOUT;
	rt->rt_expire = 0;

#ifdef DEBUG
	fprintf(stderr, "(%2d) - %2d sending Route Request, dst: %d, tout %f ms\n",
			++route_request,
			index, rt->rt_dst,
			rt->rt_req_timeout - CURRENT_TIME);
#endif // DEBUG

	// Fill out the RREQ packet
	// ch->uid() = 0;
	ch->ptype() = PT_AOMDV_QPV;
	ch->size() = IP_HDR_LEN + rq->size();
	ch->iface() = -2;
	ch->error() = 0;
	ch->addr_type() = NS_AF_NONE;
	ch->prev_hop_ = index; // AODV hack

	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;

	// Fill up some more fields.
	rq->rq_type = AOMDV_QPVTYPE_RREQ;
	// AOMDV_QPV code
	rq->rq_hop_count = 0;
	rq->rq_bcast_id = bid++;
	rq->rq_dst = dst;
	rq->rq_dst_seqno = (rt ? rt->rt_seqno : 0);
	rq->rq_src = index;
	seqno += 2;
	assert((seqno % 2) == 0);
	rq->rq_src_seqno = seqno;
	rq->rq_timestamp = CURRENT_TIME;

	if (mac_)
	{
		// --- Cross-layer: coût cumulé (déjà initialisé à 0.0 ci-dessus) ---
		// MAJ PRR pour chaque envoi de RREQ (broadcast)
		updatePRR_onSend(p, IP_BROADCAST);
		// --- Fin cross-layer ---
	}

	Scheduler::instance().schedule(target_, p, 0.);
}

// AOMDV_QPV code
/**
 * @brief Sends a Route Reply (RREP) packet in the AOMDV_QPV protocol.
 *
 * This function constructs and sends a RREP packet in response to a route request,
 * filling in all necessary header fields, including cross-layer metrics such as
 * cumulative link cost if MAC layer information is available. The packet is then
 * scheduled for transmission to the specified next hop.
 *
 * @param ipdst        The IP address of the destination node for the RREP.
 * @param hop_count    The hop count from the source to the destination.
 * @param rpdst        The destination address for which the route is being replied.
 * @param rpseq        The destination sequence number.
 * @param lifetime     The lifetime of the route being advertised.
 * @param timestamp    The timestamp associated with the route discovery.
 * @param nexthop      The next hop node to which the RREP should be sent.
 * @param bcast_id     The broadcast ID associated with the route request.
 * @param rp_first_hop The first hop address on the reverse path to the source.
 */
void AOMDV_QPV::sendReply(nsaddr_t ipdst, u_int32_t hop_count, nsaddr_t rpdst,
						  u_int32_t rpseq, double lifetime, double timestamp,
						  nsaddr_t nexthop, u_int32_t bcast_id, nsaddr_t rp_first_hop)
{
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_reply *rp = HDR_AOMDV_QPV_REPLY(p);

#ifdef DEBUG
	fprintf(stderr, "sending Reply from %d at %.2f\n", index, Scheduler::instance().clock());
#endif // DEBUG

	rp->rp_type = AOMDV_QPVTYPE_RREP;
	// rp->rp_flags = 0x00;
	rp->rp_hop_count = hop_count;
	rp->rp_dst = rpdst;
	rp->rp_dst_seqno = rpseq;
	rp->rp_src = index;
	rp->rp_lifetime = lifetime;
	rp->rp_timestamp = timestamp;
	rp->rp_bcast_id = bcast_id;
	rp->rp_first_hop = rp_first_hop;

	if (mac_)
	{
		// --- Cross-layer: coût cumulé ---
		// Calcule la métrique composite pour nexthop (le voisin vers qui on envoie le RREP)
		LinkQuality q = getLinkQuality(p, nexthop);
		double cost = linkCost(p, q);

		// On suppose que le coût cumulé précédent est 0 pour la source du RREP, sinon à récupérer du chemin
		// Ici, pour la première émission, on initialise à cost, sinon à cost + coût cumulé précédent
		rp->rp_cost_cumule = cost; // Ajoutez ce champ dans hdr_aomdv_qpv_reply (aomdv_qpv_packet.h)
								   // --- Fin cross-layer ---
	}

	// ch->uid() = 0;
	ch->ptype() = PT_AOMDV_QPV;
	ch->size() = IP_HDR_LEN + rp->size();
	ch->iface() = -2;
	ch->error() = 0;
	ch->addr_type() = NS_AF_INET;

	ch->next_hop_ = nexthop;

	ch->xmit_failure_ = aomdv_qpv_rt_failed_callback;
	ch->xmit_failure_data_ = (void *)this;

	ih->saddr() = index;
	ih->daddr() = ipdst;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = NETWORK_DIAMETER;

	if (mac_)
	{
		// MAJ PRR pour chaque envoi de RREP (unicast)
		updatePRR_onSend(p, nexthop);
	}

	Scheduler::instance().schedule(target_, p, 0.);
}

void AOMDV_QPV::sendError(Packet *p, bool jitter)
{
#ifdef ERROR
	fprintf(stderr, "%s: node=%d\n", __FUNCTION__, index);
#endif // DEBUG
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(p);

#ifdef ERROR
	fprintf(stderr, "sending Error from %d at %.2f\n", index, Scheduler::instance().clock());
#endif // DEBUG

	re->re_type = AOMDV_QPVTYPE_RERR;
	// re->reserved[0] = 0x00; re->reserved[1] = 0x00;
	//  DestCount and list of unreachable destinations are already filled

	// ch->uid() = 0;
	ch->ptype() = PT_AOMDV_QPV;
	ch->size() = IP_HDR_LEN + re->size();
	ch->iface() = -2;
	ch->error() = 0;
	ch->addr_type() = NS_AF_NONE;
	ch->next_hop_ = 0;
	ch->prev_hop_ = index;			 // AODV hack
	ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction

	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = 1;

	// Do we need any jitter? Yes
	if (jitter)
		Scheduler::instance().schedule(target_, p, 0.01 * Random::uniform());
	else
		Scheduler::instance().schedule(target_, p, 0.0);
}

/*
 Neighbor Management Functions
 */

void AOMDV_QPV::sendHello()
{
	Packet *p = Packet::alloc();
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_reply *rh = HDR_AOMDV_QPV_REPLY(p);

#ifdef DEBUG
	fprintf(stderr, "sending Hello from %d at %.2f\n", index, Scheduler::instance().clock());
#endif // DEBUG

	rh->rp_type = AOMDV_QPVTYPE_HELLO;
	// rh->rp_flags = 0x00;
	//  AOMDV_QPV code
	rh->rp_hop_count = 0;
	rh->rp_dst = index;
	rh->rp_dst_seqno = seqno;
	rh->rp_lifetime = (1 + ALLOWED_HELLO_LOSS) * HELLO_INTERVAL;

	// ch->uid() = 0;
	ch->ptype() = PT_AOMDV_QPV;
	ch->size() = IP_HDR_LEN + rh->size();
	ch->iface() = -2;
	ch->error() = 0;
	ch->addr_type() = NS_AF_NONE;
	ch->prev_hop_ = index; // AODV hack

	ih->saddr() = index;
	ih->daddr() = IP_BROADCAST;
	ih->sport() = RT_PORT;
	ih->dport() = RT_PORT;
	ih->ttl_ = 1;

	Scheduler::instance().schedule(target_, p, 0.0);
}

void AOMDV_QPV::recvHello(Packet *p)
{
	// AOMDV_QPV code
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_reply *rp = HDR_AOMDV_QPV_REPLY(p);
	AOMDV_QPV_Neighbor *nb;

	nb = nb_lookup(rp->rp_dst);
	if (nb == 0)
	{
		nb_insert(rp->rp_dst);
	}
	else
	{
		nb->nb_expire = CURRENT_TIME +
						(1.5 * ALLOWED_HELLO_LOSS * HELLO_INTERVAL);
	}

	// AOMDV_QPV code
	// Add a route to this neighbor
	ih->daddr() = index;
	rp->rp_src = ih->saddr();
	rp->rp_first_hop = index;
	recvReply(p);

	// Packet::free(p);  // CHANGED BY ME (commented this line)
}

// AOMDV_QPV code
void AOMDV_QPV::nb_insert(nsaddr_t id)
{
	// CHANGE
	AOMDV_QPV_Neighbor *nb;
	if ((nb = nb_lookup(id)) == NULL)
	{
		nb = new AOMDV_QPV_Neighbor(id);
		assert(nb);
		// CHANGE
		nb->nb_expire = CURRENT_TIME + (HELLO_INTERVAL * ALLOWED_HELLO_LOSS);

		LIST_INSERT_HEAD(&nbhead, nb, nb_link);
	}
	else
	{
		// CHANGE
		nb->nb_expire = CURRENT_TIME + (HELLO_INTERVAL * ALLOWED_HELLO_LOSS);
	}
}

AOMDV_QPV_Neighbor *
AOMDV_QPV::nb_lookup(nsaddr_t id)
{
	AOMDV_QPV_Neighbor *nb = nbhead.lh_first;

	for (; nb; nb = nb->nb_link.le_next)
	{
		if (nb->nb_addr == id)
			break;
	}
	return nb;
}

/*
 * Called when we receive *explicit* notification that a Neighbor
 * is no longer reachable.
 */
void AOMDV_QPV::nb_delete(nsaddr_t id)
{
	AOMDV_QPV_Neighbor *nb = nbhead.lh_first;

	log_link_del(id);
	seqno += 2; // Set of neighbors changed
	assert((seqno % 2) == 0);

	for (; nb; nb = nb->nb_link.le_next)
	{
		if (nb->nb_addr == id)
		{
			LIST_REMOVE(nb, nb_link);
			delete nb;
			break;
		}
	}

	handle_link_failure(id);

	// AOMDV_QPV code
	Packet *p;
#ifdef AOMDV_QPV_PACKET_SALVAGING
	//	while((p = AOMDV_QPVifqueue->prq_get_nexthop(id))) {
	while ((p = AOMDV_QPVifqueue->filter(id)))
	{
		struct hdr_cmn *ch = HDR_CMN(p);
		struct hdr_ip *ih = HDR_IP(p);

		if (!DATA_PACKET(ch->ptype()))
			drop(p, mac_->phy_->dropReason(p, DROP_RTR_HELLO));
		else
		{
			// salvage the packet using an alternate path if available.
			aomdv_qpv_rt_entry *rt = rtable.rt_lookup(ih->daddr());
			if (rt && (rt->rt_flags == RTF_UP) && (ch->aomdv_qpv_salvage_count_ < AOMDV_QPV_MAX_SALVAGE_COUNT))
			{
				ch->aomdv_qpv_salvage_count_ += 1;
				forward(rt, p, NO_AOMDV_QPV_DELAY);
			}
			else
				drop(p, mac_->phy_->dropReason(p, DROP_RTR_HELLO));
		}
	}

#else // NO PACKET SALVAGING

	while ((p = AOMDV_QPVifqueue->filter(id)))
	{
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_HELLO));
	}

	/*while((p = AOMDV_QPVifqueue->prq_get_nexthop(id))) {
		drop(p, mac_->phy_->dropReason(p, DROP_RTR_HELLO));
	} */

#endif // NO PACKET SALVAGING
}

/*
 * Purges all timed-out Neighbor Entries - runs every
 * HELLO_INTERVAL * 1.5 seconds.
 */
void AOMDV_QPV::nb_purge()
{
	AOMDV_QPV_Neighbor *nb = nbhead.lh_first;
	AOMDV_QPV_Neighbor *nbn;
	double now = CURRENT_TIME;

	for (; nb; nb = nbn)
	{
		nbn = nb->nb_link.le_next;
		if (nb->nb_expire <= now)
		{
			nb_delete(nb->nb_addr);
		}
	}
}

// AOMDV_QPV code
void AOMDV_QPV::forwardReply(aomdv_qpv_rt_entry *rt, Packet *p, double delay)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);

	if (ih->ttl_ == 0)
	{

#ifdef DEBUG
		fprintf(stderr, "%s: calling drop()\n", __PRETTY_FUNCTION__);
#endif // DEBUG

		drop(p, mac_->phy_->dropReason(p, DROP_RTR_TTL));
		return;
	}

	if (rt)
	{
		assert(rt->rt_flags == RTF_UP);
		ch->addr_type() = NS_AF_INET;

		// CHANGE (don't want to get a new nexthop for this route entry. Use nexthop set in recvReply().
		//   AODV_Path *path = rt->path_find();
		//   ch->next_hop() = path->nexthop;
		//   path->expire = CURRENT_TIME + ACTIVE_ROUTE_TIMEOUT;
		// CHANGE

		// CHANGE
		if ((ih->saddr() != index) && DATA_PACKET(ch->ptype()))
		{
			rt->rt_error = true;
		}
		// CHANGE
		ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
	}
	else
	{ // if it is a broadcast packet
	  // assert(ch->ptype() == PT_AODV); // maybe a diff pkt type like gaf
		assert(ih->daddr() == (nsaddr_t)IP_BROADCAST);
		ch->addr_type() = NS_AF_NONE;
		ch->direction() = hdr_cmn::DOWN; // important: change the packet's direction
	}

	if (ih->daddr() == (nsaddr_t)IP_BROADCAST)
	{
		// If it is a broadcast packet
		assert(rt == 0);
		/*
		 *  Jitter the sending of broadcast packets by 10ms
		 */
		Scheduler::instance().schedule(target_, p,
									   0.01 * Random::uniform());
	}
	else
	{ // Not a broadcast packet
		if (delay > 0.0)
		{
			Scheduler::instance().schedule(target_, p, delay);
		}
		else
		{
			// Not a broadcast packet, no delay, send immediately
			Scheduler::instance().schedule(target_, p, 0.);
		}
	}
}

void AOMDV_QPV::log(char *event, char *additional)
{
	if (AOMDV_QPV_DBG)
		cout << "L " << Scheduler::instance().clock() << " " << index << " " << "AOMDV_QPV" << " " << event << " " << additional
			 << endl;
}

// Determine weights based on frame type and priority
/**
 * @brief Calculates dynamic weights for link quality metrics based on packet type, frame type, and priority.
 *
 * This function assigns weights to three link quality indicators: LQI (Link Quality Indicator),
 * SNR (Signal-to-Noise Ratio), and PRR (Packet Reception Ratio), depending on the type of packet,
 * the frame type (for video packets), and the priority of the packet.
 *
 * - For video packets (packetType == PT_VIDEO):
 *   - If frameType is 'M' (main frame), assigns highest reliability to PRR.
 *   - If frameType is 'D' (diff frame), assigns moderate reliability.
 * - For non-video packets, assigns a strong weight to PRR.
 * - The PRR weight is further adjusted by the packet's priority: higher priority increases PRR's weight.
 * - The weights are normalized so their sum equals 1.0.
 *
 * @param packetType The type of the packet (e.g., PT_VIDEO).
 * @param frameType  The frame type for video packets ('M' for main frame, 'D' for delta frame, '0' for non-video).
 * @param priority   The priority level of the packet (lower value means higher priority).
 * @return DynamicWeights Struct containing normalized weights for LQI, SNR, and PRR.
 */
DynamicWeights AOMDV_QPV::getDynamicWeights(packet_t packetType, char frameType, int priority)
{
	DynamicWeights dw;

	// Pour NON-VIDEO : frameType = '0', priority = 0, pondération forte sur PRR

	if (packetType == PT_VIDEO)
	{
		// VIDEO : logique existante selon frameType/priorité
		if (frameType == 'M')
		{
			// main frame: highest reliability
			dw.wLQI = 0.2;
			dw.wSNR = 0.3;
			dw.wPRR = 0.5;
		}
		else if (frameType == 'D')
		{
			// delta frame: moderate reliability
			dw.wLQI = 0.25;
			dw.wSNR = 0.35;
			dw.wPRR = 0.4;
		}
	}
	else
	{
		dw.wLQI = 0.1;
		dw.wSNR = 0.2;
		dw.wPRR = 0.7;
	}

	// adjust PRR weight by priority (higher priority -> more weight)
	// Les priorites vont de 1 (la plus critique) a getMaxPrioNb(), et non de 0 :
	// avec (max - p) / max le dernier niveau obtenait exactement zero, ce qui
	// annulait wPRR et le retirait de la metrique. Le +1 lui laisse un poids
	// faible mais non nul -- 1,0 au niveau 1 et 0,077 au niveau 13 pour
	// getMaxPrioNb() = 13 -- de sorte que la ponderation decroisse sans jamais
	// degenerer.
	double prioFactor = double(mac_->phy_->getMaxPrioNb() + 1 - priority) / double(mac_->phy_->getMaxPrioNb());

	dw.wPRR *= prioFactor;

	// normalize sum to 1.0
	double sum = dw.wLQI + dw.wSNR + dw.wPRR;

	if (sum > 0.0)
	{
		dw.wLQI /= sum;
		dw.wSNR /= sum;
		dw.wPRR /= sum;
	}

	return dw;
}

/**
 * @brief Calculates the link cost for a given packet based on link quality metrics and dynamic weights.
 *
 * This function computes a cost value representing the quality of a wireless link for a specific packet.
 * The cost is determined using normalized Link Quality Indicator (LQI), Signal-to-Noise Ratio (SNR),
 * and Packet Reception Ratio (PRR), each weighted dynamically according to the packet type, frame type,
 * and priority. Lower cost values indicate better link quality.
 *
 * @param p Pointer to the packet for which the link cost is being calculated.
 * @param q Reference to the LinkQuality structure containing LQI, SNR, and PRR metrics.
 * @return The computed link cost as a double. Lower values represent higher quality links.
 */
double AOMDV_QPV::linkCost(Packet *p, LinkQuality &q)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	double normLQI = mac_->phy_->clampDouble(q.LQI / 255.0, 0.0, 1.0);
	double normSNR = mac_->phy_->clampDouble(q.SNR / 50.0, 0.0, 1.0);
	double normPRR = mac_->phy_->clampDouble(q.PRR, 0.0, 1.0);

	// Get dynamic weights based on packet type, frame type, and priority
	DynamicWeights dw = getDynamicWeights(packetType, frameType, priority);

	double cost = dw.wLQI * (1.0 - normLQI) + dw.wSNR * (1.0 - normSNR) + dw.wPRR * (1.0 - normPRR);

	// debug log
	if (AOMDV_QPV_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "using: LQI=%.2f SNR=%.2f PRR=%.2f cost=%.4f for VIDEO packetType=%s, frameType='%c', priority=%d",
					q.LQI, q.SNR, q.PRR, cost, packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: LQI=%.2f SNR=%.2f PRR=%.2f cost=%.4f for NON-VIDEO packetType=%s",
					q.LQI, q.SNR, q.PRR, cost, packet_info.name(packetType));
		}

		log("linkCost (cross-layer)", msg);
	}

	return cost;
}

/**
 * @brief Computes and retrieves the link quality metrics for a given neighbor based on the provided packet.
 *
 * This function extracts relevant information from the packet, such as packet type, frame type, and priority.
 * It then queries the MAC layer (if available) to obtain the average Link Quality Indicator (LQI) and Signal-to-Noise Ratio (SNR)
 * for the specified neighbor. The Packet Reception Ratio (PRR) is computed using the extracted packet information and neighbor address.
 * If debugging is enabled, a detailed log message is generated describing the link quality metrics used.
 *
 * @param p Pointer to the packet for which link quality is being assessed.
 * @param neighbor The address of the neighbor node for which link quality metrics are to be retrieved.
 * @return LinkQuality A structure containing the computed LQI, SNR, and PRR values for the specified neighbor.
 */
LinkQuality AOMDV_QPV::getLinkQuality(Packet *p, nsaddr_t neighbor)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;
	LinkQuality q;

	if (mac_)
	{
		q.LQI = mac_->get_average_lqi(neighbor);
		q.SNR = mac_->get_average_snr(neighbor);
	}

	q.PRR = computePRR(packetType, frameType, priority, neighbor);

	// debug log
	if (AOMDV_QPV_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "using: neighbor=%d LQI=%.2f SNR=%.2f PRR=%.2f for VIDEO packetType=%s, frameType='%c', priority=%d",
					neighbor, q.LQI, q.SNR, q.PRR, packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: neighbor=%d LQI=%.2f SNR=%.2f PRR=%.2f for NON-VIDEO packetType=%s",
					neighbor, q.LQI, q.SNR, q.PRR, packet_info.name(packetType));
		}

		log("getLinkQuality (cross-layer)", msg);
	}

	return q;
}

/**
 * @brief Updates the Packet Reception Ratio (PRR) statistics when a packet is sent to a neighbor.
 *
 * This function increments the count of packets sent to a specific neighbor and, if debugging is enabled,
 * logs detailed information about the packet being sent. The log includes the neighbor address, the total
 * number of packets sent, the packet type, and, for video packets, the frame type and priority.
 *
 * @param p Pointer to the packet being sent.
 * @param neighbor The address of the neighbor to which the packet is sent.
 */
void AOMDV_QPV::updatePRR_onSend(Packet *p, nsaddr_t neighbor)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	pkts_sent_[neighbor]++;

	// debug log
	if (AOMDV_QPV_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "using: neighbor=%d pkts_sent=%d for VIDEO packetType=%s, frameType='%c', priority=%d",
					neighbor, pkts_sent_[neighbor], packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: neighbor=%d pkts_sent=%d for NON-VIDEO packetType=%s",
					neighbor, pkts_sent_[neighbor], packet_info.name(packetType));
		}

		log("updatePRR_onSend (cross-layer)", msg);
	}
}

/**
 * @brief Updates the Packet Reception Ratio (PRR) statistics upon receiving a packet from a neighbor.
 *
 * This method increments the received packet count for the specified neighbor and logs debug information
 * if debugging is enabled. The debug log includes details such as the neighbor address, number of packets
 * received, packet type, frame type (for video packets), and priority.
 *
 * @param p Pointer to the received Packet.
 * @param neighbor The address of the neighbor from which the packet was received.
 */
void AOMDV_QPV::updatePRR_onRecv(Packet *p, nsaddr_t neighbor)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	pkts_recv_[neighbor]++;

	// debug log
	if (AOMDV_QPV_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "using: neighbor=%d pkts_recv=%d for VIDEO packetType=%s, frameType='%c', priority=%d",
					neighbor, pkts_recv_[neighbor], packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: neighbor=%d pkts_recv=%d for NON-VIDEO packetType=%s",
					neighbor, pkts_recv_[neighbor], packet_info.name(packetType));
		}

		log("updatePRR_onRecv (cross-layer)", msg);
	}
}

/**
 * @brief Computes the Packet Reception Ratio (PRR) for a given neighbor.
 *
 * This function calculates the PRR as the ratio of packets received to packets sent
 * for a specified neighbor. The PRR value is clamped between 0.0 and 1.0. If no packets
 * have been sent to the neighbor, the PRR is set to 1.0 by default.
 *
 * Optionally logs debug information if debugging is enabled.
 *
 * @param packetType   The type of the packet (e.g., PT_VIDEO).
 * @param frameType    The frame type character (used for video packets).
 * @param priority     The priority of the packet.
 * @param neighbor     The address of the neighbor node.
 * @return             The computed PRR value in the range [0.0, 1.0].
 */
double AOMDV_QPV::computePRR(packet_t packetType, char frameType, int priority, nsaddr_t neighbor)
{
	double prr;
	int sent = pkts_sent_[neighbor];
	int recv = pkts_recv_[neighbor];

	if (sent != 0)
		prr = double(recv) / double(sent);
	else if (sent == 0)
		prr = 1.0;

	if (prr < 0.0)
		prr = 0.0;

	if (prr > 1.0)
		prr = 1.0;

	// debug log
	if (AOMDV_QPV_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "using: neighbor=%d sent=%d recv=%d prr=%.2f for VIDEO packetType=%s, frameType='%c', priority=%d",
					neighbor, sent, recv, prr, packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: neighbor=%d sent=%d recv=%d prr=%.2f for NON-VIDEO packetType=%s",
					neighbor, sent, recv, prr, packet_info.name(packetType));
		}

		log("computePRR (cross-layer)", msg);
	}

	return prr;
}

// Notify congestion IFQ (Input Frame Queue) based on packet stats
/**
 * @brief Notifies and logs if the Interface Queue (IFQ) is congested.
 *
 * This function checks the congestion status of the provided IFQ (priqueueEvalVSN).
 * If congestion is detected, and logging is enabled, it logs detailed statistics
 * about the packet counts in the IFQ, including total, video, non-video, and
 * specific video packet types. It also logs the count of packets per priority.
 *
 * @param ifq Pointer to the priqueueEvalVSN interface queue to check for congestion.
 * @return true if the IFQ is congested, false otherwise.
 */
bool AOMDV_QPV::notifyCongestionIFQ(priqueueEvalVSN *ifq)
{
	PacketCountStats stats = ifq->countPackets();
	bool congestedIFQ = ifq->congestedIFQ_;

	if (congestedIFQ)
	{
		if (logtarget && AOMDV_QPV_DBG)
		{
			sprintf(logtarget->pt_->buffer(),
					"AOMDV_QPV (cross-layer): Congestion IFQ détectée, total=%d, video=%d, non_video=%d, video_M=%d, video_D=%d",
					stats.total, stats.video, stats.non_video, stats.video_M, stats.video_D);
			logtarget->pt_->dump();

			// Log des priorités
			for (map<int, int>::iterator it = stats.prio_count.begin(); it != stats.prio_count.end(); ++it)
			{
				char prio_msg[1000];
				sprintf(prio_msg, "AOMDV_QPV (cross-layer): IFQ prio=%d count=%d", it->first, it->second);
				sprintf(logtarget->pt_->buffer(), "%s", prio_msg);
				logtarget->pt_->dump();
			}
		}
	}

	return congestedIFQ;
}

// Enregistre l'IFQ associée à un nexthop
/**
 * @brief Registers the interface queue (IFQ) for a specified next hop.
 *
 * This method associates a given next hop address with its corresponding
 * priority queue evaluation structure (priqueueEvalVSN). The mapping is
 * stored internally and can be used for efficient packet forwarding or
 * queue management based on the next hop.
 *
 * @param nexthop The address of the next hop node.
 * @param ifq Pointer to the priqueueEvalVSN structure representing the interface queue for the next hop.
 */
void AOMDV_QPV::register_ifq_for_nexthop(nsaddr_t nexthop, priqueueEvalVSN *ifq)
{
	nexthop_ifq_map_[nexthop] = ifq;
}

// Retourne le pointeur IFQ pour un nexthop
/**
 * Retrieves the interface queue (IFQ) associated with a given next hop address.
 *
 * This function searches the internal mapping of next hop addresses to their corresponding
 * priqueueEvalVSN pointers. If an entry for the specified `nexthop` exists, the associated
 * queue pointer is returned. Otherwise, the function returns 0.
 *
 * @param nexthop The next hop address for which the IFQ is requested.
 * @return Pointer to the priqueueEvalVSN associated with the given next hop, or 0 if not found.
 */
priqueueEvalVSN *AOMDV_QPV::get_ifq_for_nexthop(nsaddr_t nexthop)
{
	map<nsaddr_t, priqueueEvalVSN *>::iterator it = nexthop_ifq_map_.find(nexthop);

	if (it != nexthop_ifq_map_.end())
		return it->second;

	return 0;
}
