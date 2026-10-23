/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// power-monitorEvalVSN.h - the PMD-level power monitor of the EvalVSN PHYs,
// and the interface a PHY implements to host it.
//
// Extracted from wireless-phyEvalVSN.{h,cc}. What it asks of its host PHY
// fits in the interface below: two radio constants and the LQI mapping.
// The 802.15.4 PHY of the project (phy-802_15_4EvalVSN) is its host.
//
// The monitor corresponds to the PMD (Physical Media Dependent) sub-layer
// within the PHY, the only sub-layer that directly interacts with the analog
// RF signals. It keeps track of all the noise and interferences experienced
// by the node individually for their respective durations, and signals its
// host whenever the cumulative level crosses the carrier sense threshold.
// It comes from the Karlsruhe/Daimler overhaul of IEEE 802.11 modelling in
// ns-2 (Copyright (C) 2007 Mercedes-Benz Research & Development North
// America, Inc. and University of Karlsruhe (TH); authors Qi Chen, Felix
// Schmidt-Eisenlohr, Daniel Jiang; GPL v2 - see wireless-phyEvalVSN.h for
// the full original notice), extended by EvalVSN with the RSSI, SNR, PER and
// LQI measures.

#ifndef ns_power_monitorEvalVSN_h
#define ns_power_monitorEvalVSN_h

#include <list>
#include <packet.h>
#include "timer-handler.h"

// What the power monitor asks of the PHY that hosts it.
class PowerMonitorHostEvalVSN
{
public:
	virtual ~PowerMonitorHostEvalVSN() {}

	// The radio constants the measures are computed against, in watts:
	// lowest power level worth recording as interference, and
	// environmental noise floor.
	virtual double monitorRecordThresh() = 0;
	virtual double monitorNoiseFloor() = 0;

	// The LQI normalisation is the radio's business, like its thresholds:
	// the monitor measures Pr and SINR, the host maps them onto 0-255. The
	// 802.15.4 host grades in dB, the only domain that discriminates over
	// the six decades its usable range spans (a linear mapping was measured
	// flat at 255 from 50 m to 500 m).
	virtual unsigned char monitorMapLQI(double Pr, double sinrLin) = 0;
};

struct interfEvalVSN
{
	double PtEvalVSN;
	double endEvalVSN;
};

class PowerMonitorEvalVSN : public TimerHandler
{
public:
	PowerMonitorEvalVSN(PowerMonitorHostEvalVSN *);
	void recordPowerLevel(double power, double duration);
	double getPowerLevel();
	void setPowerLevel(double);
	double SINR(double Pr);
	void expire(Event *);				// virtual function, which must be implemented
	double measureRSSI(double Pr);		// Computes the RSSI
	double measureSNR(double Pr);		// Computes the SNR
	/**
	 * Computes an LQI indicator (0-255) from the received power Pr.
	 * It combines an energy ratio and a SINR ratio, so as to reflect both
	 * the strength of the signal and its ratio to noise and interference.
	 *
	 * @param Pr  instantaneous received power (W)
	 * @return    LQI normalised between 0 (poor) and 255 (excellent)
	 */
	unsigned char measureLQI(double Pr);

private:
	double monitor_Thresh; // packet with power > monitor_thresh will be recorded in the monitor
	double powerLevel;
	PowerMonitorHostEvalVSN *host;
	std::list<interfEvalVSN> interfList_;
};

#endif /* ns_power_monitorEvalVSN_h */
