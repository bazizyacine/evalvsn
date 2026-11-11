#!/usr/bin/env python3
"""Statistics of the controls of the revision (study 10) from src/data/revision.tsv
and src/data/break-agent.tsv. Two arms share their seeds, so every test pairs the
same seeds: mean difference, 95 % confidence interval, p, then Holm within each
family of comparisons.

Usage: analyse_revision.py
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'experiments', 'metrics')))
import stats  # noqa: E402

DATA = os.path.join(HERE, '..', 'src', 'data')
SPLITS = ('9M3D', '6M6D', '3M9D')
FIRES = ('F1', 'F2', 'F3')
COUNTS = (1, 2, 3, 5)
BINS = (60, 78, 96, 114, 132, 150)


def table(name):
    lines = [l for l in open(os.path.join(DATA, name)) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:]]


REV = table('revision.tsv')
BRK = table('break-agent.tsv')


def values(arm, metric, split='9M3D', period='p9'):
    return {(r['fire'], r['realisation']): float(r[metric]) for r in REV
            if r['arm'] == arm and r['split'] == split and r['period'] == period and r[metric] != ''}


def mean(x):
    return sum(x) / len(x)


def paired(a, b):
    """Mean of a - b over the seeds both hold, its 95 % interval and p."""
    d = [a[k] - b[k] for k in a if k in b]
    n, m = len(d), mean(d)
    s = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1))
    h = stats.t_quantile(0.95, n - 1) * s / math.sqrt(n)
    return n, m, m - h, m + h, stats.t_sf2(m / (s / math.sqrt(n)), n - 1) if s > 0 else 1.0


def family(title, tests):
    """tests: (label, a, b). The p values printed are Holm-corrected in the family."""
    print(title)
    out = [(label,) + paired(a, b) for label, a, b in tests]
    ps = stats.holm([(o[0], '', o[5]) for o in out])
    for o, p in zip(out, ps):
        print('  %-34s n=%d  %+.2f [%+.2f, %+.2f]  p=%.3g  Holm %.3g' % (o[0], o[1], o[2], o[3], o[4], o[5], p[-1]))


def over_splits(a, b):
    """The difference of each seed averaged over the three GOPs."""
    d = {}
    for s in SPLITS:
        y = values(b, 'psnr', s)
        for k, v in values(a, 'psnr', s).items():
            d.setdefault(k, []).append(v - y[k])
    d = {k: mean(v) for k, v in d.items()}
    return paired(d, {k: 0.0 for k in d})


def cell(arm, metric):
    return mean(list(values(arm, metric).values()))


def brk(arm, col):
    v = [float(r[col]) for r in BRK if r['arm'] == arm and r[col] != '']
    return mean(v) if v else float('nan')


def main():
    print('Arms at GOP 9-3, means over the 60 seeds')
    arms = ('pq', 'pq-ctl', 'rules', 'agents-frozen', 'hybrid-frozen', 'agents', 'hybrid', 'agents-edec', 'hybrid-edec')
    print('  %-14s %6s %6s %6s %6s %7s %7s %6s %6s' % ('arm', 'psnr', 'm1loss', 'loss', 'usable', 'declared', 'false', 'rreq', 'rerr'))
    for a in arms:
        ctl = {'agents': 'agents-tr', 'hybrid': 'hybrid-tr'}.get(a, a)
        row = [cell(a, 'psnr'), cell(a, 'm1loss'), cell(a, 'loss'), cell(a, 'usable')]
        row += [cell(a, m) if values(a, m) else float('nan') for m in ('declared', 'false_declared')]
        row += [cell(ctl, m) if values(ctl, m) else float('nan') for m in ('rreq_all', 'rerr')]
        print('  %-14s %6.2f %6.2f %6.2f %6.2f %7.2f %7.2f %6.0f %6.0f' % ((a,) + tuple(row)))

    print('\nTraced arms against study 09 (identical runs give 0)')
    for a, b in (('agents-tr', 'agents'), ('hybrid-tr', 'hybrid')):
        for m in ('psnr', 'm1loss', 'usable'):
            x, y = values(a, m), values(b, m)
            print('  %s - %s %s: largest difference %.4f' % (a, b, m, max(abs(x[k] - y[k]) for k in x)))

    print('\nLink-break agent, agents stack, per simulation (traced runs)')
    total = sum(brk('agents', 'pick%d' % k) for k in COUNTS)
    declared = sum(brk('agents', 'decl%d' % k) for k in COUNTS)
    print('  decisions %.1f, share a uniform draw gives one count %.1f %%' % (total, 100 * 0.1 / 4))
    for k in COUNTS:
        print('  count %d: picked %6.1f (%4.1f %%), declared %5.2f (%4.1f %%), best value on %4.1f node-states'
              % (k, brk('agents', 'pick%d' % k), 100 * brk('agents', 'pick%d' % k) / total, brk('agents', 'decl%d' % k),
                 100 * brk('agents', 'decl%d' % k) / declared, brk('agents', 'best%d' % k)))
    print('  declared %.2f, proved alive within 15 s %.2f, silent %.2f' % (declared, brk('agents', 'alive'), brk('agents', 'silent')))
    other = declared - brk('agents', 'decl1')
    print('  one failure: at least %.0f %% of its %.2f declarations proved alive (the other counts declare %.2f)'
          % (100 * (brk('agents', 'alive') - other) / brk('agents', 'decl1'), brk('agents', 'decl1'), other))

    print('\nDeclarations per 18 s of the alarm, per simulation')
    print('  %-14s' % 'arm' + ''.join('%7d' % t for t in BINS) + '   sum')
    for a in ('rules', 'agents-frozen', 'hybrid', 'agents', 'hybrid-edec', 'agents-edec'):
        v = [brk(a, 't%d' % t) for t in BINS]
        print('  %-14s' % a + ''.join('%7.2f' % x for x in v) + '%7.2f' % sum(v))

    print('\nPSNR of the twelve images, mean over the seeds (dB)')
    for a in ('hybrid', 'agents', 'hybrid-edec', 'agents-edec'):
        print('  %-14s' % a + ''.join('%6.1f' % brk(a, 'img%d' % i) for i in range(1, 13)))

    print()
    family('E1: AOMDV control against AOMDV, per GOP', [
        (s, values('pq-ctl', 'psnr', s), values('pq', 'psnr', s)) for s in SPLITS])
    family('E1: rules against the AOMDV control, per GOP', [
        (s, values('rules', 'psnr', s), values('pq-ctl', 'psnr', s)) for s in SPLITS])
    print('  rules - control, per seed over the three GOPs: n=%d %+.2f [%+.2f, %+.2f] p=%.3g'
          % over_splits('rules', 'pq-ctl'))
    print('  control - AOMDV, per seed over the three GOPs: n=%d %+.2f [%+.2f, %+.2f] p=%.3g'
          % over_splits('pq-ctl', 'pq'))

    print()
    family('E3: exploration decay against constant exploration, PSNR', [
        ('agents', values('agents-edec', 'psnr'), values('agents', 'psnr')),
        ('hybrid', values('hybrid-edec', 'psnr'), values('hybrid', 'psnr'))])
    family('E3: stacks under decay and under constant exploration, PSNR', [
        ('hybrid - agents, constant', values('hybrid', 'psnr'), values('agents', 'psnr')),
        ('hybrid - agents, decay', values('hybrid-edec', 'psnr'), values('agents-edec', 'psnr')),
        ('agents decay - rules', values('agents-edec', 'psnr'), values('rules', 'psnr')),
        ('hybrid decay - rules', values('hybrid-edec', 'psnr'), values('rules', 'psnr'))])
    ae, a, he, h = (values(x, 'psnr') for x in ('agents-edec', 'agents', 'hybrid-edec', 'hybrid'))
    print('  effect of the decay, agents minus hybrid: n=%d %+.2f [%+.2f, %+.2f] p=%.3g'
          % paired({k: ae[k] - a[k] for k in ae}, {k: he[k] - h[k] for k in he}))
    for m in ('m1loss', 'loss', 'usable', 'declared'):
        family('E3: decay against constant, %s' % m, [
            ('agents', values('agents-edec', m), values('agents', m)),
            ('hybrid', values('hybrid-edec', m), values('hybrid', m))])

    print()
    family('Hybrid and the frozen hybrid, per GOP', [
        (s, values('hybrid', 'psnr', s), values('hybrid-frozen', 'psnr', s)) for s in SPLITS])
    family('Frozen hybrid and the rules, per GOP', [
        (s, values('hybrid-frozen', 'psnr', s), values('rules', 'psnr', s)) for s in SPLITS])
    family('Agents and the frozen agents, per GOP', [
        (s, values('agents', 'psnr', s), values('agents-frozen', 'psnr', s)) for s in SPLITS])
    family('Frozen agents and the rules, per GOP', [
        (s, values('agents-frozen', 'psnr', s), values('rules', 'psnr', s)) for s in SPLITS])
    print('  hybrid - frozen, per seed over the three GOPs: n=%d %+.2f [%+.2f, %+.2f] p=%.3g'
          % over_splits('hybrid', 'hybrid-frozen'))
    print('  frozen hybrid - rules, per seed over the three GOPs: n=%d %+.2f [%+.2f, %+.2f] p=%.3g'
          % over_splits('hybrid-frozen', 'rules'))
    family('Hybrid against the agents under decay', [
        ('hybrid - agents decay', values('hybrid', 'psnr'), values('agents-edec', 'psnr')),
        ('hybrid - hybrid decay', values('hybrid', 'psnr'), values('hybrid-edec', 'psnr'))])

    late = {a: {(r['fire'], r['realisation']): sum(float(r['t%d' % t]) for t in BINS[3:]) for r in BRK if r['arm'] == a}
            for a in ('agents', 'hybrid', 'agents-edec', 'hybrid-edec')}
    print('\nDeclarations after 114 s: ' + ', '.join('%s %.2f' % (a, mean(list(v.values()))) for a, v in late.items()))
    print('  agents decay - hybrid: n=%d %+.2f [%+.2f, %+.2f] p=%.3g' % paired(late['agents-edec'], late['hybrid']))
    print('  agents decay - hybrid decay: n=%d %+.2f [%+.2f, %+.2f] p=%.3g' % paired(late['agents-edec'], late['hybrid-edec']))

    # Image 7 is captured at 114 s, when the decay reaches 0.
    def images(arm, first, last):
        return {(r['fire'], r['realisation']): mean([float(r['img%d' % i]) for i in range(first, last + 1)])
                for r in BRK if r['arm'] == arm}
    for first, last in ((1, 6), (7, 12)):
        print('Images %d to %d: ' % (first, last) + ', '.join(
            '%s %.2f' % (a, mean(list(images(a, first, last).values()))) for a in ('agents', 'hybrid', 'agents-edec', 'hybrid-edec')))
        family('  paired, images %d to %d' % (first, last), [
            ('agents decay - hybrid', images('agents-edec', first, last), images('hybrid', first, last)),
            ('agents decay - agents', images('agents-edec', first, last), images('agents', first, last))])


if __name__ == '__main__':
    main()
