#!/usr/bin/env python3
"""Why a D frame of the fire video holds more bytes than an M frame (a question of
the Sensors revision): EvalVSN's coding chain reproduced on the images EvalVSN
itself captured for the article's three GOPs, compiled into one TSV so that no
number of the methods or of the reply letter is read by hand.

  check    the reproduction against the traces: bytes and packets of the 36 frames
           of the three GOPs, and, with --encoder, the quality sweep against
           EvalVSN itself
  bytes    frames, mean bytes and mean packets per M and D frame, per GOP, from
           st-frame.dat and st-priority.dat
  paired   the same image coded as M in one GOP and as D in another: the extra
           bytes of the D frame, over the 18 comparisons
  corr     correlation between the DCT coefficients of a D frame's image and of its
           M reference, per band of the zigzag scan (DC, AC 1-9, AC 10-63), mean
           over the 15 (D frame, reference) pairs of the three GOPs
  energy   energy of the residual over that of the image, per band; per pair, the
           AC ratio and the AC positions where the residual exceeds the image
  nonzero  share of non-zero AC coefficients after quantisation, M and D frames
  bits     Exp-Golomb bits per coefficient for the values and for the run lengths
  quality  mean M and D bytes and D/M at qualityCoef 0.005 to 4, GOP 9-3 (the M/D
           decision reads the original pixels, so the GOP does not move with it)
  source   step between the coded images in the source video and its duration,
           and the AC 10-63 correlation of the source against the lag

Inputs: the encodings of the article's sets,
experiments/sets/set_AOMDV_QPV_GC-PN-{0p01597,0p02525,0p03}-13_p9-n100-d40-fire-rules/
encoding/fire_qcif-*/ (capturedImages/, allImages/, st-frame.dat, st-priority.dat),
their encoder files experiments/ipFiles/GC-PN-*-13.dat, and the frame rate of
evalvsn/videos/fire_qcif.avi (ffprobe). The chain mirrors evalvsn/src, file:line
in the comments; the tool stops without writing if one frame differs from its
trace. Needs numpy and Pillow.

Usage: python3 tools/analyse_codec.py [--encoder] [OUT.tsv]   (default: src/data/codec-frames.tsv)
  --encoder  also encodes the video with evalvsn/bin/evalvsn in a temporary
             directory at every quality of the sweep (no network simulation,
             about 15 s per quality) and requires the same type and bytes for
             each of the 12 frames
"""
import math
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
SETS = os.path.join(ROOT, 'experiments', 'sets')
IPFILES = os.path.join(ROOT, 'experiments', 'ipFiles')
VIDEO = os.path.join(ROOT, 'evalvsn', 'videos', 'fire_qcif.avi')
EVALVSN = os.path.join(ROOT, 'evalvsn', 'bin', 'evalvsn')

args = [a for a in sys.argv[1:] if a != '--encoder']
ENCODER = '--encoder' in sys.argv[1:]
OUT = args[0] if args else os.path.join(HERE, '..', 'src', 'data', 'codec-frames.tsv')

GOPS = (('0.01597', '3-9'), ('0.02525', '6-6'), ('0.03', '9-3'))
QUALITIES = (0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 4.0)
LAGS = (1, 2, 5, 13, 26)
MPL = 74                        # sets.conf, payload column of the article's sets
HEADER_BITS = 16                # packet.cc:11, the index of the packet's first coefficient
rows = []


def add(quantity, key, value, source):
    rows.append((quantity, key, value, source))


def encoding(gc):
    """The encoding directory of a GOP, relative to experiments/sets."""
    return 'set_AOMDV_QPV_GC-PN-%s-13_p9-n100-d40-fire-rules/encoding/fire_qcif-%s-13' % (gc.replace('0.', '0p'), gc)


def ipfile(gc):
    """The encoder file of a GOP: simId MMPEG FPS threshold gopCoef qualityCoef levels DCT RLE bin."""
    line = [l for l in open(os.path.join(IPFILES, 'GC-PN-%s-13.dat' % gc.replace('0.', '0p')))
            if l.strip() and not l.startswith('#')][0].split()
    assert line[1] == 'MMPEG' and line[7] == 'CLA8' and line[8] == 'RLE1' and line[9] == 'EG', line
    assert int(line[6]) == 13 and float(line[4]) == float(gc), line
    return dict(fps=float(line[2]), threshold=int(line[3]), qc=float(line[5]))


# quantification.cc:5, the JPEG luminance table
Z = np.array([16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56,
              14, 17, 22, 29, 51, 87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
              49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99], float).reshape(8, 8)
# dct.cc:23-63, the orthonormal 8x8 DCT-II (x the row, y the column), computed in the
# order of the C++ so that a coefficient on a rounding tie (a DC of k + 1/2 steps, for
# one) rounds as EvalVSN rounds it: EvalVSN is a Debug build (-g, no optimisation, no
# fused multiply-add), so every operation is one IEEE double operation in source order,
# and math.cos is the C library's cos.
COS = np.array([[math.cos(((2.0 * n + 1.0) * k * math.pi) / (2.0 * 8)) for k in range(8)] for n in range(8)])
NORM = np.array([[(math.sqrt(1.0 / 8) if x == 0 else math.sqrt(2.0 / 8))
                  * (math.sqrt(1.0 / 8) if y == 0 else math.sqrt(2.0 / 8)) for y in range(8)] for x in range(8)])
# zigzag.cc:3-71, the JPEG zigzag scan as (row, column)
ZZ = []
for s in range(15):
    cells = [(i, s - i) for i in range(8) if 0 <= s - i < 8]
    ZZ += cells[::-1] if s % 2 == 0 else cells
ZR = np.array([r for r, _ in ZZ])
ZC = np.array([c for _, c in ZZ])
# priority.cc:15-53, the 13 levels of CLA8 as ranges of the zigzag vector
LEVELS = ((0, 3), (3, 6), (6, 10), (10, 15), (15, 21), (21, 28), (28, 36), (36, 43), (43, 49),
          (49, 54), (54, 58), (58, 61), (61, 64))
BANDS = (('DC', 0, 1), ('AC 1-9', 1, 10), ('AC 10-63', 10, 64))


def dct_zigzag(img):
    """frame.cc:10-19: 8x8 blocks in raster order, each block's DCT in zigzag order (blocks x 64)."""
    H, W = img.shape
    blocks = img.reshape(H // 8, 8, W // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 8, 8)
    total = np.zeros((blocks.shape[0], 8, 8))
    for i in range(8):                       # dct.cc:33-39: SUMij += block(i, j) cos(j, y) cos(i, x)
        for j in range(8):
            total = total + (blocks[:, i, j][:, None, None] * COS[j][None, None, :]) * COS[i][None, :, None]
    coefs = NORM[None, :, :] * total         # dct.cc:60, Cx * Cy * SUMij
    return coefs[:, ZR, ZC]


def quantise(coefs, qc):
    """quantification.cc:11, round(c / (Z qc)) with C++ round(), half away from zero, exact."""
    x = coefs / (Z[ZR, ZC] * qc)
    a = np.abs(x)
    r = np.floor(a)
    return (np.sign(x) * (r + (a - r >= 0.5))).astype(int)


def eg_bits(v):
    """golomb.cc:13-23, 86, 90, 146: sign folded to 2v or -2v-1, then order-0 Exp-Golomb."""
    n = 2 * v if v >= 0 else -2 * v - 1
    return 1 if n == 0 else 2 * int(math.floor(math.log2(n + 1))) + 1


def rle(seq):
    """rle.cc:12-21: (run, value) pairs of equal successive values; a final run of zeros is dropped."""
    out, prev, n = [], -1, len(seq)
    for i in range(n):
        if (i < n - 1 and seq[i] != seq[i + 1]) or (i == n - 1 and seq[i] != 0):
            out.append((i - prev, seq[i]))
            prev = i
    return out


def frame_cost(img, qc):
    """trace.cc:240-305 and packet.cc:23-104: per level, the coefficients of every block in a
    row, run-length coded (RLE1: every level), Exp-Golomb coded, then cut on code words into
    packets of at most MPL bytes with a two-byte index, padded to the byte."""
    q = quantise(dct_zigzag(img), qc)
    cost = dict(bytes=0, packets=0, value_bits=0, run_bits=0, coefs=q)
    budget = MPL * 8 - HEADER_BITS                              # packet.cc:65
    for a, b in LEVELS:
        seq = q[:, a:b].reshape(-1).tolist()
        if not any(seq):                                        # trace.cc:266, an all-zero level is not sent
            continue
        fill = None
        for run, value in rle(seq):
            group = eg_bits(run) + eg_bits(value)
            cost['run_bits'] += eg_bits(run)
            cost['value_bits'] += eg_bits(value)
            if fill is not None and fill + group > budget:
                cost['bytes'] += math.ceil((HEADER_BITS + fill) / 8)
                cost['packets'] += 1
                fill = None
            fill = group if fill is None else fill + group
        if fill is not None:
            cost['bytes'] += math.ceil((HEADER_BITS + fill) / 8)
            cost['packets'] += 1
    return cost


def gray(path):
    return np.array(Image.open(path).convert('L'), float)


def sequence(gc):
    """video.cc:133-153: the type of each frame from the trace, the M frame coded as image - 128,
    the D frame as image - last M image, the original one (video.cc:145), not the decoded one."""
    d = os.path.join(SETS, encoding(gc))
    lines = [l.rstrip('\n').split('\t') for l in open(os.path.join(d, 'st-frame.dat')) if not l.startswith('#')]
    packets = {}
    for l in open(os.path.join(d, 'st-priority.dat')):
        if not l.startswith('#'):
            k = int(l.split('\t')[1])
            packets[k] = packets.get(k, 0) + 1
    frames, ref_img, ref = [], None, None
    for f in lines:
        k, t = int(f[0]), f[1]
        img = gray(os.path.join(d, 'capturedImages', 'frame_%d.png' % k))
        if t == 'M':
            ref_img, ref = img, k
            coded = img - 128
        else:
            coded = img - ref_img
        frames.append(dict(k=k, type=t, ref=ref, img=img, coded=coded, bytes=int(f[2]), packets=packets[k]))
    return frames


def rng(values, fmt='%.1f'):
    return (fmt + ' to ' + fmt) % (min(values), max(values))


params = {gc: ipfile(gc) for gc, _ in GOPS}
QC = params['0.03']['qc']
assert all(p['qc'] == QC for p in params.values())
seqs = {gc: sequence(gc) for gc, _ in GOPS}
src_all = 'the three encodings (%s)' % ', '.join(encoding(gc) for gc, _ in GOPS)

# check: the reproduction against the traces, frame by frame
costs, mismatches = {}, []
for gc, g in GOPS:
    for f in seqs[gc]:
        c = frame_cost(f['coded'], QC)
        costs[(gc, f['k'])] = c
        if (c['bytes'], c['packets']) != (f['bytes'], f['packets']):
            mismatches.append('%s frame %d: %d B %d packets, trace %d B %d packets'
                              % (g, f['k'], c['bytes'], c['packets'], f['bytes'], f['packets']))
if mismatches:
    sys.exit('analyse_codec.py: the reproduction differs from the traces, nothing written:\n  '
             + '\n  '.join(mismatches))
n_frames = sum(len(s) for s in seqs.values())
add('check', 'reproduction', '%d of %d frames, bytes and packets identical to the traces' % (n_frames, n_frames),
    src_all)

# bytes: per GOP and frame type, from the traces
for gc, g in GOPS:
    d = encoding(gc)
    for t in 'MD':
        fr = [f for f in seqs[gc] if f['type'] == t]
        add('bytes', '%s %s frames' % (g, t), '%d' % len(fr), d + '/st-frame.dat')
        add('bytes', '%s %s mean bytes' % (g, t), '%.0f' % np.mean([f['bytes'] for f in fr]), d + '/st-frame.dat')
        add('bytes', '%s %s mean packets' % (g, t), '%.1f' % np.mean([f['packets'] for f in fr]),
            d + '/st-priority.dat')
    total_b = sum(f['bytes'] for f in seqs[gc])
    total_p = sum(f['packets'] for f in seqs[gc])
    add('bytes', '%s packets' % g, '%d' % total_p, d + '/st-priority.dat')
    add('bytes', '%s bytes per packet' % g, '%.2f' % (total_b / total_p), d + '/st-priority.dat')
add('bytes', 'raw image', '%d' % seqs['0.03'][0]['img'].size, 'one byte per pixel of the QCIF image')

# paired: the same image coded as M in one GOP and as D in another
as_m = {f['k']: f['bytes'] for gc, _ in GOPS for f in seqs[gc] if f['type'] == 'M'}
extra = [100 * (f['bytes'] / as_m[f['k']] - 1) for gc, _ in GOPS for f in seqs[gc]
         if f['type'] == 'D' and f['k'] in as_m]
add('paired', 'comparisons', '%d' % len(extra), src_all + ', st-frame.dat')
add('paired', 'D over M', rng(extra) + ' %', src_all + ', st-frame.dat')

# corr and energy: the 15 (D frame, M reference) pairs of the three GOPs
imgs = {f['k']: f['img'] for gc, _ in GOPS for f in seqs[gc]}
pairs = sorted({(f['k'], f['ref']) for gc, _ in GOPS for f in seqs[gc] if f['type'] == 'D'})
src_pairs = '%d pairs (D frame, M reference) of the three GOPs: %s' % (
    len(pairs), ' '.join('%d-%d' % p for p in pairs))
acc = {name: [0.0, 0.0, 0.0] for name, _, _ in BANDS}
ac_ratio, ac_positions = [], []
for k, r in pairs:
    a = dct_zigzag(imgs[k] - 128)
    b = dct_zigzag(imgs[r] - 128)
    res = a - b
    for name, lo, hi in BANDS:
        A, B = a[:, lo:hi].ravel(), b[:, lo:hi].ravel()
        acc[name][0] += np.corrcoef(A, B)[0, 1]
        acc[name][1] += np.mean(A ** 2)
        acc[name][2] += np.mean((A - B) ** 2)
    ea, er = np.mean(a[:, 1:] ** 2, axis=0), np.mean(res[:, 1:] ** 2, axis=0)
    ac_ratio.append(er.sum() / ea.sum())
    ac_positions.append(int((er > ea).sum()))
for name, _, _ in BANDS:
    rho, ea, er = acc[name]
    add('corr', name, '%.2f' % (rho / len(pairs)), src_pairs)
    add('energy', name, '%.2f' % (er / ea), src_pairs + ', residual over image (image - 128)')
add('energy', 'AC per pair', rng(ac_ratio, '%.2f'), src_pairs)
add('energy', 'AC pairs above 1', '%d of %d' % (sum(x > 1 for x in ac_ratio), len(pairs)), src_pairs)
add('energy', 'AC positions above the image per pair', '%d to %d of 63' % (min(ac_positions), max(ac_positions)),
    src_pairs)

# nonzero and bits: the 36 coded frames at the article's quality
for t in 'MD':
    cs = [costs[(gc, f['k'])] for gc, _ in GOPS for f in seqs[gc] if f['type'] == t]
    src = '%d %s frames of the three GOPs, qualityCoef %g' % (len(cs), t, QC)
    pix = cs[0]['coefs'].size
    add('nonzero', '%s AC' % t, rng([100 * np.mean(c['coefs'][:, 1:] != 0) for c in cs]) + ' %', src)
    add('bits', '%s values per coefficient' % t, rng([c['value_bits'] / pix for c in cs], '%.2f'), src)
    add('bits', '%s runs per coefficient' % t, rng([c['run_bits'] / pix for c in cs], '%.2f'), src)
add('bits', 'quantisation step', '%.2f to %.2f' % (Z.min() * QC, Z.max() * QC),
    'JPEG luminance table times qualityCoef %g (quantification.cc:5, 11)' % QC)

# quality: the sweep, GOP 9-3
sweep, per_frame = {}, {}
for qc in QUALITIES:
    per_frame[qc] = [(f['type'], frame_cost(f['coded'], qc)['bytes']) for f in seqs['0.03']]
    m = np.mean([b for t, b in per_frame[qc] if t == 'M'])
    d = np.mean([b for t, b in per_frame[qc] if t == 'D'])
    sweep[qc] = (m, d)
    src = 'GOP 9-3 (%s), qualityCoef %g' % (encoding('0.03'), qc)
    add('quality', '%g M mean bytes' % qc, '%.0f' % m, src)
    add('quality', '%g D mean bytes' % qc, '%.0f' % d, src)
    add('quality', '%g D over M' % qc, '%.3f' % (d / m), src)
add('quality', 'D over M, sweep', rng([d / m for m, d in sweep.values()], '%.3f'),
    'GOP 9-3, qualityCoef %g to %g' % (QUALITIES[0], QUALITIES[-1]))

# check: the sweep against EvalVSN itself, in a temporary directory
if ENCODER:
    p = params['0.03']
    with tempfile.TemporaryDirectory() as tmp:
        ip = os.path.join(tmp, 'sweep.dat')
        with open(ip, 'w') as f:
            for qc in QUALITIES:
                f.write('QC-%g MMPEG %g %d 0.03 %g 13 CLA8 RLE1 EG\n' % (qc, p['fps'], p['threshold'], qc))
        subprocess.run([EVALVSN, '-ip', ip, '-vf', VIDEO, '-od', os.path.join(tmp, 'enc'), '-mpl', str(MPL)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        same = 0
        for qc in QUALITIES:
            lines = [l.split('\t') for l in open(os.path.join(tmp, 'enc', 'fire_qcif-QC-%g' % qc, 'st-frame.dat'))
                     if not l.startswith('#')]
            encoder = [(l[1], int(l[2])) for l in lines]
            if encoder != per_frame[qc]:
                sys.exit('analyse_codec.py: at qualityCoef %g EvalVSN gives %s, the reproduction %s; nothing written'
                         % (qc, encoder, per_frame[qc]))
            same += 1
    add('check', 'encoder sweep', '%d of %d qualities, type and bytes of the 12 frames identical to '
        'evalvsn/bin/evalvsn' % (same, len(QUALITIES)), 'evalvsn -mpl %d on evalvsn/videos/fire_qcif.avi' % MPL)
else:
    add('check', 'encoder sweep', 'not run (--encoder)', 'evalvsn/bin/evalvsn')

# source: the step between the coded images, video.cc:60 with evalvsn.cc:143
probe = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                        'stream=r_frame_rate,nb_frames', '-of', 'csv=p=0', VIDEO],
                       check=True, capture_output=True, text=True).stdout.strip().split(',')
num, den = probe[0].split('/')
rate, n_src = float(num) / float(den), int(probe[1])
fps = params['0.03']['fps']
step = math.ceil(n_src / (round(n_src / rate) * fps))
d = os.path.join(SETS, encoding('0.03'))
for f in seqs['0.03']:
    s = 1 + step * (f['k'] - 1)
    if np.abs(gray(os.path.join(d, 'allImages', 'fire_qcif_%d.png' % s)) - f['img']).max() != 0:
        sys.exit('analyse_codec.py: coded image %d is not source image %d; nothing written' % (f['k'], s))
src_video = 'evalvsn/videos/fire_qcif.avi, %d images at %g images/s; video.cc:60' % (n_src, rate)
add('source', 'images', '%d' % n_src, src_video)
add('source', 'step', '%d images' % step,
    src_video + '; each coded image equals source image 1 + %d (k - 1) (%s/allImages)' % (step, encoding('0.03')))
add('source', 'interval', '%.2f s' % (step / rate), src_video)
base = dct_zigzag(gray(os.path.join(d, 'allImages', 'fire_qcif_%d.png' % (1 + step))))[:, 10:].ravel()
for lag in LAGS:
    other = dct_zigzag(gray(os.path.join(d, 'allImages', 'fire_qcif_%d.png' % (1 + step + lag))))[:, 10:].ravel()
    src = 'source images %d and %d (%s/allImages)' % (1 + step, 1 + step + lag, encoding('0.03'))
    add('source', 'AC 10-63 corr, lag %d' % lag, '%.2f' % np.corrcoef(base, other)[0, 1], src)
    add('source', 'AC 10-63 energy, lag %d' % lag, '%.2f' % (np.mean((base - other) ** 2) / np.mean(base ** 2)), src)

with open(OUT, 'w') as f:
    f.write('# compiled by analyse_codec.py from the encodings of experiments/sets and evalvsn/videos/fire_qcif.avi\n')
    f.write('quantity\tkey\tvalue\tsource\n')
    for r in rows:
        f.write('\t'.join(r) + '\n')
for r in rows:
    print('%-8s %-40s %s' % r[:3])
