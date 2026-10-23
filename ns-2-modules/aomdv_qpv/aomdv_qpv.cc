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
#include <mac/link-metrics.h>
#include <aomdv_qpv/aomdv_qpv.h>
#include <wpan/p802_15_4trace.h>
#include <aomdv_qpv/aomdv_qpv_packet.h>
#include <random.h>
// #include <energy-model.h>
#include <iostream>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define CURRENT_TIME Scheduler::instance().clock()

// AOMDV_QPV
#define DROP_RTR_RTEXPIRE "REXP"
#define DROP_RTR_HELLO "HELO"



/// Map: next-hop address -> pointer to the interface queue it belongs to
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

			// HELLO maintenance runs together with the link-layer
			// failure hysteresis (chapter 4): the hellos keep the
			// neighbour table and carry the queue occupancy, the
			// hysteresis decides when MAC failures mean a dead link.
			htimer.handle((Event *)0);
			ntimer.handle((Event *)0);

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
			// dynamic_cast, not a C cast: Mac802_15_4EvalVSN derives from
			// the wpan MAC and from LinkMetricsStore, and a C cast would
			// silently produce a wrong pointer to the second base.
			mac_ = dynamic_cast<LinkMetricsStore *>(TclObject::lookup(argv[2]));

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
									rtimer(this), rqueue(), mac_(NULL), outcome_(this)
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
	hello_interval_ = 1.0;
	bind("HELLO_INTERVAL_", &hello_interval_);
	max_rreq_timeout_ = 1.0;
	bind("MAX_RREQ_TIMEOUT_", &max_rreq_timeout_);
	active_route_timeout_ = 10.0;
	bind("ACTIVE_ROUTE_TIMEOUT_", &active_route_timeout_);
	class_cost_ = 1;
	bind("CLASS_COST_", &class_cost_);
	hello_queue_ = 1;
	bind("HELLO_QUEUE_", &hello_queue_);
	rl_alpha_ = 0.3;
	bind("RL_ALPHA_", &rl_alpha_);
	rl_epsilon_ = 0.1;
	bind("RL_EPSILON_", &rl_epsilon_);
	rl_path_ = 0;
	bind("RL_PATH_", &rl_path_);
	rl_congestion_ = 0;
	bind("RL_CONGESTION_", &rl_congestion_);
	rl_break_ = 0;
	bind("RL_BREAK_", &rl_break_);
	rl_discovery_ = 0;
	bind("RL_DISCOVERY_", &rl_discovery_);
	discard_rule_ = 0;
	bind("DISCARD_RULE_", &discard_rule_);
	rl_discard_ = 0;
	bind("RL_DISCARD_", &rl_discard_);
	discard_fail_ = 0.0;
	for (int a = 0; a < 5; a++)
		rle_q_[a] = (a == 1) ? 0.05 : 0.0; // the rule's 0.2, by a small margin
	rle_a_ = 1;
	rle_ack_ = rle_fail_ = rle_disc_ = 0.0;
	rle_n_ = 0;
	outcome_registered_ = false;
	// Each agent starts on the rule's own choice, by a small margin: the
	// 0.5 threshold, three failures, 50 ms, the rule's weights.
	for (int k = 0; k < 3; k++)
		for (int a = 0; a < 4; a++)
			rlc_q_[k][a] = (a == 1) ? 0.05 : 0.0;
	for (int s = 0; s < 2; s++)
		for (int a = 0; a < 4; a++)
			rlb_q_[s][a] = (a == 2) ? 0.05 : 0.0;
	// Path: the rule's own profile of the class, M frames for the critical
	// class and D frames for the others.
	for (int k = 0; k < 3; k++)
		for (int a = 0; a < 5; a++)
			rlp_q_[k][a] = (a == (k == 0 ? 1 : 2)) ? 0.05 : 0.0;
	rlp_cur_ = -1;
	for (int b = 0; b < 2; b++)
		for (int a = 0; a < 3; a++)
			rld_q_[b][a] = (a == 2) ? 0.05 : 0.0;

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
	/* Do not send a HELLO message unless we have a valid route entry. */
	if (agent->rtable.rt_has_active_route())
		agent->sendHello();
	// AODV ns-2.31 code
	double interval = 0.75 * agent->hello_interval_ + (0.5 * agent->hello_interval_ * Random::uniform());
	assert(interval >= 0);
	Scheduler::instance().schedule(this, &intr, interval);
}

void AOMDV_QPVNeighborTimer::handle(Event *)
{
	agent->nb_purge();
	Scheduler::instance().schedule(this, &intr, agent->hello_interval_);
}

void AOMDV_QPVRouteCacheTimer::handle(Event *)
{
	agent->rt_purge();
#define FREQUENCY 0.5 // sec
	Scheduler::instance().schedule(this, &intr, FREQUENCY);
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
	// The treatment of a MAC failure, every cross-layer stack. The
	// invalidation hysteresis that once took its place - K(p,t) failures in
	// a second, a route error at once - was measured against it and refused
	// (study 07, decisions dt07 and dt08 of 15 Oct 2026); it replays from
	// the git tag pre-refactor-2026-10-15.
	// A transmission abandoned after the MAC's retries is a non-delivery
	// observed on that link, not evidence that the link is dead: it
	// enters the delivery ratio at once, so that the next packets weigh
	// the neighbour by it, and the packet leaves again on the best path of
	// the reserve, with no route error and no flood. Only three failures in
	// a row or the silence of the neighbour's HELLOs remove it.
	// Without this a destroyed neighbour kept its last ratio and its
	// paths for 22.5 s: on S1 at half destruction 96 to 100 % of the
	// video losses were packets sent to it (audit of 5 Oct 2026).
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	nsaddr_t failed = ch->next_hop_;

	// The MAC reports a missing acknowledgement and a channel that stayed
	// busy alike. Only a packet that left the radio towards the
	// neighbour at least once and got no acknowledgement says something
	// of that neighbour; a busy channel is the node's own surroundings
	// and counts neither in pi nor towards the three failures.
	bool transmitted = mac_ && mac_->lastTxUid(failed) == ch->uid();

	if ((u_int32_t)failed != IP_BROADCAST && transmitted)
	{
		map<nsaddr_t, double>::iterator it = prr_.find(failed);
		double before = (it == prr_.end()) ? 1.0 : it->second;
		prr_[failed] = 0.5 * before;

		// Three packets in a row that left the radio and got no
		// acknowledgement, with no packet sent to the neighbour in
		// between, declare it dead as its HELLO silence would:
		// a relay with no other path otherwise kept losing its traffic
		// for 22.5 s while upstream knew nothing (S1, audit of 5 Oct
		// 2026). Consecutive means the radio's count of packets sent to
		// it moved by this packet alone since the previous failure.
		int total = mac_ ? mac_->totalTxPackets(failed) : -1;
		if (total >= 0)
		{
			map<nsaddr_t, int>::iterator m = fail_mark_.find(failed);
			if (m != fail_mark_.end() && total - m->second <= 1)
				fail_streak_[failed] += 1;
			else
				fail_streak_[failed] = 1;
			fail_mark_[failed] = total;
			// The link-break detection agent (d22) picks the count when a
			// run starts and keeps it for the run.
			static const int RLW_K[4] = {1, 2, 3, 5};
			if (rl_break_ && fail_streak_[failed] == 1)
			{
				rlb_run_s_[failed] = rlwState();
				rlb_run_k_[failed] = rlwPick(rlb_run_s_[failed]);
			}
			int dead_after = rl_break_ ? RLW_K[rlb_run_k_[failed]] : AOMDV_QPV_DEAD_AFTER_FAILURES;
			if (fail_streak_[failed] >= dead_after)
			{
				if (rl_break_)
				{
					RlwDecl d;
					d.t = CURRENT_TIME;
					d.kidx = rlb_run_k_[failed];
					d.st = rlb_run_s_[failed];
					rlb_decl_[failed] = d;
					rlb_run_k_.erase(failed);
					rlb_run_s_.erase(failed);
				}
				fail_streak_.erase(failed);
				fail_mark_.erase(failed);
				nb_delete(failed);
			}
		}
	}

	aomdv_qpv_rt_entry *rt = rtable.rt_lookup(ih->daddr());
	// Priority protection (d27), with the early discard: a failed D packet
	// is given up at once, its salvage would take the channel from the
	// main frames. Every other packet takes the salvage below (the extra
	// retries once given to M1-M3 changed nothing since the corrected
	// salvage and were removed, finding X-24, 16 Oct 2026).
	if ((discard_rule_ || rl_discard_) && ch->ptype() == PT_VIDEO && ch->frameType_[0] == 'D')
	{
		drop(p, mac_->dropReason(p, DROP_RTR_MAC_CALLBACK));
		return;
	}
	if (DATA_PACKET(ch->ptype()) && (u_int32_t)ih->daddr() != IP_BROADCAST &&
		rt && rt->rt_flags == RTF_UP &&
		ch->aomdv_qpv_salvage_count_ < AOMDV_QPV_MAX_SALVAGE_COUNT)
	{
		ch->aomdv_qpv_salvage_count_ += 1;
		// The salvaged packet takes the best path by cost. The halved pi
		// already steers it away from the failed neighbour when another
		// path exists; when none does it goes back to that neighbour, not
		// declared dead, instead of being dropped as the former strict
		// exclusion did (finding X-18, study 07, 15 Oct 2026: +2.6 dB on
		// the base point, +6.6 dB on S1).
		forward(rt, p, NO_AOMDV_QPV_DELAY);
		return;
	}
	// A packet of this node whose route has just gone waits for the
	// new discovery instead of being lost.
	if (DATA_PACKET(ch->ptype()) && ih->saddr() == index &&
		(u_int32_t)ih->daddr() != IP_BROADCAST && rt && rt->rt_flags != RTF_UP)
	{
		rqueue.enque(p);
		sendRequest(ih->daddr());
		return;
	}
	drop(p, mac_->dropReason(p, DROP_RTR_MAC_CALLBACK));
}

// AOMDV_QPV code
void AOMDV_QPV::handle_link_failure(nsaddr_t id)
{
	bool error = true;
	aomdv_qpv_rt_entry *rt, *rtn;
	Packet *rerr = Packet::alloc();
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(rerr);
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
		sendError(rerr, false);
	}
	else
	{
		Packet::free(rerr);
	}
}

void AOMDV_QPV::rt_down(aomdv_qpv_rt_entry *rt)
{
	/*
	 *  Make sure that you don't "down" a route more than once.
	 */

	// AOMDV_QPV code

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
		sendError(rerr, false);

		drop(p, mac_->dropReason(p, DROP_RTR_NO_ROUTE));
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
					drop(p, mac_->dropReason(p, DROP_RTR_RTEXPIRE));
				}

				rt->rt_seqno++;
				rt->rt_seqno = max(rt->rt_seqno, rt->rt_highest_seqno_heard);
				if (rt->rt_seqno % 2 == 0)
					rt->rt_seqno += 1;
				rt_down(rt);
			}
		}
		else if (rt->rt_flags == RTF_UP)
		{
			// If the route is not expired,
			// and there are packets in the sendbuffer waiting,
			// forward them. This should not be needed, but this extra
			// check does no harm.
			// The queue is drained before the packets are forwarded: forward() puts a
			// packet back in the queue when the route, though up, has no live path,
			// and a loop that dequeued and forwarded in turn took the same packet
			// out again for ever (the simulator spun at 100 % CPU, alarm mode under
			// fire, 10 Oct 2026). Order and behaviour are otherwise unchanged.
			std::vector<Packet *> drained;
			while ((p = rqueue.deque(rt->rt_dst)))
				drained.push_back(p);
			for (size_t i = 0; i < drained.size(); i++)
			{
				forward(rt, drained[i], delay);
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

		// The neighbour that sent this frame is its MAC source, not its IP
		// source: a relayed video packet carries the camera's address,
		// which filed every relayed sample under the camera instead of the
		// previous hop. Packets originated here carry no MAC header yet.
		if (ch->direction() == hdr_cmn::UP)
		{
			nsaddr_t prevHop = p802_15_4macSA(p);
			mac_->update_link_metrics(prevHop, lqi, snr);

			// Any frame from a neighbour the link-break detection agent declared dead
			// proves it alive: the counts up to the one used were too short.
			// HELLOs alone were not enough evidence - under the alarm load
			// they are lost often, and a live neighbour silent for 10 s was
			// scored dead (10 Oct 2026).
			if (rl_break_)
			{
				map<nsaddr_t, RlwDecl>::iterator d = rlb_decl_.find(prevHop);
				if (d != rlb_decl_.end())
				{
					if (CURRENT_TIME - d->second.t <= 15.0)
						for (int k = 0; k <= d->second.kidx; k++)
							rlwScore(d->second.st, k, -2.0);
					rlb_decl_.erase(d);
				}
			}

			// A unicast frame addressed to this node, sent at the power the
			// previous hop keeps for it: measured for the next HELLO.
			if (p802_15_4macDA(p) == index &&
				(packetType == PT_VIDEO || packetType == PT_AOMDV_QPV))
			{
				RxFeedback &fb = rx_fb_[prevHop];
				fb.rssi_sum += ch->receivedSignalStrengthIndicator_;
				fb.sinr_sum += snr;
				fb.n += 1;
			}
		}
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
		drop(p, mac_->dropReason(p, DROP_RTR_ROUTE_LOOP));
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
			drop(p, mac_->dropReason(p, DROP_RTR_TTL));
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
	double rq_downstream_cost = -1.0; // cost of the reverse path beyond its first hop


	/*
	 * Drop if:
	 *      - I'm the source
	 *      - I recently heard this request.
	 */

	if (rq->rq_src == index)
	{
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
		// --- Cross-layer: cumulative cost along the reverse path ---
		// Composite metric for the previous hop
		nsaddr_t prev_hop = ih->saddr();
		LinkQuality q = getLinkQuality(p, prev_hop);
		double cost = rlDiscoveryCost(p, q); // the path agent's weights when it is on (d22)

		// Bottleneck accumulation, as on the forward path: the maximum
		// per-hop cost plus the per-hop tax (see recvReply).
		rq_downstream_cost = (rq->rq_hop_count == 0) ? -1.0 : rq->rq_cost_cumule;
		if (rq->rq_hop_count == 0)
			rq->rq_cost_cumule = cost + AOMDV_QPV_PATH_HOP_TAX;
		else
			rq->rq_cost_cumule = max(cost, rq->rq_cost_cumule) + AOMDV_QPV_PATH_HOP_TAX;
		// --- Cross-layer: cumulative cost along the reverse path ---
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
			// --- Cross-layer: store the cumulative cost in the reverse path ---
			if (reverse_path)
			{
				reverse_path->path_cost = rq->rq_cost_cumule;
				reverse_path->downstream_cost = rq_downstream_cost;
			}
			// --- Cross-layer: store the cumulative cost in the reverse path ---
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

	// Every reverse path this request installed or refreshed carries its
	// cost, the alternate disjoint ones included (they kept 0 and -1).
	if (mac_ && reverse_path)
	{
		reverse_path->path_cost = rq->rq_cost_cumule;
		reverse_path->downstream_cost = rq_downstream_cost;
	}

	/* If route is up */
	if (rt0->rt_flags == RTF_UP)
	{
		// Reset the soft state
		rt0->rt_req_timeout = 0.0;
		rt0->rt_req_last_ttl = 0;
		rt0->rt_req_cnt = 0;
		rt0->rt_req_sent = 0;

		/*
		 * Find out whether any buffered packet can benefit from the
		 * reverse route.
		 */
		Packet *buffered_pkt;
		// Drained before forwarding, as in rt_purge().
		std::vector<Packet *> drained;
		while ((buffered_pkt = rqueue.deque(rt0->rt_dst)))
			drained.push_back(buffered_pkt);
		for (size_t i = 0; i < drained.size(); i++)
		{
			if (rt0 && (rt0->rt_flags == RTF_UP))
			{
				forward(rt0, drained[i], NO_AOMDV_QPV_DELAY);
			}
		}
	}
	/* Check route entry for RREQ destination */
	rt = rtable.rt_lookup(rq->rq_dst);

	/* I am the intended receiver of the RREQ - so send a RREP */
	if (rq->rq_dst == index)
	{

		// RFC 3561 section 6.6.1: the destination MUST increment its own
		// sequence number when the RREQ asks for a seqno EQUAL to its
		// current one - <= where the ns-2 port wrote <. Without the equal
		// case, a requester whose route died still holding seqno S and a
		// destination sitting at the same S answer each other S forever:
		// every RREP is rejected as not fresher, and the route only comes
		// back when a stray MAC callback finally inflates the requester
		// (measured: 35 s of livelock on the 0.019/0.001 codecs' first
		// frame). The parity bump below keeps the published seqno even.
		if (seqno <= rq->rq_dst_seqno)
		{
			seqno = rq->rq_dst_seqno + 1;
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
			rq->rq_first_hop = (rt0->path_find())->lasthop;

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
		// The AOMDV port assumed a route can only hold an even seqno while
		// UP (the assert this branch used to carry) - so it never restored
		// rt_flags. A route brought down at an even seqno then collected
		// fresh paths here while staying DOWN: the send buffer never
		// drained and discovery looped (the other half of the measured
		// 35 s livelock). Reaching this line means a valid path exists at
		// the current seqno - the route is usable. The drop sub-branch
		// above returns, so a pathless entry can never be marked UP here.
		rt->rt_flags = RTF_UP;
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
		// --- Cross-layer: cumulative cost along the path ---
		if (forward_path)
		{
			nsaddr_t prev_hop = rp->rp_src;
			LinkQuality q = getLinkQuality(p, prev_hop);
			double cost = rlDiscoveryCost(p, q); // the path agent's weights when it is on (d22)

			// Bottleneck accumulation: a chain is as lossy as its worst
			// link, so the path metric is the maximum per-hop cost, plus
			// a small per-hop tax so equal bottlenecks still prefer the
			// shorter chain. The former sum rewarded fewer hops more than
			// it punished weakness: eleven marginal diagonal hops priced
			// under thirteen sound rectilinear ones, and the reference
			// frame died on the capture losses of the weak chain.
			if (rp->rp_hop_count == 0)
				forward_path->path_cost = cost + AOMDV_QPV_PATH_HOP_TAX;
			else
				forward_path->path_cost = max(cost, rp->rp_cost_cumule) + AOMDV_QPV_PATH_HOP_TAX;
			forward_path->downstream_cost = (rp->rp_hop_count == 0) ? -1.0 : rp->rp_cost_cumule;

			// Propagate the cumulative cost to the next hop
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
		rt->rt_req_sent = 0;

		if (ih->daddr() == index)
		{
			// I am the RREP destination
			if (rl_discovery_)
			{
				map<nsaddr_t, RldPending>::iterator pd = rld_pending_.find(rp->rp_dst);
				if (pd != rld_pending_.end())
				{
					double l = CURRENT_TIME - pd->second.t;
					double r = 1.0 - (l < 1.0 ? l : 1.0);
					double &q = rld_q_[pd->second.bucket][pd->second.a];
					q += rl_alpha_ * (r - q);
					rld_pending_.erase(pd);
				}
			}

		}

		/*
		 * Find out whether any buffered packet can benefit from the
		 * forward route.
		 */
		Packet *buffered_pkt;
		// Drained before forwarding, as in rt_purge().
		std::vector<Packet *> drained;
		while ((buffered_pkt = rqueue.deque(rt->rt_dst)))
			drained.push_back(buffered_pkt);
		for (size_t i = 0; i < drained.size(); i++)
		{
			if (rt && (rt->rt_flags == RTF_UP))
			{
				forward(rt, drained[i], NO_AOMDV_QPV_DELAY);
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

	reverse_path->expire = CURRENT_TIME + active_route_timeout_;

	if (mac_)
	{
		// --- Cross-layer: propagate the cumulative cost ---
		// rp->rp_cost_cumule was already updated above
		// Update the PRR on every RREP sent (reverse)
		updatePRR_onSend(p, reverse_path->nexthop);
		// --- Fin cross-layer ---
	}

	// CHANGE
	rt->rt_error = true;
	// CHANGE
	forward(rt0, p, NO_AOMDV_QPV_DELAY);
}

// AOMDV_QPV code
void AOMDV_QPV::recvError(Packet *p)
{
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(p);
	aomdv_qpv_rt_entry *rt;
	u_int8_t i;
	Packet *rerr = Packet::alloc();
	struct hdr_aomdv_qpv_error *nre = HDR_AOMDV_QPV_ERROR(rerr);


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

			rt->path_delete(ih->saddr());
			rt->rt_highest_seqno_heard = max(rt->rt_highest_seqno_heard, re->unreachable_dst_seqno[i]);
			if (rt->path_empty())
			{
				rt->rt_seqno = rt->rt_highest_seqno_heard;
				// RFC 3561 section 6.1: invalidating a route increments its
				// sequence number - the odd value is what makes the next
				// RREQ ask for strictly fresher state. A RERR carrying the
				// same even seqno used to bring the route down still
				// "valid": every cached intermediate then answered that
				// same seqno forever and recvReply's freshness rule
				// rejected them all (measured: 35 s outage over the first
				// video frame). rt_purge already applies this parity;
				// this path had lost it.
				if (rt->rt_seqno % 2 == 0)
					rt->rt_seqno++;
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


		drop(p, mac_->dropReason(p, DROP_RTR_TTL));
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
			rlRegisterOutcome();

			// --- Cross-layer: path selection accounting for interface-queue congestion ---
			// This block picks the best path while avoiding congested ones,
			// unless every path is congested, in which case the least bad one is taken.

			// One path selection for every stack (selectPath): the
			// chapter-4 rules, with the path or the congestion agent taking
			// over its own decision when it is on. The agents' decision is
			// kept by packet uid until the MAC reports the frame's outcome.
			int theta = -1, prof = -1;
			double occ = 0.0, min_cost = 1e9;
			AOMDV_QPV_Path *best = selectPath(rt, p, theta, occ, prof, min_cost);
			if (best && (rl_path_ || rl_congestion_))
			{
				RlCtx x;
				x.cls = rlClass(p);
				x.theta = theta;
				x.prof = prof;
				x.occ = occ;
				x.t = CURRENT_TIME;
				rl_ctx_[ch->uid()] = x;
			}

			if (!best)
			{
				// No live path left (all expired): a packet of this node
				// waits for a new discovery, a relayed one is dropped.
				if (ih->saddr() == index)
				{
					rqueue.enque(p);
					sendRequest(rt->rt_dst);
				}
				else
					drop(p, mac_->dropReason(p, DROP_RTR_NO_ROUTE));
				return;
			}
			if (best)
			{
				ch->addr_type() = NS_AF_INET;
				ch->next_hop() = best->nexthop;
				best->expire = CURRENT_TIME + active_route_timeout_;

				// Update the PRR on every data or routing packet sent
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
			path->expire = CURRENT_TIME + active_route_timeout_;
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
		 *  Jitter the sending of broadcast packets. The port's 10 ms
		 *  window bunches a hundred nodes' RREQ rebroadcasts into one
		 *  collision wall that the single unicast RREP must then cross
		 *  hop by hop: on the 19-hop 250 m chain the sink's reply died
		 *  three floods in a row and the first route took 11.26 s
		 *  (measured, deterministic). Fifty milliseconds staggers the
		 *  flood beyond the MAC's vulnerable window - the RFC's own
		 *  anti-collision jitter, sized for this node count - and the
		 *  flood still crosses the network in well under a second.
		 *
		 */
		double window = 0.05;
		if (rl_discovery_ && ch->ptype() == PT_AOMDV_QPV &&
			HDR_AOMDV_QPV(p)->ah_type == AOMDV_QPVTYPE_RREQ &&
			HDR_AOMDV_QPV_REQUEST(p)->reserved[1] >= 1 &&
			HDR_AOMDV_QPV_REQUEST(p)->reserved[1] <= 3)
		{
			// The discovery agent of the source chose the window (d22).
			static const double W[3] = {0.01, 0.025, 0.05};
			window = W[HDR_AOMDV_QPV_REQUEST(p)->reserved[1] - 1];
		}
		Scheduler::instance().schedule(target_, p,
									   window * Random::uniform());
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
		// Initialise the cumulative cost (cross-layer)
		rq->rq_cost_cumule = 0.0; // first hop, zero cost
	}

	/*
	 *  Rate limit sending of Route Requests. We are very conservative
	 *  about sending out route requests.
	 */

	if (rt->rt_flags == RTF_UP)
	{
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
		rt->rt_req_timeout = CURRENT_TIME + max_rreq_timeout_;
		rt->rt_req_cnt = 0;
		rt->rt_req_sent = 0;
		Packet *buf_pkt;
		while ((buf_pkt = rqueue.deque(rt->rt_dst)))
		{
			drop(buf_pkt, mac_->dropReason(buf_pkt, DROP_RTR_NO_ROUTE));
		}
		Packet::free((Packet *)p);
		return;
	}


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
	if (rt->rt_req_timeout > CURRENT_TIME + max_rreq_timeout_)
		rt->rt_req_timeout = CURRENT_TIME + max_rreq_timeout_;
	rt->rt_expire = 0;


	// Fill out the RREQ packet
	rq->reserved[0] = 0;
	rq->reserved[1] = 0;
	if (rl_discovery_)
	{
		// A flood still awaiting its reply when the next one of the round
		// leaves got none in time.
		map<nsaddr_t, RldPending>::iterator pd = rld_pending_.find(dst);
		if (pd != rld_pending_.end())
		{
			double &q = rld_q_[pd->second.bucket][pd->second.a];
			q += rl_alpha_ * (-1.0 - q);
		}
		int bucket = (rt->rt_req_sent == 0) ? 0 : 1;
		int a = 0;
		for (int k = 1; k < 3; k++)
			if (rld_q_[bucket][k] > rld_q_[bucket][a])
				a = k;
		if (Random::uniform() < rl_epsilon_)
			a = (int)(Random::uniform() * 3) % 3;
		RldPending np;
		np.a = a;
		np.bucket = bucket;
		np.t = CURRENT_TIME;
		rld_pending_[dst] = np;
		rq->reserved[1] = a + 1;
	}
	if (rt->rt_req_sent < 255)
		rt->rt_req_sent++;
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
		// --- Cross-layer: cumulative cost (already initialised to 0.0 above) ---
		// Update the PRR on every RREQ sent (broadcast)
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


	rp->rp_type = AOMDV_QPVTYPE_RREP;
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
		// --- Cross-layer: cumulative cost ---
		// Composite metric for the next hop, the neighbour the RREP is sent to
		LinkQuality q = getLinkQuality(p, nexthop);
		double cost = rlDiscoveryCost(p, q); // the path agent's weights when it is on (d22)

		// The previous cumulative cost is 0 at the RREP source; otherwise it is read from the path
		// On the first send it starts at cost, afterwards at cost + the previous cumulative cost
		rp->rp_cost_cumule = cost; // this field lives in hdr_aomdv_qpv_reply (aomdv_qpv_packet.h)
								   // --- Fin cross-layer ---
	}

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
		// Update the PRR on every RREP sent (unicast)
		updatePRR_onSend(p, nexthop);
	}

	Scheduler::instance().schedule(target_, p, 0.);
}

void AOMDV_QPV::sendError(Packet *p, bool jitter)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_ip *ih = HDR_IP(p);
	struct hdr_aomdv_qpv_error *re = HDR_AOMDV_QPV_ERROR(p);


	re->re_type = AOMDV_QPVTYPE_RERR;
	//  DestCount and list of unreachable destinations are already filled

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


	rlRegisterOutcome();
	if (rl_break_)
		rlwVerdicts();
	if (rl_discard_)
		rlsEpoch();
	if (rl_path_ || rl_congestion_)
	{
		// Decisions whose frame never reached the MAC (dropped in a queue)
		// are forgotten after 5 s.
		for (map<int, RlCtx>::iterator it = rl_ctx_.begin(); it != rl_ctx_.end();)
		{
			if (CURRENT_TIME - it->second.t > 5.0)
				rl_ctx_.erase(it++);
			else
				++it;
		}
	}

	rh->rp_type = AOMDV_QPVTYPE_HELLO;
	//  AOMDV_QPV code
	rh->rp_hop_count = 0;
	rh->rp_dst = index;
	rh->rp_dst_seqno = seqno;
	rh->rp_lifetime = (1 + ALLOWED_HELLO_LOSS) * hello_interval_;

	// The sender's own interface queue, advertised to the neighbours.
	{
		priqueueEvalVSN *own = get_ifq_for_nexthop(index);
		int ql = own ? own->getQueueLength() : 0;
		int qm = own ? own->getQlim() : 0;
		rh->rp_qlen = (u_int8_t)(ql > 255 ? 255 : ql);
		rh->rp_qlim = (u_int8_t)(qm > 255 ? 255 : qm);
		rh->rp_qcong = (own && own->congestedIFQ_) ? 1 : 0;
	}

	// What this node received from each neighbour since its last HELLO:
	// the count gives the neighbour its delivery ratio, the means its
	// transmit power control.
	rh->rp_fb_n = 0;
	for (map<nsaddr_t, RxFeedback>::iterator it = rx_fb_.begin();
		 it != rx_fb_.end() && rh->rp_fb_n < AOMDV_QPV_MAX_TXFB; ++it)
	{
		if (it->second.n == 0)
			continue;
		double rssi = it->second.rssi_sum / it->second.n;
		double sinr = it->second.sinr_sum / it->second.n;
		int k = rh->rp_fb_n++;
		rh->rp_fb_addr[k] = it->first;
		rh->rp_fb_cnt[k] = (u_int16_t)(it->second.n > 65535 ? 65535 : it->second.n);
		rh->rp_fb_rssi[k] = (int8_t)clampMetric(floor(rssi + 0.5), -128.0, 127.0);
		rh->rp_fb_sinr[k] = (int8_t)clampMetric(floor(sinr + 0.5), -128.0, 127.0);
	}
	rx_fb_.clear();

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

	// The neighbour's queue as it advertised it.
	{
		NbQueueState st;
		st.qlen = rp->rp_qlen;
		st.qlim = rp->rp_qlim;
		st.congested = rp->rp_qcong != 0;
		st.t = CURRENT_TIME;
		nb_queue_[rp->rp_dst] = st;
	}

	// The neighbour's report on the packets this node sent it since its
	// previous HELLO. The delivery ratio of the window, smoothed with the
	// previous ones: a neighbour that was sent packets and reports none
	// received none. A lost HELLO folds two windows of sending into one
	// window of reception and lowers the sample, which a lossy link
	// deserves anyway.
	{
		bool reported = false;
		int cnt = 0;
		double rssi = 0.0, sinr = 0.0;
		for (int k = 0; k < rp->rp_fb_n && k < AOMDV_QPV_MAX_TXFB; k++)
			if (rp->rp_fb_addr[k] == index)
			{
				reported = true;
				cnt = rp->rp_fb_cnt[k];
				rssi = rp->rp_fb_rssi[k];
				sinr = rp->rp_fb_sinr[k];
				break;
			}
		// Sent means left the radio; the routing's own count, which also
		// holds the packets the node's queue dropped, is the fallback of
		// a MAC that cannot tell.
		int sent = sent_win_[rp->rp_dst];
		sent_win_[rp->rp_dst] = 0;
		int radio = mac_ ? mac_->takeTxPackets(rp->rp_dst) : -1;
		if (radio >= 0)
			sent = radio;
		if (sent > 0)
		{
			double sample = reported ? min(1.0, double(cnt) / double(sent)) : 0.0;
			map<nsaddr_t, double>::iterator it = prr_.find(rp->rp_dst);
			prr_[rp->rp_dst] = (it == prr_.end()) ? sample : 0.5 * it->second + 0.5 * sample;
		}
		if (mac_ && mac_->txPowerControlled())
			mac_->txPowerFeedback(rp->rp_dst, reported, rssi, sinr);
	}

	nb = nb_lookup(rp->rp_dst);
	if (nb == 0)
	{
		nb_insert(rp->rp_dst);
	}
	else
	{
		nb->nb_expire = CURRENT_TIME +
						(1.5 * ALLOWED_HELLO_LOSS * hello_interval_);
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
		nb->nb_expire = CURRENT_TIME + (hello_interval_ * ALLOWED_HELLO_LOSS);

		LIST_INSERT_HEAD(&nbhead, nb, nb_link);
	}
	else
	{
		// CHANGE
		nb->nb_expire = CURRENT_TIME + (hello_interval_ * ALLOWED_HELLO_LOSS);
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
	while ((p = AOMDV_QPVifqueue->filter(id)))
	{
		struct hdr_cmn *ch = HDR_CMN(p);
		struct hdr_ip *ih = HDR_IP(p);

		if (!DATA_PACKET(ch->ptype()))
			drop(p, mac_->dropReason(p, DROP_RTR_HELLO));
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
				drop(p, mac_->dropReason(p, DROP_RTR_HELLO));
		}
	}

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


		drop(p, mac_->dropReason(p, DROP_RTR_TTL));
		return;
	}

	if (rt)
	{
		assert(rt->rt_flags == RTF_UP);
		ch->addr_type() = NS_AF_INET;

		// CHANGE (don't want to get a new nexthop for this route entry. Use nexthop set in recvReply().

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
		 *  Jitter the sending of broadcast packets. The port's 10 ms
		 *  window bunches a hundred nodes' RREQ rebroadcasts into one
		 *  collision wall that the single unicast RREP must then cross
		 *  hop by hop: on the 19-hop 250 m chain the sink's reply died
		 *  three floods in a row and the first route took 11.26 s
		 *  (measured, deterministic). Fifty milliseconds staggers the
		 *  flood beyond the MAC's vulnerable window - the RFC's own
		 *  anti-collision jitter, sized for this node count - and the
		 *  flood still crosses the network in well under a second.
		 */
		Scheduler::instance().schedule(target_, p,
									   0.05 * Random::uniform());
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

	// The path agent (RL_PATH_, d22) replaces the weights below by the
	// profile it picked for the packet's class.
	if (rl_path_ && rlp_cur_ >= 0)
	{
		static const double P[5][3] = {{0.1, 0.2, 0.7}, {0.2, 0.3, 0.5}, {0.25, 0.35, 0.4},
					       {0.6, 0.2, 0.2}, {0.2, 0.6, 0.2}};
		dw.wLQI = P[rlp_cur_][0];
		dw.wSNR = P[rlp_cur_][1];
		dw.wPRR = P[rlp_cur_][2];
		return dw;
	}

	// NON-VIDEO: frameType = '0', priority = 0, weighted heavily towards PRR

	if (packetType == PT_VIDEO)
	{
		// VIDEO: the existing logic, driven by frameType and priority
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
	// Priorities run from 1 (the most critical) to getMaxPrioNb(), not from 0:
	// with (max - p) / max the last level came out at exactly zero, which
	// cancelled wPRR and removed it from the metric. The +1 leaves it a small
	// but non-zero weight -- 1.0 at level 1 and 0.077 at level 13 for
	// getMaxPrioNb() = 13 -- so the weighting decreases without ever
	// degenerating.
	// kappa of the priority level; 1 outside the codec's levels (control,
	// scalar readings carry no level), as chapter 4 defines it.
	int maxPrio = mac_->getMaxPrioNb();
	double prioFactor = (priority >= 1 && priority <= maxPrio)
		? double(maxPrio + 1 - priority) / double(maxPrio) : 1.0;

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
// Chapter 4: the weights of the link cost depend on the class of the packet
// (type of frame and level). The path cost stored at discovery was priced
// on the control messages that built the path; at forwarding time the node
// prices its own first link again with the weights of the packet in hand and
// keeps the downstream part as discovered (each node forwards hop by hop, so
// every hop of the path is priced for the class at its own turn). Until the
// MAC has averaged the neighbour, the discovered cost stands.
// The next hop's queue: from its last HELLO (hello_queue_ = 1, what a real
// node can know), or read directly in memory (hello_queue_ = 0, the former
// simulation shortcut). An unheard neighbour counts as an empty queue.
void AOMDV_QPV::nextHopQueue(Packet *p, priqueueEvalVSN *own_ifq, nsaddr_t nh, bool &congested, int &qlen, int &qlim, int &threshold)
{
	if (!hello_queue_)
	{
		priqueueEvalVSN *nh_ifq = get_ifq_for_nexthop(nh);
		congested = nh_ifq ? nh_ifq->congestedIFQ_ : false;
		qlen = nh_ifq ? nh_ifq->getQueueLength() : 0;
		qlim = nh_ifq ? nh_ifq->getQlim() : 0;
		threshold = nh_ifq ? nh_ifq->computeCongestionThreshold(p) : 0;
		return;
	}
	map<nsaddr_t, NbQueueState>::iterator nq = nb_queue_.find(nh);
	congested = (nq != nb_queue_.end()) ? nq->second.congested : false;
	qlen = (nq != nb_queue_.end()) ? nq->second.qlen : 0;
	qlim = (nq != nb_queue_.end()) ? nq->second.qlim : 0;
	threshold = own_ifq ? own_ifq->computeCongestionThresholdFor(p, qlen, qlim) : 0;
}

double AOMDV_QPV::classPathCost(Packet *p, AOMDV_QPV_Path *path)
{
	if (!mac_ || mac_->get_average_lqi(path->nexthop) <= 0.0)
		return path->path_cost;
	LinkQuality q = getLinkQuality(p, path->nexthop);
	double c = linkCost(p, q);
	if (path->downstream_cost < 0.0)
		return c + AOMDV_QPV_PATH_HOP_TAX;
	return max(c, path->downstream_cost) + AOMDV_QPV_PATH_HOP_TAX;
}

double AOMDV_QPV::linkCost(Packet *p, LinkQuality &q)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	double normLQI = clampMetric(q.LQI / 255.0, 0.0, 1.0);
	// The SNR is graded on the radio's own window, as everywhere else in
	// the chain: the 10 dB decode floor the radio states (SINRThresh_)
	// up to floor+30 dB, the same 30 dB
	// span the LQI mapping and the PHY score use. The former /50 was a
	// span that kept a 23 dB link (283 m, past the two-ray
	// crossover) within 0.10 of a 28 dB one (200 m) - not enough to
	// outweigh a two-hop saving, although the weaker link loses the
	// capture contests that decide everything under pipeline collisions.
	double normSNR = clampMetric((q.SNR - 10.0) / 30.0, 0.0, 1.0);
	double normPRR = clampMetric(q.PRR, 0.0, 1.0);

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

	// Cold-start fallback: the store answers 0.0 for a neighbour it has
	// not averaged yet, which priced every unknown link identically and
	// left the first route choice to hop count alone - the 283 m diagonal
	// chain then always won the discovery race over the 200 m rectilinear
	// one, and the reference frame paid for its capture losses (measured:
	// 127 of 161 first-hop retries on the diagonal neighbour). The packet
	// in hand crossed the very link being priced, and the PHY wrote its
	// measures into the common header on reception - the freshest truth
	// available, so use it until the average exists.
	if (q.LQI <= 0.0)
		q.LQI = ch->linkQualityIndicator_;
	if (q.SNR <= 0.0)
		q.SNR = ch->signalToNoiseRatio_;

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
 * This function increments the count of packets sent to a specific neighbor, in total and since that
 * neighbor's last HELLO report (the window recvHello() turns into a delivery ratio), and, if debugging
 * is enabled, logs detailed information about the packet being sent. The log includes the neighbor address, the total
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
	if ((u_int32_t)neighbor != IP_BROADCAST)
		sent_win_[neighbor]++;

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
 * @brief Computes the Packet Reception Ratio (PRR) for a given neighbor.
 *
 * This function returns the delivery ratio of the link towards a neighbor: the packets the
 * neighbor reported in its HELLO as received from this node, over the packets this node sent
 * it in that window, smoothed by recvHello(). The value is clamped between 0.0 and 1.0, and is
 * 1.0 until the first report.
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
	// 1 until the neighbour's first report on packets this node sent it.
	map<nsaddr_t, double>::iterator it = prr_.find(neighbor);
	double prr = (it == prr_.end()) ? 1.0 : it->second;

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
			sprintf(msg, "using: neighbor=%d prr=%.2f for VIDEO packetType=%s, frameType='%c', priority=%d",
					neighbor, prr, packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "using: neighbor=%d prr=%.2f for NON-VIDEO packetType=%s",
					neighbor, prr, packet_info.name(packetType));
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
					"AOMDV_QPV (cross-layer): interface-queue congestion detected, total=%d, video=%d, non_video=%d, video_M=%d, video_D=%d",
					stats.total, stats.video, stats.non_video, stats.video_M, stats.video_D);
			logtarget->pt_->dump();

			// Per-priority log
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

// Registers the interface queue bound to a next hop
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

// Returns the interface-queue pointer for a next hop
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

// ---------------------------------------------------------------------------
// The learning agents that replace the chapter-4 rules (d22, 10 Oct 2026).
// ---------------------------------------------------------------------------

void AOMDV_QPVOutcome::frameOutcome(nsaddr_t da, int uid, bool ok, double delay, int cls)
{
	agent->rlOutcome(da, uid, ok, delay, cls);
}

void AOMDV_QPV::rlRegisterOutcome()
{
	if (!outcome_registered_ && mac_ && (rl_path_ || rl_congestion_ || rl_break_ || discard_rule_ || rl_discard_))
	{
		mac_->setOutcomeListener(&outcome_);
		outcome_registered_ = true;
	}
}

int AOMDV_QPV::rlClass(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	if (ch->ptype() != PT_VIDEO || ch->frameType_[0] == 'M' || ch->prioNb_ <= 1)
		return 0;
	int maxPrio = mac_ ? mac_->getMaxPrioNb() : 13;
	return (ch->prioNb_ <= maxPrio / 2) ? 1 : 2;
}

int AOMDV_QPV::rlpProfile(int cls, bool explore)
{
	int a = 0;
	for (int k = 1; k < 5; k++)
		if (rlp_q_[cls][k] > rlp_q_[cls][a])
			a = k;
	if (explore)
		a = (int)(Random::uniform() * 5) % 5;
	return a;
}

// The link cost of a discovery: the rule's weights, or the critical class's
// greedy profile when the path agent is on.
double AOMDV_QPV::rlDiscoveryCost(Packet *p, LinkQuality &q)
{
	if (!rl_path_)
		return linkCost(p, q);
	int saved = rlp_cur_;
	rlp_cur_ = rlpProfile(0, false);
	double c = linkCost(p, q);
	rlp_cur_ = saved;
	return c;
}

int AOMDV_QPV::rlcTheta(int cls, bool explore)
{
	int a = 0;
	for (int k = 1; k < 4; k++)
		if (rlc_q_[cls][k] > rlc_q_[cls][a])
			a = k;
	if (explore)
		a = (int)(Random::uniform() * 4) % 4;
	return a;
}

// The path for a packet, every stack. Each live path of the reserve (not
// expired) is priced and judged congested or not; a congested path is
// skipped while an uncongested one exists, and the cheapest remaining path
// wins, the first on a tie. The chapter-4 rules decide by default: the
// class-priced cost plus the occupancy term, and the next hop's announced
// congestion flag and class threshold. The path agent (RL_PATH_) replaces the cost's weights by a
// learnt profile; the congestion agent (RL_CONGESTION_) replaces the threshold and
// the occupancy term by a learnt occupancy threshold. Neither draws a random
// number when it is off.
AOMDV_QPV_Path *AOMDV_QPV::selectPath(aomdv_qpv_rt_entry *rt, Packet *p, int &theta, double &occChosen, int &prof,
				      double &costChosen)
{
	static const double TH[4] = {0.25, 0.5, 0.75, 1.01};
	int cls = rlClass(p);
	bool notCritical = cls > 0;
	theta = rl_congestion_ ? rlcTheta(cls, notCritical && Random::uniform() < rl_epsilon_) : -1;
	prof = rl_path_ ? rlpProfile(cls, notCritical && Random::uniform() < rl_epsilon_) : -1;
	rlp_cur_ = prof;
	priqueueEvalVSN *own_ifq = get_ifq_for_nexthop(index);
	std::vector<AOMDV_QPV_Path *> cand;
	std::vector<double> occ, cost;
	std::vector<bool> cong;
	int free = 0;
	for (AOMDV_QPV_Path *path = rt->rt_path_list.lh_first; path; path = path->path_link.le_next)
	{
		if (path->expire < CURRENT_TIME)
			continue;
		bool congestedIFQ;
		int qlen, qlim, thr;
		nextHopQueue(p, own_ifq, path->nexthop, congestedIFQ, qlen, qlim, thr);
		double o = (qlim > 0) ? double(qlen) / double(qlim) : 0.0;
		bool c = rl_congestion_ ? (o > TH[theta]) : (congestedIFQ || qlen > thr);
		double k = class_cost_ ? classPathCost(p, path) : path->path_cost;
		if (!rl_congestion_ && qlim > 0 && qlen > 0)
			k += thr * o;
		cand.push_back(path);
		occ.push_back(o);
		cost.push_back(k);
		cong.push_back(c);
		if (!c)
			free++;
	}
	int bi = -1;
	for (int i = 0; i < (int)cand.size(); i++)
	{
		if (free > 0 && cong[i])
			continue;
		if (bi < 0)
		{
			bi = i;
			continue;
		}
		if (cost[i] < cost[bi])
			bi = i;
	}
	rlp_cur_ = -1;
	if (bi < 0)
		return NULL;
	occChosen = occ[bi];
	costChosen = cost[bi];
	return cand[bi];
}

void AOMDV_QPV::rlOutcome(nsaddr_t da, int uid, bool ok, double delay, int cls)
{
	if (discard_rule_ || rl_discard_)
		discard_fail_ = 0.9 * discard_fail_ + 0.1 * (ok ? 0.0 : 1.0);
	if (rl_discard_)
	{
		map<int, double>::iterator v = discard_val_.find(uid);
		if (v != discard_val_.end())
		{
			if (ok)
				rle_ack_ += v->second;
			else
				rle_fail_ += v->second;
			discard_val_.erase(v);
		}
	}
	double r = ok ? 1.0 - 0.5 * (delay / 0.1 < 1.0 ? delay / 0.1 : 1.0) : -1.0;
	map<int, RlCtx>::iterator x = rl_ctx_.find(uid);
	if (rl_path_ && x != rl_ctx_.end() && x->second.prof >= 0)
	{
		double &q = rlp_q_[x->second.cls][x->second.prof];
		q += rl_alpha_ * (r - q);
	}
	if (rl_congestion_ && x != rl_ctx_.end() && x->second.theta >= 0)
	{
		double &q = rlc_q_[x->second.cls][x->second.theta];
		q += rl_alpha_ * ((r - x->second.occ) - q);
	}
	if (x != rl_ctx_.end())
		rl_ctx_.erase(x);
	if (rl_break_ && ok)
	{
		map<nsaddr_t, int>::iterator s = fail_streak_.find(da);
		if (s != fail_streak_.end() && s->second > 0)
		{
			rlwAlive(da, s->second);
			fail_streak_.erase(s);
			fail_mark_.erase(da);
			rlb_run_k_.erase(da);
			rlb_run_s_.erase(da);
		}
	}
}

// The node's own interface queue, under or over half full.
int AOMDV_QPV::rlwState()
{
	priqueueEvalVSN *own = get_ifq_for_nexthop(index);
	if (!own || own->getQlim() <= 0)
		return 0;
	return (2 * own->getQueueLength() >= own->getQlim()) ? 1 : 0;
}

int AOMDV_QPV::rlwPick(int st)
{
	int a = 0;
	for (int k = 1; k < 4; k++)
		if (rlb_q_[st][k] > rlb_q_[st][a])
			a = k;
	if (Random::uniform() < rl_epsilon_)
		a = (int)(Random::uniform() * 4) % 4;
	return a;
}

void AOMDV_QPV::rlwScore(int st, int kidx, double target)
{
	rlb_q_[st][kidx] += rl_alpha_ * (target - rlb_q_[st][kidx]);
}

// Declarations older than 15 s whose neighbour sent no frame at all: it was
// dead, and every count would have declared it, the smaller the sooner.
void AOMDV_QPV::rlwVerdicts()
{
	static const int RLW_K[4] = {1, 2, 3, 5};
	for (map<nsaddr_t, RlwDecl>::iterator d = rlb_decl_.begin(); d != rlb_decl_.end();)
	{
		if (CURRENT_TIME - d->second.t > 15.0)
		{
			for (int k = 0; k < 4; k++)
				rlwScore(d->second.st, k, 1.0 - 0.2 * (RLW_K[k] - 1));
			rlb_decl_.erase(d++);
		}
		else
			++d;
	}
}

// A run of failures ended by an acknowledgement: the neighbour was alive.
// A count up to the run's length would have declared it dead wrongly.
void AOMDV_QPV::rlwAlive(nsaddr_t nb, int streak)
{
	static const int RLW_K[4] = {1, 2, 3, 5};
	map<nsaddr_t, int>::iterator s = rlb_run_s_.find(nb);
	int st = (s != rlb_run_s_.end()) ? s->second : rlwState();
	for (int k = 0; k < 4; k++)
		rlwScore(st, k, RLW_K[k] <= streak ? -2.0 : 0.5);
}

// Early discard by priority (d27): admit or discard a packet entering the interface
// queue.
bool AOMDV_QPV::discardAdmit(Packet *p)
{
	if (!discard_rule_ && !rl_discard_)
		return true;
	struct hdr_cmn *ch = HDR_CMN(p);
	if (ch->ptype() != PT_VIDEO || ch->prioNb_ < 1)
		return true;
	rlRegisterOutcome();
	int maxPrio = mac_ ? mac_->getMaxPrioNb() : 13;
	int lvl = ch->prioNb_ > maxPrio ? maxPrio : ch->prioNb_;
	bool isD = ch->frameType_[0] == 'D';
	double rank = double((isD ? maxPrio : 0) + lvl - 1) / (2.0 * maxPrio);
	priqueueEvalVSN *own = get_ifq_for_nexthop(index);
	double q = (own && own->getQlim() > 0) ? double(own->getQueueLength()) / double(own->getQlim()) : 0.0;
	double c = q > discard_fail_ ? q : discard_fail_;
	double c0 = discardThreshold();
	// The cut is complete 0.4 above the threshold; the three lowest levels
	// of the main frames always pass (the first version cut down to c = 1
	// and left every M level in: it moved the losses by two points, 10 Oct
	// 2026, while the main frames are three packets in four).
	double floorRank = 3.0 / (2.0 * maxPrio);
	bool admit = (c <= c0) || (rank < floorRank) || (rank < 1.0 - (c - c0) / 0.4);
	if (rl_discard_)
	{
		// The agent's value of a packet falls by 0.7 per rank step: M1 1,
		// M2 0.7, M3 0.49, M13 0.014, the D levels next to nothing. The
		// first version took 1 - rank, nearly flat, and under the heaviest
		// load it admitted more at the expense of M1, which decides whether
		// a frame decodes (-1.2 dB against the rule, 10 Oct 2026).
		double value = pow(0.7, double((isD ? maxPrio : 0) + lvl - 1));
		rle_n_++;
		if (admit)
			discard_val_[ch->uid()] = value;
		else
			rle_disc_ += value;
	}
	return admit;
}

double AOMDV_QPV::discardThreshold()
{
	static const double C0[5] = {0.1, 0.2, 0.3, 0.5, 2.0}; // 2.0: never discards
	return rl_discard_ ? C0[rle_a_] : 0.2;
}

// One decision of the early-discard agent per HELLO interval.
void AOMDV_QPV::rlsEpoch()
{
	if (rle_n_ > 0)
	{
		double r = (rle_ack_ - rle_fail_ - 0.5 * rle_disc_) / rle_n_;
		rle_q_[rle_a_] += rl_alpha_ * (r - rle_q_[rle_a_]);
	}
	rle_ack_ = rle_fail_ = rle_disc_ = 0.0;
	rle_n_ = 0;
	discard_val_.clear();
	int a = 0;
	for (int k = 1; k < 5; k++)
		if (rle_q_[k] > rle_q_[a])
			a = k;
	if (Random::uniform() < rl_epsilon_)
		a = (int)(Random::uniform() * 5) % 5;
	rle_a_ = a;
}
