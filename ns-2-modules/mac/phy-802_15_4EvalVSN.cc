/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// phy-802_15_4EvalVSN.cc - see phy-802_15_4EvalVSN.h.
//
// The received power is computed here exactly as WirelessPhy::sendUp()
// computes it a moment later: same stamp, same propagation model, same
// inputs, hence the same deterministic value. Recomputing it is the price of
// measuring without modifying the base chain, and the wpan receive path
// already pays it once more itself (p802_15_4phy.cc recomputes Pr for weak
// packets). The measures are written before the base decides acceptance, in
// the order the power monitor expects.

#include "mac/phy-802_15_4EvalVSN.h"

#include "mobilenode.h"
#include "propagation.h"
#include "wpan/p802_15_4trace.h"

static class Phy802_15_4EvalVSNClass : public TclClass
{
public:
	Phy802_15_4EvalVSNClass() : TclClass("Phy/WirelessPhy/802_15_4EvalVSN") {}

	TclObject *create(int, const char *const *)
	{
		// The same public PHY PIB the original Phy/WirelessPhy/802_15_4
		// class hands to every instance.
		return (new Phy802_15_4EvalVSN(&(Phy802_15_4::PPIB)));
	}
} class_Phy802_15_4EvalVSN;

Phy802_15_4EvalVSN::Phy802_15_4EvalVSN(PHY_PIB *pp) : Phy802_15_4(pp)
{
	bind("noise_floor_", &noise_floor_);
	bind("PowerMonitorThresh_", &PowerMonitorThresh_);
	bind("SINRThresh_", &SINRThresh_);
	bind("TxPowerAdapt_", &TxPowerAdapt_);
	bind("MAX_PRIO_NB", &MAX_PRIO_NB);

	// After the binds: the monitor snapshots the
	// thresholds at construction.
	powerMonitor = new PowerMonitorEvalVSN(this);

	ptMax_ = 0.0; // read at the first transmission, once Tcl has set Pt_
	externalPower_ = false;
}

int Phy802_15_4EvalVSN::sendUp(Packet *p)
{
	if (propagation_)
	{
		PacketStamp s;
		s.stamp((MobileNode *)node(), ant_, 0, lambda_);

		double Pr = propagation_->Pr(&p->txinfo_, &s, this);
		hdr_cmn *cmh = HDR_CMN(p);

		powerMonitor->recordPowerLevel(Pr, cmh->txtime());

		cmh->receivedSignalStrengthIndicator_ = powerMonitor->measureRSSI(Pr);
		cmh->signalToNoiseRatio_ = powerMonitor->measureSNR(Pr);
		cmh->linkQualityIndicator_ = powerMonitor->measureLQI(Pr);
	}

	return WirelessPhy::sendUp(p);
}

// The AT86RF231's sixteen output steps (datasheet table 9-15, TX_PWR
// register), in dBm, from the +3 dBm ceiling (step 0) down to -17 dBm.
static const double kTxPowerStepsDbm[] = {
	3.0, 2.8, 2.3, 1.8, 1.3, 0.7, 0.0, -1.0,
	-2.0, -3.0, -4.0, -5.0, -7.0, -9.0, -12.0, -17.0
};
static const int kTxPowerStepsNb = 16;

// Unicast frames go at the step kept for their MAC destination, broadcasts
// at the ceiling. The step is applied here, where WirelessPhy::sendDown()
// stamps Pt_ into the frame, exactly as a driver writes TX_PWR before
// loading the frame buffer.
void Phy802_15_4EvalVSN::sendDown(Packet *p)
{
	if (ptMax_ <= 0.0)
		ptMax_ = Pt_;

	int da = p802_15_4macDA(p);
	packet_t t = HDR_CMN(p)->ptype();
	bool unicast = da >= 0 && (t == PT_VIDEO || t == PT_AOMDV_QPV);

	// One count per packet: a MAC retry carries the same uid.
	if (unicast)
	{
		int uid = HDR_CMN(p)->uid();
		std::map<nsaddr_t, int>::iterator last = txLastUid_.find(da);
		if (last == txLastUid_.end() || last->second != uid)
		{
			txPkts_[da] += 1;
			txPktsTotal_[da] += 1;
			txLastUid_[da] = uid;
		}
	}

	if (TxPowerAdapt_ || externalPower_)
	{
		if (unicast)
		{
			std::map<nsaddr_t, int>::iterator it = txStep_.find(da);
			int step = (it == txStep_.end()) ? 0 : it->second;
			Pt_ = pow(10.0, kTxPowerStepsDbm[step] / 10.0) / 1000.0;
			txSent_[da] += 1;
		}
		else
			Pt_ = ptMax_;
	}

	WirelessPhy::sendDown(p);
}

int Phy802_15_4EvalVSN::lastTxUid(nsaddr_t neighbor)
{
	std::map<nsaddr_t, int>::iterator it = txLastUid_.find(neighbor);
	return it == txLastUid_.end() ? -1 : it->second;
}

int Phy802_15_4EvalVSN::totalTxPackets(nsaddr_t neighbor)
{
	std::map<nsaddr_t, int>::iterator it = txPktsTotal_.find(neighbor);
	return it == txPktsTotal_.end() ? 0 : it->second;
}

int Phy802_15_4EvalVSN::takeTxPackets(nsaddr_t neighbor)
{
	int n = txPkts_[neighbor];
	txPkts_[neighbor] = 0;
	return n;
}

// One report of a neighbour about the frames this node sent it since its
// previous HELLO. The score keeps the monitor's form - RSSI margin, SINR
// margin and delivery, weighted 0.6 / 0.3 / 0.1, each normalised over a
// 30 dB window - but the RSSI margin is counted from the floor the stack
// applies, max(RXThresh_, CSThresh_), and both quantities are measured at
// the far end. The rungs keep their thresholds; each moves whole steps.
void Phy802_15_4EvalVSN::onTxPowerFeedback(nsaddr_t neighbor, bool reported,
					   double rssiDbm, double sinrDb)
{
	if (!TxPowerAdapt_ || externalPower_)
		return;

	int sent = txSent_[neighbor];
	txSent_[neighbor] = 0;
	if (sent == 0)
		return; // nothing sent to that neighbour: nothing to judge

	int &step = txStep_[neighbor]; // 0, the ceiling, when first seen
	int move;                      // in steps, positive = more power

	if (!reported)
		move = +2;
	else
	{
		double floorW = RXThresh_ > CSThresh_ ? RXThresh_ : CSThresh_;
		double floorDbm = 10.0 * log10(floorW * 1000.0);
		double sinrThreshDb = 10.0 * log10(SINRThresh_);
		double normRSSI = clampMetric((rssiDbm - floorDbm) / 30.0, 0.0, 1.0);
		double normSINR = clampMetric((sinrDb - sinrThreshDb) / 30.0, 0.0, 1.0);
		double per = exp(-pow(10.0, sinrDb / 10.0) / SINRThresh_);
		double score = 0.6 * normRSSI + 0.3 * normSINR +
			       0.1 * clampMetric(1.0 - per, 0.0, 1.0);

		if (score > 0.8)
			move = -2;
		else if (score > 0.6)
			move = -1;
		else if (score > 0.4)
			move = 0;
		else if (score > 0.2)
			move = +1;
		else
			move = +2;
	}

	step -= move;
	if (step < 0)
		step = 0;
	if (step > kTxPowerStepsNb - 1)
		step = kTxPowerStepsNb - 1;
}
