#!/bin/bash
#
# run_set_test.sh - historical shortcut for "run_sets.sh set_test".
#
# The execution logic now lives in run_sets.sh, which drives any set declared
# in sets.conf. This script is kept so as not to break existing habits and
# documentation.
#
# All run_sets.sh options are passed straight through:
#   bash run_set_test.sh --dry-run
#   bash run_set_test.sh --dbg=0 --quiet-log

exec "$(dirname "$(readlink -f "$0")")/run_sets.sh" set_test "$@"
