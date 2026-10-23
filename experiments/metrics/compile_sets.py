#!/usr/bin/env python3
"""compile_sets.py - one table of every set's harvest, for the thesis.

The sets under experiments/sets/ are machine-local and unversioned (traces,
images, hundreds of gigabytes); what the thesis reads is a compilation of
their harvest, small and versioned beside the chapters. This writes it: one
row per set, its declaration (protocol, label, terrain, scenario, knobs)
and, per metric, the moments the statistical protocol of the comparative
study runs on.

For the metrics of sim/mt.dat (psnr, refPSNR, ssim, refSSIM, pktLoss,
pktLossM, pktLossD, pktlossM1..13, pktlossD1..13) the realisations are read
one by one: mean, standard deviation, minimum, maximum and count - the
minimum carries the 28 dB criterion. For the rest of the catalogue (energy,
life time, delay, jitter, throughput, delivery, alert latency, usable
images, ...) graphs/<Metric>/<Metric>.dat gives mean, standard deviation
and count, as setGraphs.pl aggregated them.

    python3 compile_sets.py -o ../../thesis/data/palier-a.tsv $(grep -hv '^#' ../studies/02-codec/sets.txt ../studies/03-network/sets.txt)
    python3 compile_sets.py -o out.tsv set_A set_B                       # some sets
    python3 compile_sets.py --list                                       # the columns

Columns: set, protocol, label, payload, nodes, dist, capture, video, prop,
standby, fire, sims, knobs, metrics, then <metric>_mean, _sd, _n (and _min, _max for
the per-realisation ones; psnr also carries _below28, the number of
realisations under the 28 dB acceptability threshold, which the elimination
criterion of the comparative study reads). Metric names keep the catalogue's spelling with
spaces and dots turned into underscores: Packet_Delivery_Ratio, Ref_PSNR.
"""
import argparse
import glob
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
STUDY = os.path.dirname(HERE)
SETS = os.path.join(STUDY, 'sets')
CONF = os.path.join(STUDY, 'sets.conf')

GRAPH_METRICS = ['PSNR', 'Ref. PSNR', 'SSIM', 'Ref. SSIM', 'Packet Delivery Ratio', 'Packet Loss Ratio',
                 'Packet Loss Ratio by Type-M', 'Packet Loss Ratio by Type-D', 'Total Packets Sent',
                 'Mean Compression Ratio', 'Energy Consumption', 'Power Consumption', 'Life Time',
                 'Delay', 'Jitter', 'Throughput', 'Alert Latency', 'Alert Delivery', 'Service Duration',
                 'Usable Images', 'Usable Image Ratio']


def slug(m):
    return m.replace('. ', '_').replace(' ', '_').replace('.', '').replace('-', '_')


def declarations():
    out = {}
    with open(CONF, encoding='utf8') as fh:
        for line in fh:
            if not line.strip() or line.startswith('#'):
                continue
            f = line.split()
            # name protocol label payload nodes dist capture video prop standby fire metrics sims knobs
            keys = ['protocol', 'label', 'payload', 'nodes', 'dist', 'capture', 'video', 'prop', 'standby',
                    'fire', 'metrics', 'sims', 'knobs']
            d = {k: (f[i + 1] if i + 1 < len(f) else '') for i, k in enumerate(keys)}
            d['knobs'] = d['knobs'] or '0'
            out[f[0]] = d
    return out


USABLE_PSNR_DB = 28.0   # the acceptability threshold, evalvsn's and the thesis's


def moments(values):
    n = len(values)
    mean = sum(values) / n
    sd = math.sqrt(sum((v - mean) ** 2 for v in values) / (n - 1)) if n > 1 else 0.0
    return mean, sd, min(values), max(values), n


def read_mt(setdir):
    """Per-realisation rows of sim/mt.dat: {column: [values]}."""
    cols, data = None, {}
    for f in sorted(glob.glob(os.path.join(setdir, 'r[0-9]*', '*', 'sim', 'mt.dat'))):
        with open(f, encoding='utf8') as fh:
            lines = [l for l in fh if l.strip()]
        header = [c for c in lines[0].lstrip('#').split()]
        cols = cols or header
        vals = lines[1].split()
        for c, v in zip(header, vals):
            try:
                data.setdefault(c, []).append(float(v))
            except ValueError:
                pass      # na: the level or type is absent from this set
    return cols or [], data


def read_graph(setdir, metric):
    f = os.path.join(setdir, 'graphs', metric, metric + '.dat')
    if not os.path.isfile(f):
        return None
    with open(f, encoding='utf8') as fh:
        for line in fh:
            if line.startswith('#') or not line.strip():
                continue
            parts = line.split()
            if len(parts) < 4:
                return None
            return float(parts[1]), float(parts[2]), int(parts[3])
    return None


def compile_set(name, decl):
    d = os.path.join(SETS, name)
    row = {'set': name}
    row.update(decl)
    cols, mt = read_mt(d)
    for c in cols:
        if c in mt and mt[c]:
            mean, sd, lo, hi, n = moments(mt[c])
            row[c + '_mean'], row[c + '_sd'], row[c + '_min'], row[c + '_max'], row[c + '_n'] = mean, sd, lo, hi, n
            if c == 'psnr':
                # the elimination criterion of the comparative study: how many of the
                # realisations fall under the 28 dB acceptability threshold
                row['psnr_below28'] = sum(1 for v in mt[c] if v < USABLE_PSNR_DB)
    for m in GRAPH_METRICS:
        g = read_graph(d, m)
        if g:
            s = slug(m)
            row[s + '_mean'], row[s + '_sd'], row[s + '_n'] = g
    return row


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-o', '--out', default='-')
    ap.add_argument('--list', action='store_true')
    ap.add_argument('sets', nargs='*')
    a = ap.parse_args()
    decl = declarations()
    names = a.sets or sorted(os.path.basename(p) for p in glob.glob(os.path.join(SETS, 'set_*')) if os.path.isdir(p))
    rows = []
    for n in names:
        if n not in decl:
            print(f'compile_sets.py: {n} is not declared in sets.conf, skipped', file=sys.stderr)
            continue
        if not os.path.isdir(os.path.join(SETS, n, 'graphs')):
            print(f'compile_sets.py: {n} has no harvest (graphs/), skipped', file=sys.stderr)
            continue
        rows.append(compile_set(n, decl[n]))
    head = ['set', 'protocol', 'label', 'payload', 'nodes', 'dist', 'capture', 'video', 'prop', 'standby', 'fire',
            'sims', 'knobs']
    metrics = []
    for r in rows:
        for k in r:
            if k not in head and k not in metrics:
                metrics.append(k)
    cols = head + metrics
    if a.list:
        print('\n'.join(cols))
        return
    out = sys.stdout if a.out == '-' else open(a.out, 'w', encoding='utf8')
    out.write('# compiled by experiments/metrics/compile_sets.py from the harvest of %d set(s); per-realisation\n'
              '# moments for the mt.dat metrics, setGraphs.pl aggregates for the rest\n' % len(rows))
    out.write('\t'.join(cols) + '\n')
    for r in rows:
        out.write('\t'.join(fmt(r.get(c, '')) for c in cols) + '\n')
    if out is not sys.stdout:
        out.close()
        print(f'wrote {a.out}: {len(rows)} set(s), {len(metrics)} metric column(s)')


def fmt(v):
    if isinstance(v, float):
        return ('%.6g' % v)
    return str(v)


if __name__ == '__main__':
    main()
