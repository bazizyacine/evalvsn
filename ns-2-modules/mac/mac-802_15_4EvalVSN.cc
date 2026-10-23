/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// mac-802_15_4EvalVSN.cc - see mac-802_15_4EvalVSN.h.
//
// The averages are computed on read rather than maintained on write, which
// costs nothing at these sample counts and keeps the accumulator to three
// numbers per neighbour. An unknown neighbour answers 0.0 rather than a
// division by zero, and the routing agent reads that as "no measurement yet".

#include "mac/mac-802_15_4EvalVSN.h"
#include "mac/phy-802_15_4EvalVSN.h"
#include "random.h"
#include "wpan/p802_15_4trace.h"

// The wpan implementation keeps its MAC PIB as a file-local static, so it
// cannot be shared from here. These are the same standard defaults, taken from
// the same def_* constants of p802_15_4const.h: a copy of the standard's
// values, not a fork of any behaviour. Each EvalVSN MAC gets its own PIB,
// which is if anything more correct than one global shared by every node.
static MAC_PIB EvalVSN_MPIB =
{
	def_macAckWaitDuration,		def_macAssociationPermit,
	def_macAutoRequest,		def_macBattLifeExt,
	def_macBattLifeExtPeriods,	def_macBeaconPayload,
	def_macBeaconPayloadLength,	def_macBeaconOrder,
	def_macBeaconTxTime,		0/*def_macBSN*/,
	def_macCoordExtendedAddress,	def_macCoordShortAddress,
	0/*def_macDSN*/,		def_macGTSPermit,
	def_macMaxCSMABackoffs,		def_macMinBE,
	def_macPANId,			def_macPromiscuousMode,
	def_macRxOnWhenIdle,		def_macShortAddress,
	def_macSuperframeOrder,		def_macTransactionPersistenceTime,
	def_macACLEntryDescriptorSet,	def_macACLEntryDescriptorSetSize,
	def_macDefaultSecurity,		def_macACLDefaultSecurityMaterialLength,
	def_macDefaultSecurityMaterial,	def_macDefaultSecuritySuite,
	def_macSecurityMode
};

static class Mac802_15_4EvalVSNClass : public TclClass
{
public:
	Mac802_15_4EvalVSNClass() : TclClass("Mac/802_15_4EvalVSN") {}

	TclObject *create(int, const char *const *)
	{
		return (new Mac802_15_4EvalVSN(&EvalVSN_MPIB));
	}
} class_mac802_15_4EvalVSN;

// The tick has to out-wait the longest lawful service of one direct frame
// (under 200 ms, see the header); three still ticks of half a second keep a
// margin above even a saturation-stretched CSMA round before declaring the
// state machine dead.
#define TX_WATCHDOG_PERIOD 0.5
#define TX_WATCHDOG_STILL_TICKS 3

// The learning power's actions: six of the AT86RF231's TX_PWR steps (the
// PHY's table, phy-802_15_4EvalVSN.cc) and their output in dBm.
static const int kRlpSteps[] = {0, 4, 6, 9, 12, 14};
static const double kRlpStepsDbm[] = {3.0, 1.3, 0.0, -3.0, -7.0, -12.0};
static const int kRlpStepsNb = 6;

Mac802_15_4EvalVSN::Mac802_15_4EvalVSN(MAC_PIB *mp) : Mac802_15_4(mp), max_prio_nb_(13),
	watchdog_(this), watchdog_armed_(false), wd_activity_(false),
	wd_last_active_(false), wd_last_step_(0), wd_stall_ticks_(0),
	rl_access_(0), rlm_alpha_(0.2), rlm_epsilon_(0.1), rlmPending_(false),
	rlmFailed_(false), rlmC_(0), rlmB_(0), rlmA_(4), rlmStart_(0.0),
	rlmUpstream_(0), rlmOrigFail_(0), rlmOrigFailData_(0), rlmDone_(this),
	rlmDa_(-1), rlmUid_(-1), rlmS_(-1),
	outcomeListener_(0), rl_power_(0), rlp_kappa_(0.2)
{
	// Tcl-bound so the scenario can carry the set's priorityNb; the default
	// in ns-default.tcl is the thirteen the MMPEG chain produces.
	bind("MAX_PRIO_NB", &max_prio_nb_);
	bind("RL_ACCESS_", &rl_access_);
	bind("RL_MAC_ALPHA_", &rlm_alpha_);
	bind("RL_MAC_EPSILON_", &rlm_epsilon_);
	bind("RL_POWER_", &rl_power_);
	bind("RL_POWER_KAPPA_", &rlp_kappa_);
	wd_last_frfunc_[0] = '\0';
	// Every state starts on the standard's action, by a small margin.
	for (int c = 0; c < 3; c++)
		for (int b = 0; b < 3; b++)
			for (int a = 0; a < 9; a++)
				rlmQ_[c][b][a] = (a == 4) ? 0.05 : 0.0;
}

void MacTxStallWatchdog::expire(Event *)
{
	mac_->watchdogTick();
}

void Mac802_15_4EvalVSN::recv(Packet *p, Handler *h)
{
	wd_activity_ = true;
	if (!watchdog_armed_)
	{
		watchdog_armed_ = true;
		watchdog_.resched(TX_WATCHDOG_PERIOD);
	}

	// The learning MAC configures each unicast frame the queue hands over.
	struct hdr_cmn *ch = HDR_CMN(p);
	// A received copy still carries the sender's wrapper, the channel having
	// copied the frame while it was in flight: the sender's original
	// callback goes back in, as the copy would carry without the learning
	// MAC. The sender still waits for this frame's ACK, so its saved
	// callback is this frame's.
	if (ch->direction() == hdr_cmn::UP && ch->xmit_failure_ == rlmXmitFailed)
	{
		Mac802_15_4EvalVSN *s = (Mac802_15_4EvalVSN *)ch->xmit_failure_data_;
		ch->xmit_failure_ = s->rlmOrigFail_;
		ch->xmit_failure_data_ = s->rlmOrigFailData_;
	}
	// A frame is tracked - its outcome learnt - when the learning MAC, the
	// learning power or a routing agent listening to outcomes is on; with
	// all three off the queue's handler goes to the base untouched.
	if ((rl_access_ || rl_power_ || outcomeListener_) && h && ch->direction() == hdr_cmn::DOWN)
	{
		static const UINT_8 BE[3] = {2, 3, 4};
		static const UINT_8 NB[3] = {3, 4, 5};
		// Unicast by the MAC destination, the base's own test: a forwarded
		// RREQ keeps a stale next hop (-2), and wrapping a broadcast let the
		// wrapper travel with the flood until two nodes' wrappers called
		// each other without end (segfault, 9 Oct 2026). A frame already
		// wrapped is never wrapped twice.
		int da = p802_15_4macDA(p);
		bool unicast = da != (int)MAC_BROADCAST &&
			       ch->xmit_failure_ != rlmXmitFailed;
		// A frame whose completion never came back (it should not happen:
		// the queue waits for the resume handler) is abandoned unlearnt.
		if (rlmPending_)
			rlmPending_ = false;
		if (!unicast)
		{
			if (rl_access_)
			{
				mpib.macMinBE = def_macMinBE;
				mpib.macMaxCSMABackoffs = def_macMaxCSMABackoffs;
			}
		}
		else
		{
			int c = rlmClass(p);
			int b = rlmLinkBucket(ch->next_hop());
			int a = -1;
			if (rl_access_)
			{
				a = 0;
				for (int k = 1; k < 9; k++)
					if (rlmQ_[c][b][k] > rlmQ_[c][b][a])
						a = k;
				if (c > 0 && Random::uniform() < rlm_epsilon_)
					a = (int)(Random::uniform() * 9) % 9;
				mpib.macMinBE = BE[a / 3];
				mpib.macMaxCSMABackoffs = NB[a % 3];
			}
			int s = -1;
			if (rl_power_)
			{
				// The base keeps its PHY pointer private; Mac::netif_ is the
				// same interface (the downstream target may be an error
				// model in between).
				Phy802_15_4EvalVSN *ephy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
				if (ephy)
				{
					std::vector<double> &q = rlpQ_[da];
					if (q.empty())
						q.assign(kRlpStepsNb, 0.0);
					// Only the steps the link can carry: the SNR measured on
					// this neighbour's frames, lowered by the step's power
					// cut, must stay 1 dB above the reception floor. With no
					// measure yet, the maximum alone (on the 40 m grid the
					// links stand 3 dB above the floor: an unconstrained
					// exploration lost 15 points of delivery, 10 Oct 2026).
					double snr = get_average_snr(da);
					int allowed = 1;
					if (snr > 0.0)
						while (allowed < kRlpStepsNb &&
						       snr - (kRlpStepsDbm[0] - kRlpStepsDbm[allowed]) >= ephy->snrFloorDb() + 1.0)
							allowed++;
					s = 0;
					for (int k = 1; k < allowed; k++)
						if (q[k] > q[s])
							s = k;
					if (c > 0 && allowed > 1 && Random::uniform() < rlm_epsilon_)
						s = (int)(Random::uniform() * allowed) % allowed;
					ephy->setExternalStep(da, kRlpSteps[s]);
				}
			}
			rlmPending_ = true;
			rlmFailed_ = false;
			rlmC_ = c;
			rlmB_ = b;
			rlmA_ = a;
			rlmS_ = s;
			rlmDa_ = da;
			rlmUid_ = ch->uid();
			rlmStart_ = Scheduler::instance().clock();
			rlmUpstream_ = h;
			rlmOrigFail_ = ch->xmit_failure_;
			rlmOrigFailData_ = ch->xmit_failure_data_;
			ch->xmit_failure_ = rlmXmitFailed;
			ch->xmit_failure_data_ = (void *)this;
			Mac802_15_4::recv(p, &rlmDone_);
			return;
		}
	}
	Mac802_15_4::recv(p, h);
}

void MacRlmDone::handle(Event *)
{
	mac_->rlmFrameDone();
}

// The frame failed: noted for the reward, then the routing's own callback,
// exactly as the base would have called it.
void Mac802_15_4EvalVSN::rlmXmitFailed(Packet *p, void *arg)
{
	Mac802_15_4EvalVSN *m = (Mac802_15_4EvalVSN *)arg;
	struct hdr_cmn *ch = HDR_CMN(p);
	if (ch->xmit_reason_ == 0)
		m->rlmFailed_ = true;
	ch->xmit_failure_ = m->rlmOrigFail_;
	ch->xmit_failure_data_ = m->rlmOrigFailData_;
	if (m->rlmOrigFail_)
		m->rlmOrigFail_(p, m->rlmOrigFailData_);
	else
		Packet::free(p);
}

// The frame is done: the reward, the Q step, the standard PIB back, and the
// queue's resume handler.
void Mac802_15_4EvalVSN::rlmFrameDone()
{
	if (rlmPending_)
	{
		static const double W[3] = {2.0, 1.0, 0.5};
		double delay = Scheduler::instance().clock() - rlmStart_;
		bool ok = !rlmFailed_;
		if (rl_access_ && rlmA_ >= 0)
		{
			double r = !ok ? -W[rlmC_]
				       : W[rlmC_] - 0.5 * (delay / 0.1 < 1.0 ? delay / 0.1 : 1.0);
			double &q = rlmQ_[rlmC_][rlmB_][rlmA_];
			q += rlm_alpha_ * (r - q);
		}
		if (rl_power_ && rlmS_ >= 0)
		{
			double share = pow(10.0, (kRlpStepsDbm[rlmS_] - kRlpStepsDbm[0]) / 10.0);
			double r = (ok ? 1.0 : -1.0) - rlp_kappa_ * share;
			double &q = rlpQ_[rlmDa_][rlmS_];
			q += rlm_alpha_ * (r - q);
		}
		if (outcomeListener_)
			outcomeListener_->frameOutcome(rlmDa_, rlmUid_, ok, delay, rlmC_);
		rlmPending_ = false;
	}
	mpib.macMinBE = def_macMinBE;
	mpib.macMaxCSMABackoffs = def_macMaxCSMABackoffs;
	Handler *h = rlmUpstream_;
	rlmUpstream_ = 0;
	if (h)
		h->handle((Event *)0);
}

// Critical: control and scalar traffic, frames of type M, priority level 1;
// normal: the upper half of the remaining levels; low: the rest.
int Mac802_15_4EvalVSN::rlmClass(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	if (ch->ptype() != PT_VIDEO || ch->frameType_[0] == 'M' || ch->prioNb_ <= 1)
		return 0;
	return (ch->prioNb_ <= max_prio_nb_ / 2) ? 1 : 2;
}

// The link to the destination by its mean LQI: under 120 poor, under 180
// fair, else good; fair until measured.
int Mac802_15_4EvalVSN::rlmLinkBucket(nsaddr_t dst)
{
	std::map<nsaddr_t, LinkMetrics>::iterator it = link_metrics_.find(dst);
	if (it == link_metrics_.end() || it->second.count == 0)
		return 1;
	double lqi = it->second.lqi_avg;
	return lqi < 120.0 ? 0 : (lqi < 180.0 ? 1 : 2);
}

void Mac802_15_4EvalVSN::watchdogTick()
{
	bool active = taskP.taskStatus(TP_mcps_data_request);
	UINT_8 step = taskP.taskStep(TP_mcps_data_request);
	const char *fr = taskP.taskFrFunc(TP_mcps_data_request);

	// Indirect transactions may lawfully wait for their persistence time;
	// only the direct path is bounded tightly enough to police.
	bool direct = !(taskP.mcps_data_request_TxOptions & TxOp_Indirect);

	if (active && direct && !wd_activity_ && wd_last_active_ &&
		step == wd_last_step_ && strncmp(fr, wd_last_frfunc_, 80) == 0)
		wd_stall_ticks_++;
	else
		wd_stall_ticks_ = 0;

	wd_last_active_ = active;
	wd_last_step_ = step;
	strncpy(wd_last_frfunc_, fr, 80);
	wd_last_frfunc_[80] = '\0';
	wd_activity_ = false;

	if (wd_stall_ticks_ >= TX_WATCHDOG_STILL_TICKS)
	{
		fprintf(stderr,
				"Mac802_15_4EvalVSN: TX watchdog reset at %f (node %d, step %d, %s)\n",
				Scheduler::instance().clock(), index_, step, wd_last_frfunc_);
		init(false);
		wd_stall_ticks_ = 0;
		wd_last_active_ = false;
		if (rlmPending_)
			rlmFailed_ = true;
		if (callback_)
		{
			Handler *h = callback_;
			callback_ = 0;
			h->handle((Event *)0);
		}
	}

	watchdog_.resched(TX_WATCHDOG_PERIOD);
}

// The drop reason composed as "<reason> -Frt <t>
// -Pri <n>" - so ns2TrStats.sh reads one drop typology whichever radio wrote
// the trace. Control packets carry no frame type and print 'C', priority 0.
// The string is strdup'd: ns-2's drop() keeps the pointer
// for the trace line and never frees it.
char *Mac802_15_4EvalVSN::dropReason(Packet *p, char *reason)
{
	hdr_cmn *ch = HDR_CMN(p);

	char frameType = ch->frameType_[0] ? ch->frameType_[0] : 'C';
	int priority = ch->prioNb_ ? ch->prioNb_ : 0;

	char buf[1000];
	snprintf(buf, sizeof(buf), "%s -Frt %c -Pri %d",
			 reason ? reason : "---", frameType, priority);

	return strdup(buf);
}

void Mac802_15_4EvalVSN::update_link_metrics(nsaddr_t neighbor, double lqi, double snr)
{
	LinkMetrics &m = link_metrics_[neighbor];

	if (m.count == 0)
	{
		m.lqi_avg = lqi;
		m.snr_avg = snr;
	}
	else
	{
		m.lqi_avg += LINK_METRICS_EWMA_WEIGHT * (lqi - m.lqi_avg);
		m.snr_avg += LINK_METRICS_EWMA_WEIGHT * (snr - m.snr_avg);
	}
	m.count++;
}

double Mac802_15_4EvalVSN::get_average_lqi(nsaddr_t neighbor)
{
	std::map<nsaddr_t, LinkMetrics>::const_iterator it = link_metrics_.find(neighbor);

	if (it == link_metrics_.end() || it->second.count == 0)
		return 0.0;

	return it->second.lqi_avg;
}

bool Mac802_15_4EvalVSN::txPowerControlled()
{
	Phy802_15_4EvalVSN *phy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
	return phy && phy->txPowerControlled();
}

void Mac802_15_4EvalVSN::txPowerFeedback(nsaddr_t neighbor, bool reported,
					 double rssiDbm, double sinrDb)
{
	Phy802_15_4EvalVSN *phy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
	if (phy)
		phy->onTxPowerFeedback(neighbor, reported, rssiDbm, sinrDb);
}

int Mac802_15_4EvalVSN::lastTxUid(nsaddr_t neighbor)
{
	Phy802_15_4EvalVSN *phy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
	return phy ? phy->lastTxUid(neighbor) : -1;
}

int Mac802_15_4EvalVSN::totalTxPackets(nsaddr_t neighbor)
{
	Phy802_15_4EvalVSN *phy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
	return phy ? phy->totalTxPackets(neighbor) : -1;
}

int Mac802_15_4EvalVSN::takeTxPackets(nsaddr_t neighbor)
{
	Phy802_15_4EvalVSN *phy = dynamic_cast<Phy802_15_4EvalVSN *>(netif_);
	return phy ? phy->takeTxPackets(neighbor) : -1;
}

double Mac802_15_4EvalVSN::get_average_snr(nsaddr_t neighbor)
{
	std::map<nsaddr_t, LinkMetrics>::const_iterator it = link_metrics_.find(neighbor);

	if (it == link_metrics_.end() || it->second.count == 0)
		return 0.0;

	return it->second.snr_avg;
}

