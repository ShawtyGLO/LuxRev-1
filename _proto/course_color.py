# -*- coding: utf-8 -*-
"""Combien de rugosite le bouton COLOR deplace-t-il DEJA, a 8 lignes ?

Sans ce point de comparaison, un gain de 0,30 dB apporte par 16 lignes ne veut
rien dire : il faut savoir si la course entiere du bouton en vaut 0,1 ou 1,0.
Meme protocole que banc_lignes.py — huit phases d'impulsion, ecart-type affiche.
"""
import json, sys
from pathlib import Path
import numpy as np

ICI = Path(r"D:\1 Plugin Vibecodding\LuxRev-1 VST\_proto")
sys.path.insert(0, str(ICI))
import moteur
from analyse_ir import rugosite_spectrale

DECALAGES = (200, 201, 202, 203, 204, 205, 206, 207)
NOMS = ("lxp1_room_small2", "lxp1_room_small1", "lxp1_room_medium1",
        "lxp1_room_plate_b", "lxp1_room_large2", "lxp1_hall_b")


def rug(q, dec):
    """⚠️ LA MODULATION RESTE ACTIVE.

    Une premiere version la coupait, « pour ne mesurer que la structure ». C'est
    exactement le mecanisme qu'on cherchait a mesurer : la moitie cremeuse de
    COLOR pousse mod_profondeur a 26 echantillons, et c'est la modulation qui
    etale les modes et lisse le spectre. En la coupant, COLOR = 0 sortait PLUS
    rugueux que le centre sur cinq programmes sur six — un resultat absurde qui
    ne disait rien du moteur et tout du protocole.

    La quantification, elle, reste desactivee : le grain 16 bits ajoute un
    plancher de bruit qui fausse une mesure de rugosite spectrale sans rien
    apprendre sur la structure.
    """
    r = dict(q)
    r["bits"] = 0
    r["correction_fir"] = np.array(r["correction_fir"])
    m = moteur.Moteur(fs=moteur.FS, **r)
    x = np.zeros(int(moteur.FS * 3.0)); x[dec] = 1.0
    L, _ = m.rendre(x)
    return rugosite_spectrale(L, moteur.FS)


p = json.loads((ICI / "accord" / "programmes.json").read_text(encoding="utf-8"))
print("  programme            COLOR 0    COLOR 50   COLOR 100   course")
for nom in NOMS:
    # ⚠️ ON PASSE LE JEU COMPLET, compensations comprises. macro_color les
    # consomme elle-meme et les retire avant de rendre les kwargs. Les enlever
    # d'avance — ce que faisait une premiere version — supprime justement ce qui
    # maintient le T60 et le filtre de sortie constants sur toute la course : la
    # mesure comparait alors des reverbes de longueurs differentes.
    base = p[nom]
    vals = []
    for c in (0.0, 0.5, 1.0):
        q = moteur.macro_color(base, c)
        e = [rug(q, d) for d in DECALAGES]
        vals.append((float(np.mean(e)), float(np.std(e))))
    course = vals[2][0] - vals[0][0]
    print("  %-20s %.2f±%.2f  %.2f±%.2f  %.2f±%.2f   %+.2f"
          % (nom, vals[0][0], vals[0][1], vals[1][0], vals[1][1],
             vals[2][0], vals[2][1], course))
