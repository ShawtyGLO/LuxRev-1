#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Calcule le profil d'enveloppe de Gate et Inverse pour le plugin.

⚠️ CE SCRIPT CORRIGE UN DEFAUT DU PROTOTYPE.

`moteur.enveloppe_gate` multiplie le rendu par l'enveloppe ABSOLUE relevee sur
l'IR. Or le tank decroit deja tout seul : le produit applique donc la forme deux
fois. Verifie sur `gate`, a 600 ms :

    machine        -52,4 dB
    tank seul      -32,6 dB
    tank x profil  -83,7 dB      <- ce que fait le prototype

Le portage reproduisait fidelement ce comportement, donc fidelement l'erreur.

La reference etant **la machine** (decision du 2026-09-08), le profil doit etre
la CORRECTION entre ce que le tank fait et ce que la machine fait, pas la cible
brute :

    profil = enveloppe_machine / enveloppe_du_tank

C'est la meme philosophie que le FIR de voicing — on inverse un residu mesure
plutot que de deviner la forme qui le produirait.

Marche a suivre :
    python _proto/enveloppe_portage.py
    python "LuxRev-1/tools/generer_programmes.py"
"""

import json
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

from analyse_ir import db, lire_wav        # noqa: E402
from moteur import FS                      # noqa: E402

ICI = Path(__file__).parent
RACINE = ICI.parent
BANC = RACINE / "LuxRev-1" / "tools" / "banc_moteur.exe"
RENDUS = ICI / "rendus_moteur" / "PORTAGE"
SORTIE = ICI / "accord" / "enveloppe_portage.json"

PROGRAMMES = [("lxp1_room_gate", "Gate"), ("lxp1_room_inverse", "Inverse")]
PAS_MS = 5.0
DUREE = 4.0
DECALAGE = 200                 # l'impulsion, en echantillons
PLANCHER_DB = -75.0            # au-dela, la queue est sous le bruit : on ferme
# ⚠️ Plafond de gain BAS, et c'est deliberé. A 4,0 le profil partait en butee
# sur les premiers points — la ou le tank n'a pas encore etabli sa queue et ou
# le rapport machine/tank explose — ce qui amplifiait l'attaque quatre fois et
# deformait la forme au lieu de la corriger. Un profil d'enveloppe corrige une
# pente, il ne doit pas fabriquer de niveau.
GAIN_MAX = 2.0


def enveloppe(y, pas_ms=PAS_MS):
    """RMS glissant, meme fenetre que analyse_ir."""
    pas = int(FS * pas_ms * 1e-3)
    n = len(y) // pas
    return np.array([db(np.sqrt(np.mean(y[i * pas:(i + 1) * pas] ** 2)) + 1e-12)
                     for i in range(n)])


def main():
    if not BANC.exists():
        raise SystemExit(f"banc absent : {BANC}")

    # ⚠️ ITERATION. A la premiere passe on mesure le tank SEUL et le profil est
    # machine/tank. Ensuite on mesure le moteur AVEC son profil courant et on
    # multiplie : profil <- profil x (machine/obtenu). Sans cette seconde passe,
    # `inverse` restait a 5,6 dB — sa cible a son maximum a 100 ms, pas a zero,
    # donc normaliser les deux courbes sur leur propre maximum laisse un decalage
    # que seule une iteration resorbe.
    # ⚠️ Le profil n'est plus une CORRECTION de decroissance : depuis que Gate et
    # Inverse sont un banc de prises lineaire et invariant, il n'y a plus de queue
    # exponentielle a rattraper. Le profil EST les gains des prises, donc il part
    # de l'enveloppe relevee et on l'affine par iteration.
    base = {}
    if SORTIE.exists():
        base = json.loads(SORTIE.read_text(encoding="utf-8"))
    else:
        for cle, _ in PROGRAMMES:
            c = json.loads((ICI / "mesures" / f"{cle}.json").read_text(encoding="utf-8"))
            e = np.array([v for _, v in c["enveloppe_db"]])
            base[cle] = [float(x) for x in 10.0 ** ((e - e.max()) / 20.0)]
    subprocess.run([str(BANC), str(RENDUS), str(DUREE), "0.5", "1.0",
                    "0", "16", "0", str(DECALAGE), "1"],
                   check=True, stdout=subprocess.DEVNULL)
    suffixe = ""

    out = {}
    print("Profil d'enveloppe de Gate et Inverse — correction, pas cible brute\n")
    for cle, court in PROGRAMMES:
        cible = json.loads((ICI / "mesures" / f"{cle}.json").read_text(encoding="utf-8"))
        vise = np.array([v for _, v in cible["enveloppe_db"]])
        vise = vise - vise.max()

        x, _ = lire_wav(RENDUS / f"CPP_{court}{suffixe}_d{DECALAGE}.wav")
        nu = enveloppe(np.abs(x[:, 0]) + np.abs(x[:, 1]))

        # ⚠️ ALIGNEMENT PAR CORRELATION, pas par une position supposee.
        #
        # Le rendu est en retard sur la cible de la position de l'impulsion PLUS
        # la latence declaree du plugin — les deux FIR a phase lineaire et
        # l'avance du reechantillonnage, soit pres de 8 ms. Sur `inverse`, dont
        # le profil monte a pic dans les 100 premieres millisecondes, ce decalage
        # produisait 46 dB d'ecart apparent a t = 0 : l'ajustement essayait alors
        # de le rattraper en montant le gain en butee, ce qui deformait tout le
        # profil. Chercher le decalage plutot que le supposer supprime le
        # probleme a la racine.
        best, decal = None, 0
        for d in range(0, 12):
            a = nu[d:d + len(vise)]
            if len(a) < len(vise):
                break
            u0 = vise > -45.0
            err = np.mean(np.abs((a - np.mean(a[u0]) + np.mean(vise[u0]))[u0] - vise[u0]))
            if best is None or err < best:
                best, decal = err, d
        nu = nu[decal:decal + len(vise)]
        n = min(len(vise), len(nu))
        vise, nu = vise[:n], nu[:n]

        # ⚠️ On aligne les deux courbes par un DECALAGE AJUSTE, pas sur leur
        # maximum respectif. Normaliser chaque passe sur son propre maximum rend
        # l'iteration instable : le maximum se deplace a mesure que le profil
        # change, et la boucle poursuit sa propre queue — mesure faite, `gate`
        # passait de 1,4 a 4,5 dB en iterant. Ici on ne compare que des FORMES,
        # le niveau absolu etant de toute facon repris par Mix.
        utile = vise > -45.0
        nu = nu - float(np.mean(nu[utile]) - np.mean(vise[utile]))

        # la correction, en dB, puis en amplitude
        corr = np.clip(vise - nu, -120.0, db(GAIN_MAX))
        prof = 10.0 ** (corr / 20.0)
        prof[vise < PLANCHER_DB] = 0.0

        # lissage leger : le releve de la machine est bruite point a point, et un
        # profil qui tremble s'entendrait comme une modulation d'amplitude
        noyau = np.hanning(9); noyau /= noyau.sum()
        prof = np.convolve(prof, noyau, mode="same")
        # ⚠️ On ne force PLUS le premier point a 1,0. Sur , dont le
        # profil demarre bas, cela creait un echelon de 1,0 a 0,1 en un
        # echantillon — un claquement a chaque attaque.

        if base:                                 # on affine le profil en place
            anc = np.array(base[cle][:len(prof)], dtype=float)
            prof = np.clip(anc * prof, 0.0, GAIN_MAX)

        out[cle] = [round(float(v), 6) for v in prof]
        print(f"  {court:9s} {n} points, gain median {np.median(prof):.3f}, "
              f"max {prof.max():.2f}")

    SORTIE.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print(f"\n  ecrit : {SORTIE.relative_to(RACINE)}")
    print("  A VERIFIER sur un nouveau rendu apres regeneration de Programmes.h.")


if __name__ == "__main__":
    main()
