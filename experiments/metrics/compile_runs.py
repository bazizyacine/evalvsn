#!/usr/bin/env python3
"""compile_runs.py - one row per simulation of a study: the raw results that
the article's compiled tables are drawn from.

The sets under experiments/sets/ are machine-local and unversioned (traces,
images, hundreds of gigabytes), and they are purged once their study is
delivered. This keeps, for every realisation of every set a study lists in
studies/<study>/sets.txt, what the simulation measured on it and the seed it ran
on, small enough to be versioned and published:

  - sim/mt.dat: the PSNR, the SSIM and their loss-free references, the packet
    loss overall, of the M and D frames and of each priority level;
  - sim/rt-frame.dat: the PSNR, SSIM and packet loss of each image at the sink;
  - the routing counters of metrics/routing_events.py, from the set's
    events.tsv or from the study's results/routing.tsv, where they exist.

The seed is recomputed with run_sets.sh's law (cksum of "<name>-<index>",
modulo 900000, plus 1; seed 0 for a single-realisation set), from the set's
own name or, with --seed-base, from the name the study pairs its sets on
(BASE-<fire token>, as run_sets.sh --seed-name=BASE-<fire> runs them). It is
checked against the seed the set's PROGRESS.txt recorded; a mismatch fails.

    python3 compile_runs.py --seed-base=set_AOMDV_QPV_GC-PN-0p03-13_p9-n100-d40 09-comparative
    python3 compile_runs.py --seed-base=... -o out.tsv 10-revision
    python3 compile_runs.py --allow-partial ... 10-revision     # leave out the sets still running

The default output is studies/<study>/results/runs.tsv. A set is complete when
graphs/PSNR/PSNR.dat exists and its realisations number the sims of its
declaration; without --allow-partial, an incomplete or missing set fails the
compilation, so that a published table never silently lacks a set.

Columns: set, realisation, seed; arm, fire, split, period, dist, the keys of
the article's revision.tsv (empty for a set revision.tsv does not hold, such
as the sets of the quality coefficient 0.1 that stopped at 165 s); then the
mt.dat columns as the tools wrote them (psnr refPSNR ssim refSSIM pktLoss
pktLossM pktLossD pktlossM1..13 pktlossD1..13; dB, SSIM in [0, 1], losses in
%); then psnr_i<k>, ssim_i<k>, loss_i<k> for each image k at the sink; then
rreq_camera rreq_all rerr mac_drop rq_overflow cbk rrep hello (messages or
packets per simulation). A value the set does not provide stays empty.
"""
import argparse
import csv
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# The keys of the article's src/data/revision.tsv (tools/compile_revision.py),
# so that a row here joins a row there.
NAME = re.compile(r'set_(AOMDV_QPV|AOMDV)_GC-PN-(0p\d+)-13_(p[0-9r]+)-n100-d(\d+)-(fire[23]?)-(.+?)(?:-s10)?$')
# The quality coefficient 0.1 (ipFiles/GC-QC-0p1.dat): the arm of the stack with
# the suffix -q01, at GOP 9-3; only the sets simulated for 168 s (-t168-s10) are
# in revision.tsv, as compile_revision.py reads them.
QUALITY = re.compile(r'set_(AOMDV_QPV|AOMDV)_GC-QC-0p1_(p9)-n100-d(40)-(fire[23]?)-(rules|hybrid|agents|pq)-t168-s10$')
SPLIT = {'0p03': '9M3D', '0p02525': '6M6D', '0p01597': '3M9D'}
FIRE = {'fire': 'F1', 'fire2': 'F2', 'fire3': 'F3'}
FIRE_TOKEN = re.compile(r'-d\d+-(fire[23]?)(?:-|$)')
PROGRESS = re.compile(r'realisation (\d+)/(\d+) \((r\d+), seed (\d+)\)')
ROUTING = ('rreq_camera', 'rreq_all', 'rerr', 'mac_drop', 'rq_overflow', 'cbk', 'rrep', 'hello')
IMAGES = 12  # the twelve frames of the alarm video; more are added if a set has them


def _crc_table():
    table = []
    for i in range(256):
        c = i << 24
        for _ in range(8):
            c = ((c << 1) ^ 0x04C11DB7) if c & 0x80000000 else (c << 1)
        table.append(c & 0xFFFFFFFF)
    return table


CRC = _crc_table()


def cksum(data):
    """POSIX cksum: CRC-32 of the bytes, then of their length, complemented."""
    crc = 0
    for b in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CRC[((crc >> 24) ^ b) & 0xFF]
    n = len(data)
    while n:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CRC[((crc >> 24) ^ (n & 0xFF)) & 0xFF]
        n >>= 8
    return ~crc & 0xFFFFFFFF


def seed_for(name, i, sims):
    """run_sets.sh's seed_for(): a single-realisation set runs ns-2's own sequence."""
    if sims == 1:
        return 0
    return cksum(('%s-%d' % (name, i)).encode()) % 900000 + 1


def declarations(conf):
    """name -> sims, from sets.conf (the 13th column)."""
    out = {}
    for line in open(conf, encoding='utf8'):
        if not line.strip() or line.startswith('#'):
            continue
        f = line.split()
        if len(f) >= 13:
            out[f[0]] = int(f[12])
    return out


def keys(name):
    q = QUALITY.match(name)
    if q:
        _, period, dist, fire, stack = q.groups()
        return stack + '-q01', FIRE[fire], '9M3D', period, dist
    m = NAME.match(name)
    if not m:
        return ('',) * 5
    _, gc, period, dist, fire, arm = m.groups()
    split = '9M3D' if ('-forest' in arm or '-smoke' in arm) else SPLIT.get(gc, '')
    return arm, FIRE[fire], split, period, dist


def recorded_seeds(setdir):
    path = os.path.join(setdir, 'PROGRESS.txt')
    if not os.path.exists(path):
        return {}
    return {m[3]: int(m[4]) for m in PROGRESS.finditer(open(path, errors='ignore').read())}


def read_mt(rdir):
    mt = glob.glob(os.path.join(rdir, '*', 'sim', 'mt.dat'))
    if not mt:
        return None
    lines = [l for l in open(mt[0]) if l.strip()]
    return dict(zip(lines[0].lstrip('#').split(), lines[1].split()))


def read_frames(rdir):
    """image -> (psnr, ssim, loss), as rt-frame.dat wrote them."""
    f = glob.glob(os.path.join(rdir, '*', 'sim', 'rt-frame.dat'))
    out = {}
    if f:
        for line in open(f[0]):
            if line.startswith('#') or not line.strip():
                continue
            v = line.split('\t')
            out[int(v[0])] = (v[1], v[2], v[3])
    return out


def read_routing(path, name):
    if not os.path.exists(path):
        return {}
    rows = csv.DictReader((l for l in open(path) if not l.startswith('#')), delimiter='\t')
    return {r['realisation']: r for r in rows if r['set'] == name}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('study', help='a folder of studies/, e.g. 10-revision')
    ap.add_argument('-o', '--out', help='default: studies/<study>/results/runs.tsv')
    ap.add_argument('--exp', default=os.path.dirname(HERE), help='the experiments/ directory (default: the one above this file)')
    ap.add_argument('--seed-base', default='', help='pairing name: each set takes the seeds of BASE-<fire token>')
    ap.add_argument('--allow-partial', action='store_true', help='leave out, with a warning, the sets not complete')
    a = ap.parse_args()

    exp = os.path.abspath(a.exp)
    sets_dir = os.path.join(exp, 'sets')
    study = os.path.join(exp, 'studies', a.study)
    out = a.out or os.path.join(study, 'results', 'runs.tsv')
    names = [l.split()[0] for l in open(os.path.join(study, 'sets.txt')) if l.strip() and not l.startswith('#')]
    sims_of = declarations(os.path.join(exp, 'sets.conf'))
    study_routing = os.path.join(study, 'results', 'routing.tsv')

    rows, missing, bad_seed, images = [], [], [], IMAGES
    for name in names:
        d = os.path.join(sets_dir, name)
        sims = sims_of.get(name)
        if sims is None:
            sys.exit('compile_runs.py: %s is not declared in sets.conf' % name)
        reals = sorted(glob.glob(os.path.join(d, 'r[0-9][0-9]')))
        done = [r for r in reals if glob.glob(os.path.join(r, '*', 'sim', 'mt.dat'))]
        if not os.path.exists(os.path.join(d, 'graphs', 'PSNR', 'PSNR.dat')) or len(done) < sims:
            missing.append('%s (%d / %d)' % (name, len(done), sims))
            continue
        if a.seed_base:
            fire = FIRE_TOKEN.search(name)
            if not fire:
                sys.exit('compile_runs.py: %s has no fire token for --seed-base' % name)
            seed_name = '%s-%s' % (a.seed_base, fire[1])
        else:
            seed_name = name
        logged = recorded_seeds(d)
        routing = read_routing(os.path.join(d, 'events.tsv'), name) or read_routing(study_routing, name)
        for r in done:
            real = os.path.basename(r)
            seed = seed_for(seed_name, int(real[1:]), sims)
            if real in logged and logged[real] != seed:
                bad_seed.append('%s %s: computed %d, recorded %d' % (name, real, seed, logged[real]))
            frames = read_frames(r)
            images = max([images] + list(frames))
            rows.append((name, real, seed, keys(name), read_mt(r), frames, routing.get(real, {})))

    mt_cols = []
    for row in rows:
        for c in row[4]:
            if c not in mt_cols:
                mt_cols.append(c)
    head = ['set', 'realisation', 'seed', 'arm', 'fire', 'split', 'period', 'dist'] + mt_cols
    for k in range(1, images + 1):
        head += ['psnr_i%d' % k, 'ssim_i%d' % k, 'loss_i%d' % k]
    head += list(ROUTING)

    if bad_seed:
        print('compile_runs.py: %d seed(s) differ from PROGRESS.txt; check --seed-base:\n  %s'
              % (len(bad_seed), '\n  '.join(bad_seed[:10])), file=sys.stderr)
        sys.exit(1)
    if missing:
        print('compile_runs.py: %d set(s) of %s not complete%s:\n  %s'
              % (len(missing), a.study, ', left out' if a.allow_partial else '', '\n  '.join(missing)), file=sys.stderr)
        if not a.allow_partial:
            sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    # Written aside, then renamed: a reader never sees a half-written file.
    with open(out + '.tmp', 'w') as f:
        f.write('# compiled by metrics/compile_runs.py from experiments/sets (study %s): one row per simulation, '
                '%d of %d sets%s\n' % (a.study, len(names) - len(missing), len(names),
                                       ', seeds of %s-<fire>' % a.seed_base if a.seed_base else ''))
        f.write('\t'.join(head) + '\n')
        for name, real, seed, k, mt, frames, ev in rows:
            v = [name, real, str(seed)] + list(k) + [mt.get(c, '') for c in mt_cols]
            for i in range(1, images + 1):
                v += list(frames.get(i, ('', '', '')))
            v += [ev.get(c, '') for c in ROUTING]
            f.write('\t'.join(v) + '\n')
    os.replace(out + '.tmp', out)
    print('%s: %d simulations of %d sets' % (out, len(rows), len(names) - len(missing)))


if __name__ == '__main__':
    main()
