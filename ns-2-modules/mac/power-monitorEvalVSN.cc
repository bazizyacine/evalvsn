/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:t; -*- */
//
// power-monitorEvalVSN.cc - see power-monitorEvalVSN.h.
//
// The bodies are those that lived in wireless-phyEvalVSN.cc, retargeted to
// the host interface. The 802.15.4 base PHY does its own clear-channel
// assessment, so the monitor keeps the interference account only.

#include "mac/power-monitorEvalVSN.h"
#include "mac/link-metrics.h"

#include <math.h>
#include <float.h>
#include <algorithm>

PowerMonitorEvalVSN::PowerMonitorEvalVSN(PowerMonitorHostEvalVSN *h)
{
	// initialize, the NOISE is the environmental noise
	host = h;
	monitor_Thresh = host->monitorRecordThresh();
	powerLevel = host->monitorNoiseFloor();
}

void PowerMonitorEvalVSN::recordPowerLevel(double signalPower, double duration)
{
	// to reduce the number of entries recorded in the interfList
	if (signalPower < monitor_Thresh)
		return;

	interfEvalVSN timerEntry;
	timerEntry.PtEvalVSN = signalPower;
	timerEntry.endEvalVSN = Scheduler::instance().clock() + duration;

	std::list<interfEvalVSN>::iterator i;
	for (i = interfList_.begin(); i != interfList_.end() && i->endEvalVSN <= timerEntry.endEvalVSN; i++)
	{
	}
	interfList_.insert(i, timerEntry);

	resched((interfList_.begin())->endEvalVSN - Scheduler::instance().clock());

	powerLevel += signalPower; // update the powerLevel
}

double PowerMonitorEvalVSN::getPowerLevel()
{
	if (powerLevel > host->monitorNoiseFloor())
		return powerLevel;
	else
		return host->monitorNoiseFloor();
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
		return 0.0; // internal event contention, new msg arrives betweeen the expire of two timers.
	}

	return Pr / (getPowerLevel() - Pr);
}

void PowerMonitorEvalVSN::expire(Event *)
{
	double time = Scheduler::instance().clock();

	std::list<interfEvalVSN>::iterator i;
	i = interfList_.begin();
	while (i != interfList_.end() && i->endEvalVSN <= time)
	{
		powerLevel -= i->PtEvalVSN;
		interfList_.erase(i++);
	}
	if (!interfList_.empty())
		resched((interfList_.begin())->endEvalVSN - Scheduler::instance().clock());

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
	// Total noise floor (noise + interference)
	double totalNoiseFloor = getPowerLevel();

	if (Pr <= 0.0 || totalNoiseFloor <= 0.0)
	{
		// signal too weak, or an incorrect measure
		return -DBL_MAX;
	}

	// Conversion of Pr (Watts) into dBm
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
	// Noise plus interference, excluding the signal itself
	double noisePlusInterf = getPowerLevel() - Pr;

	if (Pr <= 0.0 || noisePlusInterf <= 0.0)
	{
		// no detectable signal, or a measurement error
		return -DBL_MAX;
	}

	// SNR in dB
	return 10.0 * log10(Pr / noisePlusInterf);
}

/**
 * @brief Measures the Link Quality Indicator (LQI) based on received power.
 *
 * The monitor supplies the measures - the received power and the SINR it
 * derives from its interference accounting - and the host maps them onto
 * the 0-255 scale, since the mapping is a property of the radio exactly as
 * the thresholds are.
 *
 * @param Pr  instantaneous received power (W)
 * @return    LQI normalised between 0 (poor) and 255 (excellent)
 */
unsigned char PowerMonitorEvalVSN::measureLQI(double Pr)
{
	return host->monitorMapLQI(Pr, SINR(Pr));
}
