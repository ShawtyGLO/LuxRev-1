#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Releve la structure des deux programmes Delay depuis leurs IR.

⚠️ SEUL ENDROIT, AVEC accorder.py, OU UNE CIBLE DU LXP-1 ENTRE DANS LA CONCEPTION.
Le moteur n'expose que des parametres ; les valeurs viennent d'ici, par
`accord/delays.json`, puis de generer_programmes.py.

Ce que les IR disent, releve a l'echantillon sur les quatre canaux (cf.
CONCEPTION.md 3quater) :

  delay2   ping-pong pur, prises alternees gauche/droite toutes les ~99,6 ms,
           36 a 44 dB de separation, aucune diffusion (densite finale 0,18)
  delay1   centre, repetitions toutes les ~285,7 ms, chacune etant un amas de
           cinq prises espacees de ~13,5 ms au profil symetrique

⚠️ Deux fausses pistes avant d'y arriver, notees pour qu'on n'y retombe pas : le
pre-delai MESURE n'est pas la periode de repetition, et une autocorrelation sur
une enveloppe a pas de 2 ms annonce une grille de 22 ms qui n'est que 11 fois le
pas d'analyse.

Usage : python _proto/structure_delays.py
"""

import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from analyse_ir import db, lire_wav          # noqa: E402

ICI = Path(__file__).parent
IR = ICI.parent / "Impulse Responce LXP-1" / "Reverb IR"
SORTIE = ICI / "accord" / "delays.json"

PROGRAMMES = ("lxp1_room_delay1", "lxp1_room_delay2")


def periode_par_autocorrelation(env, fs, lo_ms=40.0, hi_ms=400.0):
    """Periode de repetition, par autocorrelation de l'enveloppe.

    ⚠️ ET NON PAR DETECTION DE CRETES. Prendre les N plus fortes cretes en
    ABSOLU marche tant qu'il y en a de vraies, puis se rabat sur l'ondulation de
    fond — sur delay2, apres les six vraies repetitions, le detecteur remplissait
    avec des pics a -44 dB et la mediane des ecarts devenait du bruit (31 ms
    annonces pour 99,6 reels).

    ⚠️ Et le pas d'analyse doit etre FIN. A 2 ms, l'autocorrelation de ces deux
    IR annonce 22 ms sur les deux — soit exactement 11 fois le pas. Ici 0,25 ms.
    """
    e = env - float(np.mean(env))
    ac = np.correlate(e, e, "full")[len(e) - 1:]
    ac /= (ac[0] + 1e-30)
    pas_s = 1.0 / fs
    lo = int(lo_ms * 1e-3 / pas_s)
    hi = min(int(hi_ms * 1e-3 / pas_s), len(ac) - 1)
    k = lo + int(np.argmax(ac[lo:hi]))
    return k * pas_s * 1000.0, float(ac[k])


def niveaux_aux_multiples(X, fs, t0_ms, periode_ms, n=10):
    """Niveau gauche et droite a chaque repetition, en dB sous le maximum.

    On cherche la crete dans une fenetre de +/- 15 % de la periode autour de
    chaque multiple : la periode mesuree n'est pas exacte au dixieme, et une
    fenetre trop serree raterait les repetitions tardives par accumulation.
    """
    ref = float(np.abs(X).max())
    nc = X.shape[1]
    out = []
    for k in range(n):
        c = (t0_ms + k * periode_ms) * 1e-3 * fs
        demi = int(periode_ms * 0.15 * 1e-3 * fs)
        a, b = max(0, int(c) - demi), min(len(X), int(c) + demi)
        if b - a < 8:
            break
        g = float(np.abs(X[a:b, 0]).max())
        d = float(np.abs(X[a:b, min(1, nc - 1)]).max())
        out.append((db(max(g, 1e-20) / ref), db(max(d, 1e-20) / ref)))
    return out


def analyser(cle):
    x, fs = lire_wav(IR / f"{cle}.wav")
    X = np.asarray(x)
    nc = X.shape[1]

    # enveloppe a 0,25 ms, sur la somme des deux voies
    pas = max(int(fs * 0.00025), 1)
    somme = np.abs(X[:, 0]) + np.abs(X[:, min(1, nc - 1)])
    env = np.array([somme[i:i + pas].max()
                    for i in range(0, int(fs * 3.0) - pas, pas)])
    fs_env = fs / pas
    periode, conf = periode_par_autocorrelation(env, fs_env)

    # premiere repetition : la crete globale, ramenee au premier multiple
    i0 = int(np.argmax(somme[:int(fs * 2.0)]))
    t0 = i0 / fs * 1000.0
    while t0 - periode > periode * 0.5:
        t0 -= periode

    niv = niveaux_aux_multiples(X, fs, t0, periode)
    cotes = [0 if g >= d else 1 for g, d in niv]
    seps = [abs(g - d) for g, d in niv]
    bascules = sum(1 for a, b in zip(cotes, cotes[1:]) if a != b)
    ping_pong = bool(len(cotes) > 3
                     and bascules >= 0.7 * (len(cotes) - 1)
                     and float(np.median(seps)) > 12.0)

    # decroissance : pente sur le plus fort des deux canaux, tant qu'on est
    # au-dessus de -50 dB
    fort = np.array([max(g, d) for g, d in niv])
    u = fort > -50.0
    n = np.arange(len(fort))
    pente, r2 = -6.0, 0.0
    if u.sum() > 3:
        c = np.polyfit(n[u], fort[u], 1)
        pente = float(c[0])
        res = fort[u] - np.polyval(c, n[u])
        var = fort[u] - float(np.mean(fort[u]))
        r2 = float(1.0 - np.sum(res ** 2) / max(np.sum(var ** 2), 1e-12))

    # ⚠️ RECOUPEMENT PAR LE T60. La contre-reaction deduite de la pente n'a de
    # sens que si la decroissance EST exponentielle. Le T60 mesure donne une
    # seconde estimation, independante : g = 10^(-3 . periode / T60). Quand les
    # deux se rejoignent, on tient la valeur ; quand elles divergent, c'est que la
    # decroissance n'est pas exponentielle et qu'aucune contre-reaction unique ne
    # la decrit.
    cible = json.loads((ICI / "mesures" / f"{cle}.json").read_text(encoding="utf-8"))
    t60 = float(cible.get("t60_global_s") or 0.0)
    fb_t60 = (10.0 ** (-3.0 * periode * 1e-3 / t60)) if t60 > 0 else 0.0

    return {
        "fs_mesure": float(fs),
        "periode_ms": round(periode, 2),
        "confiance_autocorrelation": round(conf, 3),
        "ping_pong": ping_pong,
        "separation_db": round(float(np.median(seps)), 1),
        "premiere_repetition_ms": round(t0, 2),
        "pente_db_par_repetition": round(pente, 2),
        "r2_exponentielle": round(r2, 3),
        "contre_reaction_pente": round(float(10.0 ** (pente / 20.0)), 4),
        "contre_reaction_t60": round(float(fb_t60), 4),
        "t60_mesure_s": round(t60, 3),
        "niveaux_gauche_db": [round(g, 1) for g, _ in niv],
        "niveaux_droite_db": [round(d, 1) for _, d in niv],
    }


def main():
    out = {}
    print("Structure des programmes Delay, relevee sur les IR\n")
    for cle in PROGRAMMES:
        r = analyser(cle)
        out[cle] = r
        print(f"  {cle}")
        print(f"    periode           {r['periode_ms']:.2f} ms")
        print(f"    ping-pong         {'OUI' if r['ping_pong'] else 'non'}"
              f"   (separation mediane {r['separation_db']:.1f} dB)")
        print(f"    premiere rep.     {r['premiere_repetition_ms']:.2f} ms"
              f"   (confiance {r['confiance_autocorrelation']:.2f})")
        print(f"    decroissance      {r['pente_db_par_repetition']:+.2f} dB "
              f"par repetition   (R2 {r['r2_exponentielle']:.2f})")
        ec = abs(r['contre_reaction_pente'] - r['contre_reaction_t60'])
        print(f"    contre-reaction   {r['contre_reaction_pente']:.3f} par la pente"
              f"   {r['contre_reaction_t60']:.3f} par le T60 de "
              f"{r['t60_mesure_s']:.2f} s"
              + ("   ACCORD" if ec < 0.06 else "   DIVERGENT !"))
        print("    gauche (dB)       "
              + " ".join(f"{v:5.1f}" for v in r["niveaux_gauche_db"]))
        print("    droite (dB)       "
              + " ".join(f"{v:5.1f}" for v in r["niveaux_droite_db"]))
        print()
    SORTIE.write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(f"  ecrit : {SORTIE.relative_to(ICI.parent)}")


if __name__ == "__main__":
    main()
