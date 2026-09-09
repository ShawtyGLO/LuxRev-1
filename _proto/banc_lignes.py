# -*- coding: utf-8 -*-
"""Monter le nombre de lignes du FDN : qu'est-ce que ca rapporte vraiment ?

La question posee : ouvrir la moitie cremeuse de COLOR avec 12 ou 16 lignes
change-t-il le son GLOBAL ou seulement le cote voulu ?

⚠️ Deux choses a ne pas confondre.

  - La STRUCTURE. `macro_color` tient deja n_lignes a 8 sur toute la moitie
    cremeuse et le fait DESCENDRE a 4 du cote metallique. Monter le maximum
    monterait donc aussi le CENTRE, qui est le point accorde sur les IR, et
    deplacerait les douze programmes clones. La forme utilisable fait monter
    les lignes uniquement pour u < 0. Etabli par lecture du code.

  - Le RENDEMENT. Est-ce que 12 ou 16 lignes densifient assez pour valoir la
    refonte — nouvelles longueurs mutuellement premieres, matrice d'ordre
    different, courbes de compensation a remesurer ? C'est ce que ce banc
    mesure, et lui seul peut trancher.

⚠️ `densite_echos` renvoie un temps en MILLISECONDES et une densite NORMALISEE
(1,0 = queue statistiquement lisse), pas des echos par seconde. Une premiere
version lisait le temps en secondes et sortait zero partout.

Resultat de la premiere passe (2026-09-09), a COLOR = 0 :

    plate_b    8 -> 16 lignes   rugosite 1,32 -> 1,12
    small1     8 -> 16 lignes   rugosite 2,51 -> 1,91
    hall_b     8 -> 16 lignes   rugosite 0,76 -> 0,93   <- dans l'AUTRE sens

et la densite d'echos ne bouge pas d'un centieme dans les trois cas. ⛔ A ne pas
conclure en l'etat : hall_b contredit les deux autres, ce qui sent le protocole
plutot que le phenomene. A reprendre avant toute decision.
"""
import json, sys
from pathlib import Path
import numpy as np

ICI = Path(r"D:\1 Plugin Vibecodding\LuxRev-1 VST\_proto")
sys.path.insert(0, str(ICI))
import moteur
from analyse_ir import densite_echos, rugosite_spectrale, temps_melange

SUPPLEMENT = (1187, 1409, 1607, 1873, 2113, 2417, 2789, 3121)
BASE = moteur.FDN_LONGUEURS


def rendre(nl, params):
    moteur.FDN_LONGUEURS = (BASE + SUPPLEMENT)[:nl]
    moteur.N_FDN = nl
    q = dict(params)
    q["n_lignes"] = nl
    q["mod_profondeur"] = 0.0
    q["bits"] = 0
    q["correction_fir"] = np.array(q["correction_fir"])
    m = moteur.Moteur(fs=moteur.FS, **q)
    x = np.zeros(int(moteur.FS * 3.0)); x[200] = 1.0
    return m.rendre(x)


def main():
    p = json.loads((ICI / "accord" / "programmes.json").read_text(encoding="utf-8"))
    for nom in ("lxp1_room_plate_b", "lxp1_hall_b", "lxp1_room_small1"):
        base = {k: v for k, v in p[nom].items()
                if k not in ("compensation_color", "compensation_lp")}
        q = moteur.macro_color(base, 0.0)          # cote cremeux
        print("\n  %s   (COLOR a 0 %%, le cote cremeux)" % nom)
        print("    lignes   melange   dens 50ms  dens 100ms   rugosite")
        for nl in (8, 12, 16):
            L, _ = rendre(nl, q)
            tt, d, _ = densite_echos(L, moteur.FS)
            tt = np.asarray(tt, dtype=float)
            d = np.asarray(d, dtype=float)
            lire = lambda ms: float(d[int(np.argmin(np.abs(tt - ms)))])
            tm = temps_melange(tt, d)
            rug = rugosite_spectrale(L, moteur.FS)
            print("    %6d  %6.0f ms   %9.2f  %10.2f  %9.2f"
                  % (nl, tm, lire(50.0), lire(100.0), rug))
    moteur.FDN_LONGUEURS = BASE
    moteur.N_FDN = len(BASE)


main()
