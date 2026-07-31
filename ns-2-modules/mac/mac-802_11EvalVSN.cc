/*
 * Copyright (C) 2007
 * Mercedes-Benz Research & Development North America, Inc. and
 * University of Karlsruhe (TH)
 * All rights reserved.
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
 * This code was designed and developed by:
 *
 * Qi Chen                 : qi.chen@daimler.com
 * Felix Schmidt-Eisenlohr : felix.schmidt-eisenlohr@kit.edu
 * Daniel Jiang            : daniel.jiang@daimler.com
 *
 * For further information see:
 * http://dsn.tm.uni-karlsruhe.de/english/Overhaul_NS-2.php
 */

#include "mac-802_11EvalVSN.h"
#include <aomdv_qpv/aomdv_qpv.h>

#include "delay.h"
#include "connector.h"
#include "packet.h"
#include "random.h"
#include "mobilenode.h"
#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "cmu-trace.h"
#include <iostream>
#include "common/agent.h"

#define DROP_MAC_NAV "NAV"
#define DROP_MAC_TXTIME_ERR "XTIM"

PHY_MIBEvalVSN::PHY_MIBEvalVSN(Mac802_11EvalVSN *parent)
{
	parent->bind("SlotTime_", &SlotTime);
	parent->bind("HeaderDuration_", &HeaderDuration);
	parent->bind("SymbolDuration_", &SymbolDuration);
	parent->bind("BasicModulationScheme_", &BasicModulationScheme);
	parent->bind_bool("use_802_11a_flag_", &use_802_11a_flag_);
}

MAC_MIBEvalVSN::MAC_MIBEvalVSN(Mac802_11EvalVSN *parent)
{
	parent->bind("CWMin_", &CWMin);
	parent->bind("CWMax_", &CWMax);
	parent->bind("SIFS_", &SIFSTime);
	parent->bind("RTSThreshold_", &RTSThreshold);
	parent->bind("ShortRetryLimit_", &ShortRetryLimit);
	parent->bind("LongRetryLimit_", &LongRetryLimit);
}

/* ======================================================================
 TCL Hooks for the simulator
 ====================================================================== */
static class Mac802_11EvalVSNClass : public TclClass
{
public:
	Mac802_11EvalVSNClass() : TclClass("Mac/802_11EvalVSN")
	{
	}
	TclObject *create(int, const char *const *)
	{
		return (new Mac802_11EvalVSN());
	}
} class_mac802_11;

/* ======================================================================
 Mac Class Functions
 ====================================================================== */
Mac802_11EvalVSN::Mac802_11EvalVSN() : Mac(), bkmgr(this), csmgr(this), txc(this), rxc(this), phymib_(this),
									   macmib_(this), phy_(NULL)
{
	bind("MAC_DBG", &MAC_DBG);

	ssrc_ = slrc_ = 0;
	sifs_ = macmib_.getSIFS();
	difs_ = macmib_.getSIFS() + 2 * phymib_.getSlotTime();
	eifs_ = macmib_.getSIFS() + difs_ + txtime(phymib_.getACKlen(), phymib_.getBasicModulationScheme());
	pifs_ = sifs_ + phymib_.getSlotTime();
	cw_ = macmib_.getCWMin();
	bkmgr.setSlotTime(phymib_.getSlotTime());
	csmgr.setIFS(eifs_);
	sta_seqno_ = 1;
	cache_ = 0;
	cache_node_count_ = 0;
	txConfirmCallback_ = Callback_TXC;
}

/**
 * @brief Handles custom command requests for the Mac802_11EvalVSN object.
 *
 * This method processes Tcl commands passed to the object, allowing dynamic configuration
 * of internal parameters and associations. Supported commands include:
 * - "log-target": Sets the logging target object.
 * - "nodes": Initializes the internal cache for a specified number of nodes.
 * - "setPhy": Associates a WirelessPhyEvalVSN physical layer object.
 *
 * @param argc The number of arguments in the command.
 * @param argv The array of argument strings.
 * @return TCL_OK on success, TCL_ERROR on failure, or the result of the base class command handler.
 */
int Mac802_11EvalVSN::command(int argc, const char *const *argv)
{
	if (argc == 3)
	{
		if (strcmp(argv[1], "log-target") == 0)
		{
			logtarget_ = (NsObject *)TclObject::lookup(argv[2]);
			if (logtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		else if (strcmp(argv[1], "nodes") == 0)
		{
			if (cache_)
				return TCL_ERROR;
			cache_node_count_ = atoi(argv[2]);
			cache_ = new HostEvalVSN[cache_node_count_ + 1];
			assert(cache_);
			bzero(cache_, sizeof(HostEvalVSN) * (cache_node_count_ + 1));
			return TCL_OK;
		}
		else if (strcmp(argv[1], "setPhy") == 0)
		{
			phy_ = (WirelessPhyEvalVSN *)TclObject::lookup(argv[2]);

			if (phy_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
	}
	return Mac::command(argc, argv);
}

/* ======================================================================
 Debugging Routines
 ====================================================================== */
void Mac802_11EvalVSN::trace_pkt(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	u_int16_t *t = (u_int16_t *)&dh->dh_fc;

	fprintf(stderr, "\t[ %2x %2x %2x %2x ] %x %s %d\n",
			*t, dh->dh_duration,
			ETHER_ADDR(dh->dh_ra), ETHER_ADDR(dh->dh_ta),
			index_, packet_info.name(ch->ptype()), ch->size());
}

void Mac802_11EvalVSN::dump(char *)
{
}

void Mac802_11EvalVSN::log(char *event, char *additional)
{
	if (MAC_DBG)
		cout << "L " << Scheduler::instance().clock() << " " << index_ << " " << "MAC" << " " << event << " " << additional
			 << endl;
}

/* ======================================================================
 Packet Headers Routines
 ====================================================================== */
inline int Mac802_11EvalVSN::hdr_dst(char *hdr, int dst)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11 *)hdr;

	if (dst > -2)
		STORE4BYTE(&dst, (dh->dh_ra));

	return ETHER_ADDR(dh->dh_ra);
}

inline int Mac802_11EvalVSN::hdr_src(char *hdr, int src)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11 *)hdr;
	if (src > -2)
		STORE4BYTE(&src, (dh->dh_ta));
	return ETHER_ADDR(dh->dh_ta);
}

inline int Mac802_11EvalVSN::hdr_type(char *hdr, u_int16_t type)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11 *)hdr;
	if (type)
		STORE2BYTE(&type, (dh->dh_body));
	return GET2BYTE(dh->dh_body);
}

/* ======================================================================
 Misc Routines
 ====================================================================== */

void Mac802_11EvalVSN::discard(Packet *p, char *why)
{
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	hdr_cmn *ch = HDR_CMN(p);

	if (MAC_DBG)
	{
		char msg[1000];

		if (ch->ptype() == PT_VIDEO)
		{
			sprintf(msg, "Drop a video packet: VIDEO - PacketType=%s, UID=%d, Size=%d, SeqNb=%d, FrameType=%c, PrioNb=%d, Reason=%s",
					packet_info.name(ch->ptype()),
					ch->uid(),
					ch->size(),
					ch->seqNb_,
					ch->frameType_[0],
					ch->prioNb_,
					why);
		}
		else
		{
			sprintf(msg, "Drop a control packet: NON-VIDEO - PacketType=%s, UID=%d, Size=%d, Reason=%s",
					packet_info.name(ch->ptype()),
					ch->uid(),
					ch->size(),
					why);
		}

		log("Discard", msg);
	}

	/* if the rcvd pkt contains errors, a real MAC layer couldn't
	 necessarily read any data from it, so we just toss it now */
	if (ch->error() != 0)
	{
		Packet::free(p);
		// p = 0;
		return;
	}

	switch (mh->dh_fc.fc_type)
	{
	case MAC_Type_Management:
		drop(p, phy_->dropReason(p, why));
		return;
	case MAC_Type_Control:
		switch (mh->dh_fc.fc_subtype)
		{
		case MAC_Subtype_RTS:
			if ((u_int32_t)ETHER_ADDR(mh->dh_ta) == (u_int32_t)index_)
			{
				drop(p, phy_->dropReason(p, why));
				return;
			}
			/* fall through - if necessary */
		case MAC_Subtype_CTS:
		case MAC_Subtype_ACK:
			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)index_)
			{
				drop(p, phy_->dropReason(p, why));
				return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Control subtype\n");
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch (mh->dh_fc.fc_subtype)
		{
		case MAC_Subtype_Data:
			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)index_ || (u_int32_t)ETHER_ADDR(mh->dh_ta) == (u_int32_t)index_ || (u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST)
			{
				drop(p, phy_->dropReason(p, why)); // check! drop does not work
				// Packet::free(p);
				return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Data subtype\n");
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "invalid MAC type (%x)\n", mh->dh_fc.fc_type);
		trace_pkt(p);
		exit(1);
	}

	Packet::free(p);
}

/*new code starts here*/

/*--------------1.0 Channel State module-------------------------------*/
/*----------------IFSTimerEvalVSN NAVTimerEvalVSN and ChannelStateMgrEvalVSN----------------*/
/*---------------------------------------------------------------------*/

// IFSTimerEvalVSN
void IFSTimerEvalVSN::expire(Event *)
{
	csmgr->handleIFSTimer();
}

// NAVTimerEvalVSN
void NAVTimerEvalVSN::expire(Event *)
{
	csmgr->handleNAVTimer();
}

// ChannelStateMgrEvalVSN
ChannelStateMgrEvalVSN::ChannelStateMgrEvalVSN(Mac802_11EvalVSN *m)
{
	mac_ = m;
	channel_state_ = noCSnoNAV;
	ifs_value_ = -1.0;
	ifs_value_ = mac_->eifs_;
	ifsTimer_ = new IFSTimerEvalVSN(this);
	navTimer_ = new NAVTimerEvalVSN(this);
}

void ChannelStateMgrEvalVSN::setChannelState(ChannelState newState)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%d -> %d", channel_state_, newState);
		mac_->log("ChState", msg);
	}
	channel_state_ = newState;
}

void ChannelStateMgrEvalVSN::setIFS(double tifs)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%f", tifs);
		mac_->log("ChSetIFS", msg);
	}
	ifs_value_ = tifs;
	// the following line of codes is used to solved the race conditon of event scheduling
	// IDleIndication is handled before RXEndIndication
	// therefore the IFSTimerEvalVSN is started before the new ifs_value_ is set in RXEndIndication
	if (ifsTimer_->status() == TIMER_PENDING) // ifsTimer has been scheduled
	{
		ifsTimer_->cancel();
		ifsTimer_->sched(ifs_value_);
	}
	// end of this change
}

void ChannelStateMgrEvalVSN::handlePHYBusyIndication()
{
	switch (channel_state_)
	{
	case noCSnoNAV:
		mac_->bkmgr.handleCSBUSY();
		setChannelState(CSnoNAV);
		break;
	case noCSNAV:
		setChannelState(CSNAV);
		break;
	case WIFS:
		if (ifsTimer_->status() == TIMER_PENDING)
			ifsTimer_->cancel();
		setChannelState(CSnoNAV);
		break;
	default:
		//  the busy indication from phy will be ignored if the channel statemgr is already in CS_BUSY
		break;
	}
}

void ChannelStateMgrEvalVSN::handlePHYIdleIndication()
{
	switch (channel_state_)
	{
	case CSnoNAV:
		ifsTimer_->sched(ifs_value_);
		setChannelState(WIFS);
		break;
	case CSNAV:
		setChannelState(noCSNAV);
		break;
	default:
		// the idle indication from phy will be ignored if the channel statemgr is already CS free
		break;
	}
}

void ChannelStateMgrEvalVSN::handleSetNAV(double t)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%f", t);
		mac_->log("ChSetNAV", msg);
	}
	switch (channel_state_)
	{
	case noCSnoNAV:
		navTimer_->update_and_sched(t);
		mac_->bkmgr.handleCSBUSY();
		setChannelState(noCSNAV);
		break;
	case CSnoNAV:
		navTimer_->update_and_sched(t);
		setChannelState(CSNAV);
		break;
	case noCSNAV:
	case CSNAV:
		if (t > navTimer_->remaining())
		{
			navTimer_->cancel();
			navTimer_->update_and_sched(t);
		}
		break;
	case WIFS:
		navTimer_->update_and_sched(t);
		if (ifsTimer_->status() == TIMER_PENDING)
			ifsTimer_->cancel();
		setChannelState(noCSNAV);
		// needs further attention
		// cout<<"logic error at ChannelStateMgrEvalVSN, event handleSetNAV"<<endl;
		// exit(-1);
		break;
	default:
		cout << "logic error at ChannelStateMgrEvalVSN, event handleSetNAV" << endl;
		exit(-1);
	}
}

void ChannelStateMgrEvalVSN::handleIFSTimer()
{
	if (channel_state_ == WIFS)
	{
		// chi changed the order
		setChannelState(noCSnoNAV);
		mac_->bkmgr.handleCSIDLE();
	}
	else
	{
		cout << "logic error at ChannelStateMgrEvalVSN, event handleIFSTimer" << endl;
		exit(-1);
	}
}

void ChannelStateMgrEvalVSN::handleNAVTimer()
{
	switch (channel_state_)
	{
	case noCSNAV:
		ifsTimer_->sched(ifs_value_);
		setChannelState(WIFS);
		break;
	case CSNAV:
		setChannelState(CSnoNAV);
		break;
	default:
		cout << "logic error at ChannelStateMgrEvalVSN, event handleNAVTimer" << endl;
		exit(-1);
	}
}
/*---------------2.0 Backoff Module----------------*/
/*--- BackoffTimer_tEvalVSN and BackoffMgrEvalVSN -----------*/
/*---------------------------------------------*/
/* BackoffTimer_tEvalVSN */

BackoffTimer_tEvalVSN::BackoffTimer_tEvalVSN(BackoffMgrEvalVSN *b)
{
	bkmgr_ = b;
	tSlot = -1.0; // the real initialization is in int()
	startTime = -1.0;
	remainingSlots_ = -1;
}

void BackoffTimer_tEvalVSN::setSlotTime(double t)
{
	tSlot = t;
}

int BackoffTimer_tEvalVSN::init(int CW)
{
	remainingSlots_ = Random::integer(CW + 1); // choose a number within [0,CW]
	startTime = -1.0;
	return remainingSlots_;
}

void BackoffTimer_tEvalVSN::pause()
{
	if (startTime == -1.0)
	{
		cout << "logic error at BackoffTimer_tEvalVSN, event pause" << endl;
		exit(-1);
	}
	remainingSlots_ -= (int)(floor((Scheduler::instance().clock() - startTime) / tSlot));
	if (status() == TIMER_PENDING)
		cancel();
	startTime = -1.0;
}

void BackoffTimer_tEvalVSN::run()
{
	// chi added for testing remainging slots
	if (remainingSlots_ == 0)
	{
		remainingSlots_ = -1;
		bkmgr_->handleBackoffTimer();
	}
	else
	{
		startTime = Scheduler::instance().clock();
		sched(remainingSlots_ * tSlot);
	}
}

void BackoffTimer_tEvalVSN::expire(Event *)
{
	remainingSlots_ = -1;
	bkmgr_->handleBackoffTimer();
	return;
}

BackoffMgrEvalVSN::BackoffMgrEvalVSN(Mac802_11EvalVSN *m)
{
	mac_ = m;
	bk_state_ = noBackoff; // needs further attention
	bkTimer_ = new BackoffTimer_tEvalVSN(this);
}

void BackoffMgrEvalVSN::setSlotTime(double t)
{
	bkTimer_->setSlotTime(t);
}

void BackoffMgrEvalVSN::setBackoffMgrState(BackoffMgrState newState)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%d -> %d, rslots: %d", bk_state_, newState,
				bkTimer_->remainingSlots_);
		mac_->log("BkState", msg);
	}
	bk_state_ = newState;
}

void BackoffMgrEvalVSN::handleCSIDLE()
{
	switch (bk_state_)
	{
	case BackoffPause:
		setBackoffMgrState(BackoffRunning);
		bkTimer_->run();
		break;
	case noBackoff:
		// ignore this signal
		break;
	default:
		cout << "logic error at BackoffMgrEvalVSN, event handleCSIDLE" << endl;
		exit(-1);
	}
}

void BackoffMgrEvalVSN::handleCSBUSY()
{
	switch (bk_state_)
	{
	case noBackoff:
		// ignore this signal
		break;
	case BackoffPause:
		// ignore this signal
		break;
	case BackoffRunning:

		setBackoffMgrState(BackoffPause);
		bkTimer_->pause();
		break;
	default:
		cout << "logic error at BackoffMgrEvalVSN, event handleCSBUSY" << endl;
		exit(-1);
	}
}

void BackoffMgrEvalVSN::handleBKStart(int CW)
{
	switch (bk_state_)
	{
	case noBackoff:
		// Chi changed

		if (mac_->csmgr.getChannelState() == noCSnoNAV)
		{
			if (bkTimer_->init(CW) == 0)
			{
				setBackoffMgrState(noBackoff);
				BKDone();
			}
			else
			{
				setBackoffMgrState(BackoffRunning);
				bkTimer_->run();
			}
		}
		else
		{

			bkTimer_->init(CW);
			setBackoffMgrState(BackoffPause);
		}
		break;
	case BackoffRunning:
		// ignore this signal
		break;
	case BackoffPause:
		// ignore this signal
		break;
	default:
		cout << "logic error at BackoffMgrEvalVSN, event handleBKStart" << endl;
		exit(-1);
	}
}

void BackoffMgrEvalVSN::handleBackoffTimer()
{
	switch (bk_state_)
	{
	case BackoffRunning:

		setBackoffMgrState(noBackoff);
		BKDone();
		break;
	default:
		cout << "logic error at BackoffMgrEvalVSN, event handleBackoffTimer" << endl;
		exit(-1);
	}
}

void BackoffMgrEvalVSN::BKDone()
{
	mac_->handleBKDone();
}

/*----------------------------------------------------*/
/*-------------3.0 reception module-------------------*/
/*---------------PHYBusyIndication PHYIdleIndication--*/
/*---------------RXStartIndication RXEndIndication----*/

void Mac802_11EvalVSN::handlePHYBusyIndication()
{
	if (MAC_DBG)
		log("PHYBusyInd", "");
	csmgr.handlePHYBusyIndication();
}

void Mac802_11EvalVSN::handlePHYIdleIndication()
{
	if (MAC_DBG)
		log("PHYIdleInd", "");
	csmgr.handlePHYIdleIndication();
}

void Mac802_11EvalVSN::handleRXStartIndication()
{
	if (MAC_DBG)
		log("RxStartInd", "");
}

void Mac802_11EvalVSN::handleRXEndIndication(Packet *pktRx_)
{
	if (!pktRx_)
	{
		if (MAC_DBG)
			log("RxEndInd", "FAIL");
		csmgr.setIFS(eifs_);
		return;
	}
	if (MAC_DBG)
		log("RxEndInd", "SUCC");

	hdr_mac802_11 *mh = HDR_MAC802_11(pktRx_);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	u_int8_t type = mh->dh_fc.fc_type;
	u_int8_t subtype = mh->dh_fc.fc_subtype;
	csmgr.setIFS(difs_);

	if (tap_ && type == MAC_Type_Data && MAC_Subtype_Data == subtype)
		tap_->tap(pktRx_);
	/*
	 * Address Filtering
	 */
	if (dst == (u_int32_t)index_ || dst == MAC_BROADCAST)
	{
		rxc.handleMsgFromBelow(pktRx_);
	}
	else
	{
		/*
		 *  We don't want to log this event, so we just free
		 *  the packet instead of calling the drop routine.
		 */
		if (MAC_DBG)
			log("RxEndInd", "NAV");

		csmgr.handleSetNAV(sec(mh->dh_duration));
		discard(pktRx_, phy_->dropReason(pktRx_, DROP_MAC_NAV));
		return;
	}
}

//-------------------4.0 transmission module----------------//
//---------------------TxTimer and MAC handlers-------------//

// this function is used by transmission module
void Mac802_11EvalVSN::transmit(Packet *p, TXConfirmCallback callback)
{
	hdr_cmn *ch = HDR_CMN(p);
	txConfirmCallback_ = callback;

	// check memory leak?
	if (MAC_DBG)
		log("Tx", "Start");

	downtarget_->recv(p->copy(), this);
}

void Mac802_11EvalVSN::handleTXEndIndication()
{
	if (MAC_DBG)
		log("Tx", "Fin");
	switch (txConfirmCallback_)
	{
	case Callback_TXC:
		txc.handleTXConfirm();
		break;
	case Callback_RXC:
		rxc.handleTXConfirm();
		break;
	default:
		cout << "logic error at handleTXTimer" << endl;
	}
}

/*-------------------5.0 protocol coodination module----------
 ---------------------5.1 tx coordination----------------------
 ---------------------TxTimeout and MAC handlers-------------*/

// this function is used by tx coordination to prepare the MAC frame in detail
void Mac802_11EvalVSN::handleBKDone()
{
	txc.handleBKDone();
}

// this function is used to handle the packets from the upper layer
void Mac802_11EvalVSN::recv(Packet *p, Handler *h)
{
	struct hdr_cmn *hdr = HDR_CMN(p);
	/*
	 * Sanity Check
	 */
	assert(initialized());

	if (hdr->direction() == hdr_cmn::DOWN)
	{
		if (MAC_DBG)
			log("FROM_IFQ", "");
		//			send(p, h); replaced by txc
		callback_ = h;
		txc.handleMsgFromUp(p);
		return;
	}
	else
	{
		// the reception of a packet is moved to handleRXStart/EndIndication
		cout << "logic error at recv, event sendUP" << endl;
	}
}

//-------------------5.2 rx coordination----------------------------------//
//------------------------------------------------------------------------//
/**
 * @brief Handles the reception of a DATA frame in the 802.11 MAC layer.
 *
 * This function processes incoming DATA packets, performing duplicate detection,
 * PHY metric adjustment, and forwarding the packet up the protocol stack.
 *
 * Steps performed:
 * - Extracts MAC and common headers from the packet.
 * - Adjusts the packet size to exclude the PHY header.
 * - Increments the forward count for the packet.
 * - If a PHY layer object is present, updates PHY metrics for the packet.
 * - For unicast packets, checks for duplicates using a per-source cache:
 *   - If the packet is a duplicate (same sequence number as last seen from source), discards it.
 *   - Otherwise, updates the cache with the new sequence number.
 *   - Logs a warning if the source index is out of cache bounds (limited to 10 times).
 * - Logs the packet forwarding event if debugging is enabled.
 * - Forwards the packet to the upper layer via the uptarget_ handler.
 *
 * @param p Pointer to the received Packet.
 */
void Mac802_11EvalVSN::recvDATA(Packet *p)
{
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	struct hdr_cmn *ch = HDR_CMN(p);

	u_int32_t dst = ETHER_ADDR(dh->dh_ra);
	u_int32_t src = ETHER_ADDR(dh->dh_ta);

	ch->size() -= phymib_.getHdrLen11();
	ch->num_forwards() += 1;

	if (phy_)
	{
		// ----- PHY->MAC->RP -----
		phy_->adjustPhyMetrics(p);
	}

	if (dst != MAC_BROADCAST)
	{
		if (src < (u_int32_t)cache_node_count_)
		{
			HostEvalVSN *h = &cache_[src];

			if (h->seqno && h->seqno == dh->dh_scontrol)
			{
				if (MAC_DBG)
					log("DUPLICATE", "");
				discard(p, DROP_MAC_DUPLICATE);
				return;
			}
			h->seqno = dh->dh_scontrol;
		}
		else
		{
			if (MAC_DBG)
			{
				static int count = 0;
				if (++count <= 10)
				{
					log("DUPLICATE", "Accessing MAC CacheArray out of range");
					if (count == 10)
						log("DUPLICATE", "Suppressing additional MAC Cache warnings");
				}
			}
		}
	}

	if (MAC_DBG)
		log("SEND_UP", "");

	// if (tap_ && type == MAC_Type_Data && MAC_Subtype_Data == subtype)
	uptarget_->recv(p, (Handler *)0);
}

//-------------------6.0 txtime----------------------------------//
//------------------------------------------------------------------------//

/*
 * txtime()	- pluck the precomputed tx time from the packet header
 */
double Mac802_11EvalVSN::txtime(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	double t = ch->txtime();

	if (t < 0.0)
	{
		drop(p, phy_->dropReason(p, DROP_MAC_TXTIME_ERR));
		exit(1);
	}

	return t;
}

/*
 * txtime()	- calculate tx time for packet of size "psz" bytes
 *		  at rate "drt" bps
 */
double Mac802_11EvalVSN::txtime(double psz, double drt)
{
	int datalen = (int)psz << 3;
	double t = phymib_.getHeaderDuration() + (double)datalen / drt;
	return (t);
}

/*
 * txtime()	- calculate tx time for packet of size "psz" bytes
 *		  with modulation scheme mod_scheme
 *                this is the function used in reality
 */
double Mac802_11EvalVSN::txtime(double psz, int mod_scheme)
{
	int datalen = (int)psz << 3;
	int DBPS = modulation_tableEvalVSN[mod_scheme].NDBPS;

	// 802.11p
	if (phymib_.use_802_11a())
	{
		datalen = datalen + 16 + 6; // 16 SYMBOL BITS, 6 TAIL BITS
	}
	int symbols = (int)(ceil((double)datalen / DBPS)); // PADDING BITS, FILL SYMBOl

	double t = phymib_.getHeaderDuration() + (double)symbols * phymib_.getSymbolDuration();
	return (t);
}

/*new code ends here*/

//-------tx coordination function-------//
void TXC_CTSTimerEvalVSN::expire(Event *)
{
	txc_->handleTCTStimeout();
}

void TXC_SIFSTimerEvalVSN::expire(Event *)
{
	txc_->handleSIFStimeout();
}

void TXC_ACKTimerEvalVSN::expire(Event *)
{
	txc_->handleTACKtimeout();
}

TXCEvalVSN::TXCEvalVSN(Mac802_11EvalVSN *m) : txcCTSTimer(this), txcSIFSTimer(this), txcACKTimer(this)
{
	mac_ = m;
	pRTS = 0;
	pDATA = 0;
	txc_state_ = TXC_Idle;
	shortretrycounter = 0;
	longretrycounter = 0;
	//	rtsretrycounter=0;
	//	dataretrycounter=0;
}

void TXCEvalVSN::handleMsgFromUp(Packet *p)
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "msgFromUp");
	pDATA = p;
	prepareMPDU(pDATA);
	hdr_mac802_11 *mh = HDR_MAC802_11(pDATA);
	struct hdr_cmn *ch = HDR_CMN(pDATA);

	if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST && (unsigned int)(ch->size()) > mac_->macmib_.getRTSThreshold()) // should get the right size //check type
	{
		if (mac_->MAC_DBG)
			mac_->log("TXCEvalVSN", "msgFromUp, RTS/CTS");
		generateRTSFrame(pDATA);
	}
	if (mac_->bkmgr.getBackoffMgrState() == noBackoff)
	{
		if (mac_->csmgr.getChannelState() == noCSnoNAV)
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "msgFromUp, noBackoff, noCSnoNAV");
			if (pRTS != 0)
			{
				mac_->transmit(pRTS, Callback_TXC);
				setTXCState(TXC_wait_RTSsent);
			}
			else
			{
				mac_->transmit(pDATA, Callback_TXC);
				setTXCState(TXC_wait_PDUsent);
			}
		}
		else
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "msgFromUp, Backoff started");
			if (pRTS != 0)
			{
				setTXCState(TXC_RTS_pending);
			}
			else
			{
				setTXCState(TXC_DATA_pending);
			}
			mac_->bkmgr.handleBKStart(mac_->cw_);
		}
	}
	else
	{
		if (mac_->MAC_DBG)
			mac_->log("TXCEvalVSN", "msgFromUp, Backoff pending");
		if (pRTS != 0)
		{
			setTXCState(TXC_RTS_pending);
		}
		else
		{
			setTXCState(TXC_DATA_pending);
		}
	}
}

void TXCEvalVSN::prepareMPDU(Packet *p)
{
	hdr_cmn *ch = HDR_CMN(p);
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	ch->size() += mac_->phymib_.getHdrLen11();

	dh->dh_fc.fc_protocol_version = MAC_ProtocolVersion;
	dh->dh_fc.fc_type = MAC_Type_Data;
	dh->dh_fc.fc_subtype = MAC_Subtype_Data;
	dh->dh_fc.fc_to_ds = 0;
	dh->dh_fc.fc_from_ds = 0;
	dh->dh_fc.fc_more_frag = 0;
	dh->dh_fc.fc_retry = 0;
	dh->dh_fc.fc_pwr_mgt = 0;
	dh->dh_fc.fc_more_data = 0;
	dh->dh_fc.fc_wep = 0;
	dh->dh_fc.fc_order = 0;
	dh->dh_scontrol = mac_->sta_seqno_++;

	ch->txtime() = mac_->txtime(ch->size(), ch->mod_scheme_);

	if ((u_int32_t)ETHER_ADDR(dh->dh_ra) != MAC_BROADCAST)
	{
		dh->dh_duration = mac_->usec(mac_->txtime(mac_->phymib_.getACKlen(),
												  mac_->phymib_.getBasicModulationScheme()) +
									 mac_->macmib_.getSIFS());
	}
	else
	{
		dh->dh_duration = 0;
	}
}

void TXCEvalVSN::handleBKDone()
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "BK Done");
	switch (txc_state_)
	{
	case TXC_RTS_pending:
		mac_->transmit(pRTS, Callback_TXC);
		setTXCState(TXC_wait_RTSsent);
		break;
	case TXC_DATA_pending:
		mac_->transmit(pDATA, Callback_TXC);
		setTXCState(TXC_wait_PDUsent);
		break;
	case TXC_Idle: // check : not in the SDL
	case TXC_wait_CTS:
	case TXC_wait_ACK:
		break;
	default:
		cout << txc_state_ << endl;
		cout << "logic error at TXCEvalVSN handling BKDone" << endl;
	}
}

void TXCEvalVSN::handleTXConfirm()
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "TX Confirm");
	hdr_mac802_11 *mh = HDR_MAC802_11(pDATA);
	switch (txc_state_)
	{
	case TXC_wait_PDUsent:
		if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST)
		{
			txcACKTimer.sched(mac_->macmib_.getSIFS() + mac_->txtime(mac_->phymib_.getACKlen(), mac_->phymib_.getBasicModulationScheme()) + DSSS_MaxPropagationDelay);
			setTXCState(TXC_wait_ACK);
		}
		else
		{
			Packet::free(pDATA);
			pDATA = 0;
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		}
		break;
	case TXC_wait_RTSsent:
		txcCTSTimer.sched(mac_->macmib_.getSIFS() + mac_->txtime(mac_->phymib_.getCTSlen(), mac_->phymib_.getBasicModulationScheme()) + DSSS_MaxPropagationDelay);
		setTXCState(TXC_wait_CTS);
		break;
	default:
		cout << "logic error at TXCEvalVSN handling txconfirm" << endl;
	}
}

void TXCEvalVSN::checkQueue()
{
	setTXCState(TXC_Idle);
	if (mac_->callback_)
	{
		if (mac_->MAC_DBG)
			mac_->log("TXCEvalVSN", "checkQueue, empty");
		Handler *h = mac_->callback_;
		mac_->callback_ = 0;
		h->handle((Event *)0);
	}
	else
	{
		if (mac_->MAC_DBG)
			mac_->log("TXCEvalVSN", "checkQueue, pending");
	}
}

void TXCEvalVSN::handleCTSIndication()
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "CTS indication");
	if (txc_state_ == TXC_wait_CTS)
	{
		Packet::free(pRTS);
		pRTS = 0;
		txcCTSTimer.cancel();
		shortretrycounter = 0;
		setTXCState(TXC_wait_SIFS);
		txcSIFSTimer.sched(mac_->macmib_.getSIFS());
	}
	else
	{
		cout << "txc_state_" << txc_state_ << endl;
		cout << "logic error at TXCEvalVSN handling CTS indication" << endl;
	}
}

void TXCEvalVSN::handleTCTStimeout()
{
	if (txc_state_ == TXC_wait_CTS)
	{
		shortretrycounter++;
		mac_->inc_cw();
		if (shortretrycounter >= mac_->macmib_.getShortRetryLimit())
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "CTS timeout, limit reached");
			mac_->discard(pRTS, DROP_MAC_RETRY_COUNT_EXCEEDED);
			pRTS = 0;

			// higher layers feedback support
			struct hdr_cmn *ch = HDR_CMN(pDATA);
			if (ch->xmit_failure_)
			{
				ch->xmit_reason_ = XMIT_REASON_RTS;
				ch->xmit_failure_(pDATA->copy(), ch->xmit_failure_data_);
			}
			mac_->discard(pDATA, DROP_MAC_RETRY_COUNT_EXCEEDED);
			pDATA = 0;

			shortretrycounter = 0;
			longretrycounter = 0;
			mac_->cw_ = mac_->macmib_.getCWMin();
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		}
		else
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "CTS timeout, retry");
			setTXCState(TXC_RTS_pending);
			mac_->bkmgr.handleBKStart(mac_->cw_);
		}
	}
	else
		cout << "logic error at TXCEvalVSN handleTCTS timeout" << endl;
}

void TXCEvalVSN::handleSIFStimeout()
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "SIFS finished");
	if (txc_state_ == TXC_wait_SIFS)
	{
		mac_->transmit(pDATA, Callback_TXC);
		setTXCState(TXC_wait_PDUsent);
	}
	else
		cout << "logic error at TXCEvalVSN handling SIFS timeout" << endl;
}

void TXCEvalVSN::handleTACKtimeout()
{
	if (txc_state_ == TXC_wait_ACK)
	{
		hdr_mac802_11 *mh = HDR_MAC802_11(pDATA);
		struct hdr_cmn *ch = HDR_CMN(pDATA);

		unsigned int limit = 0;
		unsigned int *counter;
		if ((unsigned int)ch->size() <= mac_->macmib_.getRTSThreshold())
		{
			limit = mac_->macmib_.getShortRetryLimit();
			counter = &shortretrycounter;
		}
		else
		{
			limit = mac_->macmib_.getLongRetryLimit();
			counter = &longretrycounter;
		}
		(*counter)++;

		mac_->inc_cw();

		if (*counter >= limit)
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "ACK timeout, limit reached");

			// higher layers feedback support
			struct hdr_cmn *ch = HDR_CMN(pDATA);
			if (ch->xmit_failure_)
			{
				ch->xmit_reason_ = XMIT_REASON_RTS;
				ch->xmit_failure_(pDATA->copy(), ch->xmit_failure_data_);
			}

			mac_->discard(pDATA, DROP_MAC_RETRY_COUNT_EXCEEDED);
			pDATA = 0;

			shortretrycounter = 0;
			longretrycounter = 0;
			mac_->cw_ = mac_->macmib_.getCWMin();
			mac_->bkmgr.handleBKStart(mac_->cw_);
			checkQueue();
		}
		else
		{
			if (mac_->MAC_DBG)
				mac_->log("TXCEvalVSN", "ACK timeout, retry");

			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST && (unsigned int)(ch->size()) > mac_->macmib_.getRTSThreshold())
			{
				generateRTSFrame(pDATA);
				setTXCState(TXC_RTS_pending);
			}
			else
			{
				setTXCState(TXC_DATA_pending);
			}

			mac_->bkmgr.handleBKStart(mac_->cw_);
		}
	}
	else
		cout << "logic error at TXCEvalVSN handling TACK timeout" << endl;
}

void TXCEvalVSN::generateRTSFrame(Packet *p)
{
	Packet *rts = Packet::alloc();
	hdr_cmn *ch = HDR_CMN(rts);
	struct rts_frame *rf = (struct rts_frame *)rts->access(hdr_mac::offset_);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getRTSlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->mod_scheme_ = (ModulationScheme)mac_->phymib_.getBasicModulationScheme();
	bzero(rf, MAC_HDR_LEN);

	rf->rf_fc.fc_protocol_version = MAC_ProtocolVersion;
	rf->rf_fc.fc_type = MAC_Type_Control;
	rf->rf_fc.fc_subtype = MAC_Subtype_RTS;
	rf->rf_fc.fc_to_ds = 0;
	rf->rf_fc.fc_from_ds = 0;
	rf->rf_fc.fc_more_frag = 0;
	rf->rf_fc.fc_retry = 0;
	rf->rf_fc.fc_pwr_mgt = 0;
	rf->rf_fc.fc_more_data = 0;
	rf->rf_fc.fc_wep = 0;
	rf->rf_fc.fc_order = 0;

	struct hdr_mac802_11 *mh = HDR_MAC802_11(p);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	STORE4BYTE(&dst, (rf->rf_ra));
	STORE4BYTE(&(mac_->index_), (rf->rf_ta));

	ch->txtime() = mac_->txtime(ch->size(), ch->mod_scheme_);
	rf->rf_duration = mac_->usec(mac_->macmib_.getSIFS() + mac_->txtime(mac_->phymib_.getCTSlen(), mac_->phymib_.getBasicModulationScheme()) + mac_->macmib_.getSIFS() + mac_->txtime(p) + mac_->macmib_.getSIFS() + mac_->txtime(mac_->phymib_.getACKlen(), mac_->phymib_.getBasicModulationScheme()));
	pRTS = rts;
}

void TXCEvalVSN::handleACKIndication()
{
	if (mac_->MAC_DBG)
		mac_->log("TXCEvalVSN", "ACK indication");
	if (txc_state_ == TXC_wait_ACK)
	{
		txcACKTimer.cancel();
		shortretrycounter = 0;
		longretrycounter = 0;
		Packet::free(pDATA);
		pDATA = 0;
		mac_->cw_ = mac_->macmib_.getCWMin();
		mac_->bkmgr.handleBKStart(mac_->cw_);
		checkQueue();
	}
	else
	{
		cout << "logic error at TXCEvalVSN handling ACKIndication" << endl;
		cout << txc_state_ << endl;
	}
}

void TXCEvalVSN::setTXCState(TXCState newstate)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%d -> %d", txc_state_, newstate);
		if (mac_->MAC_DBG)
			mac_->log("TXCState", msg);
	}
	txc_state_ = newstate;
}

//-------tx coordination-------//

//-------rx coordination-------//

void RXC_SIFSTimerEvalVSN::expire(Event *)
{
	rxc_->handleSIFStimeout();
}

RXCEvalVSN::RXCEvalVSN(Mac802_11EvalVSN *m) : rxcSIFSTimer_(this)
{
	mac_ = m;
	pCTRL = 0;
	rxc_state_ = RXC_Idle;
}

void RXCEvalVSN::handleMsgFromBelow(Packet *p)
{
	hdr_mac802_11 *mh = HDR_MAC802_11(p);

	switch (mh->dh_fc.fc_type)
	{

	case MAC_Type_Control:
		switch (mh->dh_fc.fc_subtype)
		{
		case MAC_Subtype_ACK:
			if (mac_->MAC_DBG)
				mac_->log("RXCEvalVSN", "msgFromBelow: ACK");
			mac_->txc.handleACKIndication();
			mac_->mac_log(p);
			setRXCState(RXC_Idle);
			break;

		case MAC_Subtype_RTS:
			if (mac_->MAC_DBG)
				mac_->log("RXCEvalVSN", "msgFromBelow: RTS");
			if (mac_->csmgr.getChannelState() == CSNAV || mac_->csmgr.getChannelState() == noCSNAV)
			{
				setRXCState(RXC_Idle);
				mac_->mac_log(p);
			}
			else
			{
				generateCTSFrame(p);
				mac_->mac_log(p);
				rxcSIFSTimer_.sched(mac_->macmib_.getSIFS());
				setRXCState(RXC_wait_SIFS);
			}
			break;
		case MAC_Subtype_CTS:
			if (mac_->MAC_DBG)
				mac_->log("RXCEvalVSN", "msgFromBelow: CTS");
			mac_->txc.handleCTSIndication();
			mac_->mac_log(p);
			setRXCState(RXC_Idle);
			break;
		default:
			//
			;
		}
		break;

	case MAC_Type_Data:
		if (mac_->MAC_DBG)
			mac_->log("RXCEvalVSN", "msgFromBelow: DATA");
		switch (mh->dh_fc.fc_subtype)
		{
		case MAC_Subtype_Data:
			if ((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)(mac_->index_))
			{
				generateACKFrame(p);
				rxcSIFSTimer_.sched(mac_->macmib_.getSIFS());
				mac_->recvDATA(p);
				setRXCState(RXC_wait_SIFS);
			}
			else
			{
				mac_->recvDATA(p);
				setRXCState(RXC_Idle);
			}
			break;
		default:
			cout << "unkown mac data type" << endl;
		}
		break;
	default:
		cout << "logic error at handleincomingframe" << endl;
	}
}

void RXCEvalVSN::handleSIFStimeout()
{
	if (mac_->MAC_DBG)
		mac_->log("RXCEvalVSN", "SIFS finished");
	if (pCTRL != 0)
	{
		mac_->transmit(pCTRL, Callback_RXC);
	}
	setRXCState(RXC_wait_sent);
}

void RXCEvalVSN::handleTXConfirm()
{
	Packet::free(pCTRL);
	pCTRL = 0;
	setRXCState(RXC_Idle);
}

void RXCEvalVSN::generateACKFrame(Packet *p)
{
	Packet *ack = Packet::alloc();
	hdr_cmn *ch = HDR_CMN(ack);
	struct ack_frame *af = (struct ack_frame *)ack->access(hdr_mac::offset_);
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getACKlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->mod_scheme_ = (ModulationScheme)mac_->phymib_.getBasicModulationScheme();
	bzero(af, MAC_HDR_LEN);

	af->af_fc.fc_protocol_version = MAC_ProtocolVersion;
	af->af_fc.fc_type = MAC_Type_Control;
	af->af_fc.fc_subtype = MAC_Subtype_ACK;
	af->af_fc.fc_to_ds = 0;
	af->af_fc.fc_from_ds = 0;
	af->af_fc.fc_more_frag = 0;
	af->af_fc.fc_retry = 0;
	af->af_fc.fc_pwr_mgt = 0;
	af->af_fc.fc_more_data = 0;
	af->af_fc.fc_wep = 0;
	af->af_fc.fc_order = 0;

	u_int32_t dst = ETHER_ADDR(dh->dh_ta);
	STORE4BYTE(&dst, (af->af_ra));
	ch->txtime() = mac_->txtime(ch->size(),
								mac_->phymib_.getBasicModulationScheme());
	af->af_duration = 0;
	pCTRL = ack;
}

void RXCEvalVSN::generateCTSFrame(Packet *p)
{
	Packet *cts = Packet::alloc();
	hdr_cmn *ch = HDR_CMN(cts);
	struct rts_frame *rf = (struct rts_frame *)p->access(hdr_mac::offset_);
	struct cts_frame *cf = (struct cts_frame *)cts->access(hdr_mac::offset_);
	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = mac_->phymib_.getCTSlen();
	ch->iface() = -2;
	ch->error() = 0;
	ch->mod_scheme_ = (ModulationScheme)mac_->phymib_.getBasicModulationScheme();
	bzero(cf, MAC_HDR_LEN);

	cf->cf_fc.fc_protocol_version = MAC_ProtocolVersion;
	cf->cf_fc.fc_type = MAC_Type_Control;
	cf->cf_fc.fc_subtype = MAC_Subtype_CTS;
	cf->cf_fc.fc_to_ds = 0;
	cf->cf_fc.fc_from_ds = 0;
	cf->cf_fc.fc_more_frag = 0;
	cf->cf_fc.fc_retry = 0;
	cf->cf_fc.fc_pwr_mgt = 0;
	cf->cf_fc.fc_more_data = 0;
	cf->cf_fc.fc_wep = 0;
	cf->cf_fc.fc_order = 0;

	u_int32_t dst = ETHER_ADDR(rf->rf_ta);
	STORE4BYTE(&dst, (cf->cf_ra));
	ch->txtime() = mac_->txtime(ch->size(),
								mac_->phymib_.getBasicModulationScheme());
	cf->cf_duration = mac_->usec(mac_->sec(rf->rf_duration) - mac_->macmib_.getSIFS() - mac_->txtime(mac_->phymib_.getCTSlen(), mac_->phymib_.getBasicModulationScheme()));
	pCTRL = cts;
}

void RXCEvalVSN::setRXCState(RXCState newstate)
{
	if (mac_->MAC_DBG)
	{
		char msg[1000];
		sprintf(msg, "%d -> %d", rxc_state_, newstate);
		mac_->log("RXCState", msg);
	}
	rxc_state_ = newstate;
}

double Mac802_11EvalVSN::txtime(int)
{
	/* clobber inherited txtime() */
	abort();
}

//-------rx coordination-------//

// ------- Link Metrics -------
// Mise à jour des métriques de lien pour un voisin donné
// (LQI et SNR cumulés sur le nombre d’échantillons)
/**
 * @brief Updates the link quality metrics (LQI and SNR) for a given neighbor.
 *
 * This function accumulates the Link Quality Indicator (LQI) and Signal-to-Noise Ratio (SNR)
 * values for the specified neighbor address. It maintains running sums and a count of updates,
 * allowing for calculation of average LQI and SNR over time. If debugging is enabled (MAC_DBG),
 * it logs the updated metrics for the neighbor.
 *
 * @param neighbor The address of the neighbor node whose metrics are being updated.
 * @param lqi The Link Quality Indicator value to add for this update.
 * @param snr The Signal-to-Noise Ratio value to add for this update.
 */
void Mac802_11EvalVSN::update_link_metrics(nsaddr_t neighbor, double lqi, double snr)
{
	LinkMetrics &m = link_metrics_[neighbor];
	m.lqi_sum += lqi;
	m.snr_sum += snr;
	m.count++;

	// Debug log
	if (MAC_DBG)
	{
		char msg[256];
		sprintf(msg, "using: neighbor=%d, lqi=%.2f, snr=%.2f, count=%d, avg_lqi=%.2f, avg_snr=%.2f",
				neighbor, lqi, snr, m.count,
				m.lqi_sum / m.count, m.snr_sum / m.count);
		log("update_link_metrics (cross-layer)", msg);
	}
}

// Récupération de la valeur moyenne de LQI pour un voisin donné
/**
 * @brief Calculates the average Link Quality Indicator (LQI) for a given neighbor.
 *
 * This function looks up the link metrics for the specified neighbor address.
 * If no metrics are found or the count is zero, it returns 0.0.
 * Otherwise, it computes and returns the average LQI by dividing the sum of LQI values
 * by the count of received measurements.
 *
 * If debugging is enabled (MAC_DBG), a debug message is logged with the neighbor address
 * and the computed average LQI.
 *
 * @param neighbor The address of the neighbor node whose average LQI is to be calculated.
 * @return The average LQI value for the specified neighbor, or 0.0 if no data is available.
 */
double Mac802_11EvalVSN::get_average_lqi(nsaddr_t neighbor)
{
	map<nsaddr_t, LinkMetrics>::const_iterator it = link_metrics_.find(neighbor);

	if (it == link_metrics_.end() || it->second.count == 0)
		return 0.0;

	double avg = it->second.lqi_sum / it->second.count;

	// Debug log
	if (MAC_DBG)
	{
		char msg[256];
		sprintf(msg, "using: neighbor=%d, avg_lqi=%.2f", neighbor, avg);
		log("get_average_lqi (cross-layer)", msg);
	}

	return avg;
}

// Récupération de la valeur moyenne de SNR pour un voisin donné
/**
 * @brief Calculates the average Signal-to-Noise Ratio (SNR) for a given neighbor.
 *
 * This method looks up the link metrics for the specified neighbor address and computes
 * the average SNR by dividing the accumulated SNR sum by the count of SNR samples.
 * If the neighbor is not found or there are no SNR samples, the function returns 0.0.
 * Optionally logs debug information if MAC_DBG is enabled.
 *
 * @param neighbor The address of the neighbor node whose average SNR is to be calculated.
 * @return The average SNR value for the specified neighbor, or 0.0 if unavailable.
 */
double Mac802_11EvalVSN::get_average_snr(nsaddr_t neighbor)
{
	map<nsaddr_t, LinkMetrics>::const_iterator it = link_metrics_.find(neighbor);

	if (it == link_metrics_.end() || it->second.count == 0)
		return 0.0;

	double avg = it->second.snr_sum / it->second.count;

	// Debug log
	if (MAC_DBG)
	{
		char msg[256];
		sprintf(msg, "using: neighbor=%d, avg_snr=%.2f", neighbor, avg);
		log("get_average_snr (cross-layer)", msg);
	}

	return avg;
}
