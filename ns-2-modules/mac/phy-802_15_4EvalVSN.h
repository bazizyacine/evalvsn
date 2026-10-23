/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// phy-802_15_4EvalVSN.h - the 802.15.4 PHY carrying the EvalVSN cross-layer
// measures.
//
// Derivation, never modification: wpan/p802_15_4phy.cc is the reference
// implementation of the standard and stays untouched, exactly as
// Mac802_15_4EvalVSN left wpan/p802_15_4mac.cc alone. Keeping the original
// intact is what allows cross-layer 802.15.4 and plain 802.15.4 to run in the
// same binary, so the difference measured is the contribution.
//
// The graft is one override. Phy802_15_4::recv() hands every incoming packet
// to the virtual sendUp(); this class measures there - the five fields of the
// common header, computed from the received power by the PowerMonitor -
// then lets WirelessPhy::sendUp() decide acceptance as it always did.
// Without this class the derived MAC's store was ready but fed by nothing.

#ifndef ns_phy_802_15_4EvalVSN_h
#define ns_phy_802_15_4EvalVSN_h

#include "wpan/p802_15_4phy.h"
#include "mac/power-monitorEvalVSN.h"
#include "mac/link-metrics.h"
#include <math.h>
#include <map>

class Phy802_15_4EvalVSN : public Phy802_15_4, public PowerMonitorHostEvalVSN
{
public:
	Phy802_15_4EvalVSN(PHY_PIB *pp);

	// The measuring override; the base reception chain is otherwise intact.
	int sendUp(Packet *p);

	// Transmit power control in closed loop, per neighbour (chapter 4;
	// rewritten after the bench of 4 Oct 2026). The former open loop set
	// the node's single Pt_ from the packets it received - the power of the
	// neighbour, not its own - and scored the margin above RXThresh_
	// (-101 dBm) while WirelessPhy::sendUp() drops every frame under
	// CSThresh_ (-91 dBm): on the 200 m grid it parked 42 nodes of 100 at
	// -5 dBm, 0.07 dB under the floor, mute (audit file
	// power-control-diagnosis.md). Here each neighbour reports, in its
	// HELLO, the mean power and SINR at which it received this node's
	// unicast frames; the score is taken on that report, its margin counted
	// from the floor the stack really applies, and the step towards that
	// neighbour moves one or two whole TX_PWR steps at a time, so that no
	// decision is absorbed by the register's rounding. A neighbour that was
	// sent frames and does not report them raises the step: silence means
	// the frames do not arrive. Broadcasts (HELLO, RREQ) go at the ceiling,
	// so discovery and the reports keep the full range. Gated by
	// TxPowerAdapt_ (default 0).
	void sendDown(Packet *p);
	void onTxPowerFeedback(nsaddr_t neighbor, bool reported, double rssiDbm,
			       double sinrDb);
	bool txPowerControlled() const { return TxPowerAdapt_ != 0; }
	// The learning power of the MAC (RL_POWER_, d22) sets the step towards
	// a neighbour frame by frame; from its first call the PHY applies the
	// steps it is given and stops scoring the HELLO reports.
	void setExternalStep(nsaddr_t neighbor, int step)
	{
		externalPower_ = true;
		txStep_[neighbor] = step;
	}
	// The SNR (dB) under which a frame is not received: the floor the stack
	// applies, max(RXThresh_, CSThresh_), over the noise floor.
	double snrFloorDb() const
	{
		double floorW = RXThresh_ > CSThresh_ ? RXThresh_ : CSThresh_;
		return 10.0 * log10(floorW / noise_floor_);
	}

	// Packets that left the radio towards a neighbour since the last call,
	// each counted once whatever its MAC retries: the denominator of the
	// link's delivery ratio, which a count at the routing would spoil with
	// the packets the node's own queue drops before they are sent.
	int takeTxPackets(nsaddr_t neighbor);
	// The same packets, counted since the start and never reset.
	int totalTxPackets(nsaddr_t neighbor);
	// The uid of the last packet that left the radio towards a neighbour,
	// -1 if none: a failed packet that never left it failed on a busy
	// channel, which says nothing of the neighbour.
	int lastTxUid(nsaddr_t neighbor);

	// PowerMonitorHostEvalVSN. The thresholds are the WirelessPhy ones the
	// wpan scenarios already set; the radio-specific constants are bound
	// below. The carrier-sense edges are ignored: the wpan base class does
	// CCA and ED on its own accounting (rxTotPower), and the monitor is an
	// instrument here, not a carrier sense.
	double monitorRecordThresh() { return PowerMonitorThresh_; }
	double monitorNoiseFloor() { return noise_floor_; }

	// LQI graded in dB: the usable window of this radio spans six decades
	// of received power, over all of which a linear mapping is
	// flat at 255 (measured, 50 m to 500 m). Zero sits where the radio
	// stops - the energy floor is the sensitivity (RXThresh_), the SINR
	// floor is the PER threshold (SINRThresh_) - and 30 dB above either
	// floor is excellent; the worse of the two, as the instrument has
	// always combined them.
	UINT_8 monitorMapLQI(double Pr, double sinrLin)
	{
		if (Pr <= 0.0 || sinrLin <= 0.0)
			return 0;

		double e = 10.0 * log10(Pr / RXThresh_) * 255.0 / 30.0;
		double s = 10.0 * log10(sinrLin / SINRThresh_) * 255.0 / 30.0;
		double worse = e < s ? e : s;

		return (UINT_8)clampMetric(worse, 0, 255);
	}

protected:
	// Bound variables; defaults in tcl/lib/ns-default.tcl, anchored to the
	// 2.4 GHz O-QPSK channel and the AT86RF231 radio of the IoT-LAB M3 node.
	double noise_floor_;		// environmental noise floor (W)
	double PowerMonitorThresh_;	// record interference above this level (W)
	double SINRThresh_;		// linear SINR the modulation requires
	int TxPowerAdapt_;		// 1 = adapt Pt_ on the AT86RF231 steps (R51)
	int MAX_PRIO_NB;		// the PHY's own copy for its score;
					// the MAC holds the authoritative one (R58)

	PowerMonitorEvalVSN *powerMonitor;

	// Closed-loop state: the TX_PWR step used towards each neighbour (0 is
	// the +3 dBm ceiling), the unicast frames sent to it since its last
	// report, and the configured ceiling, read once the Tcl binds are set.
	std::map<nsaddr_t, int> txStep_;
	std::map<nsaddr_t, int> txSent_;
	std::map<nsaddr_t, int> txPkts_;
	std::map<nsaddr_t, int> txPktsTotal_;
	std::map<nsaddr_t, int> txLastUid_;
	double ptMax_;
	bool externalPower_; // the steps come from the MAC's learning power
};

#endif /* ns_phy_802_15_4EvalVSN_h */
