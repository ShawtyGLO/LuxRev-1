#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Compare le moteur C++ du plugin au prototype Python, programme par programme.

⚠️ POURQUOI CE BANC EXISTE. pluginval a repondu SUCCESS a severite 10 sur le
VST3. Il valide la CONSTRUCTION — pas de plantage, pas de NaN, l'etat se
recharge, l'automation ne casse rien — et il n'a pas d'oreilles. Un moteur qui
compile, ne plante pas et sonne faux passe au vert. Fautes de construction et
fautes de son sont deux familles disjointes.

Le seul juge du portage est le prototype, parce que c'est lui qui a ete accorde
sur les mesures des 14 IR du LXP-1 et valide a l'oreille en A/B.

Marche a suivre :
    LuxRev-1/tools/banc_moteur.exe  _proto/rendus_moteur/PORTAGE  4.0  0.5
    python _proto/comparer_portage.py
"""

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

from analyse_ir import db, lire_wav, schroeder_multi, temps_decroissance  # noqa: E402
from moteur import FS, Moteur, macro_color                                # noqa: E402

ICI = Path(__file__).parent
PORTAGE = ICI / "rendus_moteur" / "PORTAGE"

# Multiplicateur de modulation, a passer aussi au banc C++.
MOD = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0

NOMS = {
    "lxp1_room_small1": "Small_1", "lxp1_room_small2": "Small_2",
    "lxp1_room_medium1": "Medium_1", "lxp1_room_medium2": "Medium_2",
    "lxp1_room_large1": "Large_1", "lxp1_room_large2": "Large_2",
    "lxp1_hall_b": "Hall_B", "lxp1_hall_d": "Hall_D",
    "lxp1_room_plate_b": "Plate_B", "lxp1_room_plate_d": "Plate_D",
    "lxp1_room_gate": "Gate", "lxp1_room_inverse": "Inverse",
}

BANDES = [250, 500, 1000, 2000, 4000, 8000, 16000]


def t60(x, fs):
    """T60 par integration de Schroeder sur l'energie des deux canaux."""
    c = schroeder_multi([x[:, 0], x[:, 1]])
    return temps_decroissance(c, fs)[0]


def spectre(x, fs, bandes=BANDES):
    """Reponse en tiers d'octave, normalisee sur 1 kHz.

    On compare des FORMES, pas des niveaux : le portage n'a aucune raison de
    retomber sur le meme gain absolu (le FIR de voicing est applique avec un
    retard different), et un ecart de gain global ne dit rien sur le timbre.
    """
    mono = x[:, 0] + x[:, 1]
    n = 1 << int(np.ceil(np.log2(len(mono))))
    sp = np.abs(np.fft.rfft(mono, n))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    out = []
    for fc in bandes:
        m = (f >= fc / 1.26) & (f <= fc * 1.26)
        out.append(db(np.sqrt(np.mean(sp[m] ** 2))) if m.any() else -120.0)
    out = np.array(out)
    return out - out[bandes.index(1000)]


def main():
    import json

    params = json.loads((ICI / "accord" / "programmes.json").read_text(encoding="utf-8"))

    print("Portage JUCE contre prototype Python — COLOR a 50 %\n")
    print(f"  {'programme':11s} {'T60 py':>8s} {'T60 c++':>8s} {'ecart':>8s} "
          f"{'ecart spectre max':>18s} {'bande':>7s}")
    print("  " + "-" * 68)

    ecarts_t60, ecarts_sp = [], []
    for cle, court in NOMS.items():
        f = PORTAGE / f"CPP_{court}.wav"
        if not f.exists():
            print(f"  {court:11s} rendu C++ absent — lancer banc_moteur.exe")
            continue

        cpp, fs = lire_wav(f)

        base = dict(params[cle])
        if base.get("correction_fir") is not None:
            base["correction_fir"] = np.array(base["correction_fir"])
        q = macro_color(base, 0.5)
        q['mod_profondeur'] *= float(MOD)
        L, R = Moteur(fs=FS, **q).reponse_impulsionnelle(len(cpp) / FS)
        py = np.stack([L, R], axis=1)

        n = min(len(py), len(cpp))
        t_py, t_cpp = t60(py[:n], fs), t60(cpp[:n], fs)
        d_t60 = (t_cpp / t_py - 1.0) * 100.0 if t_py > 0 else float("nan")

        s_py, s_cpp = spectre(py[:n], fs), spectre(cpp[:n], fs)
        d_sp = s_cpp - s_py
        k = int(np.argmax(np.abs(d_sp)))

        ecarts_t60.append(abs(d_t60))
        ecarts_sp.append(np.max(np.abs(d_sp)))
        print(f"  {court:11s} {t_py:7.2f}s {t_cpp:7.2f}s {d_t60:+7.1f}% "
              f"{d_sp[k]:+17.1f} dB {BANDES[k]:6d}")

    if ecarts_t60:
        print("\n  " + "-" * 68)
        print(f"  T60      : ecart moyen {np.mean(ecarts_t60):.1f} %, "
              f"pire {np.max(ecarts_t60):.1f} %")
        print(f"  Spectre  : ecart moyen {np.mean(ecarts_sp):.1f} dB, "
              f"pire {np.max(ecarts_sp):.1f} dB")


if __name__ == "__main__":
    main()
