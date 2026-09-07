#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Ecart du PROTOTYPE a la machine, sur la meme grille que le portage.

C'est l'etalon manquant : sans lui, un ecart de 1,2 dB entre le portage et les IR
ne veut rien dire — on ne sait pas si c'est le portage qui devie ou si le
prototype lui-meme est deja a cette distance de la machine.

Moyenne sur huit phases d'impulsion : le bloqueur d'ordre zero rend la reponse
impulsionnelle dependante de la phase, une seule mesure varie de plusieurs dB.
"""
import json, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).parent))
from analyse_ir import db
from moteur import FS, Moteur, macro_color

ICI = Path(__file__).parent
CLES = [("lxp1_room_small1","Small_1"),("lxp1_room_small2","Small_2"),
        ("lxp1_room_medium1","Medium_1"),("lxp1_room_medium2","Medium_2"),
        ("lxp1_room_large1","Large_1"),("lxp1_room_large2","Large_2"),
        ("lxp1_hall_b","Hall_B"),("lxp1_hall_d","Hall_D"),
        ("lxp1_room_plate_b","Plate_B"),("lxp1_room_plate_d","Plate_D"),
        ("lxp1_room_gate","Gate"),("lxp1_room_inverse","Inverse")]
DEC = [200,201,202,203,204,205,206,207]
G = [250,500,1000,2000,3000,4000,6000,8000,10000,12000,14000,16000,17000,18000]

def profil(v):
    n = 1 << 17
    v = v[:n] if len(v) >= n else np.pad(v, (0, n-len(v)))
    S = np.abs(np.fft.rfft(v, n)); f = np.fft.rfftfreq(n, 1/FS)
    b = lambda c: float(np.mean(S[(f >= c/1.12) & (f <= c*1.12)]**2))
    r = b(1000.0)
    return np.array([db(np.sqrt(b(c)/r)) for c in G])

params = json.loads((ICI/"accord"/"programmes.json").read_text(encoding="utf-8"))
n = int(FS*4.0)
tout = []
print(f"  {'programme':11s} {'ecart moyen':>12s} {'pire':>8s}")
for cle, court in CLES:
    cible = json.loads((ICI/"mesures"/f"{cle}.json").read_text(encoding="utf-8"))
    sp = dict((int(a), b) for a, b in cible["spectre_db"])
    vise = np.array([sp[c] - sp[1000] for c in G])
    b = dict(params[cle]); b["correction_fir"] = np.array(b["correction_fir"])
    q = macro_color(b, 0.5); q["mod_profondeur"] = 0.0; q["bits"] = 0
    ps = []
    for d in DEC:
        x = np.zeros(n); x[d] = 1.0
        L, R = Moteur(fs=FS, **q).rendre(x)
        ps.append(profil(L + R))
    e = np.abs(np.mean(ps, axis=0) - vise)
    tout.append(e)
    print(f"  {court:11s} {e.mean():11.2f} dB {e.max():7.2f}")
t = np.array(tout)
print(f"\n  PROTOTYPE contre la machine : moyen {t.mean():.2f} dB, pire {t.max():.2f} dB")
