#!/usr/bin/env python3
"""Draw the agents' learning trajectories of study 10 (the agents' traces) into
src/figures/<lang>_trajectoires.png at 600 dpi, from src/data/agent-trace.tsv
(compiled by compile_agent_trace.py).

Two panels against the simulated time t, means of the 60 traced simulations
at GOP 9-3: (a) the share of node-state pairs whose best action has left the
initial one, (b) the mean cumulative decisions per node that has decided, on a
log axis. One line per agent: path, congestion, channel access and early
discard from the hybrid stack, link-break detection from the agents stack.

What the figure leaves out, and why:
- the 160 s bin, which is partial (the simulation ends at 168 s and burnt
  nodes stop writing), and the t = 999 rows (each node's last snapshot, taken
  at different instants): the end-of-simulation values are in the agents
  table (tables/<lang>_revision-agents.tex);
- the discovery agent, which decides about eleven times a simulation on one
  node, and the power agent, which writes no values;
- in both panels, the bins where fewer than one node a simulation decided
  (the link-break detection before the alarm, 0.0 to 0.1 node): a share of
  almost no node-state, and a ratio of rounded means, are meaningless.

A node writes its snapshot at its first HELLO after each 20 s mark, so the
snapshots of the bin [t, t + 20 s) fall anywhere in it: each point is drawn
at the bin's middle, t + 10 s.

The style is make_figures.py's (sans-serif type, Okabe-Ito palette, framed
legend at the top right, in two columns: the first three agents of the hybrid
stack, then its early discard and the agents stack's link-break detection;
lettered panels, the edition's decimal marker); importing it only sets
matplotlib's rcParams.

Usage:
    python3 tools/make_figure_trajectories.py [--lang fr|en|both]
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_figures as mf  # noqa: E402
from make_figures import plt, Line2D, FuncFormatter  # noqa: E402

DATA = os.path.join(mf.DATA, 'agent-trace.tsv')
BINS = range(0, 160, 20)  # complete bins only: 0 to 140 s
DETECTION = 60  # t_d, the alarm (materials and methods)
WIDTH = mf.PRINT['trajectoires']  # drawn at its print width (make_figures.PRINT)

# agent, stack, colour, marker, line style; the near-equal pairs (path and
# congestion, access and discard) differ by their line style. The hybrid
# stack's agents take colours that no stack takes in the comparative figures
# (grey AOMDV, blue rules, green hybrid); the link-break detection keeps the
# agents stack's orange, and a dash-dot line: its orange and the channel
# access's sky blue print the same grey (CIE L* 70.6 and 69.8).
AGENTS = [('path', 'hybrid', mf.INK, 'o', '-'),
          ('congestion', 'hybrid', mf.VERMIL, 's', '--'),
          ('access', 'hybrid', mf.SKY, '^', '-'),
          ('discard', 'hybrid', mf.PURPLE, 'D', '--'),
          ('break', 'agents', mf.ORANGE, 'v', '-.')]
T = {
    'fr': {'path': 'chemin', 'congestion': 'congestion', 'access': 'accès au canal', 'discard': 'rejet anticipé',
           'break': 'détection de rupture', 'hybrid': 'Hybride', 'agents': 'Agents',
           'x': '$t$ (s)', 'share': 'hors du choix initial (%)', 'count': 'décisions par nœud',
           'a': 'couples de nœud et d\u2019état', 'b': 'nœuds qui ont décidé'},
    'en': {'path': 'path', 'congestion': 'congestion', 'access': 'channel access', 'discard': 'early discard',
           'break': 'link-break detection', 'hybrid': 'Hybrid', 'agents': 'Agents',
           'x': '$t$ (s)', 'share': 'away from the initial choice (%)', 'count': 'decisions per node',
           'a': 'node-state pairs', 'b': 'nodes that decided'},
}


def load():
    lines = [l.rstrip('\n').split('\t') for l in open(DATA, encoding='utf8') if not l.startswith('#')]
    cols = lines[0]
    return {(r['arm'], r['agent'], int(r['t'])): r for r in (dict(zip(cols, l)) for l in lines[1:])}


def plain(v, _):
    """A log tick as a plain number. French sets thousands off by a thin space
    as its tables do; English follows MDPI, a comma from five digits only."""
    s = '%d' % round(v) if v >= 1 else ('%g' % v)
    if mf.LANG == 'fr':
        if len(s) > 3:
            s = s[:-3] + '\u2009' + s[-3:]
        return s.replace('.', ',')
    return s[:-3] + ',' + s[-3:] if len(s) > 4 else s


def figure(rows):
    L = T[mf.LANG]
    fig, (a, b) = plt.subplots(1, 2, figsize=(WIDTH, 2.75))
    handles, labels = [], []
    for agent, arm, col, mk, ls in AGENTS:
        style = dict(color=col, marker=mk, ls=ls, lw=1.3, ms=3.6 if mk != 'D' else 3.0)
        kept = [(t + 10, rows[(arm, agent, t)]) for t in BINS if float(rows[(arm, agent, t)]['nodes']) >= 1.0]
        a.plot([x for x, r in kept if r['changed'] != ''], [float(r['changed']) for x, r in kept if r['changed'] != ''],
               **style)
        b.plot([x for x, r in kept], [float(r['decisions']) / float(r['nodes']) for x, r in kept], **style)
        handles.append(Line2D([], [], **style))
        labels.append('%s, %s' % (L[agent], L[arm]))
    for ax in (a, b):
        ax.axvline(DETECTION, color=mf.MUTED, lw=0.8, ls=':', zorder=1)
        ax.text(DETECTION - 2, 0.97, '$t_d$', transform=ax.get_xaxis_transform(), ha='right', va='top',
                fontsize=8, color=mf.MUTED)
        ax.set_xlim(0, 160)
        ax.set_xticks(range(0, 161, 40))
        ax.set_xlabel(L['x'])
    a.set_ylim(0, 22)
    a.set_yticks(range(0, 21, 5))
    a.set_ylabel(L['share'])
    b.set_yscale('log')
    b.set_ylim(0.8, 1000)
    b.set_yticks([k * 10 ** e for e in range(3) for k in (1, 2, 5)] + [1000])
    b.yaxis.set_minor_locator(plt.NullLocator())
    b.set_ylabel(L['count'])
    mf.decimal_axes(a, b)
    b.yaxis.set_major_formatter(FuncFormatter(plain))
    mf.letter_panels((a, b), [L['a'], L['b']])
    leg = mf.top_legend(fig, handles, labels, ncol=2, columnspacing=1.2)
    mf.fit_width(fig, 'trajectoires', lambda: fig.tight_layout(rect=(0, 0, 1, mf.below(fig, leg, 0.01)), pad=0.3,
                                                                  w_pad=1.6))
    return mf.save(fig, 'trajectoires')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--lang', choices=('fr', 'en', 'both'), default='fr')
    args = ap.parse_args()
    rows = load()
    os.makedirs(mf.OUT, exist_ok=True)
    for mf.LANG in (('fr', 'en') if args.lang == 'both' else (args.lang,)):
        print(figure(rows))


if __name__ == '__main__':
    main()
