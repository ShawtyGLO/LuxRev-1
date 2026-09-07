#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reajuste le FIR de voicing contre le MOTEUR DU PLUGIN, et non contre le prototype.

⚠️ POURQUOI. Le FIR de voicing n'est pas un filtre choisi : c'est l'inverse du
residu spectral MESURE entre le moteur et les IR du LXP-1. Il a ete ajuste dans
`accorder.py` contre le moteur Python. Le portage C++ ne fait pas exactement la
meme chose en haut du spectre — sa decimation n'est pas celle de resample_poly —
donc le meme FIR n'y annule plus le meme residu, et il en reste jusqu'a 2 dB.

Decision de l'utilisateur (2026-09-08) : la reference est **la machine**, pas le
prototype. On refait donc l'ajustement contre le moteur C++, en visant les memes
cibles mesurees. Le prototype garde le sien : les deux moteurs convergent vers la
machine sans avoir a etre identiques entre eux.

⚠️ MOYENNAGE SUR HUIT PHASES D'IMPULSION, et ce n'est pas un raffinement. Le
bloqueur d'ordre zero maintient chaque echantillon jusqu'au suivant : la reponse
a une impulsion depend de la phase de celle-ci par rapport a l'horloge de
31,25 kHz. Sur une seule phase, la mesure varie de plusieurs dB et l'ajustement
suit le bruit.

Marche a suivre :
    python _proto/voicing_portage.py
    python "LuxRev-1/tools/generer_programmes.py"     # reprend le nouveau FIR
"""

import json
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy import signal as sig

sys.path.insert(0, str(Path(__file__).parent))

from analyse_ir import db, lire_wav        # noqa: E402
from moteur import FS                      # noqa: E402

ICI = Path(__file__).parent
RACINE = ICI.parent
BANC = RACINE / "LuxRev-1" / "tools" / "banc_moteur.exe"
RENDUS = ICI / "rendus_moteur" / "PORTAGE"
ACCORD = ICI / "accord" / "programmes.json"
SORTIE = ICI / "accord" / "voicing_portage.json"

PHASES = [200, 201, 202, 203, 204, 205, 206, 207]
DUREE = 4.0
N_TAPS = 63

CLES = [
    ("lxp1_room_small1", "Small_1"), ("lxp1_room_small2", "Small_2"),
    ("lxp1_room_medium1", "Medium_1"), ("lxp1_room_medium2", "Medium_2"),
    ("lxp1_room_large1", "Large_1"), ("lxp1_room_large2", "Large_2"),
    ("lxp1_hall_b", "Hall_B"), ("lxp1_hall_d", "Hall_D"),
    ("lxp1_room_plate_b", "Plate_B"), ("lxp1_room_plate_d", "Plate_D"),
    ("lxp1_room_gate", "Gate"), ("lxp1_room_inverse", "Inverse"),
]

# Grille de la cible mesuree (analyse_ir : "spectre_db").
GRILLE = [250, 500, 1000, 2000, 3000, 4000, 6000, 8000,
          10000, 12000, 14000, 16000, 17000, 18000]


def rendre_phases():
    """Fait tourner le banc C++ sur les huit phases, sans modulation ni dither."""
    if not BANC.exists():
        raise SystemExit(f"banc absent : {BANC}\n"
                         "  compiler d'abord LuxRev-1/tools/banc_moteur.cpp")
    for d in PHASES:
        subprocess.run([str(BANC), str(RENDUS), str(DUREE), "0.5", "0.0",
                        "0", "0", "0", str(d)],
                       check=True, stdout=subprocess.DEVNULL)


def profil(v, grille=GRILLE, ref=1000.0):
    """Niveaux de bande d'un rendu, rapportes a 1 kHz."""
    n = 1 << 17
    v = v[:n] if len(v) >= n else np.pad(v, (0, n - len(v)))
    s = np.abs(np.fft.rfft(v, n))
    f = np.fft.rfftfreq(n, 1.0 / FS)

    def bande(c):
        m = (f >= c / 1.12) & (f <= c * 1.12)
        return float(np.mean(s[m] ** 2)) if m.any() else 1e-30

    r = bande(ref)
    return np.array([db(np.sqrt(bande(c) / r)) for c in grille])


def fir_correcteur(residu_db, n_taps=N_TAPS):
    """FIR dont la reponse annule `residu_db` sur la grille.

    Meme methode que le prototype : on inverse la courbe mesuree plutot que de
    deviner quelle cascade de filtres la produirait. Trois essais successifs
    (mur plus haut, bloqueur, correction sinc) avaient chacun deplace le residu
    sans le supprimer.
    """
    f = np.array(GRILLE, dtype=float) / (FS / 2.0)
    g = 10.0 ** (-residu_db / 20.0)

    # bornes obligatoires pour firwin2 : 0 et 1, en prolongeant les extremites
    f = np.concatenate([[0.0], f, [1.0]])
    g = np.concatenate([[g[0]], g, [g[-1]]])
    # pas de gain delirant pres de Nyquist, ou il n'y a de toute facon plus rien
    g = np.clip(g, 0.2, 5.0)
    return sig.firwin2(n_taps, f, g)


def main():
    print("Reajustement du voicing contre le moteur du plugin\n")
    print("  rendu des huit phases ...")
    rendre_phases()

    accord = json.loads(ACCORD.read_text(encoding="utf-8"))

    # ⚠️ On repart du FIR REELLEMENT EN PLACE dans le build qu'on vient de
    # mesurer, pas de celui du prototype. Sinon l'ajustement mesure le residu
    # d'un moteur et corrige les coefficients d'un autre : ce n'est pas une
    # iteration, ca ne converge pas. Relancer plusieurs fois affine desormais.
    base = json.loads(SORTIE.read_text(encoding="utf-8")) if SORTIE.exists() else {}
    if base:
        print("  depart : le voicing du build courant (iteration)")
    else:
        print("  depart : le voicing du prototype (premiere passe)")

    out, avant, apres = {}, [], []

    print(f"\n  {'programme':11s} {'residu avant':>13s} {'residu apres':>13s}")
    print("  " + "-" * 40)
    for cle, court in CLES:
        cible = json.loads((ICI / "mesures" / f"{cle}.json").read_text(encoding="utf-8"))
        sp = dict((int(a), b) for a, b in cible["spectre_db"])
        vise = np.array([sp[c] - sp[1000] for c in GRILLE])

        mesures = []
        for d in PHASES:
            x, _ = lire_wav(RENDUS / f"CPP_{court}_d{d}.wav")
            mesures.append(profil(x.sum(axis=1)))
        obtenu = np.mean(mesures, axis=0)

        residu = obtenu - vise

        # ⚠️ On CONCOIT le FIR total d'un coup, on ne convolue pas l'ancien avec
        # une correction. La premiere version faisait convolve(ancien, correction)
        # puis retaillait a 63 points avec une fenetre : la troncature detruisait
        # justement la reponse qu'on venait de calculer. Elle promettait 0,40 dB
        # et en rendait 1,23.
        ancien = np.array(base.get(cle, accord[cle]["correction_fir"]), dtype=float)
        _, h_anc = sig.freqz(ancien, worN=np.array(GRILLE) * np.pi / (FS / 2))
        vise_db = db(np.abs(h_anc)) - residu          # reponse totale voulue
        total = fir_correcteur(-vise_db)              # fir_correcteur inverse son entree

        out[cle] = [float(v) for v in total]
        avant.append(np.abs(residu).mean())

        # ce que le FIR obtenu annonce : sa reponse contre celle qu'on visait
        _, h_tot = sig.freqz(total, worN=np.array(GRILLE) * np.pi / (FS / 2))
        apres.append(np.abs(db(np.abs(h_tot)) - vise_db).mean())
        print(f"  {court:11s} {avant[-1]:12.2f} dB {apres[-1]:12.2f} dB")

    SORTIE.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print("\n  " + "-" * 40)
    print(f"  residu moyen : {np.mean(avant):.2f} dB  ->  {np.mean(apres):.2f} dB")
    print(f"  ecrit : {SORTIE.relative_to(RACINE)}")
    # ⚠️ Ce chiffre est ce que la conception PROMET, pas ce qu'elle obtient :
    #    il faut le verifier sur un nouveau rendu apres regeneration.
    print("\n  A VERIFIER sur un nouveau rendu apres regeneration de Programmes.h.")


if __name__ == "__main__":
    main()
