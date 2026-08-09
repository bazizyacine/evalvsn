#!/usr/bin/env bash

# Usage: ns2TrStats.sh -tr trace_file
#
# Reads an ns-2 trace file and produces four tables:
#   Tab1: overall video packet statistics (sends, receives, corrected unique
#         losses plus the UID list for them, routing overhead).
#   Tab2: video drops by reason and layer, **only** for the unique lost UIDs,
#         with columns: Drop Reason (Category) | List (UIDs)
#   Tab3: grouping over **the same unique lost UIDs**, with columns:
#         UID | dropReason | packetType | frameType | priority | Count,
#         sorted by ascending UID.
#   Tab4: mean end-to-end delay and energy metrics.
#
# Only option: -tr trace_file
#
# shellcheck disable=SC1112
#   The awk program below is delimited by single quotes. Any typographic
#   apostrophe (U+2019) left in its comments or strings is deliberate:
#   replacing one with an ASCII apostrophe would close the string and break
#   the script. Keep the comments inside the awk block apostrophe-free.

if [[ $1 == "-h" || $1 == "--help" || $# -lt 2 ]]; then
  echo "Usage: $0 -tr trace_file"
  exit 0
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    -tr) traceFile="$2"; shift 2 ;;
    *)   echo "Usage: $0 -tr trace_file" >&2; exit 1 ;;
  esac
done

if [[ ! -f "$traceFile" ]]; then
  echo "Error: file '$traceFile' not found." >&2
  exit 1
fi

awk '
BEGIN {
  FS = "[ \t]+"
  OFS = "\t"

  ############################################
  # Tab1: overall video packet statistics
  ############################################
  sends       = 0
  recvs       = 0
  overheads   = 0
  delete sentUID
  delete recvUID

  ############################################
  # For Tab1 and Tab2/Tab3:
  #   - uniqueUIDs[u]: flag every UID counted as a unique loss
  #   - uniqueUIDList : liste CSV d’UIDs uniques perdus
  ############################################
  delete uniqueUIDs
  uniqueUIDList = ""

  ############################################
  # Tab2: drop detail
  #   - tab2Map[label] = total drops, across all UIDs
  #   - tab2UIDList[label] = CSV list of all UIDs, per label
  #   - tab2Seen[key2]: guards against counting a (UID, label) pair twice
  ############################################
  delete tab2Map
  delete tab2UIDList
  delete tab2Seen
  delete dropUIDs    # flag any UID that was dropped, for whatever reason

  ############################################
  # Tab3: all drops grouped, over every initial UID
  #   - grouping[key] = counter, where
  #     key = label SUBSEP UID SUBSEP packetType SUBSEP frameType SUBSEP priority
  ############################################
  delete grouping

  ############################################
  # Tab4: mean end-to-end delay and the energy metrics
  ############################################
  sumDelay           = 0
  recvnum            = 0
  initial_energy     = 0
  total_energy       = 0
  nn                 = 0
  sum_ind_dev_square = 0
  delete res_energy
  delete cons_energy
}

{
  ###################################
  # Reconstruction du tableau kv2 (paires -flag valeur)
  ###################################
  delete kv2
  for (j = 2; j <= NF; j++) {
    if ($j ~ /^-[A-Za-z]+$/ && (j + 1) <= NF) {
      k2      = substr($j, 2)
      v2      = $(j + 1)
      kv2[k2] = v2
      j++
    }
  }

  ###################################
  # Phase A: corrected Tab1, plus the energy collection Tab4 needs
  ###################################
  # "s" line: AGT sends a video packet -> remember the UID sent
  if ($1 == "s" && kv2["Nl"] == "AGT" && kv2["It"] == "video") {
    sends++
    u = kv2["Ii"]
    sentUID[u] = 1
    if (start_time[u] == "") {
      start_time[u] = $3
    }
  }

  # "r" line: AGT receives a video packet -> remember the UID received
  if ($1 == "r" && kv2["Nl"] == "AGT" && kv2["It"] == "video") {
    recvs++
    u = kv2["Ii"]
    recvUID[u] = 1
    end_time[u] = $3
  }

  # Overhead de routage : "s" ou "f" niveau RTR, It=AOMDV_QPV
  if (( $1 == "s" || $1 == "f" ) && kv2["Nl"] == "RTR" && kv2["It"] == "AOMDV_QPV") {
    overheads++
  }

  ###################################
  # Phase B: Tab2, every drop collected raw, filling tab2Map and tab2UIDList
  ###################################
  if ($1 == "d" && kv2["It"] == "video") {
    L = kv2["Nl"]
    R = kv2["Nw"]

    # Construction du label de drop (identique Tab2/Tab3)
    label = ""
    if (R == "END")                                       label = "DROP_END_OF_SIMULATION"
    else if (L == "PHY" && R == "INT")                    label = "DROP_PHY_INT"
    else if (L == "PHY" && R == "TXB")                    label = "DROP_PHY_TXB"
    else if (L == "PHY" && R == "SXB")                    label = "DROP_PHY_SXB"
    else if (L == "PHY" && R == "PXB")                    label = "DROP_PHY_PXB"
    else if (L == "PHY" && R == "RXB")                    label = "DROP_PHY_RXB"
    else if (L == "PHY" && R == "BND")                    label = "DROP_PHY_BND"
    else if (L == "PHY" && R == "PND")                    label = "DROP_PHY_PND"
    else if (L == "MAC" && R == "COL")                    label = "DROP_MAC_COLLISION"
    else if (L == "MAC" && R == "DUP")                    label = "DROP_MAC_DUPLICATE"
    else if (L == "MAC" && R == "ERR")                    label = "DROP_MAC_PACKET_ERROR"
    else if (L == "MAC" && R == "RET")                    label = "DROP_MAC_RETRY_COUNT_EXCEEDED"
    else if (L == "MAC" && R == "STA")                    label = "DROP_MAC_INVALID_STATE"
    else if (L == "MAC" && R == "BSY")                    label = "DROP_MAC_BUSY"
    else if (L == "MAC" && R == "DST")                    label = "DROP_MAC_INVALID_DST"
    else if (L == "MAC" && R == "SLP")                    label = "DROP_MAC_SLEEP"
    else if (L == "MAC" && R == "NAV")                    label = "DROP_MAC_NAV"
    else if (L == "MAC" && R == "XTIM")                   label = "DROP_MAC_TXTIME_ERR"
    else if (L == "RTR" && R == "NRTE")                   label = "DROP_RTR_NO_ROUTE"
    else if (L == "RTR" && R == "LOOP")                   label = "DROP_RTR_ROUTE_LOOP"
    else if (L == "RTR" && R == "TTL")                    label = "DROP_RTR_TTL"
    else if (L == "RTR" && R == "TOUT")                   label = "DROP_RTR_QTIMEOUT"
    else if (L == "RTR" && R == "IFQ")                    label = "DROP_RTR_QFULL"
    else if (L == "RTR" && R == "CBK")                    label = "DROP_RTR_MAC_CALLBACK"
    else if (L == "RTR" && R == "SAL")                    label = "DROP_RTR_SALVAGE"
    else if (L == "RTR" && R == "HELO")                   label = "DROP_RTR_HELLO"
    else if (L == "RTR" && R == "REXP")                   label = "DROP_RTR_RTEXPIRE"
    else if (L == "IFQ" && R == "---")                    label = "DROP_IFQ_QFULL"
    else if (L == "IFQ" && R == "ARP")                    label = "DROP_IFQ_ARP_FULL"
    else if (L == "IFQ" && R == "FIL")                    label = "DROP_IFQ_FILTER"
    else if (L == "IFQ" && R == "REPL")                   label = "DROP_IFQ_QFULL_REPL"
    else if (L == "IFQ" && R == "NEW")                    label = "DROP_IFQ_QFULL_NEW"
    else if (L == "IFQ" && R == "SHRK")                   label = "DROP_IFQ_QFULL_SHRK"
    else if (L == "IFQ" && R == "MANU")                   label = "DROP_IFQ_MANUAL_DROP"
    else if (R == "OUT")                                  label = "DROP_OUTSIDE_SUBNET"
    else                                                  label = "DROP_UNKNOWN"

    u    = kv2["Ii"]
    key2 = u SUBSEP label

    # -- Raw count, for the original Tab2 --
    if (!(key2 in tab2Seen)) {
      tab2Seen[key2] = 1
      tab2Map[label]++
      # Store every UID, not only the unique ones, per category
      if (tab2UIDList[label] == "") {
        tab2UIDList[label] = u
      } else {
        tab2UIDList[label] = tab2UIDList[label] "," u
      }
    }

    # Flag any UID dropped at least once, for Tab3
    dropUIDs[u] = 1
  }

  ###################################
  # Phase C: Tab3, grouping every video drop
  ###################################
  if ($1 == "d" && kv2["It"] == "video") {
    L = kv2["Nl"]
    R = kv2["Nw"]

    # Rebuild the label, as Tab2 does
    label = ""
    if (R == "END")                                       label = "DROP_END_OF_SIMULATION"
    else if (L == "PHY" && R == "INT")                    label = "DROP_PHY_INT"
    else if (L == "PHY" && R == "TXB")                    label = "DROP_PHY_TXB"
    else if (L == "PHY" && R == "SXB")                    label = "DROP_PHY_SXB"
    else if (L == "PHY" && R == "PXB")                    label = "DROP_PHY_PXB"
    else if (L == "PHY" && R == "RXB")                    label = "DROP_PHY_RXB"
    else if (L == "PHY" && R == "BND")                    label = "DROP_PHY_BND"
    else if (L == "PHY" && R == "PND")                    label = "DROP_PHY_PND"
    else if (L == "MAC" && R == "COL")                    label = "DROP_MAC_COLLISION"
    else if (L == "MAC" && R == "DUP")                    label = "DROP_MAC_DUPLICATE"
    else if (L == "MAC" && R == "ERR")                    label = "DROP_MAC_PACKET_ERROR"
    else if (L == "MAC" && R == "RET")                    label = "DROP_MAC_RETRY_COUNT_EXCEEDED"
    else if (L == "MAC" && R == "STA")                    label = "DROP_MAC_INVALID_STATE"
    else if (L == "MAC" && R == "BSY")                    label = "DROP_MAC_BUSY"
    else if (L == "MAC" && R == "DST")                    label = "DROP_MAC_INVALID_DST"
    else if (L == "MAC" && R == "SLP")                    label = "DROP_MAC_SLEEP"
    else if (L == "MAC" && R == "NAV")                    label = "DROP_MAC_NAV"
    else if (L == "MAC" && R == "XTIM")                   label = "DROP_MAC_TXTIME_ERR"
    else if (L == "RTR" && R == "NRTE")                   label = "DROP_RTR_NO_ROUTE"
    else if (L == "RTR" && R == "LOOP")                   label = "DROP_RTR_ROUTE_LOOP"
    else if (L == "RTR" && R == "TTL")                    label = "DROP_RTR_TTL"
    else if (L == "RTR" && R == "TOUT")                   label = "DROP_RTR_QTIMEOUT"
    else if (L == "RTR" && R == "IFQ")                    label = "DROP_RTR_QFULL"
    else if (L == "RTR" && R == "CBK")                    label = "DROP_RTR_MAC_CALLBACK"
    else if (L == "RTR" && R == "SAL")                    label = "DROP_RTR_SALVAGE"
    else if (L == "RTR" && R == "HELO")                   label = "DROP_RTR_HELLO"
    else if (L == "RTR" && R == "REXP")                   label = "DROP_RTR_RTEXPIRE"
    else if (L == "IFQ" && R == "---")                    label = "DROP_IFQ_QFULL"
    else if (L == "IFQ" && R == "ARP")                    label = "DROP_IFQ_ARP_FULL"
    else if (L == "IFQ" && R == "FIL")                    label = "DROP_IFQ_FILTER"
    else if (L == "IFQ" && R == "REPL")                   label = "DROP_IFQ_QFULL_REPL"
    else if (L == "IFQ" && R == "NEW")                    label = "DROP_IFQ_QFULL_NEW"
    else if (L == "IFQ" && R == "SHRK")                   label = "DROP_IFQ_QFULL_SHRK"
    else if (L == "IFQ" && R == "MANU")                   label = "DROP_IFQ_MANUAL_DROP"
    else if (R == "OUT")                                  label = "DROP_OUTSIDE_SUBNET"
    else                                                  label = "DROP_UNKNOWN"

    uidFld    = kv2["Ii"] + 0        # numeric, for sorting
    pktTypeFld= kv2["It"]            # "video"
    frTypeFld = kv2["Frt"]           # type de trame
    prioFld   = kv2["Pri"]           # priority

    # Raw grouping count, over all UIDs
    key = label SUBSEP uidFld SUBSEP pktTypeFld SUBSEP frTypeFld SUBSEP prioFld
    grouping[key]++
  }

  ###################################
  # Phase D: Tab4, energy collection
  ###################################
  if ($1 == "N") {
    node   = $5 + 0
    energy = $7 + 0
    if (energy > initial_energy) {
      initial_energy = energy
    }
    res_energy[node] = energy
  }
}

END {
  ########################################
  # 1) Corrected Tab1:
  #    - unique lost UIDs: those in sentUID but not in recvUID
  #    - construction de uniqueUIDList (CSV)
  ########################################
  uniqueCount = 0
  for (u in sentUID) {
    if (!(u in recvUID)) {
      uniqueCount++
      uniqueUIDs[u] = 1
      if (uniqueUIDList == "") {
        uniqueUIDList = u
      } else {
        uniqueUIDList = uniqueUIDList "," u
      }
    }
  }

  ########################################
  # Impression : Tab1
  ########################################
  print ""
  print "=== Tab1: overall video packet statistics ==="
  # Three-column header: Description | UID list | Value
  printf "%-40s| %-30s| %10s\n", "Description", "Liste (UIDs)", "Valeur"
  printf "%-40s-+-%-30s-+-%10s\n", "----------------------------------------", "------------------------------", "----------"
  printf "%-40s| %-30s| %10d\n",  "nb. of video packets sends",       "", sends
  printf "%-40s| %-30s| %10d\n",  "nb. of video packets recvs",       "", recvs
  # Parenthesise the list
  printf "%-40s| %-30s| %10d\n",
         "nb. of unique video packet drops",
         "(" uniqueUIDList ")",
         uniqueCount
  printf "%-40s| %-30s| %10.4f\n", "nb. normalized routing overhead",   "", (recvs > 0 ? overheads/recvs : 0)

  ########################################
  # 2) Tab2: only the unique lost UIDs
  #    - the Count column is dropped; only Drop Reason and the UID list remain
  ########################################
  delete tab2UniqueUIDs     # tab2UniqueUIDs[label] = CSV of unique UIDs for that label
  delete tab2UniqueCount    # tab2UniqueCount[label] = how many unique UIDs for that label

  for (label in tab2UIDList) {
    split(tab2UIDList[label], allUIDs, ",")
    tmpList = ""
    cnt     = 0
    for (i = 1; i <= length(allUIDs); i++) {
      u = allUIDs[i]
      if ((u + 0) in uniqueUIDs) {
        cnt++
        if (tmpList == "") {
          tmpList = u
        } else {
          tmpList = tmpList "," u
        }
      }
    }
    if (cnt > 0) {
      tab2UniqueUIDs[label]  = tmpList
      tab2UniqueCount[label] = cnt
    }
  }

  # Sorted by unique UID count, descending (Count is no longer printed,
  # but it still drives the ordering)
  n2 = asorti(tab2UniqueCount, sorted2, "@val_num_desc")

  print ""
  print "=== Tab2: unique video drops by reason and layer, descending ==="
  # Header, Count column included
  printf "%-50s| %-30s| %10s\n", "Drop Reason (category)", "UID list", "Count"
  printf "%-50s-+-%-30s-+-%10s\n", "--------------------------------------------------", "------------------------------", "----------"
  for (i = 1; i <= n2; i++) {
    label = sorted2[i]
    uids  = tab2UniqueUIDs[label]
    cnt   = tab2UniqueCount[label]
    printf "%-50s| %-30s| %10d\n", label, "(" uids ")", cnt
  }

  ########################################
  # 3) Tab3: group ONLY the unique lost UIDs
  ########################################
  idx3 = 0
  for (gk in grouping) {
    split(gk, arr, SUBSEP)
    label     = arr[1]
    uidFld    = arr[2] + 0
    pktTypeFld= arr[3]
    frTypeFld = arr[4]
    prioFld   = arr[5]
    cnt       = grouping[gk] + 0

    if (uidFld in uniqueUIDs) {
      idx3++
      groupingUID[idx3]   = uidFld
      groupingEntry[idx3] = sprintf("%-10d| %-25s| %-10s| %-10s| %-10s| %5d",
                                    uidFld, label, pktTypeFld, frTypeFld, prioFld, cnt)
    }
  }
  n3 = asorti(groupingUID, sorted3, "@val_num_asc")

  print ""
  print "=== Tab3: grouping table for the unique lost UIDs, by ascending UID ==="
  printf "%-10s| %-25s| %-10s| %-10s| %-10s| %5s\n", 
         "UID", "dropReason", "packetType", "frameType", "priority", "Count"
  printf "%-10s-+-%-25s-+-%-10s-+-%-10s-+-%-10s-+-%-5s\n",
         "----------", "-------------------------", "----------", "----------", "----------", "-----"
  for (i = 1; i <= n3; i++) {
    printf "%s\n", groupingEntry[sorted3[i]]
  }

  ########################################
  # 4) Tab4: end-to-end delay and the energy metrics, unchanged
  ########################################
  for (u in end_time) {
    dt = end_time[u] - start_time[u]
    if (dt > 0) {
      sumDelay += dt
      recvnum++
    }
  }
  avgDelay = (recvnum > 0 ? sumDelay/recvnum : 0)

  for (n in res_energy) {
    cons = initial_energy - res_energy[n]
    cons_energy[n] = cons
    total_energy += cons
    nn++
  }
  moyenne = (nn > 0 ? total_energy/nn : 0)

  for (n in cons_energy) {
    diff = cons_energy[n] - moyenne
    sum_ind_dev_square += diff * diff
  }
  variance = (nn > 0 ? sum_ind_dev_square/nn : 0)
  ecartType = (nn > 0 ? sqrt(variance) : 0)

  if (initial_energy > 0) {
    min_res = ""
    for (n in res_energy) {
      if (min_res == "" || res_energy[n] < min_res) {
        min_res = res_energy[n]
      }
    }
    ratioMin = (min_res / initial_energy) * 100
  } else {
    ratioMin = 0
  }

  energyPerPkt = (recvs > 0 ? total_energy/recvs : 0)

  print ""
  print "=== Tab4: mean end-to-end delay and energy metrics ==="
  printf "%-40s| %10s\n", "Description", "Valeur"
  printf "%-40s-+-%10s\n", "----------------------------------------", "----------"
  printf "%-40s| %10.4f\n", "average end to end delay",           avgDelay
  printf "%-40s| %10.4f\n", "total energy consumed (all nodes)",    total_energy
  printf "%-40s| %10.4f\n", "energy consumed per node (mean)",      moyenne
  printf "%-40s| %10.4f\n", "per-node consumption, standard deviation",    ecartType
  printf "%-40s| %10.4f\n", "minimum residual energy ratio (%)", ratioMin
  printf "%-40s| %10.4f\n", "energy consumed per packet",            energyPerPkt
}
' "$traceFile"

exit 0
