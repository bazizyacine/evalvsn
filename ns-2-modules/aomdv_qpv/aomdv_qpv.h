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

#ifndef __aomdv_qpv_h__
#define __aomdv_qpv_h__

// #include <agent.h>
// #include <packet.h>
// #include <sys/types.h>
// #include <cmu/list.h>
// #include <scheduler.h>

#include <cmu-trace.h>
#include <priqueue.h>
#include <aomdv_qpv/aomdv_qpv_rtable.h>
#include <aomdv_qpv/aomdv_qpv_rqueue.h>
// AODV ns-2.31 code
#include <classifier/classifier-port.h>
#include <map>
#include <vector>
#include <mac/link-metrics.h>

// The options of the vendor's AOMDV that the stack always runs with, compiled
// in since 14 October 2026 rather than left behind macros: packet salvaging
// (up to AOMDV_QPV_MAX_SALVAGE_COUNT), the expanding ring search, node-disjoint
// paths, local repair, and link-layer failure detection, a MAC failure being
// handled by rt_ll_failed().
#define AOMDV_QPV_MAX_SALVAGE_COUNT 10
#define AOMDV_QPV_DEAD_AFTER_FAILURES 3 // consecutive MAC failures that declare a neighbour dead
// Per-hop tax of the bottleneck path metric (recvRequest/recvReply): the
// path cost is the worst per-hop link cost plus this tax per hop, so two
// paths with equal bottlenecks still prefer the shorter one. 0.02 keeps a
// two-hop saving (0.04) well under one capture-margin step of the link
// cost (a 5 dB weaker link moves the SNR term by ~0.15 on the radio's
// 30 dB window).
#define AOMDV_QPV_PATH_HOP_TAX 0.02

class AOMDV_QPV;
class priqueueEvalVSN;

#define MY_ROUTE_TIMEOUT 10     // 100 seconds
// The route lifetime follows the traffic cadence, not a compile-time guess:
// with captures every 15 s and a 10 s lifetime, every frame found its route
// expired and repaid the discovery backoff. Bound as ACTIVE_ROUTE_TIMEOUT_
// (default 10.0, the historical value);
// radio-params.tcl scales it to the capture calendar.
#define REV_ROUTE_LIFE 6        // 5  seconds
// AODV ns-2.31 code
#define BCAST_ID_SAVE 6 // 3 seconds

// No. of times to do network-wide search before timing out for
// MAX_RREQ_TIMEOUT sec.
#define RREQ_RETRIES 3
// timeout after doing network-wide search RREQ_RETRIES times
// The cap on the network-wide RREQ backoff left its #define too: at 1.0 s a
// hundred nodes rebroadcast a source's rediscovery every second - the storm
// the 802.15.4 audit measured at ~63,000 collision drops. Bound as
// MAX_RREQ_TIMEOUT_ (default 1.0);
// radio-params.tcl sets RFC 3561's 10 s where the radio needs it.

/* Various constants used for the expanding ring search */
// AOMDV_QPV code
#define TTL_START 5     // 5
#define TTL_INCREMENT 2 // 2
#define TTL_THRESHOLD 7

// Should be set by the user using best guess (conservative)
#define NETWORK_DIAMETER 30 // 30 hops

// Must be larger than the time difference between a node propagates a route
// request and gets the route reply back.

// #define RREP_WAIT_TIME     (3 * NODE_TRAVERSAL_TIME * NETWORK_DIAMETER) // ms
// #define RREP_WAIT_TIME     (2 * REV_ROUTE_LIFE)  // seconds
#define RREP_WAIT_TIME 1.0 // sec

// #define INFINITY        0xff

// The followings are used for the forward() function. Controls pacing.
#define AOMDV_QPV_DELAY 1.0     // random delay
#define NO_AOMDV_QPV_DELAY -1.0 // no delay

// think it should be 30 ms
#define ARP_DELAY 0.01 // fixed delay to keep arp happy

// The hello interval left its #define: on a 250 kbit/s radio the per-second
// hellos of a hundred nodes are a control-plane collision storm before any
// video is sent, and real sensor stacks beacon in tens of seconds. Bound as
// HELLO_INTERVAL_ (default 1.0, the classic); radio-params.tcl scales it
// to the radio it declares.
#define ALLOWED_HELLO_LOSS 3 // packets

// This should be somewhat related to arp timeout
#define NODE_TRAVERSAL_TIME 0.03    // 30 ms

struct LinkQuality
{
        double LQI;
        double SNR;
        double PRR;
        LinkQuality() : LQI(0.0), SNR(0.0), PRR(1.0) {}
};

struct DynamicWeights
{
        double wLQI;
        double wSNR;
        double wPRR;
};

/*
  Timers (Broadcast ID, Hello, Neighbor Cache, Route Cache)
*/
class AOMDV_QPVBroadcastTimer : public Handler
{
public:
        AOMDV_QPVBroadcastTimer(AOMDV_QPV *a) : agent(a) {}
        void handle(Event *);

private:
        AOMDV_QPV *agent;
        Event intr;
};

class AOMDV_QPVHelloTimer : public Handler
{
public:
        AOMDV_QPVHelloTimer(AOMDV_QPV *a) : agent(a) {}
        void handle(Event *);

private:
        AOMDV_QPV *agent;
        Event intr;
};

class AOMDV_QPVNeighborTimer : public Handler
{
public:
        AOMDV_QPVNeighborTimer(AOMDV_QPV *a) : agent(a) {}
        void handle(Event *);

private:
        AOMDV_QPV *agent;
        Event intr;
};

class AOMDV_QPVRouteCacheTimer : public Handler
{
public:
        AOMDV_QPVRouteCacheTimer(AOMDV_QPV *a) : agent(a) {}
        void handle(Event *);

private:
        AOMDV_QPV *agent;
        Event intr;
};


// Hands the MAC's frame outcomes to the agent's learning agents (d22).
class AOMDV_QPVOutcome : public FrameOutcomeListener
{
public:
        AOMDV_QPVOutcome(AOMDV_QPV *a) : agent(a) {}
        void frameOutcome(nsaddr_t da, int uid, bool ok, double delay, int cls);

private:
        AOMDV_QPV *agent;
};

// AOMDV_QPV code
/*
  Route List
*/
class AOMDV_QPV_Route
{
        friend class AOMDV_QPVBroadcastID;

public:
        AOMDV_QPV_Route(nsaddr_t nexthop, nsaddr_t lasthop = 0)
        {
                nh_addr = nexthop;
                lh_addr = lasthop;
        }

protected:
        LIST_ENTRY(AOMDV_QPV_Route)
        route_link;
        nsaddr_t nh_addr;
        nsaddr_t lh_addr;
};

LIST_HEAD(aomdv_qpv_routes, AOMDV_QPV_Route);

/*
  Broadcast ID Cache
*/
class AOMDV_QPVBroadcastID
{
        friend class AOMDV_QPV;

public:
        AOMDV_QPVBroadcastID(nsaddr_t i, u_int32_t b)
        {
                src = i;
                id = b;
                // AOMDV_QPV code
                count = 0;
                LIST_INIT(&reverse_path_list);
                LIST_INIT(&forward_path_list);
        }

protected:
        LIST_ENTRY(AOMDV_QPVBroadcastID)
        link;
        nsaddr_t src;
        u_int32_t id;
        double expire; // now + BCAST_ID_SAVE s

        // AOMDV_QPV code
        int count;
        aomdv_qpv_routes reverse_path_list; // List of reverse paths used for forwarding replies
        aomdv_qpv_routes forward_path_list; // List of forward paths advertised already

        inline AOMDV_QPV_Route *reverse_path_insert(nsaddr_t nexthop, nsaddr_t lasthop = 0)
        {
                AOMDV_QPV_Route *route = new AOMDV_QPV_Route(nexthop, lasthop);
                assert(route);
                LIST_INSERT_HEAD(&reverse_path_list, route, route_link);
                return route;
        }

        inline AOMDV_QPV_Route *reverse_path_lookup(nsaddr_t nexthop, nsaddr_t lasthop = 0)
        {
                AOMDV_QPV_Route *route = reverse_path_list.lh_first;

                // Search the list for a match of id
                for (; route; route = route->route_link.le_next)
                {
                        if ((route->nh_addr == nexthop) && (route->lh_addr == lasthop))
                                return route;
                }
                return NULL;
        }

        inline AOMDV_QPV_Route *forward_path_insert(nsaddr_t nexthop, nsaddr_t lasthop = 0)
        {
                AOMDV_QPV_Route *route = new AOMDV_QPV_Route(nexthop, lasthop);
                assert(route);
                LIST_INSERT_HEAD(&forward_path_list, route, route_link);
                return route;
        }

        inline AOMDV_QPV_Route *forward_path_lookup(nsaddr_t nexthop, nsaddr_t lasthop = 0)
        {
                AOMDV_QPV_Route *route = forward_path_list.lh_first;

                // Search the list for a match of id
                for (; route; route = route->route_link.le_next)
                {
                        if ((route->nh_addr == nexthop) && (route->lh_addr == lasthop))
                                return route;
                }
                return NULL;
        }
};

LIST_HEAD(aomdv_qpv_bcache, AOMDV_QPVBroadcastID);

/*
  The Routing Agent
*/
class AOMDV_QPV : public Agent
{
        /*
         * make some friends first
         */
        friend class aomdv_qpv_rt_entry;
        friend class AOMDV_QPVBroadcastTimer;
        friend class AOMDV_QPVHelloTimer;
        friend class AOMDV_QPVNeighborTimer;
        friend class AOMDV_QPVRouteCacheTimer;
        friend class priqueueEvalVSN;
        friend class aomdv_qpv_rqueue;

public:
        AOMDV_QPV(nsaddr_t id);

        void recv(Packet *p, Handler *);

        void setMac(LinkMetricsStore *mac) { mac_ = mac; }
        // The agent needs a link-metrics store, not a particular MAC: the
        // IEEE 802.15.4 derivation is the one store the thesis runs.
        LinkMetricsStore *getMac() const { return mac_; }

        void setAOMDV_QPVifqueue(PriQueue *ifq) { AOMDV_QPVifqueue = ifq; }
        PriQueue *getAOMDV_QPVifqueue() const { return AOMDV_QPVifqueue; }

protected:
        int command(int, const char *const *);
        int initialized() { return 1 && target_; }

        /*
         * Route Table Management
         */
        void rt_resolve(Packet *p);
        void rt_down(aomdv_qpv_rt_entry *rt);

public:
        void rt_ll_failed(Packet *p);
        // AOMDV_QPV code
        // void            rt_update(aodv_rt_entry *rt, u_int32_t seqnum, u_int16_t metric, nsaddr_t nexthop, double expire_time);
        // void            handle_link_failure(nsaddr_t id);
        void handle_link_failure(nsaddr_t id);

        void log(char *event, char *additional);

        DynamicWeights getDynamicWeights(packet_t packetType, char frameType, int priority);
        double linkCost(Packet *p, LinkQuality &q);

        LinkQuality getLinkQuality(Packet *p, nsaddr_t neighbor);
        void updatePRR_onSend(Packet *p, nsaddr_t neighbor);
        double computePRR(packet_t packetType, char frameType, int priority, nsaddr_t neighbor);

        bool notifyCongestionIFQ(priqueueEvalVSN *ifq);

        void register_ifq_for_nexthop(nsaddr_t nexthop, priqueueEvalVSN *ifq);
        // Neighbour queues as their HELLOs advertised them.
        struct NbQueueState
        {
                int qlen;
                int qlim;
                bool congested;
                double t;
        };
        map<nsaddr_t, NbQueueState> nb_queue_;
        // Unicast frames received from each neighbour since the last HELLO
        // this node sent: what the next HELLO reports back to that
        // neighbour for its transmit power control.
        struct RxFeedback
        {
                double rssi_sum; // dBm
                double sinr_sum; // dB
                int n;
                RxFeedback() : rssi_sum(0.0), sinr_sum(0.0), n(0) {}
        };
        map<nsaddr_t, RxFeedback> rx_fb_;
        // The three mechanisms aligned on chapter 4, switchable from Tcl
        // for the bench (1 = on, the default; 0 = the former behaviour).
        // Consecutive MAC failures towards each neighbour: the radio's count
        // of packets sent to it at the last failure, and the run length.
        map<nsaddr_t, int> fail_mark_;
        map<nsaddr_t, int> fail_streak_;
        int class_cost_;    // first link priced for the class of the packet, else the discovered cost
        int hello_queue_;   // next hop's queue learnt from its HELLOs, else read in memory
        double rl_alpha_;   // learning rate of the routing agents
        double rl_epsilon_; // their exploration rate, non-critical packets only

        /*
         * The learning agents that replace the chapter-4 rules (decision
         * d22, 10 Oct 2026), each behind its own switch, all off by
         * default. They learn from the outcome of every unicast frame,
         * which the derived MAC hands over (FrameOutcomeListener): reward
         * 1 - 0.5 min(1, delay / 0.1 s) if acknowledged, -1 if not, the
         * learning MAC's form without its class weights. Tabular, one step
         * per decision, Q <- Q + alpha (r - Q), alpha = RL_ALPHA_;
         * exploration with probability RL_EPSILON_, for packets that are
         * not critical where a packet is concerned.
         *  - Path (RL_PATH_): replaces the hand-tuned weights of the link
         *    cost. The cost keeps its three measures (LQI, SNR, delivery
         *    ratio reported by the neighbour); the agent picks, per class
         *    (critical, normal, low), one of five weight profiles: the
         *    rule's three - control (0.1, 0.2, 0.7), M frames (0.2, 0.3,
         *    0.5), D frames (0.25, 0.35, 0.4) - and two leaning on LQI (0.6,
         *    0.2, 0.2) or on SNR (0.2, 0.6, 0.2). The discovery prices its
         *    links with the critical class's greedy profile. (A first
         *    version that dropped the cost and counted hops only lost 3.7
         *    points of delivery at the severe level, 10 Oct 2026.)
         *  - Congestion (RL_CONGESTION_): replaces the fixed bypass threshold. Per
         *    class, a value for each threshold in {0.25, 0.5, 0.75, never}
         *    of the next hop's announced queue; a path past the chosen
         *    threshold is set aside while another is not. Reward: the
         *    frame's, minus the occupancy of the queue it was sent to.
         *  - Link-break detection (RL_BREAK_): replaces the three consecutive
         *    failures that declare a neighbour dead. A value for each count
         *    in {1, 2, 3, 5} and for each state of the node's own interface
         *    queue (under or over half full: failures under load are
         *    mostly contention), picked when a run of failures starts. A
         *    declared neighbour from which any frame arrives within 15 s
         *    was alive: every count up to the one used scores -2, a route
         *    broken and a discovery for nothing. Silent for 15 s, it was
         *    dead: count K scores 1 - 0.2 (K - 1), the sooner the better.
         *    A run ended by an acknowledgement after s failures scores -2
         *    for the counts up to s and +0.5 for the others. Every count is
         *    scored on every verdict it can be judged on. (A first version
         *    without the queue state, with -1 for a false alarm and HELLOs
         *    alone as proof of life lost 8.4 points of delivery at the
         *    severe level, 10 Oct 2026; the second, HELLOs still alone, 9.1.)
         *  - Discovery (RL_DISCOVERY_): replaces the fixed 50 ms rebroadcast
         *    jitter. The source picks 10, 25 or 50 ms for each flood (first
         *    of a round, or retry) and writes it in the RREQ's second
         *    reserved byte; the relays apply it. Reward: 1 - min(1, l / 1 s)
         *    for a reply after l seconds, -1 for a flood followed by
         *    another of the same round.
         * The transmit power agent lives in the MAC (RL_POWER_), the
         * learning MAC too (RL_ACCESS_).
         */
        int rl_path_;
        int rl_congestion_;
        int rl_break_;
        int rl_discovery_;
        AOMDV_QPVOutcome outcome_;
        bool outcome_registered_;
        void rlRegisterOutcome();
        void rlOutcome(nsaddr_t da, int uid, bool ok, double delay, int cls);
        int rlClass(Packet *p); // 0 critical, 1 normal, 2 low, as the learning MAC
        double rlp_q_[3][5];             // path: [class][weight profile]
        int rlp_cur_;                    // the profile the link cost uses now, -1 for the rule's weights
        int rlpProfile(int cls, bool explore);
        double rlDiscoveryCost(Packet *p, LinkQuality &q);
        double rlc_q_[3][4];             // congestion: [class][threshold]
        struct RlCtx
        {
                int cls;
                int theta;
                int prof;
                double occ;
                double t;
        };
        map<int, RlCtx> rl_ctx_;         // packet uid -> its forwarding decision
        int rlcTheta(int cls, bool explore);
        AOMDV_QPV_Path *selectPath(aomdv_qpv_rt_entry *rt, Packet *p, int &theta, double &occ, int &prof, double &cost);
        double rlb_q_[2][4];             // link-break detection: [own queue under/over half][count]
        map<nsaddr_t, int> rlb_run_k_;   // the count picked for each neighbour's current run
        map<nsaddr_t, int> rlb_run_s_;   // and the queue state it was picked in
        struct RlwDecl
        {
                double t;
                int kidx;
                int st;
        };
        map<nsaddr_t, RlwDecl> rlb_decl_; // neighbours declared dead, awaiting the verdict
        int rlwState();
        int rlwPick(int st);
        void rlwVerdicts();
        void rlwAlive(nsaddr_t nb, int streak);
        void rlwScore(int st, int kidx, double target);
        double rld_q_[2][3];             // discovery: [first flood, retry][10, 25, 50 ms]
        struct RldPending
        {
                int a;
                int bucket;
                double t;
        };
        map<nsaddr_t, RldPending> rld_pending_; // destination -> the flood in flight

        /*
         * Early discard by priority (decision d27, 10 Oct 2026). Under congestion
         * the video packets of least value are discarded on arrival in the
         * interface queue - D frames before M frames, the highest levels
         * first - so that the channel carries the main frames' low levels.
         * Losses there are channel losses that strike every level alike
         * (collisions, abandoned retries), and the queue's own replacement
         * policy only acts when it is full, which it seldom is. Rank of a
         * video packet: M level p -> (p - 1) / 2P, D level p -> (P + p - 1)
         * / 2P, P the number of levels, so ranks lie in [0, 1); its value is
         * 1 - rank. Congestion index c: the larger of the node's interface
         * queue occupancy and the moving average (weight 0.1) of its MAC
         * failures. Above a threshold c0 the admitted ranks shrink linearly
         * to the floor at c = c0 + 0.4: admit when rank < 1 - (c - c0) / 0.4;
         * the three lowest levels of the M frames always pass. Non-video
         * traffic is always admitted.
         *  - Rule (DISCARD_RULE_): c0 = 0.2.
         *  - Agent (RL_DISCARD_): c0 learnt among {0.1, 0.2, 0.3, 0.5, never},
         *    one choice per HELLO interval; the interval's reward is the
         *    value of the admitted packets acknowledged, minus that of
         *    those that failed, minus half the value discarded, over the
         *    video packets seen. The agent's value of a packet falls by 0.7
         *    per rank step (M1 1, M2 0.7, M3 0.49, the D levels next to
         *    nothing): M1 decides whether a frame decodes.
         * With the early discard on, a failed D packet is given up at once and a
         * failed M1-M3 packet goes back to the same neighbour up to twice
         * more when no other path exists (rt_ll_failed).
         */
        int discard_rule_;
        int rl_discard_;
        double discard_fail_;
        double rle_q_[5];
        int rle_a_;
        double rle_ack_, rle_fail_, rle_disc_;
        int rle_n_;
        map<int, double> discard_val_; // packet uid -> value of an admitted video packet (agent)
        bool discardAdmit(Packet *p);
        double discardThreshold();
        void rlsEpoch();
        // Path cost recomputed for the class of the packet in hand: the
        // first link priced with its own weights, the downstream part kept.
        double classPathCost(Packet *p, AOMDV_QPV_Path *path);
        void nextHopQueue(Packet *p, priqueueEvalVSN *own_ifq, nsaddr_t nh, bool &congested, int &qlen, int &qlim, int &threshold);
        priqueueEvalVSN *get_ifq_for_nexthop(nsaddr_t nexthop);

protected:
        void rt_purge(void);

        void enque(aomdv_qpv_rt_entry *rt, Packet *p);
        Packet *deque(aomdv_qpv_rt_entry *rt);

        /*
         * Neighbor Management
         */
        void nb_insert(nsaddr_t id);
        AOMDV_QPV_Neighbor *nb_lookup(nsaddr_t id);
        void nb_delete(nsaddr_t id);
        void nb_purge(void);

        /*
         * Broadcast ID Management
         */

        // AODV ns-2.31 code
        void id_insert(nsaddr_t id, u_int32_t bid);
        bool id_lookup(nsaddr_t id, u_int32_t bid);
        AOMDV_QPVBroadcastID *id_get(nsaddr_t id, u_int32_t bid);
        void id_purge(void);

        /*
         * Packet TX Routines
         */
        void forward(aomdv_qpv_rt_entry *rt, Packet *p, double delay);
        // AOMDV_QPV code - should it be removed?
        void forwardReply(aomdv_qpv_rt_entry *rt, Packet *p, double delay);
        void sendHello(void);
        void sendRequest(nsaddr_t dst);

        // AOMDV_QPV code
        // void            sendReply(nsaddr_t ipdst, u_int32_t hop_count, nsaddr_t rpdst, u_int32_t rpseq, u_int32_t lifetime, double timestamp);
        void sendReply(nsaddr_t ipdst, u_int32_t hop_count,
                       nsaddr_t rpdst, u_int32_t rpseq,
                       double lifetime, double timestamp,
                       nsaddr_t nexthop, u_int32_t bcast_id, nsaddr_t rp_first_hop);
        void sendError(Packet *p, bool jitter = true);

        /*
         * Packet RX Routines
         */
        void recvAOMDV_QPV(Packet *p);
        void recvHello(Packet *p);
        void recvRequest(Packet *p);
        void recvReply(Packet *p);
        void recvError(Packet *p);

        /*
         * History management
         */

        double PerHopTime(aomdv_qpv_rt_entry *rt);

        nsaddr_t index;  // IP Address of this node
        u_int32_t seqno; // Sequence Number
        int bid;         // Broadcast ID

        aomdv_qpv_rtable rthead; // routing table
        aomdv_qpv_ncache nbhead; // Neighbor Cache
        aomdv_qpv_bcache bihead; // Broadcast ID Cache

        /*
         * Timers
         */
        AOMDV_QPVBroadcastTimer btimer;
        AOMDV_QPVHelloTimer htimer;
        AOMDV_QPVNeighborTimer ntimer;
        AOMDV_QPVRouteCacheTimer rtimer;

        /*
         * Routing Table
         */
        aomdv_qpv_rtable rtable;
        /*
         *  A "drop-front" queue used by the routing layer to buffer
         *  packets to which it does not have a route.
         */
        aomdv_qpv_rqueue rqueue;

        /*
         * A mechanism for logging the contents of the routing
         * table.
         */
        Trace *logtarget;

        /*
         * A pointer to the network interface queue that sits
         * between the "classifier" and the "link layer".
         */
        PriQueue *AOMDV_QPVifqueue;

        /*
         * Logging stuff
         */
        void log_link_del(nsaddr_t dst);
        void log_link_broke(Packet *p);
        void log_link_kept(nsaddr_t dst);

        // AOMDV_QPV code
        int aomdv_qpv_max_paths_;
        int aomdv_qpv_prim_alt_path_len_diff_;
        // AODV ns-2.31 code
        /*	for passing packets up to agents */
        PortClassifier *dmux_;

        LinkMetricsStore *mac_;

        map<nsaddr_t, int> pkts_sent_;
        // The delivery ratio of each link, pi of the link cost: the packets
        // a neighbour reports in its HELLO as received from this node, over
        // the packets that left this node's radio towards it since that
        // neighbour's previous HELLO, smoothed from one report to the next.
        map<nsaddr_t, int> sent_win_;
        map<nsaddr_t, double> prr_;

        int AOMDV_QPV_DBG;
        double hello_interval_; // seconds; Tcl-bound, see the note above ALLOWED_HELLO_LOSS
        double max_rreq_timeout_; // seconds; Tcl-bound, see the note above RREQ_RETRIES
        double active_route_timeout_; // seconds; Tcl-bound, see the note above
};

#endif /* __aomdv_qpv_h__ */
