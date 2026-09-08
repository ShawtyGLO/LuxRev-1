#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Ecart du PORTAGE C++ a la machine — le chiffre qui manquait.

⚠️ POURQUOI CE BANC EXISTE. On avait deux mesures et il en manquait une :

    etalon_prototype.py    prototype Python  contre  machine
    comparer_portage.py    portage C++       contre  prototype Python

Le plugin etait donc juge contre le prototype, c'est-a-dire contre un
intermediaire. Tant que les deux ecarts restaient petits l'approximation
passait ; des que l'accordage bouge, elle ne dit plus rien — un portage peut
s'eloigner du prototype EN SE RAPPROCHANT de la machine, et on l'aurait lu
comme une regression.

Meme methode que l'etalon du prototype, au fichier de rendu pres :
  - moyenne sur HUIT phases d'impulsion, parce que le bloqueur d'ordre zero
    rend une reponse impulsionnelle dependante de la phase (jusqu'a 10 dB) ;
  - profil de bande normalise a 1 kHz, compare au spectre releve sur l'IR.

Marche a suivre :
    python _proto/etalon_portage.py
(le banc C++ est appele automatiquement, une fois par phase)
"""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from analyse_ir import db, lire_wav          # noqa: E402
from moteur import FS                        # noqa: E402

ICI = Path(__file__).parent
RACINE = ICI.parent
BANC = RACINE / "LuxRev-1" / "tools" / "banc_moteur.exe"
RENDUS = ICI / "rendus_moteur" / "ETALON"

CLES = [("lxp1_room_small1", "Small_1"), ("lxp1_room_small2", "Small_2"),
        ("lxp1_room_medium1", "Medium_1"), ("lxp1_room_medium2", "Medium_2"),
        ("lxp1_room_large1", "Large_1"), ("lxp1_room_large2", "Large_2"),
        ("lxp1_hall_b", "Hall_B"), ("lxp1_hall_d", "Hall_D"),
        ("lxp1_room_plate_b", "Plate_B"), ("lxp1_room_plate_d", "Plate_D"),
        ("lxp1_room_gate", "Gate"), ("lxp1_room_inverse", "Inverse")]
DEC = [200, 201, 202, 203, 204, 205, 206, 207]
G = [250, 500, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 12000, 14000, 16000,
     17000, 18000]


def profil(v):
    n = 1 << 17
    v = v[:n] if len(v) >= n else np.pad(v, (0, n - len(v)))
    S = np.abs(np.fft.rfft(v, n))
    f = np.fft.rfftfreq(n, 1 / FS)
    b = lambda c: float(np.mean(S[(f >= c / 1.12) & (f <= c * 1.12)] ** 2))
    r = b(1000.0)
    return np.array([db(np.sqrt(b(c) / r)) for c in G])


def main():
    if not BANC.exists():
        raise SystemExit(f"banc absent : {BANC}")
    RENDUS.mkdir(parents=True, exist_ok=True)

    # Modulation a zero et quantification desactivee : on mesure la REPONSE, pas
    # le grain ni le vibrato. Meme convention que l'etalon du prototype, sans quoi
    # les deux chiffres ne seraient pas comparables.
    for d in DEC:
        subprocess.run([str(BANC), str(RENDUS), "4.0", "0.5", "0.0",
                        "0", "0", "0", str(d), "1"],
                       check=True, stdout=subprocess.DEVNULL)

    tout = []
    print(f"  {'programme':11s} {'ecart moyen':>12s} {'pire':>8s}")
    for cle, court in CLES:
        cible = json.loads((ICI / "mesures" / f"{cle}.json").read_text(encoding="utf-8"))
        sp = dict((int(a), b) for a, b in cible["spectre_db"])
        vise = np.array([sp[c] - sp[1000] for c in G])
        ps = []
        for d in DEC:
            x, _ = lire_wav(RENDUS / f"CPP_{court}_d{d}.wav")
            ps.append(profil(x[:, 0] + x[:, 1]))
        e = np.abs(np.mean(ps, axis=0) - vise)
        tout.append(e)
        print(f"  {court:11s} {e.mean():11.2f} dB {e.max():7.2f}")
    t = np.array(tout)
    print(f"\n  PORTAGE contre la machine : moyen {t.mean():.2f} dB, pire {t.max():.2f} dB")


if __name__ == "__main__":
    main()
