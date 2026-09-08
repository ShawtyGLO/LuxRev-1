#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LuxRev-1 — accordage du moteur sur les cibles mesurees.

Seul endroit du projet ou une cible du LXP-1 entre dans la conception. Pour chacun des 12
programmes de la v1 (les deux Delay sont reportes), derive les parametres du moteur depuis
`mesures/*.json`, rend une reponse impulsionnelle, la remesure avec le MEME banc que les
IR d'origine, et corrige.

La remesure passe par les fonctions de `analyse_ir.py` et non par une metrique maison :
cible et resultat doivent etre produits par le meme code, sinon l'ecart mesure contient
autant de difference de methode que de difference de son.

Parametres derives analytiquement (pas de recherche) :
    predelai_ms      <- pre-delai mesure
    (lp_sortie_hz est desormais ajuste, cf. phase 3)
    mur_hz           <- 16 900 Hz, constant sur les 14 programmes

Parametres corriges par iteration (la relation n'a pas de forme fermee) :
    taille           <- RUGOSITE spectrale (= le cote metallique)
    t60_s            <- T60 global
    diffusion        <- temps de melange (balayage : la relation n'est pas monotone)
    lp_sortie_hz     <- ecart spectral moyen sur 6-16 kHz
    amortissement_hz <- rapport T60(8 kHz) / T60(500 Hz)

Usage : python accorder.py [--iterations N] [--programme NOM]
"""

import argparse
import json
from pathlib import Path

import numpy as np
from scipy.signal import firwin2 as signal_firwin2

from analyse_ir import (bande, onset, schroeder_multi, spectre_multi, coin_hf,
                        densite_echos, rugosite_spectrale, temps_decroissance,
                        temps_melange)
from moteur import FS, MUR_HZ_LXP, Moteur, enveloppe_gate, macro_color

MESURES = Path(__file__).parent / "mesures"
# Valeurs NOMINALES des programmes, tirees de la Program table du manuel utilisateur
# (Lexicon/Harman). Elles remplacent le T60 et le pre-delai MESURES, parce que les IR de
# tone3000 n'ont pas ete capturees aux reglages nominaux : DECAY et DELAY sont des boutons
# physiques sans position memorisee, et la mesure montre un pre-delai a ~0 sur 8 programmes
# sur 10 la ou le nominal est 16 ou 33 ms. Les captures decrivent la position des boutons
# ce jour-la, pas les programmes de la machine.
SPECS = json.loads((Path(__file__).parent / "specs_lxp1.json").read_text(encoding="utf-8"))     if (Path(__file__).parent / "specs_lxp1.json").exists() else {}
SORTIE = Path(__file__).parent / "accord"

# Reportes en v1 : ils ne se melangent jamais (densite finale 0,80 et 0,20), c'est un
# moteur d'une autre nature.
EXCLUS = ("lxp1_room_delay1", "lxp1_room_delay2")
# Enveloppe imposee, pas une decroissance exponentielle (R2 0,59 et 0,76).
ENVELOPPES = ("lxp1_room_gate", "lxp1_room_inverse")

# 15 kHz : valeur de conception des modules U32 / U25 / U29 du LXP-1 (manuel de service).
# Le 16,9 kHz mesure sur les IR est le -20 dB de ce filtre, pas sa coupure.
MUR_HZ = MUR_HZ_LXP
TOURS_MIN = 14                  # tours de boucle minimum dans un T60 pour une queue lisse
# Longueur de boucle a taille = 1,0, mesuree sur le moteur lui-meme plutot que recopiee :
# elle depend de la frequence interne, qui vient de changer (48 kHz -> 31,25 kHz).
# `boucle` vaut (lenA + lenB)/2 ; un BRAS en fait la moitie. L'ancienne constante codee en
# dur (8705) etait bien un bras — la reprendre telle quelle divisait `taille` par deux.
_SONDE = Moteur(taille=1.0)
BRANCHE_REF = _SONDE.boucle / 2.0
FS_TANK = _SONDE.fs


def taille_pour(t60_s):
    """Echelle du tank imposee par le T60 vise.

    Une boucle longue ne peut pas decroitre vite : il faut assez de tours dans le T60 pour
    que la queue soit une decroissance et non une suite d'echos. A taille 1,0 un bras fait
    181 ms ; demander un T60 de 0,5 s revient a 2,7 tours, et la mesure sortait alors 76 %
    trop longue. Cette contrainte est aussi physiquement juste : petite salle = lignes
    courtes.
    """
    return float(np.clip(t60_s * FS_TANK / (TOURS_MIN * BRANCHE_REF), 0.12, 1.8))


# Bande sur laquelle on juge l'ecart spectral. En dessous de 6 kHz le moteur est deja
# a moins de 1 dB ; c'est au-dessus que se joue le brillant et le cote metallique.
BANDE_HF = (6000, 8000, 10000, 12000, 14000, 16000)


def mesurer(L, R, cible=None, fs=FS):
    """Remesure d'une IR rendue, avec les fonctions du banc d'origine.

    Prend les deux canaux et somme leurs ENERGIES, jamais leurs formes d'onde — meme
    raison que dans le banc : les sommer annule l'energie precoce et fabrique une montee
    de reverbe qui n'existe pas.
    """
    i = onset(L)
    cq = [L[i:], R[i:]]
    t60, meth = temps_decroissance(schroeder_multi(cq), fs)
    fc, mag = spectre_multi(cq, fs)
    t_d, dens, niv = densite_echos(cq[0], fs)
    r = {"t60_s": None if np.isnan(t60) else float(t60), "methode": meth,
         "coin_hf_3db": float(coin_hf(fc, mag, 3.0))}
    tm = temps_melange(t_d, dens)
    r["temps_melange_ms"] = None if np.isnan(tm) else float(tm)
    r["rugosite_spectrale_db"] = rugosite_spectrale(cq[0], fs)
    for f in (500, 8000):
        cb = [bande(c, fs, f) for c in cq]
        tb, _ = temps_decroissance(schroeder_multi(cb), fs)
        r[f"t60_{f}"] = None if np.isnan(tb) else float(tb)
    r["amortissement_hf"] = (r["t60_8000"] / r["t60_500"]
                             if r["t60_500"] and r["t60_8000"] else None)
    for cle, deb, fin in (("spectre_debut_db", 0.0, 0.08),
                          ("spectre_queue_db", 0.15, None)):
        if cible and cible.get(cle):
            seg = [c[int(fs * deb):] if fin is None
                   else c[int(fs * deb):int(fs * fin)] for c in cq]
            f2, m2 = spectre_multi(seg, fs)
            ref2 = dict(cible[cle])
            gr = sorted(ref2)
            e = [float(np.interp(g, f2, m2)) - ref2[g] for g in gr]
            e = [v - np.mean(e[:3]) for v in e]          # cale sur le grave
            r[cle] = {int(g): round(v, 2) for g, v in zip(gr, e)}
            r[cle.replace("spectre", "ecart_hf")] = float(
                np.mean([v for g, v in zip(gr, e) if g >= 6000]))
    if cible and cible.get("spectre_db"):
        ref = dict(cible["spectre_db"])
        r["ecart_hf_db"] = float(np.mean([np.interp(f, fc, mag) - ref[f]
                                          for f in BANDE_HF if f in ref]))
    return r


def ecart(obtenu, cible):
    if obtenu is None or cible is None or cible == 0:
        return None
    return obtenu / cible - 1.0


# Cles rangees dans le dictionnaire d'accordage mais qui ne sont PAS des arguments du
# moteur : ce sont des resultats de mesure, exportes tels quels vers Programmes.h.
# ⚠️ Sans ce filtre, tout rendu posterieur a la phase de compensation COLOR plante sur
# `Moteur.__init__() got an unexpected keyword argument`. Le script s'arretait donc juste
# avant d'ecrire ses resultats, et programmes.json datait de la signature precedente.
HORS_MOTEUR = ("compensation_color", "compensation_lp")


def params_moteur(p):
    return {k: v for k, v in p.items() if k not in HORS_MOTEUR}


def accorder(cible, iterations=3, verbeux=True):
    """Derive puis corrige les parametres du moteur pour un programme."""
    nom = cible["nom"]
    impose = nom in ENVELOPPES

    # Sur Gate et Inverse, le T60 mesure decrit l'enveloppe imposee, pas le tank qui
    # tourne derriere. On accorde le tank sur une queue nominale et on plaque l'enveloppe
    # mesuree par-dessus ; le T60 n'est alors pas une cible du tank.
    sp = SPECS.get(nom)
    if sp:
        t60_cible = float(sp[0])            # RT60 nominal du manuel
        predelai_cible = float(sp[3])       # pre-delai nominal du manuel
    else:
        t60_cible = cible["t60_global_s"] or 1.0
        predelai_cible = cible["predelai_ms"]
    t60_tank = 1.2 if impose else t60_cible

    p = {"predelai_ms": predelai_cible,
         "lp_sortie_hz": float(cible["coin_hf_3db"]),
         "mur_hz": MUR_HZ,
         "taille": taille_pour(t60_tank),
         "t60_s": t60_tank,
         "diffusion": 0.75,
         "mixage": 0.6,
         # Pose directement, sans recherche : le filtre absorbant par ligne consomme le
         # rapport T60(8k)/T60(500) mesure tel quel. Deux phases d'accordage disparaissent.
         "ratio_hf": float(cible.get("amortissement_hf") or 0.7),
         # « Bass Multiply », pose depuis la mesure : T60(125 Hz)/T60(500 Hz). Sur les 14
         # programmes le grave est quasi plat (moyenne 1,00), donc ce terme corrige peu —
         # mais small1 monte a 1,27 et small2 descend a 0,67, et ca s'entend.
         "ratio_bass": float(np.clip(
             (cible["t60_par_bande"].get("125", {}).get("t60_s") or 1.0)
             / max(cible["t60_par_bande"].get("500", {}).get("t60_s") or 1.0, 1e-6),
             0.4, 2.5)),
         # ⚠️ Coupure grave/medium NON documentee et NON deductible des mesures : le grave
         # etant plat, il n'y a pas de coude a localiser. 400 Hz par defaut, assume.
         "cross_bass_hz": 400.0,
         "gain_prises": 0.5,
         "etalement": 0.6,
         "forme": 0.5,
         "pente_prises": 0.0,
         "mod_profondeur": 8.0, "mod_hz": 0.7}

    duree = float(np.clip((t60_cible if not impose else 1.5) * 1.4, 1.5, 4.0))

    def rendre_et_mesurer():
        m = Moteur(fs=FS, **params_moteur(p))
        L, R = m.reponse_impulsionnelle(duree)
        if impose:
            L = enveloppe_gate(L, FS, cible["enveloppe_db"])
            R = enveloppe_gate(R, FS, cible["enveloppe_db"])
        return mesurer(L, R, cible)

    def trace(etiquette, obt):
        if verbeux:
            print(f"    {etiquette:8s} T60 {obt['t60_s'] or float('nan'):.2f}s  "
                  f"melange {str(round(obt['temps_melange_ms'] or 0)):>4s}ms  "
                  f"amortHF {obt['amortissement_hf'] or float('nan'):.2f}  "
                  f"(taille {p['taille']:.2f} mix {p['mixage']:.2f} "
                  f"ratioHF {p['ratio_hf']:.2f} lp {p['lp_sortie_hz']:.0f}Hz)"
                  + f"  rug {obt.get('rugosite_spectrale_db') or 0:.2f}"
                  + (f"  dHF {obt['ecart_hf_db']:+.1f}dB" if "ecart_hf_db" in obt else ""))

    # --- Phase 1 : T60, taille et amortissement. Corrections multiplicatives amorties ;
    # ces trois-la convergent en deux passes.
    n1 = max(iterations // 2, 2)
    for it in range(n1):
        obt = rendre_et_mesurer()
        trace(f"decay{it}", obt)
        if not impose and obt["t60_s"]:
            p["t60_s"] *= float(np.clip((t60_cible / obt["t60_s"]) ** 0.8, 0.4, 2.5))
            # La taille vient de la phase 0 (rugosite) et n'est plus recalculee ici ;
            # `taille_pour` ne sert plus que de plancher.
            p["taille"] = max(p["taille"], taille_pour(p["t60_s"]) * 0.25)
        ca = cible["amortissement_hf"]
        if ca and obt["amortissement_hf"]:
            # Correction residuelle sur le ratio lui-meme, pas sur une coupure.
            p["ratio_hf"] = float(np.clip(
                p["ratio_hf"] * (ca / obt["amortissement_hf"]) ** 0.7, 0.1, 1.6))

    # --- Phase 2 : diffusion, par BALAYAGE et non par bissection.
    #
    # Deux tentatives ecartees avant celle-ci. La correction multiplicative oscillait
    # (diffusion et taille agissent toutes deux sur le melange). La bissection ensuite
    # supposait la monotonie — plus de diffusion, melange plus rapide — et cette
    # hypothese est FAUSSE : au-dela d'environ 0,7, un coefficient d'allpass eleve fait
    # trainer chaque impulsion au lieu de la densifier, et le melange se rallonge. Sur
    # plate_b la bissection montait a 0,91 en degradant la mesure a chaque pas.
    #
    # Un balayage de quelques points ne suppose rien sur la forme de la relation, ce qui
    # est le seul choix defendable ici.
    # Balaye MIXAGE (force de la matrice) et non plus la diffusion : dans un FDN c'est
    # la matrice qui porte le temps de melange, et elle le fait sans toucher a la longueur
    # des lignes — donc sans se battre avec la rugosite. C'est tout l'interet du
    # changement de topologie.
    cm = cible["temps_melange_ms"]
    if cm and not impose:
        grille = np.linspace(0.05, 1.0, max(iterations - n1, 4))
        meilleur, erreur_min = p["mixage"], float("inf")
        for it, d in enumerate(grille):
            p["mixage"] = float(d)
            obt = rendre_et_mesurer()
            trace(f"diff{it}", obt)
            tm = obt["temps_melange_ms"]
            if tm:
                e = abs(np.log(tm / cm))       # ecart relatif, symetrique
                if e < erreur_min:
                    erreur_min, meilleur = e, float(d)
        p["mixage"] = meilleur

    # --- Phase 3 : filtre de sortie, par bissection sur l'ecart spectral 6-16 kHz.
    # Le poser au coin a -3 dB mesure ne marche pas : l'amortissement de boucle assombrit
    # deja le spectre, donc le filtre de sortie doit etre plus ouvert que le coin brut.
    # Sans cette phase le moteur sortait 5 a 7 dB trop sombre dans l'aigu — le manque de
    # brillant et de metallique entendu a l'ecoute.
    bas, haut = float(cible["coin_hf_3db"]), 20000.0
    for it in range(3):
        p["lp_sortie_hz"] = float(np.sqrt(bas * haut))     # bissection geometrique
        obt = rendre_et_mesurer()
        trace(f"lp{it}", obt)
        e = obt.get("ecart_hf_db")
        if e is None:
            break
        if e < 0:                      # trop sombre -> ouvrir
            bas = p["lp_sortie_hz"]
        else:
            haut = p["lp_sortie_hz"]
    p["lp_sortie_hz"] = float(np.sqrt(bas * haut))

    # --- Phase 4 : TAILLE, pilotee par la rugosite spectrale.
    #
    # C'est la correction la plus importante du projet. La taille etait derivee d'une
    # heuristique — « au moins 14 tours de boucle dans le T60 » — et non d'une mesure.
    # Cette regle forcait des tanks longs, donc beaucoup de modes, donc un spectre lisse :
    # le moteur sortait 21 a 39 % moins rugueux que la machine sur TOUS les programmes,
    # ce qui s'entend comme « pas assez metallique, trop dense ».
    #
    # La longueur des lignes est le vrai controle du cote metallique : lignes courtes =
    # modes espaces = spectre accidente. Et le T60 n'en depend quasiment pas (2,45 a
    # 2,57 s sur toute la plage pour plate_b), donc rien n'empeche de la piloter ainsi.
    # La regle des 14 tours devient un simple plancher de securite.
    #
    # Placee en DERNIER, et pas en premier : la rugosite mesuree depend aussi du filtre de
    # sortie, que la phase 3 vient de deplacer de plusieurs kHz. Lancee trop tot, la
    # recherche optimisait contre un spectre qui n'existait pas encore et s'arretait a
    # 0,70 pour une cible de 1,10.
    cr = cible.get("rugosite_spectrale_db")
    if cr:
        meilleur, err_min = p["taille"], float("inf")
        for it, t in enumerate(np.geomspace(0.06, 1.8, 8)):
            p["taille"] = float(t)
            obt = rendre_et_mesurer()
            trace(f"taille{it}", obt)
            ru = obt.get("rugosite_spectrale_db")
            if ru and ru > 0:
                e = abs(np.log(ru / cr))
                if e < err_min:
                    err_min, meilleur = e, float(t)
        p["taille"] = meilleur
        # Changer la taille deplace aussi le T60 ET l'amortissement HF : on recolle les
        # deux, sinon le gain sur la rugosite se paie sur le reste (amortissement tombe a
        # 0,46 pour une cible de 0,68 sans cette reprise).
        # L'amortissement se regle desormais sur le SPECTRE DE LA QUEUE et non sur le
        # rapport T60(8k)/T60(500). Ce rapport decrit une vitesse de decroissance ; ce
        # qu'on entend, c'est la couleur de la queue. Les deux ne coincident pas : sur
        # `gate` le rapport etait a -19 % de la cible pendant que la queue etait a
        # -13 dB a 16 kHz.
        for _ in range(4):
            obt = rendre_et_mesurer()
            if not impose and obt["t60_s"]:
                p["t60_s"] *= float(np.clip((t60_cible / obt["t60_s"]) ** 0.9, 0.5, 2.0))
            # ⚠️ RETOUR AU RAPPORT DE T60, et non plus a l'ecart de spectre
            # queue-moins-attaque (2026-09-09).
            #
            # Ce substitut avait ete adopte parce que le critere direct « ne marchait
            # pas » : le tuner s'eloignait de la cible au lieu de s'en rapprocher. La
            # cause vient d'etre trouvee ailleurs — le filtre absorbant posait son
            # rapport a NYQUIST alors que la cible est relevee a 8 kHz, si bien qu'aucun
            # reglage de `ratio_hf` ne pouvait atteindre la valeur demandee. Le critere
            # etait bon, c'est l'actionneur qui mentait.
            #
            # Une fois la correspondance reparee (cf. _pole_pour_rapport), les deux
            # grandeurs decrivent la meme chose et le rapport de T60 est le meilleur des
            # deux : il est integre par Schroeder sur toute la decroissance, la ou
            # l'ecart de spectre depend de deux fenetres arbitraires et se laisse
            # polluer par les reflexions primaires et par le voicing.
            #
            # La couleur statique reste du ressort du voicing et du filtre de sortie —
            # une commande par grandeur mesuree, c'est la seule facon d'eviter que deux
            # phases se disputent la meme correction.
            ca = cible["amortissement_hf"]
            if ca and obt.get("amortissement_hf"):
                p["ratio_hf"] = float(np.clip(
                    p["ratio_hf"] * (ca / obt["amortissement_hf"]) ** 0.6, 0.1, 1.6))
        # Et le filtre de sortie a son tour : la taille a change le spectre sous lui.
        # Chaque phase deplace ce que la precedente avait regle ; deux reprises courtes
        # suffisent a refermer la boucle sans tout relancer.
        for _ in range(3):
            obt = rendre_et_mesurer()
            e = obt.get("ecart_hf_db")
            if e is None or abs(e) < 0.3:
                break
            p["lp_sortie_hz"] = float(np.clip(p["lp_sortie_hz"] * 10.0 ** (-e / 9.0),
                                              1000.0, 22000.0))
        trace("taille*", obt)

    # --- Phase 5 : voicing de sortie, derive du residu spectral mesure.
    # Deux passes suffisent : la correction est un inverse direct, pas une recherche.
    if cible.get("spectre_db"):
        ref = dict(cible["spectre_db"])
        gr = np.array(sorted(ref), dtype=float)
        for it in range(2):
            m = Moteur(fs=FS, **params_moteur(p))
            L, R = m.reponse_impulsionnelle(duree)
            if impose:
                L = enveloppe_gate(L, FS, cible["enveloppe_db"])
                R = enveloppe_gate(R, FS, cible["enveloppe_db"])
            fc, mag = spectre_multi([L[onset(L):], R[onset(R):]], FS)
            res = np.array([np.interp(f, fc, mag) - ref[int(f)] for f in gr])
            res -= np.mean(res[gr <= 2000])           # on ne corrige que la couleur
            if np.max(np.abs(res)) < 0.4:
                break
            # FIR court : 1,3 ms de latence, portable tel quel en quelques biquads.
            fn = np.concatenate([[0.0], gr / (FS / 2), [1.0]])
            gn = np.concatenate([[1.0], 10.0 ** (-np.clip(res, -9, 9) / 20.0), [0.2]])
            ordre = np.argsort(fn)
            fir = signal_firwin2(63, fn[ordre], gn[ordre])
            p["correction_fir"] = (fir if p.get("correction_fir") is None
                                   else np.convolve(p["correction_fir"], fir, "same"))
        trace("voicing", rendre_et_mesurer())

    # --- Phase 6 : forme de la montee.
    #
    # Deux cas, que la mesure separe d'elle-meme :
    #  - PALIER profond (hall_b, -25 dB) : reflexions primaires en parallele + entree du
    #    tank retardee jusqu'a la marche. L'oreille entend un palier puis une marche, pas
    #    une montee.
    #  - PAS de palier (plate_b et small2 sont a -2,7 et -2,2 dB des le depart) : la
    #    reverbe est pleine tout de suite, et seules des prises precoces dans le bras
    #    peuvent faire ca — avec les seules prises de fin, le signal doit traverser un bras
    #    entier avant de sortir, d'ou les 65 ms de montee.
    if not impose and cible.get("enveloppe_db"):
        env_cible = np.array([v for _, v in cible["enveloppe_db"][:20]])
        pas = int(FS * 0.005)

        def erreur_enveloppe():
            # Depart au debut du rendu, PAS a l'onset : quand l'entree du tank est
            # retardee de 60 ms, l'onset tombe apres le palier et le saute purement et
            # simplement. La comparaison devenait vide de sens pour les programmes qui ont
            # justement un palier — c'est-a-dire ceux qu'on cherche a corriger.
            m = Moteur(fs=FS, **params_moteur(p))
            L, _ = m.reponse_impulsionnelle(duree)
            j = int(FS * p["predelai_ms"] * 1e-3)
            h = L[j:j + pas * 20]
            if len(h) < pas * 20:
                return float("inf")
            e = np.sqrt(np.mean(h.reshape(20, pas) ** 2, axis=1))
            e = 20.0 * np.log10(np.maximum(e / max(e.max(), 1e-30), 1e-6))
            return float(np.sqrt(np.mean((e - env_cible) ** 2)))

        # ⛔ Branche des reflexions primaires DESACTIVEE (2026-09-07).
        #
        # Elle plaquait un jeu de prises discretes tirees des pics mesures. Or l'enveloppe
        # des programmes concernes montre que l'original n'a PAS de reflexions discretes a
        # -25 dB : il a un lit diffus dense et bas. Convoluee avec une impulsion, ma
        # section ne produit que des pointes isolees separees de silence — d'ou des creux
        # a -120 dB entre elles, et une erreur d'enveloppe qui a EMPIRE sur `gate`
        # (6,1 -> 24,9 dB). Le modele etait faux, pas mal regle.
        if False and cible.get("premieres_reflexions"):
            p["reflexions"] = [(r["t_ms"], r["amp_db"])
                               for r in cible["premieres_reflexions"]]
            p["retard_tank_ms"] = float(np.clip(cible.get("montee_ms", 0.0), 0.0, 120.0))
            meilleur, err = p.get("niveau_er", 0.0), float("inf")
            for v in (0.0, 0.05, 0.15, 0.4, 1.0, 2.5):
                p["niveau_er"] = v
                e = erreur_enveloppe()
                if e < err:
                    err, meilleur = e, v
            p["niveau_er"] = meilleur
        else:
            # Balaye la PENTE des prises (forme de la montee) puis la prise instantanee
            # (etablissement immediat des programmes courts). Les deux ensemble couvrent
            # les deux familles que la mesure separe : palier profond ou pas de palier.
            meilleur, err = 0.0, float("inf")
            # Plage etendue : la recherche saturait a 30 dB sur `hall_b`, signe
            # qu'elle voulait aller plus loin.
            for v in (0.0, 8.0, 18.0, 30.0, 45.0, 60.0):
                p["pente_prises"] = v
                e = erreur_enveloppe()
                if e < err:
                    err, meilleur = e, v
            p["pente_prises"] = meilleur
            meilleur, err = 0.0, float("inf")
            for v in (0.0, 0.5, 1.5, 3.0):
                p["prise_tot"] = v
                e = erreur_enveloppe()
                if e < err:
                    err, meilleur = e, v
            p["prise_tot"] = meilleur
        trace("montee", rendre_et_mesurer())

    # --- Reconciliation. Chaque phase deplace ce que la precedente avait reglee : apres
    # la phase d'enveloppe, la rugosite avait deborde a +43 %. Plutot que de tout relancer,
    # une derniere passe reprend les trois grandeurs qui derivent le plus, dans l'ordre
    # inverse de leur sensibilite.
    for _ in range(2):
        obt = rendre_et_mesurer()
        cr = cible.get("rugosite_spectrale_db")
        ru = obt.get("rugosite_spectrale_db")
        if cr and ru and ru > 0 and abs(np.log(ru / cr)) > 0.08:
            # Lignes plus longues = plus de modes = spectre plus lisse.
            p["taille"] = float(np.clip(p["taille"] * (ru / cr) ** 0.5, 0.06, 3.2))
        if not impose and obt["t60_s"]:
            p["t60_s"] *= float(np.clip((t60_cible / obt["t60_s"]) ** 0.9, 0.6, 1.7))
        e = obt.get("ecart_hf_db")
        if e is not None and abs(e) > 0.3:
            p["lp_sortie_hz"] = float(np.clip(p["lp_sortie_hz"] * 10.0 ** (-e / 9.0),
                                              1000.0, 22000.0))
    # --- Calibration de la compensation de duree pour la macro COLOR.
    # Cinq rendus, une seule fois : on mesure de combien le T60 flechit a chaque position
    # et on stocke le facteur correctif. Le bouton devient alors neutre en duree.
    if not impose:
        comp = []
        for r in np.linspace(0.0, 1.0, 5):
            q = macro_color(params_moteur(p), r)
            m = Moteur(fs=FS, **params_moteur(q))
            L, R = m.reponse_impulsionnelle(duree)
            j = onset(L)
            t, _ = temps_decroissance(schroeder_multi([L[j:], R[j:]]), FS)
            comp.append(float(np.clip(t60_cible / t, 0.5, 3.0))
                        if t and not np.isnan(t) else 1.0)
        p["compensation_color"] = [round(c, 4) for c in comp]
        if verbeux:
            print("    color    compensation " +
                  " ".join(f"{c:.2f}" for c in comp))

    obt = rendre_et_mesurer()
    trace("final", obt)
    return p, obt, duree


def rapport(res):
    L = ["# LuxRev-1 — accord du moteur sur les cibles mesurees", "",
         "Genere par `accorder.py`. Pour chaque programme : la cible mesuree sur l'IR du",
         "LXP-1, la valeur obtenue par le moteur, et l'ecart. Cible et resultat sont",
         "mesures par le meme code (`analyse_ir.py`) — sinon l'ecart contiendrait autant de",
         "difference de methode que de difference de son.", "",
         "## Ecarts", "",
         "| Programme | T60 cible | obtenu | Δ | Rugosité cible | obtenue | Δ | Mélange cible | obtenu | AmortHF cible | obtenue |",
         "|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|"]

    def pct(v):
        return "—" if v is None else f"{v:+.0%}"

    for r in res:
        c, o = r["cible"], r["obtenu"]
        note = " *(env.)*" if r["enveloppe"] else ""
        # ⚠️ Le Δ se compte sur la valeur REELLEMENT VISEE, c'est-a-dire le RT60 nominal
        # du manuel quand il existe. La colonne affichait le nominal et l'ecart au T60
        # MESURE sur l'IR : deux nombres differents dans la meme ligne. On lisait donc
        # « cible 2,60 s, obtenu 2,72 s, Δ -21 % » — trois affirmations dont deux se
        # contredisent. Le vrai ecart sur ce programme est de +5 %, et il est sous 1 %
        # sur huit programmes. Le T60 n'a jamais ete le probleme ; le tableau, si.
        vise = SPECS.get(r["nom"], [c["t60_global_s"]])[0]
        L.append(
            f"| `{r['nom']}`{note} | "
            f"{vise:.2f} s | "
            f"{(o['t60_s'] or 0):.2f} s | {pct(ecart(o['t60_s'], vise))} | "
            f"{(c.get('rugosite_spectrale_db') or 0):.2f} | "
            f"{(o.get('rugosite_spectrale_db') or 0):.2f} | "
            f"{pct(ecart(o.get('rugosite_spectrale_db'), c.get('rugosite_spectrale_db')))} | "
            f"{(c['temps_melange_ms'] or 0):.0f} ms | {(o['temps_melange_ms'] or 0):.0f} ms | "
            f"{(c['amortissement_hf'] or 0):.2f} | {(o['amortissement_hf'] or 0):.2f} |")

    L += ["", "*(env.)* = programme a enveloppe imposee. Le T60 y decrit l'enveloppe "
          "plaquee en sortie, pas le tank : ce n'est pas une cible d'accordage du tank.",
          "", "## Parametres retenus", "",
          "| Programme | Pré-délai | Taille | Mixage | T60 tank | Ratio HF | LP sortie | Prises | Étalement |",
          "|---|--:|--:|--:|--:|--:|--:|--:|--:|"]
    for r in res:
        p = r["params"]
        L.append(f"| `{r['nom']}` | {p['predelai_ms']:.1f} ms | {p['taille']:.2f} | "
                 f"{p.get('mixage', 0):.2f} | {p['t60_s']:.2f} s | "
                 f"{p.get('ratio_hf', 0):.2f} | {p['lp_sortie_hz']:.0f} Hz | "
                 f"{p.get('gain_prises', 0):.2f} | {p.get('etalement', 0):.2f} |")

    L += ["", "## Lecture", "",
          "- **T60** et **AmortHF** sont derives puis corriges ; un ecart de quelques "
          "pourcents est sans consequence audible.",
          "- **Mélange** est le plus dur a atteindre : c'est le seul parametre ou la "
          "topologie elle-meme peut manquer de marge. Un ecart important signale qu'il "
          "faudra plus d'etages de diffusion, pas un meilleur reglage.",
          "- Le **mur anti-image** est fixe a 15 000 Hz pour tous, et n'apparait donc pas "
          "dans le tableau.",
          "- La **modulation** n'est pas accordee ici : elle est invisible dans une IR. "
          "Valeurs de depart 8 echantillons / 0,7 Hz, a regler a l'oreille.", ""]
    (SORTIE / "ACCORD.md").write_text("\n".join(L), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iterations", type=int, default=3)
    ap.add_argument("--programme", default=None)
    a = ap.parse_args()

    SORTIE.mkdir(parents=True, exist_ok=True)
    fichiers = [f for f in sorted(MESURES.glob("lxp1_*.json")) if f.stem not in EXCLUS]
    if a.programme:
        fichiers = [f for f in fichiers if a.programme in f.stem]
    if not fichiers:
        raise SystemExit("Aucune cible. Lancer analyse_ir.py d'abord.")

    print(f"{len(fichiers)} programmes a accorder "
          f"({len(EXCLUS)} Delay reportes)\n")
    res = []
    for f in fichiers:
        cible = json.loads(f.read_text(encoding="utf-8"))
        print(f"  {cible['nom']}")
        p, obt, duree = accorder(cible, a.iterations)
        res.append({"nom": cible["nom"], "params": p, "obtenu": obt, "cible": cible,
                    "enveloppe": cible["nom"] in ENVELOPPES, "duree_rendu_s": duree})

    def serialisable(d):
        return {k: (list(np.round(v, 8)) if isinstance(v, np.ndarray) else v)
                for k, v in d.items()}

    # ⚠️ FUSION, pas remplacement. Avec --programme, ecrire le seul programme accorde
    # effacait les onze autres — et comme generer_programmes.py lit ce fichier, un essai
    # sur un programme suffisait a vider le plugin. Un banc d'essai ne doit pas pouvoir
    # detruire le resultat d'une campagne complete.
    fichier = SORTIE / "programmes.json"
    tout = json.loads(fichier.read_text(encoding="utf-8")) if fichier.exists() else {}
    tout.update({r["nom"]: serialisable(r["params"]) for r in res})
    fichier.write_text(json.dumps(tout, indent=2, ensure_ascii=False), encoding="utf-8")
    # Le rapport, lui, ne decrit que ce qui vient d'etre mesure : il n'a pas de sens
    # partiel, donc on ne le reecrit que sur une campagne complete.
    if not a.programme:
        rapport(res)
    print(f"\n-> {SORTIE / 'ACCORD.md'}")
    print(f"-> {SORTIE / 'programmes.json'}  ({len(res)} programmes)")


if __name__ == "__main__":
    main()
