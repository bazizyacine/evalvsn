#!/bin/bash
#
# lib/common.sh - functions shared by the EvalVSN chain scripts.
#
# Source it at the top of a script:
#   source "$(dirname "$(readlink -f "$0")")/lib/common.sh"
#
# Provides:
#   die              error exit, message on stderr
#   log_step         step heading
#   log_info         information line
#   parse_args       fills ARG[...] from "-key value" options
#   require_args     fails when an expected key is empty
#   require_files    fails when a file or directory is missing
#   require_cmds     fails when a command is not on the PATH
#   script_dir       real directory of the calling script

set -uo pipefail

# Associative array filled by parse_args
declare -A ARG

die() {
  echo "Error: $*" >&2
  exit 1
}

# Progress goes to the terminal AND, when PROGRESS_LOG names a file, to that
# file. Without the second destination a long run leaves nothing behind: the
# headings live only in the terminal that started it, so a campaign launched in
# the background is indistinguishable from one that never started, and once the
# terminal is gone there is no way to tell how far it got.
#
# The per-set log is a different thing: it collects the raw output of ffmpeg,
# EvalVSN and ns-2, not the progress. Both are needed.
log_to_progress() {
  [[ -n "${PROGRESS_LOG:-}" ]] || return 0
  printf '%s\n' "$*" >>"$PROGRESS_LOG" 2>/dev/null || true
}

log_step() {
  echo -e "\n=== $* ==="
  log_to_progress "[$(date '+%H:%M:%S')] === $* ==="
}

log_info() {
  echo "$*"
  log_to_progress "[$(date '+%H:%M:%S')] $*"
}

script_dir() {
  dirname "$(readlink -f "${BASH_SOURCE[1]}")"
}

# parse_args "usage" "$@"
#
# Accepts options of the form "-key value". The key is stored without the
# dash: "-ip file" yields ARG[ip]=file. Recognises -h and --help.
parse_args() {
  local usage="$1"; shift

  while [[ $# -gt 0 ]]; do
    case "$1" in
      -h|--help)
        echo "$usage"
        exit 0
        ;;
      -*)
        [[ $# -ge 2 ]] || die "option $1 expects a value.\n$usage"
        ARG["${1#-}"]="$2"
        shift 2
        ;;
      *)
        die "unexpected argument: $1\n$usage"
        ;;
    esac
  done
}

# require_args "usage" key...
#
# Checks that every key was given and is not empty. This is the check ns2.sh
# lacked: called without -nn, it passed an empty value to ns-2, which silently
# fell back to its default.
require_args() {
  local usage="$1"; shift
  local missing=()

  for key in "$@"; do
    [[ -n "${ARG[$key]:-}" ]] || missing+=("-$key")
  done

  if [[ ${#missing[@]} -gt 0 ]]; then
    die "missing argument(s): ${missing[*]}\n$usage"
  fi
}

require_files() {
  local missing=()
  for f in "$@"; do
    [[ -e "$f" ]] || missing+=("$f")
  done
  [[ ${#missing[@]} -eq 0 ]] || die "not found: ${missing[*]}"
}

require_cmds() {
  local missing=()
  for c in "$@"; do
    command -v "$c" >/dev/null 2>&1 || missing+=("$c")
  done
  [[ ${#missing[@]} -eq 0 ]] || die "command(s) missing from PATH: ${missing[*]}"
}

# Pattern of the trace lines emitted by the ns-2 cross-layer instrumentation.
# Used by the optional log filtering: a twelve-second AOMDV_QPV simulation
# produces 1.3 GB, most of it from these lines.
#
# shellcheck disable=SC2034  # consumed by run_sets.sh, which sources this file
readonly CROSSLAYER_TRACE_PATTERN='\(cross-layer\) using:|MAC ChState|PHY PHYState|PHY Mod_Scheme|PHY PMX Power|MAC PHYIdleInd'
