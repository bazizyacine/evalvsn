#!/usr/bin/env python3
"""routing_events.py - the routing events of each realisation, read from its ns-2 trace.

The routing measures of study 07 rest on counts the harvest does
not keep: how many route requests the camera
and the whole network sent, how many route errors, how many video packets the
MAC gave up on, how many the camera's routing queue refused while it waited
for a route, how many went to a neighbour already dead. setGraphs.pl never
reads them, so they are counted here from sets/<set>/r*/*/st-packet.tr, which
only a set run without --prune still holds.

    python3 routing_events.py -o ../studies/09-comparative/results/routing.tsv set_A set_B ...

One row per realisation: set, realisation, the camera's node, then
rreq_camera (RREQ the camera originated), rreq_all (RREQ sent by every node,
originated or forwarded), rerr (RERR sent), mac_drop (video packets dropped at
the MAC), rq_overflow (video packets the camera's routing queue refused,
reason IFQ at the RTR layer), cbk (video packets dropped on a MAC failure
callback, reason CBK), rrep and hello (RREP and HELLO sent), and pkt_loss (% of video packets lost, from sim/mt.dat).
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SETS = os.path.join(os.path.dirname(HERE), 'sets')

# One awk pass per trace: a 150 MB trace is read in seconds, a Python loop
# would take minutes.
AWK = r'''
{
  ev = $1; node = ""; layer = ""; why = ""; type = ""; pc = ""; ps = ""; ph = ""
  for (i = 2; i < NF; i++) {
    if ($i == "-Ni") node = $(i + 1)
    else if ($i == "-Nl") layer = $(i + 1)
    else if ($i == "-Nw") why = $(i + 1)
    else if ($i == "-It") type = $(i + 1)
    else if ($i == "-Pc") pc = $(i + 1)
    else if ($i == "-Ps") ps = $(i + 1)
    else if ($i == "-Ph") ph = $(i + 1)
  }
  if (ev == "s" && layer == "AGT" && type == "video") cam[node]++
  if (ev == "s" && layer == "RTR" && pc == "REQUEST") {
    rreq_all++
    # originated here: the request's source is this node, at hop count 0
    if (ps == node && ph == "0") rreq_from[node]++
  }
  if (ev == "s" && layer == "RTR" && pc == "ERROR") rerr++
  if (ev == "s" && layer == "RTR" && pc == "REPLY") rrep++
  if (ev == "s" && layer == "RTR" && pc == "HELLO") hello++
  if (ev == "d" && type == "video") {
    if (layer == "MAC") mac_drop++
    if (layer == "RTR" && why == "IFQ") rq[node]++
    if (layer == "RTR" && why == "CBK") cbk++
  }
}
END {
  best = ""; for (n in cam) if (best == "" || cam[n] > cam[best]) best = n
  printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", best, rreq_from[best], rreq_all, rerr, mac_drop, rq[best], cbk, rrep, hello
}
'''

COLS = ['camera', 'rreq_camera', 'rreq_all', 'rerr', 'mac_drop', 'rq_overflow', 'cbk', 'rrep', 'hello']


def pkt_loss(rdir):
    for mt in glob.glob(os.path.join(rdir, '*', 'sim', 'mt.dat')):
        with open(mt) as fh:
            head = fh.readline().split()
            row = fh.readline().split()
        if 'pktLoss' in head and row:
            return row[head.index('pktLoss')]
    return ''


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('-o', '--output', required=True)
    ap.add_argument('sets', nargs='+')
    args = ap.parse_args()

    rows = []
    for name in args.sets:
        rdirs = sorted(glob.glob(os.path.join(SETS, name, 'r[0-9]*')))
        if not rdirs:
            sys.exit('routing_events.py: no realisation under sets/%s' % name)
        for rdir in rdirs:
            traces = glob.glob(os.path.join(rdir, '*', 'st-packet.tr'))
            if not traces:
                sys.exit('routing_events.py: %s has no st-packet.tr (pruned?)' % rdir)
            out = subprocess.run(['awk', AWK, traces[0]], capture_output=True, text=True, check=True)
            rows.append([name, os.path.basename(rdir)] + out.stdout.split() + [pkt_loss(rdir)])
            print('%s %s' % (name, os.path.basename(rdir)), file=sys.stderr)

    with open(args.output, 'w') as fh:
        fh.write('# routing events per realisation, from the ns-2 traces (metrics/routing_events.py)\n')
        fh.write('\t'.join(['set', 'realisation'] + COLS + ['pkt_loss']) + '\n')
        for r in rows:
            fh.write('\t'.join(r) + '\n')
    print('wrote %s: %d realisation(s)' % (args.output, len(rows)))


if __name__ == '__main__':
    main()
