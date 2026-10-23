/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// mac-802_15_4EvalVSN.h - the 802.15.4 MAC carrying the EvalVSN cross-layer
// store.
//
// Derivation, never modification: wpan/p802_15_4mac.cc is the reference
// implementation of the standard and stays untouched. Everything EvalVSN adds
// lives here. Keeping the original
// intact is what allows the two configurations the thesis compares to run in
// the same binary - cross-layer 802.15.4 against plain 802.15.4 - so that the
// difference measured is the contribution and not the build.
//
// The store itself is inherited behaviour, not new behaviour: the PHY measures
// LQI and SNR into the common header, the routing agent reads them there and
// pushes them here. This class only accumulates.

#ifndef ns_mac_802_15_4EvalVSN_h
#define ns_mac_802_15_4EvalVSN_h

#include <map>
#include <vector>

#include "wpan/p802_15_4mac.h"
#include "mac/link-metrics.h"
#include "common/timer-handler.h"

class Mac802_15_4EvalVSN;

// The TX watchdog every real AT86RF231 driver carries: if the transceiver
// state machine stops answering, the driver resets it rather than wait
// forever. The wpan base needs one because its dispatch() absorbs a
// mistimed ACK timeout silently (p802_15_4mac.cc:2394 returns without
// rescheduling anything), leaving txData held and the interface queue
// blocked for the rest of the simulation. The base cannot be fixed there -
// derivation, never modification - so the derived MAC watches its own
// protected task table instead.
class MacTxStallWatchdog : public TimerHandler
{
public:
	MacTxStallWatchdog(Mac802_15_4EvalVSN *m) : TimerHandler(), mac_(m) {}

protected:
	void expire(Event *);
	Mac802_15_4EvalVSN *mac_;
};

// The completion signal of a frame the learning MAC configured: the base
// calls the interface queue's resume handler once per frame, acknowledged or
// failed; the derived MAC passes this one instead, learns, then hands over.
class MacRlmDone : public Handler
{
public:
	MacRlmDone(Mac802_15_4EvalVSN *m) : mac_(m) {}
	void handle(Event *);
protected:
	Mac802_15_4EvalVSN *mac_;
};

class Mac802_15_4EvalVSN : public Mac802_15_4, public LinkMetricsStore
{
public:
	// The base takes the MAC PIB the wpan implementation keeps as a global;
	// the Tcl class passes the same one the original Mac/802_15_4 uses.
	Mac802_15_4EvalVSN(MAC_PIB *mp);

	// LinkMetricsStore
	void update_link_metrics(nsaddr_t neighbor, double lqi, double snr);
	double get_average_lqi(nsaddr_t neighbor);
	double get_average_snr(nsaddr_t neighbor);

	// Enriches the base reason with the frame type and priority (R58): both
	// fields live in the common
	// header, so no PHY pointer is needed - the 802.15.4 MAC keeps none by
	// design, the metrics reach it through the header.
	char *dropReason(Packet *p, char *reason);

	// The count comes from the coding scheme, not from the radio, so it is
	// held here rather than read from a PHY (R58: the MAC holds the count on
	// every stack). Thirteen is what the MMPEG chain produces (M1..M13,
	// D1..D13); the scenario sets it per run - "Mac/802_15_4EvalVSN set
	// MAX_PRIO_NB ..." - since the sets vary priorityNb.
	int getMaxPrioNb() const { return max_prio_nb_; }

	// Closed-loop transmit power control: delegated to the PHY, which sets
	// the power frame by frame.
	bool txPowerControlled();
	void txPowerFeedback(nsaddr_t neighbor, bool reported, double rssiDbm,
			     double sinrDb);
	int takeTxPackets(nsaddr_t neighbor);
	int totalTxPackets(nsaddr_t neighbor);
	int lastTxUid(nsaddr_t neighbor);
	void setMaxPrioNb(int value) { max_prio_nb_ = value; }

	// Downward entry point of the MAC; overridden only to feed the watchdog
	// (every packet the queue hands over proves the MAC is being serviced)
	// and to arm its timer once the scheduler runs.
	void recv(Packet *p, Handler *h);

	// One watchdog tick: a direct-transmission data task frozen on the same
	// step across consecutive ticks, with no packet handed over in between,
	// cannot happen legally - the longest lawful service of one frame
	// (macMaxCSMABackoffs backoffs plus 1+aMaxFrameRetries attempts with
	// their ACK waits) is under 200 ms against the 1.5 s this takes to
	// trigger. Recovery is the driver's gesture: reset the MAC state
	// (public init(false) clears the held packets and the task table; stray
	// timer expiries afterwards die on the base's own !taskStatus guards)
	// and hand the interface queue its resume callback.
	void watchdogTick();

	/*
	 * Learning MAC (RL_ACCESS_, off by default; 9 Oct 2026). Per node, a
	 * Q-table over the state (class of the packet: critical, normal, low;
	 * link to the destination: poor, fair, good by its mean LQI) and nine
	 * actions, the CSMA minimum backoff exponent in {2, 3, 4} crossed with
	 * the maximum number of backoffs in {3, 4, 5} - (3, 4) is the
	 * standard's. Each unicast frame takes the greedy action of its state,
	 * or with probability epsilon a random one when the packet is not
	 * critical; its outcome - acknowledged or failed, and the time the MAC
	 * held it - is the reward: w_c - 0.5 min(1, delay / 0.1 s) on success,
	 * -w_c on failure, w_c = 2, 1, 0.5 by class. One frame is one step
	 * (gamma = 0): Q <- Q + alpha (r - Q). The base MAC stays untouched:
	 * the parameters are the PIB's, set before the frame's channel access;
	 * the outcome comes from wrapping the frame's failure callback and the
	 * queue's resume handler, both handed on unchanged.
	 */
	void rlmFrameDone();
	static void rlmXmitFailed(Packet *p, void *arg);

	// The routing's learning agents (d22) learn from every unicast frame's
	// outcome; the frame is tracked through the same wrappers as above.
	void setOutcomeListener(FrameOutcomeListener *l) { outcomeListener_ = l; }

	/*
	 * Learning transmit power (RL_POWER_, off by default; d22, 10 Oct
	 * 2026), the agent that replaces the closed-loop rule of the PHY. Per
	 * neighbour, one value per output step among six of the AT86RF231's
	 * (+3, +1.3, 0, -3, -7, -12 dBm); each unicast frame takes the step of
	 * highest value, or with probability epsilon a random one when the
	 * packet is not critical, ties going to the highest power. The reward
	 * of the frame is +1 if acknowledged, -1 if not, minus kappa times the
	 * power spent as a share of the maximum: the agent looks for the
	 * lowest step that still gets through. Its actions are only the steps
	 * the link can carry - the SNR measured on the neighbour's frames,
	 * lowered by the step's cut, 1 dB above the reception floor - and the
	 * maximum alone until the neighbour is measured. The PHY applies the
	 * step and stops scoring the HELLO reports.
	 */

protected:
	int rl_access_;
	double rlm_alpha_;
	double rlm_epsilon_;
	double rlmQ_[3][3][9];
	bool rlmPending_;         // a frame configured by the agent is in the MAC
	bool rlmFailed_;          // its failure callback fired
	int rlmC_, rlmB_, rlmA_;  // its state and action
	double rlmStart_;         // when the queue handed it over
	Handler *rlmUpstream_;    // the queue's resume handler
	FailureCallback rlmOrigFail_;
	void *rlmOrigFailData_;
	MacRlmDone rlmDone_;
	int rlmClass(Packet *p);
	int rlmLinkBucket(nsaddr_t dst);
	nsaddr_t rlmDa_;          // the tracked frame's MAC destination
	int rlmUid_;              // and its packet uid
	int rlmS_;                // its power action, -1 if none
	FrameOutcomeListener *outcomeListener_;
	int rl_power_;
	double rlp_kappa_;
	std::map<nsaddr_t, std::vector<double> > rlpQ_; // neighbour -> value of each power step

	std::map<nsaddr_t, LinkMetrics> link_metrics_;
	int max_prio_nb_;

	MacTxStallWatchdog watchdog_;
	bool watchdog_armed_;
	bool wd_activity_;     // a recv() happened since the last tick
	bool wd_last_active_;  // task snapshot at the last tick
	UINT_8 wd_last_step_;
	char wd_last_frfunc_[81];
	int wd_stall_ticks_;
};

#endif /* ns_mac_802_15_4EvalVSN_h */
