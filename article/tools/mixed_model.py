#!/usr/bin/env python3
"""Mixed model of the PSNR of the three cross-layer stacks (study 09), random
intercept per seed (fire and realisation): the check that the methods' sentence
on the mixed model rests on. Prints the Wald test of each fixed term.

Usage: python3 tools/mixed_model.py   (needs statsmodels; the other tools do not)
"""
import os
import warnings
import pandas as pd
import statsmodels.formula.api as smf

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'data', 'comparative.tsv')
d = pd.read_csv(SRC, sep='\t', comment='#')
d = d[d['stack'] != 'aomdv'].copy()
d['seed'] = d['fire'] + '-' + d['realisation']
warnings.simplefilter('ignore')
m = smf.mixedlm('psnr ~ C(stack, Sum) * C(split, Sum) * C(fire, Sum)', d, groups=d['seed']).fit(reml=True)
print('n =', len(d), 'seeds =', d['seed'].nunique())
print(m.wald_test_terms(scalar=True))
