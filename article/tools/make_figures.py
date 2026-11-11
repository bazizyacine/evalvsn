#!/usr/bin/env python3
"""Draw the article's plots from the data the repository keeps, into
src/figures/<lang>_*.png at 600 dpi (MDPI takes PNG, JPEG or TIFF, not PDF).

The figures follow the thesis's conventions (its plot_survie.py):
serif type, means only, the panels lettered and captioned under each panel,
one framed legend at the top right with one entry per line, legend colours
equal to the marks', the edition's decimal marker on the axes. The colours
are the Okabe-Ito palette, readable under every colour-vision deficiency.

Each figure is drawn at the width it prints at (PRINT below, the width of
its \\includegraphics in sections/fr_*.tex), so that its type prints at the
size set here: 8 pt for ticks, legends and labels, 9 pt for axis titles.
The PNG is 8-bit RGB without alpha, as MDPI asks.

Inputs: src/data/comparative.tsv (the 360 simulations of study 09) and
src/data/fire-map-F1.tsv to fire-map-F3.tsv (its three fires), both from
compile_comparative.py; src/data/galerie/*.png (frames decoded at the sink).

Usage:
    python3 tools/make_figures.py [--lang fr|en|both]
"""
import argparse
import io
import math
import os
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.patches import Patch, Polygon  # noqa: E402
from matplotlib.ticker import FuncFormatter  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, '..', 'src'))
DATA = os.path.join(SRC, 'data')
OUT = os.path.join(SRC, 'figures')
sys.path.insert(0, HERE)

INK, MUTED, GRID = '#1a1a19', '#6b6a63', '#d9d8d0'
plt.rcParams.update({
    'font.family': 'serif', 'font.size': 9, 'axes.titlesize': 9.5,
    'axes.labelsize': 9, 'legend.fontsize': 8, 'xtick.labelsize': 8, 'ytick.labelsize': 8,
    'axes.edgecolor': MUTED, 'axes.labelcolor': INK, 'xtick.color': MUTED, 'ytick.color': MUTED,
    'axes.spines.top': False, 'axes.spines.right': False,
    'grid.color': GRID, 'grid.linewidth': 0.6, 'axes.grid': True, 'axes.axisbelow': True,
    'lines.linewidth': 1.6, 'lines.markersize': 5,
    'mathtext.fontset': 'dejavuserif',  # the symbols in the serif of the labels
})
# The print widths, in inches: the MDPI class's \textwidth (394.355 pt) or its
# full width under adjustwidth (525.522 pt), times the factor of the figure's
# \includegraphics in sections/fr_*.tex. When the .tex changes a factor,
# change it here and regenerate.
TEXT_W, FULL_W = 394.35522 / 72.27, 525.52245 / 72.27
PRINT = {'scenario-carte': 0.85 * FULL_W, 'rejet-anticipe': 0.72 * TEXT_W,
         'comparatif-psnr': 0.82 * FULL_W, 'comparatif-m1': 0.82 * FULL_W,
         'galerie': 0.85 * FULL_W, 'trajectoires': 0.85 * TEXT_W}
PAD = 0.04  # white margin around the drawing, inches
# Okabe-Ito
GREY, BLUE, SKY, ORANGE, GREEN, VERMIL, PURPLE, YELLOW = (
    '#999999', '#0072B2', '#56B4E9', '#E69F00', '#009E73', '#D55E00', '#CC79A7', '#F0E442')
LANG = 'fr'
T = {
    'fr': {'psnr': 'PSNR au puits (dB)',
           'floor': 'seuil de 28 dB',
           'x': '$x$ (m)', 'y': '$y$ (m)', 'alive': 'relais en service', 'zone': 'zone chauffée, TZ = 60 s',
           'burnt': 'surface brûlée', 'front': 'détruit par le front', 'early': 'en panne avant le front',
           'heated': 'relais dans la zone chauffée', 'camera': 'caméra (0, 0), mode alarme', 'sink': 'puits (360, 360)', 'ignition': 'allumage du feu',
           'wind': 'vent $U$', 't': '$t$ = %d s', 'ic': 'IC', 'rank': '$(\\rho - 1)/2L$',
           'admitted': 'admis, IC$_0$ = 0,2', 'other': 'limite pour IC$_0$ = 0,1, 0,3 ou 0,5',
           'mframes': 'frames M', 'dframes': 'frames D', 'always': '$\\rho \\leq 3$ toujours admis',
           'ref': 'codée,\nsans perte réseau',
           'stack': {'aomdv': 'AOMDV', 'rules': 'Règles', 'agents': 'Agents', 'hybrid': 'Hybride'},
           'md': 'GOP (M-D)', 'm1loss': 'perte du niveau M1 (%)'},
    'en': {'psnr': 'PSNR at the sink (dB)',
           'floor': '28 dB threshold',
           'x': '$x$ (m)', 'y': '$y$ (m)', 'alive': 'relay in service', 'zone': 'heated zone, TZ = 60 s',
           'burnt': 'burned area', 'front': 'destroyed by the front', 'early': 'failed before the front',
           'heated': 'relay in the heated zone', 'camera': 'camera (0, 0), alarm mode', 'sink': 'sink (360, 360)', 'ignition': 'fire ignition',
           'wind': 'wind $U$', 't': '$t$ = %d s', 'ic': 'IC', 'rank': '$(\\rho - 1)/2L$',
           'admitted': 'admitted, IC$_0$ = 0.2', 'other': 'limit for IC$_0$ = 0.1, 0.3 or 0.5',
           'mframes': 'M frames', 'dframes': 'D frames', 'always': '$\\rho \\leq 3$ always admitted',
           'ref': 'coded,\nno network loss',
           'stack': {'aomdv': 'AOMDV', 'rules': 'Rules', 'agents': 'Agents', 'hybrid': 'Hybrid'},
           'md': 'GOP (M-D)', 'm1loss': 'level M1 loss (%)'},
}


def num(v, nd=1):
    s = f'{v:.{nd}f}'
    return s.replace('.', ',') if LANG == 'fr' else s


def decimal_axes(*axes):
    f = FuncFormatter(lambda v, _: (('%g' % v).replace('.', ',') if LANG == 'fr' else '%g' % v).replace('-', '−'))
    for a in axes:
        a.xaxis.set_major_formatter(f)
        a.yaxis.set_major_formatter(f)


def letter_panels(axes, titles):
    for i, a in enumerate(axes):
        xl = a.get_xlabel()
        a.set_xlabel((xl + '\n\n' if xl else '') + f'({chr(97 + i)}) {titles[i]}')


def top_legend(fig, handles, labels, ncol=1):
    return fig.legend(handles, labels, loc='upper right', bbox_to_anchor=(1.0, 1.0), ncol=ncol, frameon=True,
                      fancybox=False, edgecolor='#9a9a92', framealpha=1.0, borderpad=0.5, handlelength=1.8,
                      labelspacing=0.35, borderaxespad=0.1)


def below(fig, artist, gap=0.06):
    """The figure fraction left under a legend at the top, less a gap (inches)."""
    bb = artist.get_window_extent(fig.canvas.get_renderer())
    return bb.y0 / fig.dpi / fig.get_figheight() - gap / fig.get_figheight()


def fit_width(fig, name, layout=None, keep_aspect=False):
    """Set the figure's width so that the saved PNG (its tight box plus PAD)
    is the print width of `name`; `layout` redoes the layout at each try, and
    `keep_aspect` scales the height with the width."""
    target = PRINT[name]
    for _ in range(8):
        if layout:
            layout()
        w = fig.get_tightbbox(fig.canvas.get_renderer()).width + 2 * PAD
        if abs(w - target) < 0.002:
            return
        W, H = fig.get_size_inches()
        fig.set_size_inches(W + target - w, H * (W + target - w) / W if keep_aspect else H)
    if layout:
        layout()


def save(fig, name):
    """600 dpi, 8-bit RGB: matplotlib writes RGBA, MDPI asks for RGB."""
    path = os.path.join(OUT, f'{LANG}_{name}.png')
    buf = io.BytesIO()
    fig.savefig(buf, format='png', dpi=600, bbox_inches='tight', pad_inches=PAD, facecolor='white')
    plt.close(fig)
    buf.seek(0)
    Image.open(buf).convert('RGB').save(path, dpi=(600, 600))
    return os.path.basename(path)


# --- the scenario --------------------------------------------------------------

def read_map(name):
    meta, rows = {}, []
    for line in open(os.path.join(DATA, name), encoding='utf8'):
        if line.startswith('# ignition'):
            v = line[2:].split()
            meta = {'ix': float(v[1]), 'iy': float(v[2])}
            meta.update({v[i]: float(v[i + 1]) for i in range(3, len(v), 2)})
        elif line.startswith('#') or line.startswith('id'):
            continue
        else:
            i, x, y, front, death = line.split()
            rows.append((int(i), float(x), float(y), float(front), float(death)))
    return meta, rows


def ellipse(meta, t):
    """FARSITE's ellipse at time t (evalvsn.tcl: rear focus at the ignition)."""
    U, R = meta['wind'], meta['head_speed']
    LB = min(8.0, 0.936 * math.exp(0.2566 * U) + 0.461 * math.exp(-0.1548 * U) - 0.397)
    HB = (LB + math.sqrt(LB * LB - 1)) / (LB - math.sqrt(LB * LB - 1))
    a, b = 0.5 * (R + R / HB) / LB, (R + R / HB) / 2
    c = b - R / HB
    th = math.radians(meta['wind_dir'])
    ph = np.linspace(0, 2 * math.pi, 240)
    xw, yw = c * t + b * t * np.cos(ph), a * t * np.sin(ph)
    return (meta['ix'] + xw * math.cos(th) - yw * math.sin(th),
            meta['iy'] + xw * math.sin(th) + yw * math.cos(th))


ICONS = os.path.join(SRC, 'figures-src', 'icons')
FOREST = '#e9f3e1'
S_FRONT, S_EARLY, S_ZONE, S_ALIVE = '#8e1b12', '#d94a2b', '#f2a65a', '#9a9a92'


def place(ax, name, x, y, zoom, z=5):
    """A Twemoji pictogram (CC BY 4.0), the drawings of the thesis's maps."""
    from matplotlib.image import imread
    from matplotlib.offsetbox import OffsetImage, AnnotationBbox
    ab = AnnotationBbox(OffsetImage(imread(os.path.join(ICONS, f'icon-{name}.png')), zoom=zoom), (x, y),
                        frameon=False, pad=0)
    ab.set_zorder(z)
    ax.add_artist(ab)


def scenario_figure():
    """The three fires of study 09, 90 s after the detection, in the
    illustrated style of the thesis's maps: the forest as a light ground, the
    relays as antenna pictograms on a disc whose shade gives their state at
    time t, the burnt ellipse and the heated zone ahead of it, the camera, the
    sink, the fire and its wind as pictograms.

    The maps print as large as before the print-size drawing (7.14 in drawn,
    printed at the PRINT width): the pictograms, discs and strokes are scaled
    by g, the type is not, so that it prints at 9 pt (panels), 8 pt (wind and
    legend)."""
    import random
    L = T[LANG]
    t = 150
    W = PRINT['scenario-carte'] - 2 * PAD
    g = PRINT['scenario-carte'] / 7.14  # the drawn width before, at the same map size on paper
    fires = [('F%d' % k,) + read_map('fire-map-F%d.tsv' % k) for k in (1, 2, 3)]
    fig = plt.figure(figsize=(W, 3.32))
    H = fig.get_figheight()
    axes = [fig.add_axes((0.005 + k * 0.333, 0.30, 0.325, 0.69)) for k in range(3)]
    for k, (ax, (label, meta, rows)) in enumerate(zip(axes, fires)):
        forest = ax.add_patch(plt.Rectangle((-32, -32), 424, 424, facecolor=FOREST, edgecolor='#7fae6a',
                                            lw=1.0 * g, zorder=0))
        rnd = random.Random(7)
        for _ in range(26):
            place(ax, 'tree' if rnd.random() < 0.6 else 'tree2', rnd.uniform(-15, 375), rnd.uniform(-15, 375),
                  0.028 * g, z=1)
        zx, zy = ellipse(meta, t + meta['zone_s'])
        ax.add_patch(Polygon(np.c_[zx, zy], closed=True, facecolor='#fbe3c4', alpha=0.85, edgecolor='#e0a060',
                             lw=0.8 * g, ls='--', zorder=1.5)).set_clip_path(forest)
        bx, by = ellipse(meta, t)
        ax.add_patch(Polygon(np.c_[bx, by], closed=True, facecolor='#f0b8a8', alpha=0.9, edgecolor='#b03a2e',
                             lw=1.2 * g, zorder=1.6)).set_clip_path(forest)
        for i, x, y, front, death in rows:
            if i in (0, 99):
                continue
            if front <= t:
                col = S_FRONT
            elif death <= t:
                col = S_EARLY
            elif front <= t + meta['zone_s']:
                col = S_ZONE
            else:
                col = S_ALIVE
            ax.plot(x, y, 'o', ms=8.5 * g, color=col, markeredgecolor='white', markeredgewidth=0.6 * g, zorder=3)
            place(ax, 'relay', x, y, 0.022 * g, z=4)
        place(ax, 'camera', 0, 0, 0.05 * g, z=6)
        place(ax, 'sink', 360, 360, 0.055 * g, z=6)
        place(ax, 'fire', meta['ix'], meta['iy'], 0.05 * g, z=6)
        # The wind, in a band above the map so that it hides no relay.
        th = math.radians(meta['wind_dir'])
        cx, cy, r = 150, 420, 22
        ax.annotate('', xy=(cx + r * math.cos(th), cy + r * math.sin(th)),
                    xytext=(cx - r * math.cos(th), cy - r * math.sin(th)),
                    arrowprops=dict(arrowstyle='-|>', color=INK, lw=1.4 * g), zorder=8)
        ax.text(cx - 40, cy, L['wind'], fontsize=8, color=INK, ha='right', va='center', zorder=8)
        ax.set_xlim(-35, 395); ax.set_ylim(-35, 450); ax.set_aspect('equal')
        ax.set_xticks([]); ax.set_yticks([]); ax.grid(False)
        for sp in ax.spines.values():
            sp.set_visible(False)
        ax.set_xlabel(f'({chr(97 + k)}) {label}, ' + L['t'] % t, labelpad=4)
    # The legend, framed under the panels: three columns, each as wide as its
    # longest label, so that 8 pt type fits the print width.
    items = [(('disc', S_ALIVE), L['alive']), (('disc', S_ZONE), L['heated']), (('disc', S_EARLY), L['early']),
             (('disc', S_FRONT), L['front']), (('patch', '#f0b8a8'), L['burnt']), (('patch', '#fbe3c4'), L['zone']),
             (('icon', 'camera'), L['camera']), (('icon', 'sink'), L['sink']), (('icon', 'fire'), L['ignition'])]
    rnd_ = fig.canvas.get_renderer()

    def width(s, fs):
        tx = fig.text(0, 0, s, fontsize=fs)
        w = tx.get_window_extent(rnd_).width / fig.dpi
        tx.remove()
        return w
    fs, key, gap, margin = 8, 0.24, 0.22, 0.10  # inches: the key and its gap, between columns, inside the frame
    cols = [max(width(lab, fs) for _, lab in items[c * 3:c * 3 + 3]) + key for c in range(3)]
    total = sum(cols) + 2 * gap + 2 * margin
    lh = 0.62  # legend height, inches
    lax = fig.add_axes(((W - total) / 2 / W, 0.012, total / W, lh / H))
    lax.set_xlim(0, total); lax.set_ylim(0, lh)
    lax.set_xticks([]); lax.set_yticks([]); lax.grid(False)
    for sp in lax.spines.values():
        sp.set_visible(True); sp.set_edgecolor('#9a9a92'); sp.set_linewidth(0.8)
    for k, ((kind, val), label) in enumerate(items):
        c = k // 3
        cx = margin + sum(cols[:c]) + c * gap
        cy = lh * (0.80 - (k % 3) * 0.30)
        if kind == 'disc':
            lax.plot(cx + 0.07, cy, 'o', ms=7 * g, color=val, markeredgecolor='white')
            place(lax, 'relay', cx + 0.07, cy, 0.018 * g)
        elif kind == 'icon':
            place(lax, val, cx + 0.07, cy, 0.032 * g)
        else:
            lax.add_patch(plt.Rectangle((cx, cy - 0.065), 0.15, 0.13, facecolor=val, edgecolor='#b03a2e', lw=0.6))
        lax.text(cx + key, cy, label, va='center', ha='left', fontsize=fs, color=INK)
    return save(fig, 'scenario-carte')


# --- the early discard ------------------------------------------------------------

def discard_figure():
    L = T[LANG]
    Lv = 13
    ic = np.linspace(0, 1, 401)

    def limit(ic0):
        return np.clip(np.where(ic <= ic0, 1.0, 1 - (ic - ic0) / 0.4), 0, 1)
    fig, ax = plt.subplots(figsize=(3.95, 2.85))
    ax.fill_between(ic, 0, limit(0.2), color='#cfe8d9', zorder=1)
    ax.fill_between(ic, 0, 3 / (2 * Lv), color='#9fd3b5', zorder=1)
    ax.plot(ic, limit(0.2), color=GREEN, lw=1.8, zorder=3)
    for ic0 in (0.1, 0.3, 0.5):
        ax.plot(ic, limit(ic0), color=GREY, lw=0.9, ls='--', zorder=2)
    ax.axhline(0.5, color=MUTED, lw=0.8, ls=':')
    ax.text(0.985, 0.25, L['mframes'], ha='right', va='center', fontsize=8, color=MUTED)
    ax.text(0.985, 0.75, L['dframes'], ha='right', va='center', fontsize=8, color=MUTED)
    ax.set_xlim(0, 1); ax.set_ylim(0, 1)
    ax.set_xlabel(L['ic']); ax.set_ylabel(L['rank'])
    decimal_axes(ax)
    h = [Patch(facecolor='#cfe8d9', edgecolor=GREEN), Patch(facecolor='#9fd3b5'),
         Line2D([], [], color=GREY, lw=0.9, ls='--')]
    leg = top_legend(fig, h, [L['admitted'], L['always'], L['other']])
    fit_width(fig, 'rejet-anticipe', lambda: fig.tight_layout(rect=(0, 0, 1, below(fig, leg)), pad=0.5))
    return save(fig, 'rejet-anticipe')


# --- results ----------------------------------------------------------------------

# --- the comparative study (study 09) ----------------------------------------------

STACKS = [('aomdv', GREY, 'o'), ('rules', BLUE, 's'), ('agents', ORANGE, '^'), ('hybrid', GREEN, 'v')]
SPLITS = [('3M9D', '3-9'), ('6M6D', '6-6'), ('9M3D', '9-3')]


def comparative():
    from analyse_comparative import load
    out = {}
    for r in load():
        out.setdefault((r['fire'], r['split'], r['stack']), []).append(r)
    return out


def comp_figure(c, metric, ylabel, name, floor=False, ylim=None, yticks=None):
    """One panel per fire, the M-D split on the x axis, one line per stack;
    means only, the tables carry the deviations and the tests."""
    L = T[LANG]
    fig, axes = plt.subplots(1, 3, figsize=(PRINT[name], 2.85), sharey=True)
    for ax, fire in zip(axes, ('F1', 'F2', 'F3')):
        for st, col, mk in STACKS:
            xs, ys = [], []
            for k, (sp, _) in enumerate(SPLITS):
                rows = c.get((fire, sp, st))
                if rows:
                    xs.append(k)
                    ys.append(sum(float(r[metric]) for r in rows) / len(rows))
            ax.plot(xs, ys, color=col, marker=mk, label=L['stack'][st])
        if floor:
            ax.axhline(28, color=VERMIL, lw=1.0, ls='--', label=L['floor'])
        ax.set_xticks(range(len(SPLITS)))
        ax.set_xticklabels([lab for _, lab in SPLITS])
        ax.set_xlabel(L['md'])
        if ylim:
            ax.set_ylim(*ylim)
        if yticks is not None:  # the ticks of the larger drawing, kept at print size
            ax.set_yticks(yticks)
    axes[0].set_ylabel(ylabel)
    letter_panels(axes, ['F1', 'F2', 'F3'])
    decimal_axes(*axes)
    for ax in axes:  # after decimal_axes, whose formatter would number the splits
        ax.set_xticks(range(len(SPLITS)))
        ax.set_xticklabels([lab for _, lab in SPLITS])
    h, lab = axes[0].get_legend_handles_labels()
    leg = top_legend(fig, h, lab, ncol=len(h))
    fit_width(fig, name, lambda: fig.tight_layout(rect=(0, 0, 1, below(fig, leg)), pad=0.5, w_pad=0.8))
    return save(fig, name)


def gallery_figure(c):
    """Frame 8 of the first simulation of F1 at the 9-3 split (study 09),
    coded without network loss then as the sink decodes it for each stack;
    under each, the PSNR of that simulation."""
    L = T[LANG]
    stacks = [('reference', None), ('aomdv', 'aomdv'), ('rules', 'rules'), ('agents', 'agents'),
              ('hybrid', 'hybrid')]
    fig, axes = plt.subplots(1, len(stacks), figsize=(PRINT['galerie'], 1.80))
    titles = []
    for ax, (f, st) in zip(axes, stacks):
        img = np.array(Image.open(os.path.join(DATA, 'galerie', f + '.png')).convert('L'))
        ax.imshow(img, cmap='gray', vmin=0, vmax=255)
        ax.set_xticks([]); ax.set_yticks([]); ax.grid(False)
        for s in ax.spines.values():
            s.set_visible(False)
        if st is None:
            titles.append(L['ref'])
        else:
            r01 = [r for r in c[('F1', '9M3D', st)] if r['real'] == 'r01'][0]
            titles.append(L['stack'][st] + '\n' + num(float(r01['psnr'])) + ' dB')
    for i, (ax, t) in enumerate(zip(axes, titles)):
        ax.set_xlabel(f'({chr(97 + i)}) ' + t, fontsize=8)
    fit_width(fig, 'galerie', lambda: fig.tight_layout(w_pad=0.5))
    return save(fig, 'galerie')


def main():
    global LANG
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--lang', choices=('fr', 'en', 'both'), default='fr')
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    for LANG in (('fr', 'en') if args.lang == 'both' else (args.lang,)):
        for fn in (scenario_figure, discard_figure):
            print(fn())
        # the comparative study's results (study 09)
        c = comparative()
        print(comp_figure(c, 'psnr', T[LANG]['psnr'], 'comparatif-psnr', floor=True, ylim=(16, 31),
                          yticks=range(16, 31, 2)))
        print(comp_figure(c, 'm1loss', T[LANG]['m1loss'], 'comparatif-m1', ylim=(0, 80),
                          yticks=range(0, 81, 10)))
        print(gallery_figure(c))


if __name__ == '__main__':
    main()
