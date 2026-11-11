#!/usr/bin/env python3
"""Overheads of the four stacks for the revision (the overheads for a node): energy,
control traffic and memory, in src/tables/{fr,en}_revision-couts.tex, and the
processing figures the text quotes, printed.

Energy. The simulator's energy model counts the radio's states only (transmit,
receive, idle listening, sleep; ns-2 EnergyModel, rates in
experiments/run/radio-params.tcl). setGraphs.pl writes, for each set, the mean
over its realisations of the energy consumed per node ("Energy Consumption"),
by the whole network ("Power Consumption") and the lowest residual energy as a
share of the initial one ("Life Time"), each read at every node's last event in
the trace. These three files are compiled into src/data/overhead.tsv, one row
per set, so that the numbers stay once the sets are purged; --compile rebuilds
it, otherwise the versioned file is read. The busiest node's consumption is
derived as (1 - Life Time) x the initial energy of the configuration.

Control traffic. Messages sent by every node in a simulation (HELLO, RREQ
originated or forwarded, RERR, RREP), from src/data/revision.tsv for the traced
arms (agents-tr and hybrid-tr, identical runs to the agents and hybrid stacks at
GOP 9-3) and AOMDV de controle (pq-ctl), and for the rules and AOMDV of study 09
from experiments/studies/09-comparative/results/routing.tsv when that file
exists; a dash otherwise.

Memory. What the agents add to a node: their value tables, sized from their
declarations in the simulator's headers, and the context of the decisions that
await an outcome, in a node's formats (the assumptions are the constants below).
The counters that only feed the trace (RL_TRACE_) are left out.

Processing. Printed only: the operations per packet read from the code, and the
decisions per node from src/data/agent-trace.tsv over the alarm.

Usage: make_tables_overhead.py [--compile]
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'src')
PHD = os.path.normpath(os.path.join(HERE, '..', '..'))
SETS = os.path.join(PHD, 'experiments', 'sets')
ROUTING = os.path.join(PHD, 'experiments', 'studies', '09-comparative', 'results', 'routing.tsv')
RADIO = os.path.join(PHD, 'experiments', 'run', 'radio-params.tcl')
NS = os.path.normpath(os.path.join(PHD, '..', 'ns-2', 'ns-allinone-2.35', 'ns-2.35'))
if not os.path.isdir(NS):   # the companion repository: the modules alone, at their path under ns-2.35/
    NS = os.path.join(PHD, 'ns-2-modules')
RT_H = os.path.join(NS, 'aomdv_qpv', 'aomdv_qpv.h')
MAC_H = os.path.join(NS, 'mac', 'mac-802_15_4EvalVSN.h')
MAC_CC = os.path.join(NS, 'mac', 'mac-802_15_4EvalVSN.cc')
OUT = os.path.join(SRC, 'data', 'overhead.tsv')

SPLIT = {'0p03': '9M3D', '0p02525': '6M6D', '0p01597': '3M9D'}
FIRE = {'fire': 'F1', 'fire2': 'F2', 'fire3': 'F3'}
# stack, the set's name, the traced arm of revision.tsv
STACKS = (('pq', 'set_AOMDV_GC-PN-{gc}-13_p9-n100-d40-{fire}-pq', None),
          ('pq-ctl', 'set_AOMDV_GC-PN-{gc}-13_p9-n100-d40-{fire}-pq-ctl-s10', 'pq-ctl'),
          ('rules', 'set_AOMDV_QPV_GC-PN-{gc}-13_p9-n100-d40-{fire}-rules', None),
          ('agents', 'set_AOMDV_QPV_GC-PN-{gc}-13_p9-n100-d40-{fire}-agents', 'agents-tr'),
          ('hybrid', 'set_AOMDV_QPV_GC-PN-{gc}-13_p9-n100-d40-{fire}-hybrid', 'hybrid-tr'))
LABEL = {'pq': ('AOMDV', 'AOMDV'), 'pq-ctl': ('AOMDV de contrôle', 'Control AOMDV'),
         'rules': ('Règles', 'Rules'), 'agents': ('Agents', 'Agents'), 'hybrid': ('Hybride', 'Hybrid')}
COLS = ('stack', 'fire', 'split', 'n', 'e_node', 'e_node_sd', 'e_total', 'e_total_sd', 'lifetime', 'lifetime_sd', 'e_max')

# Memory on a node (assumptions, stated in the table's footnote and the text):
# a value 4 bytes (single-precision float), an action, a state or a flag 1 byte,
# a neighbour's short address or a packet id 2 bytes, an instant 4 bytes, a
# per-interval packet count 2 bytes; per-neighbour and per-packet records sit in
# arrays indexed by the neighbour's slot or the packet's buffer, without a key,
# and a decision's context is freed with its packet. The simulator stores values
# as 8-byte doubles.
FLOAT, BYTE, ADDR, TIME, COUNT, DOUBLE = 4, 1, 2, 4, 2, 8
NEIGHBOURS = 4   # 40 m grid, range 48.6 m, diagonal 56.6 m out of reach
HELD = 16 + 1    # packets a node holds: its interface queue (16) and the frame in the MAC
DESTINATIONS = 1  # the discovery agent decides at the camera, for the sink alone
ALARM = (60.0, 168.0)  # detection to the end of a GOP 9-3 simulation (s)
EMIT = 0.015     # interval between two video packets of an image (s)
# A sensor-node microcontroller of the corpus for scale: the ATmega128L's 4 Kbytes
# of internal SRAM (refs/docs/energy-mmpeg/atmel_atmega128l_datasheet.pdf,
# features page; the bibliography's atmel_atmega128l_datasheet).
SRAM_REF = 4 * 1024

# Fields the simulator keeps per decision awaiting its outcome, in node formats.
CONTEXT = {
    # RlCtx (class, threshold, profile, occupancy, instant) and the early-discard
    # value of an admitted packet (discard_val_), per held packet
    'path': HELD * (3 * BYTE + FLOAT + TIME),
    'discard': HELD * FLOAT + BYTE + 3 * FLOAT + COUNT,  # + rle_a_, rle_ack_/fail_/disc_, rle_n_
    # the frame in the MAC: state (class, link), actions (access, power), pending
    # and failed flags, destination, packet id, start instant
    'access': 6 * BYTE + ADDR + ADDR + TIME,
    # per neighbour: the run's count and state, then the declaration awaiting its
    # verdict (instant, count, state)
    'break': NEIGHBOURS * (2 * BYTE + TIME + 2 * BYTE),
    # per destination: the flood in flight (action, bucket, instant)
    'discovery': DESTINATIONS * (2 * BYTE + TIME),
}
CONTEXT['path'] += BYTE  # rlp_cur_, the profile in use
# The agent each table belongs to, and its declaration
TABLES = (('path', RT_H, 'rlp_q_'), ('congestion', RT_H, 'rlc_q_'), ('break', RT_H, 'rlb_q_'),
          ('discovery', RT_H, 'rld_q_'), ('discard', RT_H, 'rle_q_'), ('access', MAC_H, 'rlmQ_'))
AGENTS = {'agents': ('path', 'congestion', 'break', 'discovery', 'discard', 'access', 'power'),
          'hybrid': ('path', 'congestion', 'discovery', 'discard', 'access', 'power')}


def dims(path, name):
    """Product of the array dimensions of `double name[a][b]...;` in a header."""
    m = re.search(r'double\s+%s((?:\[\d+\])+)\s*;' % re.escape(name), open(path).read())
    if not m:
        sys.exit('%s: no declaration of %s' % (path, name))
    n = 1
    for d in re.findall(r'\[(\d+)\]', m.group(1)):
        n *= int(d)
    return n


def power_steps():
    m = re.search(r'kRlpStepsNb\s*=\s*(\d+)', open(MAC_CC).read())
    return int(m.group(1))


def trace_counters():
    """Counters that only feed the trace: the long arrays and scalars named *_cnt_."""
    n = 0
    for path in (RT_H, MAC_H):
        for m in re.finditer(r'^\s*long\s+(\w+_cnt_)((?:\[\d+\])*)\s*;', open(path).read(), re.M):
            k = 1
            for d in re.findall(r'\[(\d+)\]', m.group(2)):
                k *= int(d)
            n += k
    return n


def radio():
    text = open(RADIO).read()
    get = lambda k: float(re.search(r'set val\(%s\)\s+(\S+)' % k, text).group(1))
    return {'initeng': get('initeng'), 'tx': get('txPower'), 'rx': get('rxPower'), 'idle': get('idlePower')}


def memory():
    values = {a: dims(p, n) for a, p, n in TABLES}
    values['power'] = power_steps() * NEIGHBOURS
    out = {}
    for stack, agents in AGENTS.items():
        v = sum(values[a] for a in agents)
        ctx = sum(CONTEXT.get(a, 0) for a in agents)
        out[stack] = {'values': v, 'tables': v * FLOAT, 'context': ctx, 'node': v * FLOAT + ctx, 'sim': v * DOUBLE}
    return values, out


def read_dat(path):
    v = open(path).read().split()
    return float(v[1]), float(v[2]), int(v[3])


def compile_sets():
    e0 = radio()['initeng']
    rows = []
    for gc, split in SPLIT.items():
        for fire, f in FIRE.items():
            for stack, pattern, _ in STACKS:
                s = os.path.join(SETS, pattern.format(gc=gc, fire=fire), 'graphs')
                try:
                    en = read_dat(os.path.join(s, 'Energy Consumption', 'Energy Consumption.dat'))
                    tot = read_dat(os.path.join(s, 'Power Consumption', 'Power Consumption.dat'))
                    lt = read_dat(os.path.join(s, 'Life Time', 'Life Time.dat'))
                except (OSError, IndexError, ValueError):
                    print('  %s: no energy graphs, left out' % os.path.basename(os.path.dirname(s)))
                    continue
                rows.append((stack, f, split, en[2], en[0], en[1], tot[0], tot[1], lt[0], lt[1],
                             (1.0 - lt[0] / 100.0) * e0))
    with open(OUT, 'w') as fh:
        fh.write('# compiled by tools/make_tables_overhead.py --compile from the sets\' graphs (setGraphs.pl): '
                 'radio energy per simulation, means over n realisations; e_max = (1 - lifetime/100) x %g J\n' % e0)
        fh.write('\t'.join(COLS) + '\n')
        for r in rows:
            fh.write('\t'.join(str(x) if isinstance(x, (str, int)) else '%.6f' % x for x in r) + '\n')
    print('%s: %d sets' % (os.path.relpath(OUT), len(rows)))


def load(path):
    lines = [l for l in open(path) if not l.startswith('#')]
    cols = lines[0].rstrip('\n').split('\t')
    return [dict(zip(cols, l.rstrip('\n').split('\t'))) for l in lines[1:] if l.strip()]


def energy(rows, stack, split='9M3D'):
    """Means over the simulations of the three fires, weighted by realisations."""
    sel = [r for r in rows if r['stack'] == stack and r['split'] == split]
    if not sel:
        return None
    n = sum(int(r['n']) for r in sel)
    return {k: sum(float(r[k]) * int(r['n']) for r in sel) / n for k in ('e_total', 'e_node', 'e_max')}


def control(stack, traced, rev, routing):
    out = {}
    for m in ('hello', 'rreq_all', 'rerr', 'rrep'):
        v = []
        if traced:
            v = [float(r[m]) for r in rev if r['arm'] == traced and r['split'] == '9M3D' and r['period'] == 'p9'
                 and r[m] != '']
        if not v and routing:
            name = dict((s, p) for s, p, _ in STACKS)[stack]
            sets = {name.format(gc='0p03', fire=f) for f in FIRE}
            v = [float(r[m]) for r in routing if r.get('set') in sets and r.get(m, '') != '']
        out[m] = sum(v) / len(v) if v else None
    return out


def num(x, nd, lang):
    if x is None:
        return '--'
    s = '%.*f' % (nd, x)
    i, _, d = s.partition('.')
    # French: a thin space from four digits; English (MDPI): no separator at
    # four digits, a comma from five (see check_translation_numbers.py)
    digits = len(i.lstrip('-'))
    if lang == 'fr' and digits >= 4:
        i = i[:-3] + '\\,' + i[-3:]
    elif lang != 'fr' and digits >= 5:
        i = '{:,}'.format(int(i))
    s = i + ('.' + d if d else '')
    return s.replace('.', '{,}') if lang == 'fr' else s


HEAD = {'fr': ('', '\\multicolumn{2}{c}{\\textbf{Énergie (J)\\textsuperscript{1}}}',
               '\\multicolumn{3}{c}{\\textbf{Messages de contrôle\\textsuperscript{2}}}', '\\textbf{Mémoire\\textsuperscript{3}}'),
        'en': ('', '\\multicolumn{2}{c}{\\textbf{Energy (J)\\textsuperscript{1}}}',
               '\\multicolumn{3}{c}{\\textbf{Control messages\\textsuperscript{2}}}', '\\textbf{Memory\\textsuperscript{3}}')}
# the busiest node's energy and the RREP count stay in the printed statistics only
# the stub header ("Pile") on this row, under the groups, as in the other tables
SUB = {'fr': ('Pile', 'Réseau', 'Par nœud', 'HELLO', 'RREQ', 'RERR', '(octets)'),
       'en': ('Stack', 'Network', 'Per node', 'HELLO', 'RREQ', 'RERR', '(bytes)')}


def write(lang, en_rows, rev, routing, mem):
    out = ['% generated by tools/make_tables_overhead.py from src/data/overhead.tsv, src/data/revision.tsv,',
           '% experiments/studies/09-comparative/results/routing.tsv (when present) and the agents\' declarations',
           '\\begin{tabularx}{\\linewidth}{lCCCCCC}', '\\toprule',
           ' & '.join(HEAD[lang]) + ' \\\\', '\\cmidrule(lr){2-3}\\cmidrule(lr){4-6}',
           ' & '.join('\\textbf{%s}' % h if h else '' for h in SUB[lang]) + ' \\\\', '\\midrule']
    for i, (stack, _, traced) in enumerate(STACKS):
        e = energy(en_rows, stack) or {}
        c = control(stack, traced, rev, routing)
        cells = [num(e.get('e_total'), 1, lang), num(e.get('e_node'), 2, lang)]
        cells += [num(c[m], 0, lang) for m in ('hello', 'rreq_all', 'rerr')]
        cells.append(num(float(mem[stack]['node']) if stack in mem else 0.0, 0, lang))
        out.append(' & '.join([LABEL[stack][0 if lang == 'fr' else 1]] + cells) + ' \\\\')
        out.append('\\bottomrule' if i == len(STACKS) - 1 else '\\midrule')
    out.append('\\end{tabularx}')
    with open(os.path.join(SRC, 'tables', '%s_revision-couts.tex' % lang), 'w') as f:
        f.write('\n'.join(out) + '\n')


def report(en_rows, values, mem, r):
    """The figures the text quotes, with their derivation."""
    print('\nEnergy (radio only; overhead.tsv):')
    cross = ('rules', 'agents', 'hybrid')
    spread, gap = [], []
    for split in SPLIT.values():
        for f in FIRE.values():
            e = {x['stack']: float(x['e_node']) for x in en_rows if x['split'] == split and x['fire'] == f}
            if all(s in e for s in cross):
                v = [e[s] for s in cross]
                spread.append(100 * (max(v) - min(v)) / min(v))
            if 'pq' in e and 'rules' in e:
                gap.append(100 * (e['pq'] - e['rules']) / e['rules'])
    print('  three cross-layer stacks, largest spread of the per-node energy in a cell: %.3f %% (%d cells)'
          % (max(spread), len(spread)))
    print('  AOMDV against the rules, per node: %.2f to %.2f %%' % (min(gap), max(gap)))
    for s, _, _ in STACKS:
        e = energy(en_rows, s)
        if e:
            print('  %-7s GOP 9-3: network %.1f J, per node %.3f J, busiest node %.3f J' % (s, e['e_total'], e['e_node'], e['e_max']))
    e = energy(en_rows, 'hybrid')
    print('  radio rates: tx %.3f W, rx %.3f W, idle %.3f W; tx over listening +%.1f %%'
          % (r['tx'], r['rx'], r['idle'], 100 * (r['tx'] - r['idle']) / r['idle']))
    print('  per node and simulation, %.2f J of the %g J battery: %.3f %%' % (e['e_node'], r['initeng'], 100 * e['e_node'] / r['initeng']))
    print('  listening alone empties the battery in %.1f h' % (r['initeng'] / r['idle'] / 3600))
    print('\nMemory (node formats; values %d bytes, simulator %d):' % (FLOAT, DOUBLE))
    print('  values per agent: %s' % ', '.join('%s %d' % kv for kv in values.items()))
    for s, m in mem.items():
        print('  %-7s %d values = %d bytes of tables + %d bytes of context = %d bytes; %d bytes as doubles'
              % (s, m['values'], m['tables'], m['context'], m['node'], m['sim']))
    print('  context per agent (bytes): %s' % ', '.join('%s %d' % kv for kv in CONTEXT.items()))
    print('  left out: %d trace counters (long), RL_TRACE_ only' % trace_counters())
    print('  interface queue for comparison: 16 packets of 74 bytes of payload = %d bytes' % (16 * 74))
    print('  share of the ATmega128L\'s %d bytes of SRAM: %s' % (SRAM_REF, ', '.join(
        '%s %.1f %%' % (s, 100.0 * m['node'] / SRAM_REF) for s, m in mem.items())))
    trace = load(os.path.join(SRC, 'data', 'agent-trace.tsv'))
    alarm = ALARM[1] - ALARM[0]
    print('\nProcessing (alarm %g s):' % alarm)
    for x in trace:
        if x['t'] == '999':
            print('  %-7s %-11s median %6s per node -> %.1f per s; mean per deciding node %.1f per s'
                  % (x['arm'], x['agent'], x['median'], float(x['median']) / alarm,
                     float(x['decisions']) / float(x['nodes']) / alarm))
    per_packet = 4
    print('  per packet: %d decisions (path, congestion, access, power); argmax over 5 + 4 + 9 + <=6 values: '
          '<= %d comparisons; <= %d uniform draws if not critical, none if critical; %d updates of 3 operations'
          % (per_packet, 4 + 3 + 8 + (power_steps() - 1), 2 * per_packet, per_packet))
    print('  camera: one packet every %g s = %.1f per s -> <= %.0f decisions per s'
          % (EMIT, 1 / EMIT, per_packet / EMIT))


def main():
    if '--compile' in sys.argv or not os.path.exists(OUT):
        compile_sets()
    en_rows = load(OUT)
    rev = load(os.path.join(SRC, 'data', 'revision.tsv'))
    routing = load(ROUTING) if os.path.exists(ROUTING) else None
    if routing is None:
        print('routing.tsv not there yet: the rules and AOMDV control cells stay dashes')
    values, mem = memory()
    for lang in ('fr', 'en'):
        write(lang, en_rows, rev, routing, mem)
    print('tables/{fr,en}_revision-couts.tex')
    report(en_rows, values, mem, radio())


if __name__ == '__main__':
    main()
