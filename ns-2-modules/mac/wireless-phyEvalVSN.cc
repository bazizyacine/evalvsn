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

#include <wireless-phyEvalVSN.h>
#include <mac-802_11EvalVSN.h>

#include <math.h>
#include <functional>
#include <ip.h>
#include <agent.h>
#include <trace.h>
#include <iostream>
#include <ranvar.h>
#include <float.h>
#include <algorithm>
#include <sys/param.h> /* for MIN/MAX */

using namespace std;

/* ======================================================================
 WirelessPhyEvalVSN Interface
 ====================================================================== */

#define NUM_MODULATION_SCHEMES (sizeof(modulation_tableEvalVSN) / sizeof(modulation_tableEvalVSN[0]))

#define DROP_PHY_INT "INT"
#define DROP_PHY_TXB "TXB"
#define DROP_PHY_SXB "SXB"
#define DROP_PHY_PXB "PXB"
#define DROP_PHY_RXB "RXB"
#define DROP_PHY_DND "DND"
#define DROP_PHY_PND "PND"

static class WirelessPhyEvalVSNClass : public TclClass
{
public:
	WirelessPhyEvalVSNClass() : TclClass("Phy/WirelessPhyEvalVSN")
	{
	}
	TclObject *create(int, const char *const *)
	{
		return (new WirelessPhyEvalVSN);
	}
} class_WirelessPhyEvalVSN;

WirelessPhyEvalVSN::WirelessPhyEvalVSN() : WirelessPhy(), tX_Timer(this), rX_Timer(this), preRX_Timer(this)
{
	bind("CPThresh_", &CPThresh_); // not used at the moment, BPSK_SNR_ is used in preamble capture
	bind("RXThresh_", &RXThresh_); // not used at the moment
	bind("CSThresh_", &CSThresh_);
	bind("Pt_", &Pt_);
	bind("freq_", &freq_);
	bind("L_", &L_);
	bind("PreambleCaptureSwitch_", &PreambleCaptureSwitch_);
	bind("DataCaptureSwitch_", &DataCaptureSwitch_);
	bind("SINR_PreambleCapture_", &SINR_PreambleCapture_);
	bind("SINR_DataCapture_", &SINR_DataCapture_);
	bind("HeaderDuration_", &HeaderDuration_);
	bind("PHY_DBG_", &PHY_DBG);
	bind("BasicModulationScheme_", &BasicModulationScheme_);
	bind("trace_dist_", &trace_dist_);
	bind("noise_floor_", &noise_floor_);
	bind("PowerMonitorThresh_", &PowerMonitorThresh_);
	bind("MAX_PRIO_NB", &MAX_PRIO_NB);

	lambda_ = SPEED_OF_LIGHT / freq_;
	node_ = 0;
	ant_ = 0;
	propagation_ = 0;
	powerMonitor = new PowerMonitorEvalVSN(this);

	// channel state
	state = SEARCHINGEvalVSN;
	pkt_RX = 0;
	power_RX = 0;

	HYST_COUNT = 3;
	modHystCount_ = -1;
	lastModScheme_ = -1;
}

int WirelessPhyEvalVSN::command(int argc, const char *const *argv)
{
	TclObject *obj;

	if (argc == 3)
	{
		if (strcasecmp(argv[1], "setMacAddr") == 0)
		{
			return TCL_OK;
		}
		else if (strcmp(argv[1], "propagation") == 0)
		{
			if ((obj = TclObject::lookup(argv[2])) == 0)
			{
				fprintf(stderr, "WirelessPhyEvalVSN: %s lookup of %s failed\n",
						argv[1], argv[2]);
				return TCL_ERROR;
			}

			assert(propagation_ == 0);
			propagation_ = (Propagation *)obj;
			return TCL_OK;
		}
		else if (strcasecmp(argv[1], "antenna") == 0)
		{
			if ((obj = TclObject::lookup(argv[2])) == 0)
			{
				fprintf(stderr, "WirelessPhyEvalVSN: %s lookup of %s failed\n",
						argv[1], argv[2]);
				return TCL_ERROR;
			}

			ant_ = (Antenna *)obj;
			return TCL_OK;
		}
		else if (strcasecmp(argv[1], "node") == 0)
		{
			if ((obj = TclObject::lookup(argv[2])) == 0)
			{
				fprintf(stderr, "WirelessPhyEvalVSN: %s lookup of %s failed\n",
						argv[1], argv[2]);
				return TCL_ERROR;
			}

			assert(node_ == 0);
			node_ = (Node *)obj;
			return TCL_OK;
		}
	}

	return WirelessPhy::command(argc, argv);
}

/**
 * @brief Handles the transmission of a packet from the MAC layer to the physical layer,
 *        managing node energy consumption and state transitions.
 *
 * This function performs the following operations:
 * - Checks if the WirelessPhyEvalVSN object is initialized.
 * - Manages node energy:
 *   - If the node is OFF, frees the packet and returns.
 *   - If the node is ON and sleeping, decrements sleep energy.
 *   - Decrements idle and transmission energy based on actual transmission time.
 *   - Logs the remaining energy after energy updates.
 *   - If energy is depleted, frees the packet and returns.
 * - Handles state transitions:
 *   - Cancels timers and discards packets if in RXing or PreRXing states.
 *   - Validates that the state is one of the four valid states.
 * - Stamps the packet with interface arguments (node, antenna, transmission power, wavelength).
 * - Logs debug information if enabled, including packet details.
 * - Sets the state to transmitting, schedules the transmission timer, records power usage,
 *   and sends the packet to the lower layer.
 *
 * @param p Pointer to the packet to be transmitted.
 */
void WirelessPhyEvalVSN::sendDown(Packet *p)
{
	// Send the packet &  set sender timer
	struct hdr_cmn *cmh = HDR_CMN(p);

	/*
	 * Sanity Check
	 */
	assert(initialized());

	// Gestion complète et réaliste de l'énergie (inspirée de WirelessPhy)
	if (em())
	{
		// Si le noeud est OFF, on libère le paquet et on quitte
		if (!Is_node_on())
		{
			Packet::free(p);
			return;
		}
		// Si le noeud est ON mais en sommeil, décrémenter l'énergie de sommeil
		if (Is_node_on() && Is_sleeping())
		{
			em()->DecrSleepEnergy(NOW - update_energy_time_, P_sleep_);
			update_energy_time_ = NOW;
		}
	}

	// Décrémentation de l'énergie selon le temps de transmission effectif
	if (em())
	{
		if (em()->energy() > 0)
		{
			double txtime = cmh->txtime();
			double start_time = MAX(channel_idle_time_, NOW);
			double end_time = MAX(channel_idle_time_, NOW + txtime);
			double actual_txtime = end_time - start_time;

			// Décrémentation de l'énergie idle si besoin
			if (start_time > update_energy_time_)
			{
				em()->DecrIdleEnergy(start_time - update_energy_time_, P_idle_);
				update_energy_time_ = start_time;
			}

			// Vérification de recouvrement temporel
			double temp = MAX(NOW, last_send_time_);
			double begin_adjust_time = MIN(channel_idle_time_, temp);
			double finish_adjust_time = MIN(channel_idle_time_, NOW + txtime);
			double gap_adjust_time = finish_adjust_time - begin_adjust_time;

			if (gap_adjust_time < 0.0)
				gap_adjust_time = 0.0;

			// Décrémentation de l'énergie de transmission
			em()->DecrTxEnergy(actual_txtime, Pt_consume_);

			// ----------- Log l'énergie restante -----------
			if (em()->energy() > 0)
				((MobileNode *)node_)->log_energy(1);
			else if (em()->energy() <= 0)
				((MobileNode *)node_)->log_energy(0);
			// ----------------------------------------------
		}
		else
		{
			// ----------- Log l'énergie restante -----------
			if (em()->energy() > 0)
				((MobileNode *)node_)->log_energy(1);
			else if (em()->energy() <= 0)
				((MobileNode *)node_)->log_energy(0);
			// ----------------------------------------------

			Packet::free(p);
			return;
		}
	}

	if (PHY_DBG)
		log("SendDown", "");

	switch (state)
	{
	case TXingEvalVSN:
		cout << "wrong in MAC logic case 1" << endl;
		exit(1);
		break;
	case RXingEvalVSN:
		rX_Timer.cancel();
		// case 7
		discard(pkt_RX, power_RX, DROP_PHY_INT);
		pkt_RX = 0;
		power_RX = 0;
		break;
	case PreRXingEvalVSN:
		preRX_Timer.cancel();
		// case 6
		discard(pkt_RX, power_RX, DROP_PHY_INT);
		pkt_RX = 0;
		power_RX = 0;
		break;
	case SEARCHINGEvalVSN:
		break;
	default:
		cout << "Only four states valid" << endl;
		exit(1);
	}

	/*
	 *  Stamp the packet with the interface arguments
	 */
	p->txinfo_.stamp((MobileNode *)node(), ant_->copy(), Pt_, lambda_);

	if (PHY_DBG)
	{
		char msg[1000];

		if (cmh->ptype() == PT_VIDEO)
		{
			sprintf(msg, "Send a video packet: VIDEO - PacketType=%s, UID=%d, Size=%d, SeqNb=%d, FrameType=%c, PrioNb=%d",
					packet_info.name(cmh->ptype()),
					cmh->uid(),
					cmh->size(),
					cmh->seqNb_,
					cmh->frameType_[0],
					cmh->prioNb_);
		}
		else
		{
			sprintf(msg, "Send a control packet: NON-VIDEO - PacketType=%s, UID=%d, Size=%d",
					packet_info.name(cmh->ptype()),
					cmh->uid(),
					cmh->size());
		}

		log("SendDown (cross-layer)", msg);
	}

	setState(TXingEvalVSN);
	tX_Timer.sched(cmh->txtime());
	powerMonitor->recordPowerLevel(Pt_, cmh->txtime());
	downtarget_->recv(p, this);
}

/**
 * @brief Handles the reception of a packet from the physical layer and processes it according to PHY state and energy constraints.
 *
 * This method is responsible for:
 * - Performing sanity checks on initialization and packet validity.
 * - Managing node energy states (ON, OFF, sleeping) and updating energy consumption for sleep and reception.
 * - Calculating received power (Pr) using the propagation model and recording power metrics.
 * - Logging detailed debug information if enabled.
 * - Updating packet header fields with signal quality metrics (SINR, RSSI, SNR, PER, LQI).
 * - Handling PHY state transitions and packet processing based on the current state:
 *   - TXingEvalVSN: Discards the packet as the PHY is transmitting.
 *   - SEARCHINGEvalVSN: Checks if the SINR is sufficient for reception; if so, schedules preamble/header processing.
 *   - PreRXingEvalVSN: Handles preamble capture, including possible preamble capture switching.
 *   - RXingEvalVSN: Handles data capture, including possible data capture switching.
 * - Discards or processes packets as appropriate for each state, updating internal state and timers.
 * - Logs energy usage and state transitions for debugging and analysis.
 *
 * @param p Pointer to the received Packet.
 * @return int Always returns 0; the packet will be freed by the caller (phy.cc).
 */
int WirelessPhyEvalVSN::sendUp(Packet *p)
{
	/*
	 * Sanity Check
	 */

	assert(initialized());
	assert(p);

	// struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	struct hdr_cmn *cmh = HDR_CMN(p);

	PacketStamp s;
	double Pr;

	// Gestion complète et réaliste de l'énergie (inspirée de WirelessPhy)
	if (em())
	{
		// Si le noeud est OFF, on libère le paquet et on quitte
		if (!Is_node_on())
		{
			Packet::free(p);
			return 0;
		}

		// Si le noeud est ON mais en sommeil, décrémenter l'énergie de sommeil
		if (Is_node_on() && Is_sleeping())
		{
			em()->DecrSleepEnergy(NOW - update_energy_time_, P_sleep_);
			update_energy_time_ = NOW;
		}

		// Si l'énergie est à zéro, on libère le paquet et on quitte
		if (em()->energy() <= 0)
		{
			((MobileNode *)node_)->log_energy(0);

			Packet::free(p);
			return 0;
		}
	}

	if (propagation_)
	{
		s.stamp((MobileNode *)node(), ant_, 0, lambda_);

		// pass the packet to RF model for the calculation of Pr
		Pr = propagation_->Pr(&p->txinfo_, &s, this);
		powerMonitor->recordPowerLevel(Pr, cmh->txtime());

		if (PHY_DBG)
		{
			char msg[1000];

			if (cmh->ptype() == PT_VIDEO)
			{
				sprintf(msg, "Recv a video packet: VIDEO - PacketType=%s, UID=%d, Size=%d, SeqNb=%d, FrameType=%c, PrioNb=%d",
						packet_info.name(cmh->ptype()),
						cmh->uid(),
						cmh->size(),
						cmh->seqNb_,
						cmh->frameType_[0],
						cmh->prioNb_);
			}
			else
			{
				sprintf(msg, "Recv a control packet: NON-VIDEO - PacketType=%s, UID=%d, Size=%d",
						packet_info.name(cmh->ptype()),
						cmh->uid(),
						cmh->size());
			}

			log("SendUp (cross-layer)", msg);
		}

		cmh->signalToInterferencePlusNoiseRatio_ = powerMonitor->SINR(Pr);
		cmh->receivedSignalStrengthIndicator_ = powerMonitor->measureRSSI(Pr);
		cmh->signalToNoiseRatio_ = powerMonitor->measureSNR(Pr);
		cmh->packetErrorRate_ = powerMonitor->measurePERfromPr(Pr);
		cmh->linkQualityIndicator_ = powerMonitor->measureLQI(Pr);

		if (PHY_DBG)
		{
			char msg[1000];
			sprintf(msg, "Id: %d Pr: %f PL: %f SINR: %f TXTIME: %f", cmh->uid(), Pr * 1e9, powerMonitor->getPowerLevel() * 1e9, Pr / (powerMonitor->getPowerLevel() - Pr), cmh->txtime());
			log("Sendup", msg);
		}

		// Décrémentation de l'énergie de réception (réaliste)
		if (em() && em()->energy() > 0)
		{
			double rxtime = cmh->txtime();
			em()->DecrRcvEnergy(rxtime, Pr_consume_);

			// ----------- Log l'énergie restante -----------
			if (em()->energy() > 0)
				((MobileNode *)node_)->log_energy(1);
			else if (em()->energy() <= 0)
				((MobileNode *)node_)->log_energy(0);
			// ----------------------------------------------
		}

		switch (state)
		{
		case TXingEvalVSN:
			// case 12
			discard(p, Pr, DROP_PHY_TXB);
			setState(TXingEvalVSN);
			break;
		case SEARCHINGEvalVSN:
			if ((powerMonitor->SINR(Pr) >= modulation_tableEvalVSN[BasicModulationScheme_].SINR_ratio))
			{
				power_RX = Pr;
				pkt_RX = p->copy();
				setState(PreRXingEvalVSN);
				preRX_Timer.sched(HeaderDuration_); // preamble and PCLP header
				break;
			}
			else
			{
				// case 1
				discard(p, Pr, DROP_PHY_SXB);
				setState(SEARCHINGEvalVSN);
				break;
			}
		case PreRXingEvalVSN:
			if (powerMonitor->SINR(power_RX) >= modulation_tableEvalVSN[BasicModulationScheme_].SINR_ratio)
			{
				// case 4
				discard(p, Pr, DROP_PHY_PXB);
				if (PHY_DBG)
					log("PCAP 1st SUCC", "");
				setState(PreRXingEvalVSN);
				break;
			}
			else
			{
				// the preamble of pkt_RX is corrupted
				// case 2
				discard(pkt_RX, power_RX, DROP_PHY_PXB);
				Packet::free(pkt_RX);
				power_RX = 0;
				preRX_Timer.cancel();
				pkt_RX = 0;
				if (PHY_DBG)
					log("PCAP 1st FAIL", "");

				if (PreambleCaptureSwitch_)
				{
					if (powerMonitor->SINR(Pr) >= SINR_PreambleCapture_)
					{
						pkt_RX = p->copy();
						power_RX = Pr;

						if (PHY_DBG)
							log("PCAP 2nd SUCC", "");

						setState(PreRXingEvalVSN);
						preRX_Timer.sched(HeaderDuration_);
						break;
					}
					else
					{
						// case 3
						discard(p, Pr, DROP_PHY_PXB);
						if (PHY_DBG)
							log("PCAP 2nd FAIL", "");
						setState(SEARCHINGEvalVSN);
						break;
					}
				}
				else
				{
					discard(p, Pr, DROP_PHY_PXB);
					if (PHY_DBG)
						log("PCAP 2nd FAIL N/A", "");
					setState(SEARCHINGEvalVSN);
					break;
				}
			}
		case RXingEvalVSN:
			if (powerMonitor->SINR(power_RX) >= SINR_Th_RX)
			{
				// case 8
				discard(p, Pr, DROP_PHY_RXB);
				if (PHY_DBG)
					log("DCAP 1st SUCC", "");
				setState(RXingEvalVSN);
				break;
			}
			else
			{
				if (PHY_DBG)
					log("DCAP 1st FAIL", "");
				HDR_CMN(pkt_RX)->error() = 1;
				if (DataCaptureSwitch_)
				{
					if (powerMonitor->SINR(Pr) >= SINR_DataCapture_)
					{
						// case 9
						rX_Timer.cancel();
						discard(pkt_RX, power_RX, DROP_PHY_RXB);
						pkt_RX = 0;
						//------ start the prerx of this new packet p---//
						pkt_RX = p->copy();
						power_RX = Pr;
						if (PHY_DBG)
							log("DCAP 2nd SUCC", "");
						setState(PreRXingEvalVSN);
						preRX_Timer.sched(HeaderDuration_);
					}
					else
					{
						// case 10
						discard(p, Pr, DROP_PHY_RXB);
						if (PHY_DBG)
							log("DCAP 2nd FAIL", "");
						setState(RXingEvalVSN);
					}
				}
				else
				{
					// case 11
					discard(p, Pr, DROP_PHY_RXB);
					if (PHY_DBG)
						log("DCAP 2nd FAIL N/A", "");
					setState(RXingEvalVSN);
				}
				break;
			}
		default:
			cout << "packet arrive from chanel at invalid PHY state" << endl;
			exit(-1);
		}
	}

	return 0; // the incoming MAC frame will be freed by phy.cc.
}

double WirelessPhyEvalVSN::SINR_Th(int modulationScheme)
{
	if (PHY_DBG)
	{
		char msg[1000];
		sprintf(msg, ": = %s threshold %f ",
				modulation_tableEvalVSN[modulationScheme].schemeName,
				modulation_tableEvalVSN[modulationScheme].SINR_ratio);
		log("Mod_Scheme", msg);
	}

	return modulation_tableEvalVSN[modulationScheme].SINR_ratio;
}

void WirelessPhyEvalVSN::handle_TXtimeout()
{
	assert(state == TXingEvalVSN);
	Mac802_11EvalVSN *mac_ = (Mac802_11EvalVSN *)(uptarget_);
	mac_->handleTXEndIndication();
	setState(SEARCHINGEvalVSN);
}

void WirelessPhyEvalVSN::handle_RXtimeout()
{
	assert(state == RXingEvalVSN);
	setState(SEARCHINGEvalVSN);
	struct hdr_cmn *cmh = HDR_CMN(pkt_RX);
	Mac802_11EvalVSN *mac_ = (Mac802_11EvalVSN *)(uptarget_);
	if (cmh->error())
	{
		// case 5, 9
		discard(pkt_RX, power_RX, DROP_PHY_RXB);
		pkt_RX = 0;
		power_RX = 0;
		mac_->handleRXEndIndication(0);
	}
	else
	{
		mac_->handleRXEndIndication(pkt_RX);
		pkt_RX = 0;
		power_RX = 0;
	}
}

void WirelessPhyEvalVSN::handle_PreRXtimeout()
{
	assert(state == PreRXingEvalVSN);

	assert(pkt_RX);
	struct hdr_cmn *cmh = HDR_CMN(pkt_RX);
	SINR_Th_RX = SINR_Th(cmh->mod_scheme_);
	//	cout<<"sinr"<<SINR_Th_RX<<endl;
	if (powerMonitor->SINR(power_RX) < SINR_Th_RX)
	{
		if (PHY_DBG)
			log("PreRxTimeout", "BodyError");
		cmh->error() = 1;
	}
	setState(RXingEvalVSN);
	rX_Timer.sched(cmh->txtime() - HeaderDuration_); // receiving rest of the MAC frame
	Mac802_11EvalVSN *myUpMAC = (Mac802_11EvalVSN *)(uptarget_);
	myUpMAC->handleRXStartIndication();
}

void WirelessPhyEvalVSN::sendCSBusyIndication()
{
	Mac802_11EvalVSN *myUpMAC = (Mac802_11EvalVSN *)(uptarget_);
	myUpMAC->handlePHYBusyIndication();
}

void WirelessPhyEvalVSN::sendCSIdleIndication()
{
	Mac802_11EvalVSN *myUpMAC = (Mac802_11EvalVSN *)(uptarget_);
	myUpMAC->handlePHYIdleIndication();
}

int WirelessPhyEvalVSN::discard(Packet *p, double power, char *reason)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	double modulation_SINR = SINR_Th(ch->mod_scheme_);
	double Xt, Yt, Zt; // location of transmitter
	double Xr, Yr, Zr; // location of receiver
	PacketStamp s;

	s.stamp((MobileNode *)node(), ant_, 0, lambda_);
	s.getNode()->getLoc(&Xt, &Yt, &Zt);
	p->txinfo_.getNode()->getLoc(&Xr, &Yr, &Zr);
	Xr += p->txinfo_.getAntenna()->getX();
	Yr += p->txinfo_.getAntenna()->getY();
	Zr += p->txinfo_.getAntenna()->getZ();
	Xt += s.getAntenna()->getX();
	Yt += s.getAntenna()->getY();
	Zt += s.getAntenna()->getZ();
	double dX = Xr - Xt;
	double dY = Yr - Yt;
	double dZ = Zr - Zt;
	double d = sqrt(dX * dX + dY * dY + dZ * dZ);

	if (d < trace_dist_)
	{
		if (power < modulation_SINR * noise_floor_)
			reason = DROP_PHY_DND;

		if (power < modulation_tableEvalVSN[BasicModulationScheme_].SINR_ratio * noise_floor_)
			reason = DROP_PHY_PND;

		if (PHY_DBG)
		{
			char msg[1000];

			if (ch->ptype() == PT_VIDEO)
			{
				sprintf(msg, "Drop a video packet: VIDEO - PacketType=%s, UID=%d, Size=%d, SeqNb=%d, FrameType=%c, PrioNb=%d, Power=%f, Reason=%s",
						packet_info.name(ch->ptype()),
						ch->uid(),
						ch->size(),
						ch->seqNb_,
						ch->frameType_[0],
						ch->prioNb_,
						power,
						reason);
			}
			else
			{
				sprintf(msg, "Drop a control packet: NON-VIDEO - PacketType=%s, UID=%d, Size=%d, Power=%f, Reason=%s",
						packet_info.name(ch->ptype()),
						ch->uid(),
						ch->size(),
						power,
						reason);
			}

			log("Discard", msg);
		}

		drop(p, dropReason(p, reason));
	}
	else
	{
		Packet::free(p);
	}

	return 0;
}

void WirelessPhyEvalVSN::dump(void) const
{
}

void WirelessPhyEvalVSN::log(char *event, char *additional)
{
	if (PHY_DBG)
		cout << "L " << Scheduler::instance().clock() << " " << node_->nodeid() << " " << "PHY" << " " << event << " "
			 << additional << endl;
}

void WirelessPhyEvalVSN::setState(int newstate)
{
	if (PHY_DBG)
	{
		char msg[1000];
		sprintf(msg, "%d -> %d", state, newstate);
		log("PHYState", msg);
	}

	// Si on passe de SEARCHING à un autre état, indiquer la MAC que le canal est busy
	if (state == SEARCHINGEvalVSN && newstate != SEARCHINGEvalVSN)
	{
		if (powerMonitor->getPowerLevel() < CSThresh_)
		{
			sendCSBusyIndication();
		}
	}
	// Si on passe d’un état actif vers SEARCHING, indiquer la MAC que le canal est idle
	else if (state != SEARCHINGEvalVSN && newstate == SEARCHINGEvalVSN)
	{
		if (powerMonitor->getPowerLevel() < CSThresh_)
		{
			sendCSIdleIndication();
		}
	}

	state = newstate;
}

int WirelessPhyEvalVSN::getState()
{
	return state;
}

void TX_TimerEvalVSN::expire(Event *)
{
	wirelessPhyEvalVSN->handle_TXtimeout();
	return;
}

void RX_TimerEvalVSN::expire(Event *)
{
	wirelessPhyEvalVSN->handle_RXtimeout();
	return;
}

void PreRX_TimerEvalVSN::expire(Event *)
{
	wirelessPhyEvalVSN->handle_PreRXtimeout();
	return;
}

//**************************************************************

PowerMonitorEvalVSN::PowerMonitorEvalVSN(WirelessPhyEvalVSN *phy)
{
	// initialize, the NOISE is the environmental noise
	wirelessPhyEvalVSN = phy;
	CS_Thresh = wirelessPhyEvalVSN->CSThresh_; //  monitor_Thresh = CS_Thresh;
	monitor_Thresh = wirelessPhyEvalVSN->PowerMonitorThresh_;
	powerLevel = wirelessPhyEvalVSN->noise_floor_; // noise floor is -99dbm
}

void PowerMonitorEvalVSN::recordPowerLevel(double signalPower, double duration)
{
	// to reduce the number of entries recorded in the interfList
	if (signalPower < monitor_Thresh)
		return;

	interfEvalVSN timerEntry;
	timerEntry.PtEvalVSN = signalPower;
	timerEntry.endEvalVSN = Scheduler::instance().clock() + duration;

	list<interfEvalVSN>::iterator i;
	for (i = interfList_.begin(); i != interfList_.end() && i->endEvalVSN <= timerEntry.endEvalVSN; i++)
	{
	}
	interfList_.insert(i, timerEntry);

	resched((interfList_.begin())->endEvalVSN - Scheduler::instance().clock());

	powerLevel += signalPower; // update the powerLevel

	if (wirelessPhyEvalVSN->getState() == SEARCHINGEvalVSN && powerLevel >= CS_Thresh)
	{
		wirelessPhyEvalVSN->sendCSBusyIndication();
	}
}

double PowerMonitorEvalVSN::getPowerLevel()
{
	if (powerLevel > wirelessPhyEvalVSN->noise_floor_)
		return powerLevel;
	else
		return wirelessPhyEvalVSN->noise_floor_;
}

void PowerMonitorEvalVSN::setPowerLevel(double power)
{
	powerLevel = power;
}

/**
 * @brief Calculates the Signal-to-Interference-plus-Noise Ratio (SINR).
 *
 * This function computes the SINR based on the received power (Pr) and the current power level.
 * If the difference between the current power level and the received power is less than or equal to zero,
 * the function returns 0.0 to indicate an invalid or contention scenario.
 *
 * @param Pr The received power.
 * @return The computed SINR value, or 0.0 if the power level is not sufficient.
 */
double PowerMonitorEvalVSN::SINR(double Pr)
{
	if (getPowerLevel() - Pr <= 0)
	{
		//			cout<<"PowerLevel lower than Pr"<<endl;exit(-1);
		//			char msg[1000];
		//			sprintf(msg,"PowerLevel %f lower than Pr %f = %f",getPowerLevel()*1e9,Pr*1e9,(getPowerLevel()-Pr));
		//			wirelessPhyEvalVSN->log("PMX",msg);
		//			exit(-1);
		return 0.0; // internal event contention, new msg arrives betweeen the expire of two timers.
	}

	return Pr / (getPowerLevel() - Pr);
}

double WirelessPhyEvalVSN::getDist(double Pr, double Pt, double Gt, double Gr,
								   double hr, double ht, double L, double lambda)
{
	if (propagation_)
	{
		return propagation_->getDist(Pr, Pt, Gt, Gr, hr, ht, L,
									 lambda);
	}
	return 0;
}

void PowerMonitorEvalVSN::expire(Event *)
{
	double pre_power = powerLevel;
	double time = Scheduler::instance().clock();

	list<interfEvalVSN>::iterator i;
	i = interfList_.begin();
	while (i != interfList_.end() && i->endEvalVSN <= time)
	{
		powerLevel -= i->PtEvalVSN;
		interfList_.erase(i++);
	}
	if (!interfList_.empty())
		resched((interfList_.begin())->endEvalVSN - Scheduler::instance().clock());

	char msg[1000];
	sprintf(msg, "Power: %f -> %f", pre_power * 1e9, powerLevel * 1e9);
	wirelessPhyEvalVSN->log("PMX", msg);

	// check if the channel becomes idle ( busy -> idle )
	if (wirelessPhyEvalVSN->getState() == SEARCHINGEvalVSN && powerLevel < CS_Thresh)
	{
		wirelessPhyEvalVSN->sendCSIdleIndication();
	}
}

/**
 * @brief Measures the Received Signal Strength Indicator (RSSI) in dBm.
 *
 * This function calculates the RSSI based on the provided received power (Pr) in Watts.
 * It first checks the total noise floor (including noise and interference) and ensures
 * both the received power and noise floor are positive. If either is non-positive,
 * it returns -DBL_MAX to indicate an invalid or too weak signal.
 * Otherwise, it converts the received power from Watts to dBm and returns the result.
 *
 * @param Pr The received power in Watts.
 * @return The RSSI value in dBm, or -DBL_MAX if the signal is too weak or invalid.
 */
double PowerMonitorEvalVSN::measureRSSI(double Pr)
{
	// Mesure totale du plancher (bruit + interférences)
	double totalNoiseFloor = getPowerLevel();

	if (Pr <= 0.0 || totalNoiseFloor <= 0.0)
	{
		// signal trop faible ou mesure incorrecte
		return -DBL_MAX;
	}

	// Conversion de Pr (Watts) en dBm
	double rssi_dbm = 10.0 * log10(Pr * 1000.0);

	return rssi_dbm;
}

/**
 * @brief Measures the Signal-to-Noise Ratio (SNR) in decibels (dB) for a given received power.
 *
 * This function calculates the SNR by comparing the received signal power (Pr) to the sum of noise and interference
 * (excluding the signal itself). The SNR is returned in decibels (dB) using the formula:
 *   SNR[dB] = 10 * log10(Pr / (noise + interference))
 *
 * @param Pr The received signal power (in the same units as returned by getPowerLevel()).
 * @return The SNR in decibels (dB). Returns -DBL_MAX if the signal is undetectable or if measurement is invalid.
 */
double PowerMonitorEvalVSN::measureSNR(double Pr)
{
	// Calcul du niveau de bruit + interférences hors signal
	double noisePlusInterf = getPowerLevel() - Pr;

	if (Pr <= 0.0 || noisePlusInterf <= 0.0)
	{
		// pas de signal détectable ou erreur de mesure
		return -DBL_MAX;
	}

	// SNR en dB
	return 10.0 * log10(Pr / noisePlusInterf);
}

/**
 * @brief Calculates the Packet Error Rate (PER) based on the measured SINR.
 *
 * This function computes the PER using an exponential model, given the measured
 * Signal-to-Interference-plus-Noise Ratio (SINR) and the SINR threshold for the
 * current modulation scheme. If either the measured SINR or the threshold is
 * non-positive, the function returns a PER of 1.0 (i.e., 100% error rate).
 *
 * The formula used is:
 *   PER = 1 - exp(-sinr / sinr_threshold)
 *
 * @param sinr The measured Signal-to-Interference-plus-Noise Ratio.
 * @return The calculated Packet Error Rate (PER), in the range [0.0, 1.0].
 */
double PowerMonitorEvalVSN::measurePER(double sinr)
{
	// Récupération du seuil nécessaire pour la modulation en cours
	double sinr_threshold = modulation_tableEvalVSN[wirelessPhyEvalVSN->BasicModulationScheme_].SINR_ratio;

	if (sinr <= 0.0 || sinr_threshold <= 0.0)
	{
		// conditions extrêmes : PER=100%
		return 1.0;
	}

	// PER = 1 - exp( - SINR_mesuré / SINR_min )
	return 1.0 - exp(-sinr / sinr_threshold);
}

/**
 * @brief Measures the Packet Error Rate (PER) based on the received power.
 *
 * This function calculates the actual Signal-to-Interference-plus-Noise Ratio (SINR)
 * from the given received power (Pr), and then estimates the Packet Error Rate (PER)
 * using the computed SINR value.
 *
 * @param Pr The received power level.
 * @return The estimated Packet Error Rate (PER) corresponding to the given received power.
 */
double PowerMonitorEvalVSN::measurePERfromPr(double Pr)
{
	// on calcule d’abord le SINR réel
	double sinr = SINR(Pr); // utilise getPowerLevel()
	return measurePER(sinr);
}

/**
 * @brief Measures the Link Quality Indicator (LQI) based on received power.
 *
 * This function computes the LQI value using two metrics:
 *   1. The ratio of received energy (Pr) to the reception threshold (RXThresh_),
 *      scaled to fit within the range 0–128.
 *   2. The normalized Signal-to-Interference-plus-Noise Ratio (SINR), where a
 *      linear SINR of 10 (10 dB) or higher is considered excellent, scaled to 0–255.
 *
 * Both metrics are clamped to the range 0–255. The final LQI is the minimum of
 * the two values, returned as an 8-bit unsigned integer.
 *
 * @param Pr The received signal power.
 * @return UINT_8 The computed Link Quality Indicator (LQI) in the range 0–255.
 */
UINT_8 PowerMonitorEvalVSN::measureLQI(double Pr)
{
	// 1) Ratio d’énergie relative par rapport au seuil de réception
	//    On multiplie par 128 pour cadrer sur 0..128
	double energyRatio = Pr / wirelessPhyEvalVSN->RXThresh_;
	int ener = static_cast<int>(energyRatio * 128.0);

	// 2) Ratio SINR normalisé
	//    SINR linéaire = Pr / (noise+interf)
	double sinrLin = SINR(Pr);
	// On considère qu’un SINR de 10 (10 dB) ou plus est excellent
	const double SINR_LIN_MAX = 10.0;
	double sinrNorm = sinrLin / SINR_LIN_MAX;
	int snrVal = static_cast<int>(sinrNorm * 255.0);

	// 3) Bornes 0–255
	ener = wirelessPhyEvalVSN->clampInt(ener, 0, 255);
	snrVal = wirelessPhyEvalVSN->clampInt(snrVal, 0, 255);

	int lqi = min(ener, snrVal);

	return static_cast<UINT_8>(lqi);
}

/**
 * @brief Returns the current transmission power (Pt) of the wireless physical layer.
 *
 * @return The transmission power (Pt_) as a double.
 */
double WirelessPhyEvalVSN::getPt() const
{
	return Pt_;
}

/**
 * @brief Sets the transmission power (Pt_) for the wireless physical layer.
 *
 * This method updates the transmission power to the specified value if it is positive.
 * If the new power value is valid and debugging is enabled (PHY_DBG), a log message
 * is generated indicating the updated power. If the value is invalid (non-positive),
 * and debugging is enabled, an error message is logged.
 *
 * @param newPt The new transmission power value to set. Must be positive.
 */
void WirelessPhyEvalVSN::setPt(double newPt)
{
	if (newPt > 0)
	{
		Pt_ = newPt;

		if (PHY_DBG)
		{
			char msg[1000];
			sprintf(msg, "using: Transmission power updated to: %.3f W", Pt_);
			log("UpdatePt", msg);
		}
	}
	else
	{
		if (PHY_DBG)
		{
			log("UpdatePt", "Invalid power value. Pt_ must be positive.");
		}
	}
}

/**
 * @brief Retrieves the basic modulation scheme used by this WirelessPhyEvalVSN instance.
 *
 * @return An integer representing the basic modulation scheme.
 */
int WirelessPhyEvalVSN::getBasicModulationScheme() const
{
	return BasicModulationScheme_;
}

/**
 * @brief Sets the basic modulation scheme for the wireless physical layer.
 *
 * This method updates the modulation scheme used by the physical layer to the specified value,
 * provided that the value is within the valid range of defined modulation schemes.
 * If the scheme is valid (between 0 and NUM_MODULATION_SCHEMES - 1), it updates the internal
 * state and optionally logs the change if debugging is enabled. If the scheme is invalid,
 * it logs an error message if debugging is enabled.
 *
 * @param scheme The index of the modulation scheme to set (must be between 0 and NUM_MODULATION_SCHEMES - 1).
 */
void WirelessPhyEvalVSN::setBasicModulationScheme(int scheme)
{
	if (scheme >= 0 && scheme < NUM_MODULATION_SCHEMES)
	{ // Vérifie que le schéma est valide (0 à 3 pour les 4 schémas définis)
		BasicModulationScheme_ = scheme;

		if (PHY_DBG)
		{
			char msg[1000];
			sprintf(msg, "BasicModulationScheme updated to: %d", BasicModulationScheme_);
			log("UpdateModulationScheme", msg);
		}
	}
	else
	{
		if (PHY_DBG)
		{
			log("UpdateModulationScheme", "Invalid modulation scheme. Must be between 0 and 3.");
		}
	}
}

// ------- adjustPhyMetrics -------
/**
 * @brief Adjusts PHY layer metrics and adapts modulation and transmission power based on packet characteristics.
 *
 * This method retrieves and normalizes PHY metrics (RSSI, SNR, PER) from the given packet,
 * computes a weighted score based on packet type (e.g., video or non-video), frame type,
 * and priority, and then adapts the modulation scheme and transmission power accordingly.
 * Adaptation is only performed if the PHY is not in the SEARCHINGEvalVSN state.
 * Optionally logs debug information if PHY_DBG is enabled.
 *
 * @param p Pointer to the Packet whose PHY metrics are to be adjusted.
 *
 * Metrics used:
 * - RSSI (Received Signal Strength Indicator)
 * - SNR (Signal to Noise Ratio)
 * - PER (Packet Error Rate)
 *
 * Weighting and adaptation depend on:
 * - Packet type (e.g., PT_VIDEO)
 * - Frame type (e.g., 'M' for main, others for non-main)
 * - Packet priority
 *
 * Calls:
 * - adjustModulationScheme()
 * - adjustTxPower()
 * - log() (for debugging)
 */
void WirelessPhyEvalVSN::adjustPhyMetrics(Packet *p)
{
	hdr_cmn *ch = HDR_CMN(p);

	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0];
	int priority = ch->prioNb_;

	// 1) Récupération des métriques
	double rssi = ch->receivedSignalStrengthIndicator_;
	double snr = ch->signalToNoiseRatio_;
	double per = ch->packetErrorRate_;

	// 2) Normalisation des métriques
	double normRSSI = clampDouble((rssi + 100.0) / 100.0, 0.0, 1.0);
	double normSNR = clampDouble(snr / 50.0, 0.0, 1.0);
	double normPER = clampDouble(1.0 - per, 0.0, 1.0);

	// 3) Poids dynamiques selon frameType et priority
	double w1, w2, w3;
	// Priorites de 1 (critique) a getMaxPrioNb() ; le +1 evite que le dernier
	// niveau annule la ponderation, comme dans aomdv_qpv.cc et priqueueEvalVSN.cc.
	double prioBoost = double(getMaxPrioNb() + 1 - priority) / double(getMaxPrioNb());

	if (packetType == PT_VIDEO)
	{
		w1 = (frameType == 'M' ? 0.4 : 0.5) * (prioBoost);
		w2 = (frameType == 'M' ? 0.5 : 0.3) * (prioBoost);
		w3 = (frameType == 'M' ? 0.1 : 0.2) * (prioBoost);
	}
	else
	{
		// Non vidéo : priorité la plus haute, pondération forte sur robustesse
		w1 = 0.6 * (prioBoost);
		w2 = 0.3 * (prioBoost);
		w3 = 0.1 * (prioBoost);
	}

	// 4) Calcul du score brut [0..1]
	double rawScore = clampDouble(w1 * normRSSI +
									  w2 * normSNR +
									  w3 * normPER,
								  0.0, 1.0);

	// 5) Différer l’adaptation si le PHY n’est pas idle
	int phyState = getState();

	if (phyState != SEARCHINGEvalVSN)
	{
		// 6) Adaptation modulation et puissance avec dernier contexte
		adjustModulationScheme(packetType, frameType, priority, rawScore);
		adjustTxPower(packetType, frameType, priority, rawScore);

		// 7) Debug
		if (PHY_DBG)
		{
			char msg[1000];

			if (packetType == PT_VIDEO)
			{
				sprintf(msg, "adjust: raw=%.3f "
							 "(RSSI=%.1f, SNR=%.1f, PER=%.3f) "
							 "applied for VIDEO packetType=%s, frameType='%c', priority=%d",
						rawScore, rssi, snr, per, packet_info.name(packetType), frameType, priority);
			}
			else
			{
				sprintf(msg, "adjust: raw=%.3f "
							 "(RSSI=%.1f, SNR=%.1f, PER=%.3f) "
							 "applied for NON-VIDEO packetType=%s",
						rawScore, rssi, snr, per, packet_info.name(packetType));
			}

			log("adjustPhyMetrics (cross-layer)", msg);
		}
	}
}

// ------- adjustModulationScheme -------
/**
 * @brief Adjusts the modulation scheme based on packet type, frame type, priority, and a score.
 *
 * This function determines the appropriate modulation scheme to use for a given packet,
 * considering the allowed range for the packet type, frame type, and priority. It maps
 * the provided score (in the range [0, 1]) to an index within the allowed modulation
 * schemes. To avoid rapid switching, a hysteresis mechanism is used: the modulation
 * scheme is only changed after three consecutive measurements indicate the same target.
 *
 * If debugging is enabled, a log message is generated when the modulation scheme is switched.
 *
 * @param packetType The type of the packet (e.g., PT_VIDEO).
 * @param frameType The type of the frame (e.g., data, control).
 * @param priority The priority level of the packet.
 * @param score A normalized score [0, 1] indicating the desired modulation scheme.
 */
void WirelessPhyEvalVSN::adjustModulationScheme(packet_t packetType, char frameType, int priority, double score)
{
	const int N = NUM_MODULATION_SCHEMES;

	// Détermine la plage autorisée selon type de trame et priorité
	Range r = allowedRange(packetType, frameType, priority, N, getMaxPrioNb());

	// 1) Mapping linéaire score [0..1] → index [r.min..r.max]
	double clampedScore = clampDouble(score, 0.0, 1.0);
	int target = r.min + int(clampedScore * (r.max - r.min) + 0.5);
	target = clampInt(target, r.min, r.max);

	// Initialisation du compteur d’hystérésis si nécessaire
	if (modHystCount_ < 0)
	{
		lastModScheme_ = getBasicModulationScheme();
		modHystCount_ = 0;
	}

	// Hystérésis : confirmer 3 mesures consécutives
	if (target == lastModScheme_)
	{
		// même schéma que précédemment → on réinitialise le compteur
		modHystCount_ = 0;
	}
	else if (++modHystCount_ >= HYST_COUNT)
	{
		// le nouveau schéma est stable → on l’applique
		setBasicModulationScheme(target);
		lastModScheme_ = target;
		modHystCount_ = 0;

		if (PHY_DBG)
		{
			char msg[1000];

			if (packetType == PT_VIDEO)
			{
				sprintf(msg, "using: Modulation Scheme Switched to %s (index %d) for VIDEO packetType=%s, frameType='%c', priority=%d",
						modulation_tableEvalVSN[target].schemeName, target, packet_info.name(packetType), frameType, priority);
			}
			else
			{
				sprintf(msg, "using: Modulation Scheme Switched to %s (index %d) for NON-VIDEO packetType=%s",
						modulation_tableEvalVSN[target].schemeName, target, packet_info.name(packetType));
			}

			log("adjustModulationScheme (cross-layer)", msg);
		}
	}
}

// ------- adjustTxPower (fuzzy + priorisation) -------
/**
 * @brief Adjusts the transmission power based on packet type, frame type, priority, and a quality score.
 *
 * This method uses a fuzzy logic approach to determine how much to adjust the transmission power (Pt).
 * The adjustment is influenced by:
 *   - The provided quality score (score): Higher scores reduce power, lower scores increase it.
 *   - The packet type (packetType): Video packets and non-video packets are treated differently.
 *   - The frame type (frameType): For video packets, main frames may receive a boost.
 *   - The priority (priority): Higher priority packets receive a larger boost.
 *
 * The adjustment step is calculated and applied within bounds (50% to 200% of the current power).
 * Debug information is logged if PHY_DBG is enabled.
 *
 * @param packetType The type of the packet (e.g., PT_VIDEO).
 * @param frameType The frame type character (e.g., 'M' for main frame).
 * @param priority The priority level of the packet (lower value means higher priority).
 * @param score A quality score in the range [0, 1] influencing the adjustment direction and magnitude.
 */
void WirelessPhyEvalVSN::adjustTxPower(packet_t packetType, char frameType, int priority, double score)
{
	// 1) Logique floue sur le score
	double deltaFactor;

	if (score > 0.8)
	{
		deltaFactor = -0.15; // Réduction plus importante si le score est très bon
	}
	else if (score > 0.6 && score <= 0.8)
	{
		deltaFactor = -0.1; // Réduction légère si le score est bon
	}
	else if (score > 0.4 && score <= 0.6)
	{
		deltaFactor = 0.0; // Pas de changement si le score est moyen
	}
	else if (score > 0.2 && score <= 0.4)
	{
		deltaFactor = +0.1; // Augmentation légère si le score est faible
	}
	else // score <= 0.2
	{
		deltaFactor = +0.15; // Augmentation plus importante si le score est très faible
	}

	// 2) Boost selon type de trame et priorité
	double frameBoost;
	double prioBoost;

	if (packetType == PT_VIDEO)
	{
		frameBoost = (frameType == 'M' ? 1.1 : 1.0);
	}
	else
	{
		// Non vidéo : priorité la plus haute, boost maximal
		frameBoost = 1.2;
	}

	prioBoost = double(getMaxPrioNb() + 1 - priority) / double(getMaxPrioNb());

	// 3) Calcul du pas de variation
	double currentPt = getPt();
	double step = deltaFactor * currentPt * frameBoost * prioBoost;

	// 4) Détermination des bornes sur Pt_
	const double P_MIN = currentPt * 0.5; // 50% de la valeur actuelle minimum
	const double P_MAX = currentPt * 2.0; // 200% de la valeur actuelle maximum

	// 5) Calcul et application de la nouvelle puissance
	double newPt = clampDouble(currentPt + step, P_MIN, P_MAX);
	setPt(newPt);

	// 6) Debug
	if (PHY_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "adjust: TxPower adjusted to %.3f W (delta = %.3f W) for VIDEO packetType=%s, frameType='%c', priority=%d",
					getPt(), step, packet_info.name(packetType), frameType, priority);
		}
		else
		{
			sprintf(msg, "adjust: TxPower adjusted to %.3f W (delta = %.3f W) for NON-VIDEO packetType=%s", getPt(), step, packet_info.name(packetType));
		}

		log("adjustTxPower (cross-layer)", msg);
	}
}

/**
 * Calcule dynamiquement la plage de modulation autorisée :
 * - Pour frameType 'M' (main frame), on favorise les schémas robustes ;
 * - Pour frameType 'D' (diff frame), on favorise les débits élevés ;
 * - L’influence de la priorité (1 = haute, HIGH_PRIO = basse) ajuste la largeur de la plage.
 * - Pour les paquets non-vidéo (packetType != PT_VIDEO), on force la modulation la plus robuste.
 */
/**
 * @brief Determines the allowed modulation/coding scheme range for a given packet type, frame type, and priority.
 *
 * This function computes the minimum and maximum indices of modulation/coding schemes that are allowed
 * for a packet, based on its type (e.g., video or non-video), frame type (e.g., 'M' for robust, 'D' for throughput),
 * and priority. For non-video packets, only the most robust scheme is allowed. For video packets, the allowed
 * range is adjusted according to the frame type and normalized priority, with higher priority (lower prio value)
 * favoring more robust schemes.
 *
 * @param packetType   The type of the packet (e.g., PT_VIDEO).
 * @param frameType    The frame type ('M' for robust/medium, 'D' for data/high throughput, others for default).
 * @param prio         The priority of the packet (1 = highest priority, HIGH_PRIO = lowest).
 * @param numSchemes   The total number of available modulation/coding schemes.
 * @param HIGH_PRIO    The value representing the lowest priority.
 * @return Range       The allowed range [min, max] of scheme indices for the given parameters.
 */
Range WirelessPhyEvalVSN::allowedRange(packet_t packetType, char frameType, int prio, int numSchemes, int HIGH_PRIO)
{
	const int MIN_SCHEME = 0;
	const int MAX_SCHEME = numSchemes - 1;
	Range r;

	// Pour les paquets non-vidéo : priorité maximale, modulation la plus robuste
	if (packetType != PT_VIDEO)
	{
		r.min = MIN_SCHEME;
		r.max = MIN_SCHEME; // BPSK uniquement

		return r;
	}

	// Pour les paquets vidéo : logique existante selon frameType/prio

	// On normalise la priorité en 0..1 : 0 = haute (prio=1), 1 = basse (prio=HIGH_PRIO)
	// Normalisation de la priorité en [0..1], en évitant la division par zéro
	double pnorm;

	if (HIGH_PRIO <= 1)
	{
		// Un seul niveau de priorité : on positionne pnorm à 0 (priorité « haute » par défaut)
		pnorm = 0.0;
	}
	else
	{
		pnorm = double(prio - 1) / double(HIGH_PRIO - 1);

		if (pnorm < 0.0)
			pnorm = 0.0;
		else if (pnorm > 1.0)
			pnorm = 1.0;
	}

	if (frameType == 'M')
	{
		// On reste dans la moitié basse (robuste) : [0 .. mid]
		// + un décalage vers la gauche si priorité haute (pnorm≈0)
		double mid = (MAX_SCHEME + MIN_SCHEME) * 0.5;
		// plus pnorm grand → plus on élargit jusqu’à mid
		r.min = MIN_SCHEME;
		r.max = clampInt(int(mid * (1.0 - pnorm) + pnorm * mid), MIN_SCHEME, MAX_SCHEME);
	}
	else if (frameType == 'D')
	{
		// On reste dans la moitié haute (débit) : [mid .. MAX]
		// + un décalage vers la droite si priorité haute (pnorm≈0)
		double mid = (MAX_SCHEME + MIN_SCHEME) * 0.5;
		// plus pnorm grand → plus on élargit jusqu’à MAX_SCHEME
		r.min = clampInt(int(mid * pnorm + (1.0 - pnorm) * mid), MIN_SCHEME, MAX_SCHEME);
		r.max = MAX_SCHEME;
	}
	else
	{
		// par défaut on autorise toute la plage
		r.min = MIN_SCHEME;
		r.max = MAX_SCHEME;
	}

	// Sécurité
	if (r.min > r.max)
	{
		int tmp = r.min;
		r.min = r.max;
		r.max = tmp;
	}

	return r;
}

// ------- Helpers pour clamp -------
/**
 * @brief Clamps a double value within the specified range.
 *
 * This function returns the value \p v constrained to the interval [\p lo, \p hi].
 * If \p v is less than \p lo, \p lo is returned. If \p v is greater than \p hi, \p hi is returned.
 * Otherwise, \p v is returned unchanged.
 *
 * @param v  The value to clamp.
 * @param lo The lower bound of the range.
 * @param hi The upper bound of the range.
 * @return   The clamped value within [lo, hi].
 */
double WirelessPhyEvalVSN::clampDouble(double v, double lo, double hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/**
 * @brief Clamps an integer value between a lower and upper bound.
 *
 * This function returns the value \p v constrained to the range [\p lo, \p hi].
 * If \p v is less than \p lo, \p lo is returned. If \p v is greater than \p hi,
 * \p hi is returned. Otherwise, \p v is returned unchanged.
 *
 * @param v  The integer value to clamp.
 * @param lo The lower bound.
 * @param hi The upper bound.
 * @return The clamped integer value.
 */
int WirelessPhyEvalVSN::clampInt(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/**
 * Fonction pour générer une raison de drop détaillée.
 * Elle inclut le type de paquet, le type de trame et la priorité.
 * La chaîne retournée doit être libérée par l'appelant si nécessaire.
 */
/**
 * @brief Generates a detailed drop reason string for a given packet and logs the drop event if debugging is enabled.
 *
 * This method constructs a human-readable string describing the reason a packet was dropped,
 * including additional information such as frame type and priority. If PHY_DBG is enabled,
 * it logs the drop event with detailed packet information.
 *
 * @param p Pointer to the Packet being dropped.
 * @param Reason Optional base reason string for the drop (can be nullptr).
 * @return A dynamically allocated string containing the detailed drop reason.
 *         The caller is responsible for freeing this memory using free().
 *
 * @note If PHY_DBG is defined and enabled, this function logs the drop event with
 *       detailed information depending on whether the packet is a video packet or not.
 */
char *WirelessPhyEvalVSN::dropReason(Packet *p, char *Reason)
{
	hdr_cmn *ch = HDR_CMN(p);

	// Récupère les informations du paquet
	packet_t packetType = ch->ptype();
	char frameType = ch->frameType_[0] ? ch->frameType_[0] : 'C';
	int priority = ch->prioNb_ ? ch->prioNb_ : 0;

	// Compose la nouvelle raison détaillée
	char buf[1000];
	snprintf(buf, sizeof(buf), "%s -Frt %c -Pri %d",
			 Reason ? Reason : "---", frameType, priority);

	// Alloue dynamiquement la chaîne (à libérer par l'appelant si besoin)
	char *detailedReason = strdup(buf);

	if (PHY_DBG)
	{
		char msg[1000];

		if (packetType == PT_VIDEO)
		{
			sprintf(msg, "Drop a video packet: VIDEO - PacketType=%s, UID=%d, Size=%d, SeqNb=%d, FrameType=%c, PrioNb=%d, Reason=%s",
					packet_info.name(packetType),
					ch->uid(),
					ch->size(),
					ch->seqNb_,
					frameType,
					priority,
					detailedReason);
		}
		else
		{
			sprintf(msg, "Drop a control packet: NON-VIDEO - PacketType=%s, UID=%d, Size=%d, Reason=%s",
					packet_info.name(packetType),
					ch->uid(),
					ch->size(),
					detailedReason);
		}

		log("dropReason", msg);
	}

	return detailedReason;
}
