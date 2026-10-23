/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// fire-propagationEvalVSN.cc - see fire-propagationEvalVSN.h.

#include "mac/fire-propagationEvalVSN.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mobilenode.h"
#include "scheduler.h"

static class ShadowingFireEvalVSNClass : public TclClass
{
public:
	ShadowingFireEvalVSNClass() : TclClass("Propagation/Shadowing/FireEvalVSN") {}

	TclObject *create(int, const char *const *)
	{
		return (new ShadowingFireEvalVSN);
	}
} class_ShadowingFireEvalVSN;

ShadowingFireEvalVSN::ShadowingFireEvalVSN() : Shadowing()
{
	bind("fireAlpha_", &fireAlpha_);
	bind("fireZone_", &fireZone_);
}

double ShadowingFireEvalVSN::zoneDepth(int nodeId, double now)
{
	std::map<int, double>::iterator it = frontTime_.find(nodeId);
	if (it == frontTime_.end())
		return 0.0;
	double tau = it->second - now; // seconds before the front arrives
	if (tau >= fireZone_)
		return 0.0;
	if (tau <= 0.0)
		return 1.0;
	return 1.0 - tau / fireZone_;
}

double ShadowingFireEvalVSN::Pr(PacketStamp *t, PacketStamp *r, WirelessPhy *ifp)
{
	double pr = Shadowing::Pr(t, r, ifp);
	if (fireAlpha_ <= 0.0 || fireZone_ <= 0.0 || frontTime_.empty())
		return pr;
	double now = Scheduler::instance().clock();
	double depth = zoneDepth(t->getNode()->nodeid(), now) +
		       zoneDepth(r->getNode()->nodeid(), now);
	if (depth <= 0.0)
		return pr;
	double Xt, Yt, Zt, Xr, Yr, Zr;
	t->getNode()->getLoc(&Xt, &Yt, &Zt);
	r->getNode()->getLoc(&Xr, &Yr, &Zr);
	double d = sqrt((Xr - Xt) * (Xr - Xt) + (Yr - Yt) * (Yr - Yt) +
			(Zr - Zt) * (Zr - Zt));
	double loss = fireAlpha_ * d * 0.5 * depth;
	return pr * pow(10.0, -loss / 10.0);
}

int ShadowingFireEvalVSN::command(int argc, const char *const *argv)
{
	// fire-node <id> <front arrival time>: one call per node, from the
	// scenario's destruction schedule.
	if (argc == 4 && strcmp(argv[1], "fire-node") == 0)
	{
		frontTime_[atoi(argv[2])] = atof(argv[3]);
		return TCL_OK;
	}
	return Shadowing::command(argc, argv);
}
