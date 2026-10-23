/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// link-metrics.h - the cross-layer link quality store, shared by the EvalVSN
// MAC layers.
//
// The measurement chain has three stages and only the middle one lives here.
// The PHY measures LQI and SNR on reception and writes them into the common
// packet header; the routing agent reads them from that header and hands them
// to the MAC; the MAC accumulates them per neighbour and answers averages.
//
// The MAC is therefore only a store, declared as an interface so that
// AOMDV_QPV holds one pointer to it; the 802.15.4 MAC of the project is the
// store, and plain 802.15.4 runs beside it in the same binary.
//
// The 802.15.4 classes of wpan/ are never modified: the EvalVSN behaviour is
// added by derivation.

#ifndef ns_link_metrics_h
#define ns_link_metrics_h

#include <map>
#include "config.h"
#include "packet.h"

// Pure arithmetic, kept out of the PHY where it had no business being: the
// routing agent normalises LQI and SNR before weighing them.
static inline double clampMetric(double v, double lo, double hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

// Moving averages of LQI and SNR, per neighbour: each new frame weighs
// 0.1, so that the average follows a link over its last twenty frames or
// so. The cumulative mean it replaces (audit of 5 Oct 2026) no longer moved
// after a few hundred frames, whatever became of the link.
#define LINK_METRICS_EWMA_WEIGHT 0.1
struct LinkMetrics
{
	double lqi_avg;
	double snr_avg;
	int count;

	LinkMetrics() : lqi_avg(0.0), snr_avg(0.0), count(0) {}
};

// What a cross-layer routing agent needs of a MAC, and nothing more.
// The outcome of each unicast frame, handed to the routing's learning agents
// (decision d22, 10 Oct 2026): the base MAC reports failures only
// (Mac802_15_4::callBack = 1), so an agent that learns from deliveries needs
// the successes too. The derived MAC calls this once per frame it tracks,
// when the frame is acknowledged or abandoned.
class FrameOutcomeListener
{
public:
	virtual ~FrameOutcomeListener() {}
	// da: MAC destination; uid: the packet's; ok: acknowledged; delay: time
	// the MAC held the frame (s); cls: 0 critical, 1 normal, 2 low.
	virtual void frameOutcome(nsaddr_t da, int uid, bool ok, double delay, int cls) = 0;
};

class LinkMetricsStore
{
public:
	virtual ~LinkMetricsStore() {}

	// Adds one sample for a neighbour.
	virtual void update_link_metrics(nsaddr_t neighbor, double lqi, double snr) = 0;

	// Moving average of the samples; 0.0 when the neighbour is unknown.
	virtual double get_average_lqi(nsaddr_t neighbor) = 0;
	virtual double get_average_snr(nsaddr_t neighbor) = 0;

	// The routing agent labels its drops with a reason the PHY refines. Each
	// MAC knows its own PHY, so the call is delegated here rather than the
	// PHY being exposed - which is what kept the agent tied to one MAC type.
	virtual char *dropReason(Packet *p, char *reason) = 0;

	// Number of priority levels the chain carries. It is a property of the
	// coding scheme, not of the radio, so every MAC answers the same thing.
	virtual int getMaxPrioNb() const = 0;

	// Closed-loop transmit power control (chapter 4): the routing agent
	// carries the reports in its HELLO and relays what a neighbour said of
	// this node's frames; the MAC hands it to its PHY. A MAC without power
	// control ignores both.
	virtual bool txPowerControlled() { return false; }
	virtual void txPowerFeedback(nsaddr_t neighbor, bool reported,
				     double rssiDbm, double sinrDb) {}

	// Packets that left the radio towards a neighbour since the last call,
	// counted once each; -1 when the MAC cannot tell.
	virtual int takeTxPackets(nsaddr_t neighbor) { return -1; }
	virtual int totalTxPackets(nsaddr_t neighbor) { return -1; }
	virtual int lastTxUid(nsaddr_t neighbor) { return -1; }
	// Registers the routing's listener of frame outcomes; a MAC that does not
	// track them ignores it.
	virtual void setOutcomeListener(FrameOutcomeListener *l) {}
};

#endif /* ns_link_metrics_h */
