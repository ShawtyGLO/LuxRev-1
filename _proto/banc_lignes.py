# -*- coding: utf-8 -*-
"""Monter le nombre de lignes du FDN : qu'est-ce que ca rapporte vraiment ?

⛔ VERDICT DU 2026-09-09 : CETTE QUESTION N'EST PAS DECIDABLE PAR CE BANC.
Garde pour la trace, et pour la lecon.

La moitie STRUCTURELLE de la reponse ne demandait pas de mesure. `macro_color`
tient deja n_lignes a 8 sur toute la moitie cremeuse et le fait DESCENDRE a 4
du cote metallique : monter le maximum monterait donc aussi le CENTRE, qui est
le point accorde sur les IR, et deplacerait les douze programmes clones. Seule
une montee reservee a u < 0 serait acceptable.

La moitie RENDEMENT a resiste a trois protocoles, et le troisieme a montre
pourquoi. A chaque fois j'avais retire du montage ce que je pretendais mesurer :

  1. une SEULE impulsion, alors que le bloqueur d'ordre zero rend la reponse
     dependante de sa phase jusqu'a 10 dB (etabli le 2026-09-06) ;
  2. mod_profondeur = 0, alors que la modulation EST le mecanisme du cote
     cremeux — COLOR la pousse a 26 echantillons ;
  3. les courbes de compensation retirees, alors qu'elles seules maintiennent
     le T60 et le filtre de sortie constants sur la course du bouton.

Chaque fois pour la meme raison : vouloir « isoler la structure ». Ca marche
quand les mecanismes sont independants, et le tank n'est pas comme ca.

⛔ ET AU BOUT, LE JUGE LUI-MEME EST RECUSE. Protocole complet — huit phases,
modulation active, compensations appliquees — la dispersion de la rugosite
DOUBLE du cote cremeux :

    plate_b   centre 1,39 +/- 0,14      COLOR 0   1,85 +/- 0,22
    large2    centre 1,11 +/- 0,11      COLOR 0   1,44 +/- 0,23
    hall_b    centre 1,25 +/- 0,16      COLOR 0   1,42 +/- 0,23

C'est la signature d'un systeme MODULE : le spectre change reellement d'un
rendu a l'autre selon la trajectoire du LFO. `rugosite_spectrale` mesure une
propriete STATIQUE d'un systeme qu'on rend deliberement non-statique a cet
endroit, et la « rugosite » supplementaire est en partie la modulation
elle-meme. Le signe s'inverse d'ailleurs sur hall_b, dont l'extreme metallique
sort PLUS LISSE que le centre (1,02 contre 1,25) — sans aucun sens physique.

⚠️ Portee bien au-dela de ce banc : la metrique autour de laquelle TOUTE la
macro COLOR a ete concue — « lignes longues = modes serres = spectre lisse =
cremeux » — n'est valide que sur la moitie NON MODULEE de la course. Elle reste
bonne pour accorder les programmes au centre, ou la modulation est faible.

Ce qui SURVIT a tout ca, mesure dans toutes les configurations :
la densite d'echos ne bouge pas d'un centieme entre 8, 12 et 16 lignes. Les
lignes agissent sur la finesse des modes, pas sur le nombre d'echos.

Si la piste des 12/16 lignes revient un jour, elle se tranchera A L'OREILLE.
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


# ⚠️ HUIT PHASES D'IMPULSION, pas une.
#
# Le bloqueur d'ordre zero rend la reponse impulsionnelle dependante de la phase de
# l'impulsion — jusqu'a 10 dB, etabli le 2026-09-06 et corrige partout ailleurs. Une
# seule impulsion ne peut pas trancher un ecart de rugosite de 0,17 dB : c'est ce qui
# faisait dire a la premiere passe que hall_b s'aggravait avec 16 lignes pendant que
# plate_b et small1 s'ameliorent. On moyenne, comme etalon_prototype.py.
DECALAGES = (200, 201, 202, 203, 204, 205, 206, 207)


def rendre(nl, params, decalage):
    moteur.FDN_LONGUEURS = (BASE + SUPPLEMENT)[:nl]
    moteur.N_FDN = nl
    q = dict(params)
    q["n_lignes"] = nl
    # ⚠️ LA MODULATION RESTE ACTIVE. Une premiere version la coupait « pour ne
    # mesurer que la structure » — mais c'est precisement le mecanisme du cote
    # cremeux : COLOR pousse mod_profondeur a 26 echantillons, et c'est elle qui
    # etale les modes. La couper faisait sortir COLOR = 0 PLUS rugueux que le
    # centre sur cinq programmes sur six, un resultat absurde qui ne disait rien
    # du moteur et tout du protocole. Comparer des lignes supplementaires dans
    # une configuration ou le lissage reel est desactive n'a aucun sens : la
    # modulation peut deja fournir ce que les lignes apporteraient.
    q["bits"] = 0
    q["correction_fir"] = np.array(q["correction_fir"])
    m = moteur.Moteur(fs=moteur.FS, **q)
    x = np.zeros(int(moteur.FS * 3.0)); x[decalage] = 1.0
    return m.rendre(x)


def main():
    p = json.loads((ICI / "accord" / "programmes.json").read_text(encoding="utf-8"))
    for nom in ("lxp1_room_small2", "lxp1_room_small1", "lxp1_room_medium1",
                "lxp1_room_plate_b", "lxp1_room_large2", "lxp1_hall_b"):
        # ⚠️ Jeu COMPLET, compensations comprises : macro_color les consomme
        # elle-meme. Les retirer d'avance supprime ce qui tient le T60 constant
        # sur la course de COLOR.
        base = p[nom]
        q = moteur.macro_color(base, 0.0)          # cote cremeux
        print("\n  %s   (COLOR a 0 %%, le cote cremeux)" % nom)
        print("    lignes   melange   dens 50ms  dens 100ms   rugosite  (ecart-type)")
        for nl in (8, 12, 16):
            tms, d50, d100, rugs = [], [], [], []
            for dec in DECALAGES:
                L, _ = rendre(nl, q, dec)
                tt, d, _ = densite_echos(L, moteur.FS)
                tt = np.asarray(tt, dtype=float)
                d = np.asarray(d, dtype=float)
                lire = lambda ms: float(d[int(np.argmin(np.abs(tt - ms)))])
                tm = temps_melange(tt, d)
                if np.isfinite(tm):
                    tms.append(tm)
                d50.append(lire(50.0)); d100.append(lire(100.0))
                rugs.append(rugosite_spectrale(L, moteur.FS))
            # ⚠️ L'ecart-type est affiche EXPRES : sans lui on ne sait pas si un ecart
            # entre deux nombres de lignes depasse la dispersion de la mesure elle-meme.
            print("    %6d  %6.0f ms   %9.2f  %10.2f  %9.2f  (+/- %.2f)"
                  % (nl, np.mean(tms) if tms else float("nan"),
                     np.mean(d50), np.mean(d100), np.mean(rugs), np.std(rugs)))
    moteur.FDN_LONGUEURS = BASE
    moteur.N_FDN = len(BASE)


main()
