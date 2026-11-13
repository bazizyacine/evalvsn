#!/usr/bin/env python3
"""Statistics of the quality coefficient 0.1 (study 10, tranches 9 and 10), the
second series of E4, video coding, from
src/data/revision.tsv. The four stacks at GOP 9-3 and T = 9 s with the
coefficient 0.1 (arms -q01, the sets of tranche 10, simulated for 168 s like
their counterparts) share the 60 seeds (3 fires x 20) and the failure schedules
of study 09, whose sets at the coefficient 0.01 are the reference. The tests
were fixed before the results, and the tool makes those two and no other:

  1. at the coefficient 0.1, the PSNR of the hybrid stack minus the rules stack
     and of the agents stack minus the rules stack, paired over the 60 seeds,
     Holm over these 2 tests;
  2. per stack, the PSNR at the coefficient 0.1 minus the PSNR at 0.01, paired
     over the same 60 seeds, Holm over the 4 stacks.

Paired Student t test and Holm's correction as in tools/analyse_revision.py
(metrics/stats.py): mean difference, 95 % interval, raw p, Holm-corrected p.
Then, without a test, per stack and for both coefficients: the overall loss,
the loss of level M1, the usable images (PSNR of at least 28 dB, out of 12),
the packets sent and the mean packet rate over the twelve capture periods.

Usage: python3 tools/analyse_qualite.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data')
STACKS = (('pq', 'AOMDV'), ('rules', 'rules'), ('agents', 'agents'), ('hybrid', 'hybrid'))
# The coefficient's arm suffix, and the packets the camera sends per video: the
# lines of each set's encoding/st-priority.dat, 2 670 with ipFiles/GC-QC-0p1.dat
# and 5 352 with GC-PN-0p03-13.dat (the same in each of the 240 simulations of
# either coefficient, ns2_st-packet.dat).
COEFS = (('0.01', '', 5352), ('0.1', '-q01', 2670))
PERIOD = 9.0
FRAMES = 12


def table(name):
    lines = [l for l in open(os.path.join(DATA, name)) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table('revision.tsv')


def values(arm, metric):
    """Per seed (fire, realisation), GOP 9-3, T = 9 s, 40 m grid."""
    rows = [r for r in REV if r['arm'] == arm and r['split'] == '9M3D' and r['period'] == 'p9'
            and r['dist'] == '40' and r[metric] != '']
    out = {(r['fire'], r['realisation']): float(r[metric]) for r in rows}
    if len(rows) != 60 or len(out) != 60:
        sys.exit('%s, %s: %d rows on %d seeds, 60 expected' % (arm, metric, len(rows), len(out)))
    return out


def mean(x):
    return sum(x) / len(x)


def paired(a, b):
    """Mean of a - b over the seeds both hold, its 95 % interval and p."""
    if set(a) != set(b):
        sys.exit('the two arms do not hold the same seeds')
    d = [a[k] - b[k] for k in a]
    n, m = len(d), mean(d)
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    h = stats.t_quantile(0.95, n - 1) * s / math.sqrt(n)
    return n, m, m - h, m + h, stats.t_sf2(m / (s / math.sqrt(n)), n - 1) if s > 0 else 1.0


def family(title, tests):
    """tests: (label, a, b). Prints each raw p and its Holm correction in the family."""
    print(title)
    out = [(label,) + paired(a, b) for label, a, b in tests]
    ps = stats.holm([(o[0], '', o[5]) for o in out])
    for o, p in zip(out, ps):
        print('  %-26s n=%d  %+.2f dB [%+.2f, %+.2f]  p=%.3g  Holm %.3g' % (o[0], o[1], o[2], o[3], o[4], o[5], p[-1]))


def main():
    q = {s: values(s + '-q01', 'psnr') for s, _ in STACKS}
    ref = {s: values(s, 'psnr') for s, _ in STACKS}

    family('Test 1: at the coefficient 0.1, stack minus rules, PSNR (Holm over 2)', [
        ('hybrid - rules', q['hybrid'], q['rules']),
        ('agents - rules', q['agents'], q['rules'])])
    print()
    family('Test 2: per stack, PSNR at 0.1 minus PSNR at 0.01 (Holm over 4)', [
        ('%s 0.1 - 0.01' % label, q[s], ref[s]) for s, label in STACKS])

    print('\nWithout a test: means over the 60 seeds, GOP 9-3, T = 9 s')
    print('  %-7s %-5s %8s %9s %9s %8s %8s %13s' % ('stack', 'coef', 'PSNR', 'loss(%)', 'M1(%)', 'images', 'sent', 'rate(pkt/s)'))
    for s, label in STACKS:
        for coef, suffix, sent in COEFS:
            cells = [mean(list(values(s + suffix, m).values())) for m in ('psnr', 'loss', 'm1loss', 'usable')]
            print('  %-7s %-5s %8.2f %9.1f %9.1f %8.2f %8d %13.2f'
                  % ((label, coef) + tuple(cells) + (sent, sent / (FRAMES * PERIOD))))


if __name__ == '__main__':
    main()
