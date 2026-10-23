/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// fire-propagationEvalVSN.h - log-normal shadowing with the attenuation of a
// forest fire's flame and plume (decision d23 of 10 Oct 2026).
//
// Derivation, never modification: mobile/shadowing.cc stays the reference
// model; this class adds a loss on top of it for the links whose ends stand
// close ahead of the fire front. The flame zone is weakly ionised by thermal
// ionisation of the alkalis it carries (Mphale et al. 2007, 2010), and the
// hot plume refracts the waves that cross it: Mphale, Heron and Verma (2007,
// PIER 68) computed losses of 0.136 to 0.154 dB per metre of path along the
// fuel-fire interface for a single thermal lens, and up to 1.65 dB per metre
// under two vortex pairs. The model takes that form: a link loses
// fireAlpha_ dB per metre of its length that lies in the heated zone ahead
// of the front. Each end stands at a depth in [0, 1] of that zone - zero
// when the front is fireZone_ seconds away, one when it arrives - and the
// share of the link inside the zone is taken as the mean depth of its two
// ends, so a link of length d loses fireAlpha_ * d * (depth_tx + depth_rx) / 2.
// The front arrival time of each node comes from the scenario (evalvsn.tcl,
// "fire-node id t"), which already computes it for the destruction schedule;
// distance to the front is thus expressed as time to the front, whatever the
// front's shape.
//
// Every user of the received power sees the same loss - reception, carrier
// sense, interference and the stack's measures - because they all ask the
// propagation model. With fireAlpha_ at 0 (the default) Pr() returns the
// base value untouched.

#ifndef ns_fire_propagationEvalVSN_h
#define ns_fire_propagationEvalVSN_h

#include "shadowing.h"
#include <map>

class ShadowingFireEvalVSN : public Shadowing
{
public:
	ShadowingFireEvalVSN();
	virtual double Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp);
	virtual int command(int argc, const char *const *argv);

	// How deep one end of a link stands in the heated zone at time now, in [0, 1].
	double zoneDepth(int nodeId, double now);

protected:
	double fireAlpha_;   // dB lost per metre of link inside the heated zone
	double fireZone_;    // seconds ahead of the front where the loss starts
	std::map<int, double> frontTime_; // node id -> front arrival time (s)
};

#endif /* ns_fire_propagationEvalVSN_h */
