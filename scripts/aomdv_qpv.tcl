# 802.11_aomdv_qpv default parameters

# ------------------------------------------------------------------------------
# 802.11_aomdv_qpv Default Parameters Configuration
#
# This script sets the default physical (PHY) and medium access control (MAC)
# layer parameters for 802.11 wireless simulations using the AOMDV QPV protocol.
#
# PHY Model Parameters:
#   - CPThresh_: Capture threshold in Watts.
#   - CSThresh_: Carrier sensing threshold.
#   - RXThresh_: Receiver signal threshold.
#   - Pt_: Transmitter signal power (Watts).
#   - freq_: Channel frequency (Hz).
#   - noise_floor_: Noise floor (Watts), e.g., -96 dBm for 10MHz bandwidth.
#   - L_: Default radio circuit gain/loss.
#   - PowerMonitorThresh_: Power monitor sensitivity (Watts), e.g., -99 dBm.
#   - HeaderDuration_: Duration of the PHY header (seconds).
#   - BasicModulationScheme_: Modulation scheme identifier.
#   - PreambleCaptureSwitch_: Enable/disable preamble capture.
#   - DataCaptureSwitch_: Enable/disable data capture.
#   - SINR_PreambleCapture_: SINR threshold for preamble capture.
#   - SINR_DataCapture_: SINR threshold for data capture.
#   - trace_dist_: Maximum PHY trace distance (meters).
#   - PHY_DBG_: PHY layer debug flag.
#   - MAX_PRIO_NB: Maximum number of priorities (from $maxPrioNb_).
#
# MAC Model Parameters:
#   - CWMin_: Minimum contention window size.
#   - CWMax_: Maximum contention window size.
#   - SlotTime_: Slot time duration (seconds).
#   - SIFS_: Short Interframe Space duration (seconds).
#   - ShortRetryLimit_: Maximum short frame retransmissions.
#   - LongRetryLimit_: Maximum long frame retransmissions.
#   - HeaderDuration_: Duration of the MAC header (seconds).
#   - SymbolDuration_: Duration of a symbol (seconds).
#   - BasicModulationScheme_: Modulation scheme identifier.
#   - use_802_11a_flag_: Enable/disable 802.11a mode.
#   - RTSThreshold_: RTS threshold (bytes).
#   - MAC_DBG: MAC layer debug flag.
#
# Adjust these parameters as needed for your simulation scenario.
# ------------------------------------------------------------------------------

$val(netif) set CPThresh_                10.0        ;# capture threshold in Watt
$val(netif) set CSThresh_                1.559e-11   ;# Carrier Sensing threshold
$val(netif) set RXThresh_                3.652e-10   ;# receiver signal threshol
$val(netif) set Pt_                      0.28        ;# transmitter signal power (Watt)        
$val(netif) set freq_                    2.4e9       ;# channel frequency (Hz)
$val(netif) set noise_floor_             2.512e-13   ;#-96 dBm for 10MHz bandwidth
$val(netif) set L_                       1.0         ;#default radio circuit gain/loss
$val(netif) set PowerMonitorThresh_      1.259e-13   ;#-99dBm power monitor  sensitivity
$val(netif) set HeaderDuration_          0.000020    ;#20 us
$val(netif) set BasicModulationScheme_   0
$val(netif) set PreambleCaptureSwitch_   1
$val(netif) set DataCaptureSwitch_       0
$val(netif) set SINR_PreambleCapture_    2.5118;     ;# 4 dB
$val(netif) set SINR_DataCapture_        100.0;      ;# 10 dB
$val(netif) set trace_dist_              1e6         ;# PHY trace until distance of 1 Mio. km ("infinty")
$val(netif) set PHY_DBG_                 $val(dbg)   ;# piloté par -dbg
$val(netif) set MAX_PRIO_NB              $maxPrioNb_

# ------------------------------------------------------------------------------
# Mac model
# ------------------------------------------------------------------------------

$val(mac) set CWMin_                     31
$val(mac) set CWMax_                     1023
$val(mac) set SlotTime_                  0.000020    ;# 20us
$val(mac) set SIFS_                      0.000010    ;# 10us
$val(mac) set ShortRetryLimit_           7           ;# retransmissions
$val(mac) set LongRetryLimit_            4           ;# retransmissions
$val(mac) set HeaderDuration_            0.000020
$val(mac) set SymbolDuration_            0.000004
$val(mac) set BasicModulationScheme_     0
$val(mac) set use_802_11a_flag_          true
$val(mac) set RTSThreshold_              3000        ;# bytes
$val(mac) set MAC_DBG                    $val(dbg)   ;# piloté par -dbg
