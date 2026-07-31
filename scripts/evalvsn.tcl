#------------------------------------------------------------------------------
# evalvsn.tcl - NS2 Simulation Script for Video Sensor Networks (VSN)
#
# Description:
#   This script sets up and runs a wireless ad hoc network simulation in NS2,
#   specifically designed for evaluating video sensor network (VSN) scenarios.
#   It supports two routing protocols: AOMDV and AOMDV_QPV, and dynamically
#   configures simulation parameters based on input trace files and user
#   arguments.
#
# Main Features:
#   - Parses command-line arguments to override default simulation parameters.
#   - Reads a sender trace file to determine the maximum video packet priority
#     and total number of video packets.
#   - Dynamically calculates the interface queue length (IFQ) based on the
#     number of video packets and simulation time, rounding down to the nearest
#     power of two.
#   - Configures wireless node parameters, energy models, and topologies
#     (Grid or Chain).
#   - Loads and configures the appropriate routing protocol (AOMDV or
#     AOMDV_QPV), including protocol-specific parameters and trace files.
#   - Initializes nodes, attaches agents (UDP, Sink), and sets up video
#     traffic using a trace-driven application.
#   - Sets up tracing for simulation and network animator (NAM).
#   - Schedules simulation events, including traffic start/stop, file closure,
#     and simulation termination.
#
# Usage:
#   ns evalvsn.tcl -st <st> -rt <rt> -simt <simt> -nn <nn> -rp <rp>
#   If no arguments are provided, default parameters are used.
#
# Parameters:
#   -st     : Sender trace file (default: "st-packet.dat")
#   -rt     : Receiver trace file (default: "rt-packet.dat")
#   -simt   : Simulation time in seconds (default: 12)
#   -nn     : Number of nodes (default: 100)
#   -rp     : Routing protocol ("AOMDV" or "AOMDV_QPV", default: "AOMDV_QPV")
#   -dbg    : Cross-layer debug traces, 1 or 0 (default: 1)
#             Controls AOMDV_QPV_DBG, IFQ_DBG, and, through aomdv_qpv.tcl,
#             PHY_DBG_ and MAC_DBG. These traces are what makes an AOMDV_QPV
#             log reach 1.3 GB for a twelve-second run, against 161 KB for a
#             whole AOMDV set, where the same flags are already 0.
#             The default of 1 reproduces the historical behaviour exactly;
#             pass -dbg 0 for production runs that do not need the traces.
#
# Notes:
#   - Requires the presence of "aomdv.tcl" or "aomdv_qpv.tcl" for protocol
#     configuration.
#   - The script is designed for research and evaluation of VSN routing and
#     queue management strategies.
#   - The script is written in TCL for use with the NS2 network simulator.
#
# Author:
#   [M.Y BAZIZ]
#------------------------------------------------------------------------------

#============================================================================== 
# Initialize 
#==============================================================================

#––– 1. Defaults –––
set val(st)   "st-packet.dat"
set val(rt)   "rt-packet.dat"
set val(simt) 12
set val(nn)   100
set val(rp)  "AOMDV_QPV"
set val(dbg)  1

#––– 2. Check arguments –––––
# Les options vont par paires « -clé valeur » : on exige un nombre pair plutôt
# qu'un compte figé, pour que l'ajout d'une option comme -dbg reste possible.
if {$argc == 0} {
    puts "Starting with all default parameters."
} elseif {$argc % 2 != 0} {
    puts "Wrong number of command-line arguments."
    puts "Usage: ns evalvsn.tcl -st <st> -rt <rt> -simt <simt> -nn <nn> -rp <rp> \[-dbg 0|1\]"
    exit 0
}

#––– 3. Parsing overrides –––––
proc getval {argc argv} {
     global val
     lappend st rt simt nn rp
 
     for {set i 0} {$i < $argc} {incr i} {
          set arg [lindex $argv $i]
          if {[string range $arg 0 0] != "-"} continue
          set ID [string range $arg 1 end]
          set val($ID) [lindex $argv [expr $i+1]]
     }
}

getval $argc $argv

# Fonction pour obtenir le maxPrioNb et le nombre total de paquets vidéo (maxSeqNb)
proc getPrioAndPktCount {filename} {
    set maxPrioNb 0
    set maxSeqNb 0
    set file [open $filename r]
    while {[gets $file line] >= 0} {
        # Sauter les lignes de commentaire
        if {[string match "#*" $line]} {
            continue
        }

        set fields [split $line "\t"]
        set prioNb [lindex $fields 5]
        set seqNb [lindex $fields 1]

        if {$prioNb > $maxPrioNb} {
            set maxPrioNb $prioNb
        }

        if {$seqNb > $maxSeqNb} {
            set maxSeqNb $seqNb
        }
    }
    close $file
    # maxSeqNb commence à 0, donc +1 pour le nombre total de paquets
    return [list $maxPrioNb [expr {$maxSeqNb + 1}]]
}

# Utilisation de la nouvelle fonction
set prio_pkt [getPrioAndPktCount $val(st)]
set maxPrioNb_ [lindex $prio_pkt 0]
set maxSeqNb_ [lindex $prio_pkt 1]
puts "Max Priority Number (maxPrioNb_): $maxPrioNb_"
puts "Nombre de paquets vidéo à transmettre (maxSeqNb_): $maxSeqNb_"

# Calcule dynamiquement la taille de la file d'attente IFQ selon la formule demandée
proc lower_pow2 {n} {
    # Trouve la puissance de 2 la plus proche en bas de n
    set pow 1
    while {$pow * 2 <= $n} {
        set pow [expr {$pow * 2}]
    }
    return $pow
}

# Utilisation de la fonction pour calculer val(ifqlen)
set ifqlen_calc [expr {int(ceil(double($maxSeqNb_) / $val(simt)))}]
# Trouver la puissance de 2 la plus proche en bas
set ifqlen_ [lower_pow2 $ifqlen_calc]
puts "Taille dynamique de la file d'attente IFQ (val(ifqlen)) : $ifqlen_"

#––– 4. Echo all five parameters –––
puts "Simulation time: $val(simt) sec, \
nodes: $val(nn), \
routing: $val(rp), \
sender trace: $val(st), \
recv trace: $val(rt)"

#==============================================================================
# Parameter for wireless nodes
#==============================================================================

set val(chan)       Channel/WirelessChannel       ;# Channel type
set val(prop)       Propagation/TwoRayGround      ;# Radio-propagation model
set val(ant)        Antenna/OmniAntenna           ;# Antenna type
set val(ll)         LL                            ;# Link layer type
set val(ifqlen)     $ifqlen_                      ;# Max packet in ifq
set val(nn)         $val(nn)                      ;# Number of mobile nodes
set val(rp)	        $val(rp)                      ;# Routing protocol
set val(topo)       Grid                          ;# Topography Grid or Chain
set val(dist)       100                           ;# Distance in meters
set val(engmodel)   EnergyModel
set val(txPower)    0.063                         ;# Transmitting power in Watt
set val(rxPower)    0.057                         ;# Recving power in Watt
set val(sleepPower) 0.001                         ;# Sleep power in Watt
set val(idlePower)  0.006                         ;# Idle power in Watt
set val(initeng)    1                             ;# Initial energy in Joules
set val(simt)       $val(simt)                    ;# Simulation time in Secondes
# set val(rndSeed)  500                           ;# random seed for traffic pairs

# =============================================================================
# Models for PHY & MAC & Channel with Default Parameters in IEEE802.11 aomdv/aomdv_qpv
# =============================================================================

# Load the appropriate IEEE 802.11 model based on the user's choice
if {[file exists "aomdv.tcl"] && $val(rp) == "AOMDV"} {
    set val(ifq)        Queue/DropTail/PriQueue       ;# Interface queue type
    set val(netif)      Phy/WirelessPhy               ;# Network interface type
    set val(mac)        Mac/802_11                    ;# MAC type
    source "aomdv.tcl"
} elseif {[file exists "aomdv_qpv.tcl"] && $val(rp) == "AOMDV_QPV"} {
    set val(ifq)        Queue/priqueueEvalVSN         ;# Interface queue type
    set val(netif)      Phy/WirelessPhyEvalVSN        ;# Network interface type
    set val(mac)        Mac/802_11EvalVSN             ;# MAC type
    source "aomdv_qpv.tcl"
} else {
    puts "Error: Could not find aomdv.tcl or aomdv_qpv.tcl file."
    exit 1
}

# ------------------------------------------------------------------------------
# Channel model
# ------------------------------------------------------------------------------

$val(ant) set X_    0
$val(ant) set Y_    0
$val(ant) set Z_    1.5
$val(ant) set Gt_   1            ;# transmitter antenna gain
$val(ant) set Gr_   1            ;# receiver antenna gain

# ------------------------------------------------------------------------------
# Network interface type model AOMDV_QPV
# ------------------------------------------------------------------------------

if {$val(rp) == "AOMDV_QPV"} {
    Agent/$val(rp) set aomdv_qpv_max_paths_                 [expr 2 * $maxPrioNb_]  
    Agent/$val(rp) set aomdv_qpv_prim_alt_path_len_diff_    [expr 2 * $maxPrioNb_] 
    Agent/$val(rp) set AOMDV_QPV_DBG                        $val(dbg)

    # --- Création d'un fichier de tr pour AOMDV ---
    set aomdv_qpv_tr [open aomdv_qpv.tr w]
    set aomdv_qpv_trace [new Trace/Generic]
    $aomdv_qpv_trace attach $aomdv_qpv_tr
}

# ------------------------------------------------------------------------------
# Interface queue type model
# ------------------------------------------------------------------------------

$val(ifq) set drop_front_       false
$val(ifq) set summarystats_     true
$val(ifq) set queue_in_bytes_   false
$val(ifq) set mean_pktsize_     0
$val(ifq) set IFQ_DBG           $val(dbg)

#==============================================================================
# Main Program
#==============================================================================

# packet trace Video
puts "\n*** $val(st) ***\n"

# *** Initialize Simulator ***
set ns_ [new Simulator]

# Générer une seed aléatoire en TCL
# set range $val(rndSeed)
# set minimum 0
# set rndSeed [expr {int(rand()*$range) + $minimum}]
# puts "Random seed set to $rndSeed"

# Set the random seed for the simulator
# ns-random $rndSeed
    
$ns_ use-newtrace

# *** Initialize Trace file ***
set tracefd [open st-packet.tr w]
$ns_ trace-all $tracefd

# topography
set gridspace [expr int(sqrt($val(nn)))]                ;# Grid of topography
set rlen      [expr int($val(nn) - pow($gridspace,2))]  ;# Rlen of topography
set val(x)    [expr ($gridspace + $rlen) * $val(dist)]  ;# X dimension of topography
set val(y)    [expr ($gridspace + $rlen) * $val(dist)]  ;# Y dimension of topography

# *** Initialize Network Animator ***
set namtrace [open st-packet.nam w]
$ns_ namtrace-all-wireless $namtrace $val(x) $val(y)

# *** set up topography object ***
set topo [new Topography]
$topo load_flatgrid $val(x) $val(y)

# Create  General Operations Director (GOD) object. It is used to store global information about the state of the environment, network, or nodes that an
# omniscent observer would have, but that should not be made known to any participant in the simulation.
set god_ [create-god $val(nn)]

# CONFIGURE AND CREATE NODES
$ns_ node-config -adhocRouting $val(rp) \
                 -llType $val(ll) \
                 -macType $val(mac) \
                 -ifqType $val(ifq) \
                 -ifqLen $val(ifqlen) \
                 -antType $val(ant) \
                 -propType $val(prop) \
                 -phyType $val(netif) \
                 -channelType $val(chan) \
                 -topoInstance $topo \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON \
                 -phyTrace ON \
                 -movementTrace ON \
                 -energyModel $val(engmodel) \
                 -rxPower $val(rxPower) \
                 -txPower $val(txPower) \
                 -sleepPower $val(sleepPower) \
                 -idlePower $val(idlePower) \
                 -initialEnergy $val(initeng)

# Define a 'finish' procedure
proc finish { } {
     global ns_ tracefd namtrace
    $ns_ flush-trace
    flush $tracefd
    flush $namtrace
    close $tracefd
    close $namtrace
    exit 0
}

# Create Nodes
for {set i 0} {$i < $val(nn)} {incr i} {
     set node_($i) [$ns_ node]	
     $node_($i) random-motion 0
}

# --- Attacher le log-target à chaque agent AOMDV_QPV et lier MAC/PHY/IFQ (mono-interface) ---
if {$val(rp) == "AOMDV_QPV"} {
    for {set i 0} {$i < $val(nn)} {incr i} {
        # Récupérer l'agent de routage AOMDV_QPV du noeud
        set agentAOMDV_QPV [$node_($i) set ragent_]
        # Récupérer le MAC du noeud (mono-interface)
        set macEvalVSN [$node_($i) set mac_(0)]
        # Récupérer le PHY du noeud (mono-interface)
        set phyEvalVSN [$node_($i) set netif_(0)]
        # Récupérer la file d'attente du noeud (mono-interface)
        set ifqEvalVSN [$node_($i) set ifq_(0)]
        # Lie l'IFQ à AOMDV_QPV en passant l'adresse
        set nexthop_addr [$node_($i) id]

        if {$agentAOMDV_QPV != ""} {
            $agentAOMDV_QPV log-target $aomdv_qpv_trace
            $agentAOMDV_QPV set-rtq-max-len $val(ifqlen)

            if {$macEvalVSN != ""} {
                $agentAOMDV_QPV setMac $macEvalVSN
            }

            if {$ifqEvalVSN != ""} {
                $agentAOMDV_QPV if-queue $ifqEvalVSN
                $ifqEvalVSN setAOMDV_QPV $agentAOMDV_QPV
                $agentAOMDV_QPV register_ifq_for_nexthop $nexthop_addr $ifqEvalVSN
            }
        }

        if {$macEvalVSN != "" && $phyEvalVSN != ""} {
            $macEvalVSN setPhy $phyEvalVSN
        }
    }
}

# Provide initial (X,Y,Z = 0) co-ordinates for mobilenodes
if {$val(topo) == "Grid"} {
    for {set i 0} {$i < $gridspace} {incr i} { 
        for {set j 0} {$j < $gridspace + $rlen} {incr j} {
            if {[expr $i * ($gridspace + $rlen) + $j] < $val(nn)} {
                set ln [expr $i * ($gridspace + $rlen) + $j]
                $node_($ln) set X_ [expr $i * $val(dist)]
                $node_($ln) set Y_ [expr $j * $val(dist)]
                $node_($ln) set Z_ 0.0
            }
        }
    }
} elseif {$val(topo) == "Chain"} {
        set ln 5
        $node_(0) set X_ 50.0
        $node_(0) set Y_ 80.0
        $node_(0) set Z_ 0.0
        $node_(1) set X_ 150.0
        $node_(1) set Y_ 120.0
        $node_(1) set Z_ 0.0
        $node_(2) set X_ 300.0
        $node_(2) set Y_ 120.0
        $node_(2) set Z_ 0.0
        $node_(3) set X_ 150.0
        $node_(3) set Y_ 50.0
        $node_(3) set Z_ 0.0
        $node_(4) set X_ 300.0
        $node_(4) set Y_ 50.0
        $node_(4) set Z_ 0.0
        $node_($ln) set X_ 490.0
        $node_($ln) set Y_ 80.0
        $node_($ln) set Z_ 0.0
}

# Define traffic model
# 0 connecting to last node
set udp [new Agent/evalvsnUDP]
$udp set packetSize_ 0
$ns_ attach-agent $node_(0) $udp
$node_(0) label "VBR"
set sink [new Agent/evalvsnSink]
$sink set packetSize_ 0
$ns_ attach-agent $node_($ln) $sink
$node_($ln) label "Sink"
set video [new Application/Traffic/evalvsn]
$video set packetSize_ 0
$video attach-agent $udp
$ns_ connect $udp $sink

# Video setting
puts "Video start..."
# Traffic Video
set trace_file [new Tracefile]
$trace_file filename $val(st)
$video attach-tracefile $trace_file
$udp set_filename ns2_$val(st)
$sink set_filename $val(rt)

# Define node initial position in nam
for {set i 0} {$i < $val(nn)} {incr i} {
     $ns_ initial_node_pos $node_($i) 30
}

# tell all nodes when the simulation ends
for {set i 0} {$i < $val(nn)} {incr i} {
     $ns_ at $val(simt) "$node_($i) reset"
}

$ns_ at 0.0 "$video start"
$ns_ at $val(simt) "$video stop"
$ns_ at $val(simt) "$udp closefile"
$ns_ at $val(simt) "$sink closefile"
$ns_ at $val(simt) "$udp printstatus"
$ns_ at $val(simt) "$sink printstatus"
$ns_ at $val(simt).0001 "finish"
$ns_ at $val(simt).0002 "puts \"NS EXITING...\" ; $ns_ halt"

# Run the simulation
puts "Starting Simulation..."
$ns_ run
