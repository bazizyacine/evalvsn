#!/bin/bash
#
# run_sets.sh - runs one or more EvalVSN experiment sets.
#
# Replaces sets.sh: sets are declared in sets.conf and named explicitly on the
# command line. No set is ever run implicitly, and an existing set is never
# overwritten without --force.
#
# Usage:
#   bash run_sets.sh --list
#   bash run_sets.sh set_AOMDV_QPV_GC-PN-0p03-13_p30-n100-d200
#   bash run_sets.sh set_AOMDV_GC-PN-0p03-13_p30-n100-d200 set_AOMDV_QPV_GC-PN-0p03-13_p30-n100-d200
#   bash run_sets.sh --dry-run set_AOMDV_QPV_GC-PN-0p03-13_p30-n100-d200
#
# Every option with a value takes the form --option=value.
#
# Options:
#   --list        list the declared sets and those already computed
#   --dry-run     print the commands without running anything
#   --force       allow an existing set to be overwritten
#   --resume      complete an existing set: keep its encoding and the
#                 realisations already harvested (sim/mt.dat), run the others,
#                 then the metrics over all of them. Each realisation is
#                 deterministic for its seed, so a kept one is the one the
#                 run would recompute (how a set goes from 10 to 20, 16 Oct
#                 2026).
#   --regen-avi   regenerate the .avi from the .yuv even if it exists
#   --quiet-log   filter the cross-layer traces out of the log
#   --prune       delete the heavy intermediates once the metrics are extracted.
#                 Per realisation and per configuration, st-packet.tr weighs some
#                 64 MB and st-packet.nam some 14 MB; neither is read again after
#                 setGraphs.pl has run, and a ten-realisation campaign over two
#                 protocols would otherwise leave about 19 GB behind - which is
#                 how the previous 19 GB accumulated. The measurements under
#                 graphs/ are what the article and the thesis read, and they are
#                 kept.
#   --sims=N      override the realisation count of every named set, without
#                 touching sets.conf. This is what makes the repository's
#                 probe-then-confirm method executable: --sims=1 brackets a
#                 frontier with deterministic single runs, then the bracketing
#                 points are re-measured at their declared count before any
#                 verdict. The declaration stays the record of what the set is.
#   --seed=N      ns-2 random seed. 0 (default) keeps ns-2's own sequence, so a
#                 set recomputed without this flag reproduces bit for bit. A
#                 positive value selects one realisation; averaging over several
#                 is what separates a result from single-run noise.
#   --dbg=N       ns-2 cross-layer traces: 1 (historical default) or 0.
#                 -dbg 0 removes the cause rather than the symptom: the two
#                 flags AOMDV_QPV_DBG and IFQ_DBG, which evalvsn.tcl sets, are
#                 what take a log from 161 KB to 1.3 GB.
#   --conf=F      read the declarations from F instead of ../sets.conf, and
#   --out=D       write the sets under D instead of ../sets, with
#   --ipdir=D     the encoder files under D instead of ../ipFiles and
#   --videodir=D  the videos under D instead of ../../evalvsn/videos. The four
#                 serve the test bench (../tools/bench.py), which keeps its own
#                 declarations, encoder files, videos and outputs out of the
#                 campaign's; a campaign never passes them.
#   --seed-name=N derive each realisation's seed from the set name N instead
#                 of the run set's own name: AOMDV then plays the very same
#                 fire, failures and traffic as the cross-layer set N (d39).
#   --pair-suffix=S  derive each realisation's seed from the set name with
#                 the trailing S removed: a variant set (a bench of a knob)
#                 then starts every realisation from the same seed as the
#                 campaign set it varies, and the two compare pairwise.

source "$(dirname "$(readlink -f "$0")")/lib/common.sh"

cd "$(dirname "$(readlink -f "$0")")" || exit 1

CONF="../sets.conf"; SETS_DIR="../sets"; IP_DIR="../ipFiles"; VIDEO_DIR="../../evalvsn/videos"
DRY_RUN=0; FORCE=0; RESUME=0; REGEN_AVI=0; QUIET_LOG=0; LIST=0; DBG=1; SEED=0; PRUNE=0; PAIR_SUFFIX=""; SEED_NAME=""
SETS=()

for arg in "$@"; do
  case "$arg" in
    --list)       LIST=1 ;;
    --dry-run)    DRY_RUN=1 ;;
    --force)      FORCE=1 ;;
    --resume)     RESUME=1; FORCE=1 ;;
    --regen-avi)  REGEN_AVI=1 ;;
    --quiet-log)  QUIET_LOG=1 ;;
    --prune)      PRUNE=1 ;;
    --dbg=*)      DBG="${arg#*=}" ;;
    --seed=*)     SEED="${arg#*=}" ;;
    --sims=*)     SIMS_OVERRIDE="${arg#*=}" ;;
    --conf=*)     CONF="${arg#*=}" ;;
    --pair-suffix=*) PAIR_SUFFIX="${arg#*=}" ;;
    --seed-name=*) SEED_NAME="${arg#*=}" ;;
    --out=*)      SETS_DIR="${arg#*=}" ;;
    --ipdir=*)    IP_DIR="${arg#*=}" ;;
    --videodir=*) VIDEO_DIR="${arg#*=}" ;;
    -h|--help)    sed -n '2,64p' "$0"; exit 0 ;;
    -*)           die "unknown option: $arg" ;;
    *)            SETS+=("$arg") ;;
  esac
done

require_files "$CONF"

# One campaign at a time in this directory. setGraphs.pl clears run/*.dat
# when it starts, and every campaign runs setGraphs from here, so a second
# instance deletes the first one's intermediates mid-flight. That has cost
# two campaigns already. The lock is held for the life of the run and
# released by the kernel however the script ends, kill included, so a
# crashed campaign never leaves a stale lock behind.
exec 9>".run_sets.lock"
if ! flock -n 9; then
	die "a campaign is already running in $PWD (lock: .run_sets.lock).
     Wait for it, or stop it first: two campaigns here erase each other's
     intermediates."
fi

# -------------------------------------------------------------------- catalogue
declare -A RP XL MPL NN VIDEO METRICS SIMS CAPTURE DIST FIRE PROP STANDBY KNOBS
DECLARED=()

# The trailing columns are optional: a declaration without them behaves
# exactly as before, computing everything over a single realisation of the
# static scenario on the historical propagation. The order tells the set's
# story (the author's rule of 24/08/2026): terrain, source, then the
# scenario played on it - propagation, standby regime, fire - and last the
# harvest: metrics second to last, sims closing the line.
while read -r name rp xl mpl nn dist capture video prop standby fire metrics sims knobs; do
  [[ -z "$name" || "$name" == \#* ]] && continue
  DECLARED+=("$name")
  KNOBS["$name"]="${knobs:-0}"
  RP["$name"]="$rp"; XL["$name"]="$xl"
  MPL["$name"]="$mpl"; NN["$name"]="$nn"; VIDEO["$name"]="$video"
  METRICS["$name"]="${metrics:-all}"; SIMS["$name"]="${sims:-1}"
  CAPTURE["$name"]="${capture:-0:0}"
  DIST["$name"]="${dist:-100}"
  PROP["$name"]="${prop:-tworay}"
  STANDBY["$name"]="${standby:-0}"
  FIRE["$name"]="${fire:-0}"
done < "$CONF"

# The knobs column: 0, or key=value pairs separated by commas, the keys being
# the stack knobs that evalvsn.tcl takes on its command
# line - ifqlen (packets), rule_power (0 or 1), hello_interval and rreq_timeout (seconds,
# AOMDV_QPV only), class_cost, hello_queue (0 or 1, AOMDV_QPV only: the ablation
# switches of the per-class cost and the HELLO queue field),
# rl_access (0 or 1: the channel-access agent), and the
# fire elements of decision d23 (10 Oct 2026): firewind (m/s) and firedir
# (degrees) for the wind-driven front, firealpha (dB/m) and firezone (s) for
# the heated zone ahead of it, firefail (probability) and firefailwin (s) for
# the failures before combustion; rl_path, rl_congestion, rl_break, rl_discovery, rl_power (0
# or 1: the learning agents of decision d22 that replace the chapter-4 rules),
# rule_discard and rl_discard (0 or 1: the early discard by priority of d27, rule or agent).
# Trailing, so every earlier declaration reads unchanged.
for n in "${DECLARED[@]}"; do
  [[ "${KNOBS[$n]}" == "0" ]] && continue
  IFS=',' read -r -a pairs <<< "${KNOBS[$n]}"
  for kv in "${pairs[@]}"; do
    [[ "$kv" =~ ^(ifqlen|rule_power|hello_interval|rreq_timeout|class_cost|hello_queue|rl_access|rl_path|rl_congestion|rl_break|rl_discovery|rl_power|rule_discard|rl_discard|firewind|firedir|firealpha|firezone|firefail|firefailwin)=[0-9.:]+$ ]] \
      || die "$n: invalid knobs spec \"${KNOBS[$n]}\" in $CONF (expected 0 or ifqlen=N,rule_power=0|1,hello_interval=S,rreq_timeout=S,class_cost=0|1,hello_queue=0|1,rl_access=0|1,rl_path=0|1,rl_congestion=0|1,rl_break=0|1,rl_discovery=0|1,rl_power=0|1,rule_discard=0|1,rl_discard=0|1,firewind=U,firedir=D,firealpha=A,firezone=S,firefail=P,firefailwin=S)"
  done
done

if [[ $LIST -eq 1 ]]; then
  printf '%-24s %-11s %-11s %-10s %-5s %-8s %s\n' \
         "SET" "PROTOCOL" "LABEL" "METRICS" "SIMS" "COMPUTED" "IPFILES"
  printf '%s\n' "---------------------------------------------------------------------------------------"
  for n in "${DECLARED[@]}"; do
    done_="no"; [[ -d "$SETS_DIR/$n" ]] && done_="YES"
    ip="ok"
    [[ -f "$IP_DIR/${XL[$n]}.dat" ]] || ip="MISSING"
    printf '%-24s %-11s %-11s %-10s %-5s %-8s %s\n' \
           "$n" "${RP[$n]}" "${XL[$n]}" "${METRICS[$n]}" "${SIMS[$n]}" "$done_" "$ip"
  done
  exit 0
fi

[[ ${#SETS[@]} -gt 0 ]] || die "no set named. \"$0 --list\" shows the declared sets."

for n in "${SETS[@]}"; do
  [[ -n "${RP[$n]:-}" ]] || die "unknown set in $CONF: $n"
  [[ "${SIMS[$n]}" =~ ^[0-9]+$ && "${SIMS[$n]}" -ge 1 ]] \
    || die "$n: sims must be a positive integer, got \"${SIMS[$n]}\""
  # Validated here rather than at the plotting stage: a typo must stop the run
  # before the encoding, not after several hours of it. "-metrics X -list"
  # exits non-zero when X names something the catalogue does not hold.
  perl ../metrics/setGraphs.pl -metrics "${METRICS[$n]}" -list >/dev/null \
    || die "$n: invalid metrics \"${METRICS[$n]}\" in $CONF."
  # The fire column: 0 (static scenario) or t0:v[:x:y]. A malformed spec must
  # stop the campaign before the encoding, like every other declaration error.
  [[ "${FIRE[$n]}" =~ ^(0|[0-9]+([.][0-9]+)?:[0-9]+([.][0-9]+)?(:[0-9]+([.][0-9]+)?:[0-9]+([.][0-9]+)?)?)$ ]] \
    || die "$n: invalid fire spec \"${FIRE[$n]}\" in $CONF (expected 0 or t0:v[:x:y])"
  # The propagation column: tworay, or shadowing with an optional exp:std.
  [[ "${PROP[$n]}" =~ ^(tworay|shadowing(:[0-9]+([.][0-9]+)?:[0-9]+([.][0-9]+)?)?)$ ]] \
    || die "$n: invalid prop spec \"${PROP[$n]}\" in $CONF (expected tworay or shadowing[:exp:std])"
  # The standby column: 0, or period:size in seconds and bytes.
  [[ "${STANDBY[$n]}" =~ ^(0|[0-9]+([.][0-9]+)?:[0-9]+)$ ]] \
    || die "$n: invalid standby spec \"${STANDBY[$n]}\" in $CONF (expected 0 or period:size)"
done

# ----------------------------------------------------------------- prerequisites
log_step "Checking prerequisites"
require_cmds ffmpeg perl ns
require_files ../../evalvsn/bin/evalvsn set.sh ../metrics/setGraphs.pl st-packetGenerate.sh ns2.sh evalvsn.tcl
[[ -x ../../evalvsn/bin/evalvsn ]] || die "../../evalvsn/bin/evalvsn is not executable."
# The Pham witness (Encoder=JPEG) runs two programs next to the tool; they
# build in a second and a set that needs them must not fail an hour in.
if [[ ! -x ../../evalvsn/pham/pham_encode || ! -x ../../evalvsn/pham/decode_to_bmp ]]; then
  make -s -C ../../evalvsn/pham >/dev/null 2>&1 || die "could not build the Pham programs in evalvsn/pham/ (make)."
fi

for n in "${SETS[@]}"; do
  # the bench's resized videos exist as .avi only, so either form is enough
  require_files "$IP_DIR/${XL[$n]}.dat"
  [[ -f "$VIDEO_DIR/${VIDEO[$n]}.yuv" || -f "$VIDEO_DIR/${VIDEO[$n]}.avi" ]] \
    || die "no $VIDEO_DIR/${VIDEO[$n]}.yuv nor .avi"
  if [[ -e "$SETS_DIR/$n" && $FORCE -eq 0 ]]; then
    die "$SETS_DIR/$n already exists. Move it, or re-run with --force to overwrite it."
  fi
done
log_info "ok - ${#SETS[@]} set(s): ${SETS[*]}"

log_info ""
log_info "Existing sets that will NOT be touched:"
untouched=0
for d in "$SETS_DIR"/*/; do
  # An unmatched glob comes back as the literal pattern, which printed a bare
  # "*" as though a set by that name existed.
  [[ -d "$d" ]] || continue
  b="$(basename "$d")"
  printf '%s\n' "${SETS[@]}" | grep -qx "$b" || { log_info "  - $b"; untouched=1; }
done
[[ $untouched -eq 1 ]] || log_info "  (none)"

[[ $DRY_RUN -eq 1 ]] && log_info $'\n--dry-run: commands are printed only, nothing is written.'

# Said once, up front, because a set runs for hours and is usually launched in
# the background: without this the only trace of progress is the terminal that
# started it.
if [[ $DRY_RUN -eq 0 ]]; then
  log_info ""
  log_info "Progress is written to $SETS_DIR/<set>/PROGRESS.txt as it goes. Watch it with:"
  for n in "${SETS[@]}"; do
    log_info "  tail -f $SETS_DIR/$n/PROGRESS.txt"
  done
fi

# -------------------------------------------------------------------- execution
run_logged() {
  echo "\$ $*"
  [[ $DRY_RUN -eq 1 ]] && return 0
  if [[ $QUIET_LOG -eq 1 ]]; then
    # Drops the cross-layer traces, which account for most of the volume
    # (1.3 GB for a twelve-second AOMDV_QPV simulation). Errors and application
    # messages are kept.
    "$@" 2>&1 | grep -vE "$CROSSLAYER_TRACE_PATTERN" >> "$LOGFILE"
    local rc=${PIPESTATUS[0]}
  else
    "$@" >> "$LOGFILE" 2>&1
    local rc=$?
  fi
  if [[ $rc -ne 0 ]]; then
    echo "FAILED (code $rc) - last 20 lines of $LOGFILE:" >&2
    tail -20 "$LOGFILE" >&2
    exit $rc
  fi
}

# Seeds are derived from the set name and the realisation index, never drawn at
# run time: a campaign that cannot be repeated is not a measurement. cksum gives
# a stable number across machines; the modulo keeps it inside the range ns-random
# accepts, and the +1 keeps it away from 0, which means "use ns-2's own sequence".
seed_for() {
  local n
  n=$(printf '%s-%s' "$1" "$2" | cksum | cut -d' ' -f1)
  echo $(( n % 900000 + 1 ))
}

for name in "${SETS[@]}"; do
  out="$SETS_DIR/$name"
  yuv="$VIDEO_DIR/${VIDEO[$name]}.yuv"
  avi="$VIDEO_DIR/${VIDEO[$name]}.avi"
  ip="$IP_DIR/${XL[$name]}.dat"
  sims="${SIMS_OVERRIDE:-${SIMS[$name]}}"
  LOGFILE="$PWD/$name.log"

  # The set directory and its PROGRESS.txt are created before any work starts,
  # not at the first harvest. A set takes hours; without this the directory does
  # not exist for the first half-hour and nothing distinguishes a run that is
  # working from one that failed at once. PROGRESS.txt is the file to tail.
  if [[ $DRY_RUN -eq 0 ]]; then
    mkdir -p "$out"
    PROGRESS_LOG="$out/PROGRESS.txt"
    : >"$PROGRESS_LOG"
    log_to_progress "Set $name started on $(date '+%d/%m/%Y at %H:%M')"
    log_to_progress "${RP[$name]}, ${sims} realisation(s), metrics=${METRICS[$name]}"
    log_to_progress "Raw command output: $out/$name.log"
    log_to_progress ""
  fi

  log_step "SET $name  -  ${RP[$name]}, label ${XL[$name]}, ${sims} realisation(s)"
  [[ $DRY_RUN -eq 1 ]] || echo -e "Script started at $(date)\n" > "$LOGFILE"

  log_step "$name 1/4 - YUV to AVI conversion"
  if [[ -f "$avi" && $REGEN_AVI -eq 0 ]]; then
    log_info "$avi already exists, kept (--regen-avi to regenerate it)."
  else
    # the raw video's size is the suffix of its name: <content>_qcif or
    # <content>_<WxH>, the resolution axis of study 1
    size="${VIDEO[$name]##*_}"
    run_logged ffmpeg -y -s "$size" -i "$yuv" "$avi"
  fi

  # The encoding depends only on the video and the encoder parameters - not on
  # the seed, not on the routing protocol. Running it once and sharing it across
  # realisations is what takes a ten-realisation campaign from about twenty
  # hours to about four; it is the single largest cost in the chain.
  log_step "$name 2/4 - EvalVSN, encoding (once, shared by all realisations)"
  if [[ $RESUME -eq 1 ]] && compgen -G "$out/encoding/*/st-priority.dat" >/dev/null; then
    # the realisations hard-link these files: rewriting them in place would
    # rewrite the kept realisations' copies too
    log_info "  encoding kept (--resume)"
  else
    # The encoder cuts each level into packets of the set's payload (d53).
    run_logged ../../evalvsn/bin/evalvsn -ip "$ip" -vf "$avi" -od "$out/encoding" -mpl "${MPL[$name]}"
  fi

  for ((i = 1; i <= sims; i++)); do
    r=$(printf 'r%02d' "$i")
    rdir="$out/$r"
    # An explicit --seed applies to every realisation, which is how a single run
    # is reproduced exactly; otherwise each realisation gets its own derived
    # seed - except a single-realisation set, which runs ns-2's own sequence
    # (seed 0), the author's rule of 27/08/2026: one realisation means the
    # deterministic run, no drawn seed, as every seed-0 diagnosis ran.
    if [[ "$SEED" != "0" ]]; then seed="$SEED"
    elif [[ "$sims" -eq 1 ]]; then seed=0
    elif [[ -n "$SEED_NAME" ]]; then seed="$(seed_for "$SEED_NAME" "$i")"
    else seed="$(seed_for "${name%"$PAIR_SUFFIX"}" "$i")"; fi

    log_step "$name 3/4 - realisation $i/$sims ($r, seed $seed)"
    if [[ $RESUME -eq 1 ]] && compgen -G "$rdir/*/sim/mt.dat" >/dev/null; then
      log_info "  $r already harvested, kept (--resume)"
      continue
    fi

    # Hard links, not copies: the encoding weighs some 22 MB per configuration,
    # so ten realisations of a twelve-configuration set would duplicate about
    # 2.6 GB for nothing. Decoding writes only into sim/decodedImages (video.cc,
    # the rcvFile branch), so the shared files are never written through.
    log_info "  linking the shared encoding into $r (no second encoding)"
    if [[ $DRY_RUN -eq 0 ]]; then
      mkdir -p "$rdir"
      cp -al "$out/encoding/." "$rdir/" \
        || die "could not link the encoding into $rdir"
    else
      echo "\$ cp -al $out/encoding/. $rdir/"
    fi

    log_info "  ns-2 transmission"
    run_logged bash set.sh -ip "$ip" -vf "$avi" -od "$rdir" -fire "${FIRE[$name]}" \
               -prop "${PROP[$name]}" -standby "${STANDBY[$name]}" \
               -mpl "${MPL[$name]}" -nn "${NN[$name]}" -rp "${RP[$name]}" \
               -knobs "${KNOBS[$name]}" \
               -capture "${CAPTURE[$name]}" \
               -dist "${DIST[$name]}" -dbg "$DBG" -seed "$seed"

    # Same binary and same ipFile as the encoding step; what makes this a
    # decoding is -decode, which used to be a trailing rcvFile column in a
    # second, otherwise identical ipFile. Without it EvalVSN writes
    # referenceImages/, with it it writes sim/decodedImages/.
    log_info "  EvalVSN decoding (-decode - this does NOT re-encode)"
    run_logged ../../evalvsn/bin/evalvsn -ip "$ip" -vf "$avi" -od "$rdir" -decode

  done

  log_step "$name 4/4 - Metrics and plots (${METRICS[$name]}, over $sims realisation(s))"
  run_logged perl ../metrics/setGraphs.pl -ip "$ip" -vf "$avi" -od "$out" -xl "${XL[$name]}" \
             -metrics "${METRICS[$name]}" -runs "$sims"

  # Pruning comes after the metrics are extracted, never before: what is deleted
  # here can only be regenerated by running the set again.
  if [[ $DRY_RUN -eq 0 ]]; then
    before="$(du -sh "$out" | cut -f1)"
    if [[ $PRUNE -eq 1 ]]; then
      log_step "$name - pruning the intermediates ($before before)"
      find "$out" -type f \( -name '*.tr' -o -name '*.nam' \) -delete
      find "$out" -type d \( -name 'allImages' -o -name 'capturedImages' \
                          -o -name 'decodedImages' \) -exec rm -rf {} +
      log_info "  $before -> $(du -sh "$out" | cut -f1); graphs/ kept"
    else
      log_info ""
      log_info "  $out weighs $before. Most of it is ns-2 traces and decoded"
      log_info "  frames, read only by the step that has just finished."
      log_info "  Re-run with --prune, or delete them once the figures are drawn."
    fi
  fi

  if [[ $DRY_RUN -eq 0 ]]; then
    echo -e "Script ended at $(date)\n" >> "$LOGFILE"
    mv "$LOGFILE" "$out/"
    log_step "$name done"
    log_info "Results: $out"
    log_info "Log     : $out/$name.log  ($(du -h "$out/$name.log" | cut -f1))"
  fi
done
