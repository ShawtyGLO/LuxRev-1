#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LuxRev-1 — prototype du moteur de reverbe.

Topologie de la lignee Griesinger/Dattorro : pre-delai, diffuseur d'entree a 4 allpass,
tank figure-8 a deux bras croises, amortissement dans la boucle, prises de sortie
eloignees. Les longueurs de lignes viennent de Dattorro 1997 (JAES 45(9)), transposees de
29 761 Hz a 48 kHz.

Ce fichier ne contient AUCUNE valeur accordee sur le LXP-1 : il expose des parametres.
Les valeurs sont derivees des mesures par `accorder.py`, seul endroit ou une cible entre
dans le projet.

Prototype Python, pas du code temps reel : la boucle est ecrite pour etre lisible et
verifiable. Le portage JUCE viendra une fois les cibles atteintes.
"""

import numpy as np
from preampli import Preampli
from scipy import signal as _sig

FS = 48000

# Fréquence d'échantillonnage interne du LXP-1, donnée par son manuel de service :
# word clock a 31,25 kHz, depuis un quartz maitre de 16 MHz. Nyquist a 15,6 kHz.
# Le tank tourne a cette frequence, pas a 48 kHz — c'est structurel, pas cosmetique.
FS_INTERNE = 31250

# Filtres 15 kHz du LXP-1 : module U32 en anti-repliement a l'entree, modules U25 et U29
# en anti-image sur les sorties du DAC (manuel de service). Le mur mesure a 16,9 kHz sur
# les IR est le -20 dB de ce filtre-la ; 15 kHz est la valeur de conception.
#
# ⚠️ « 15 kHz » nomme le bord de bande passante du module, pas son point a -3 dB : la
# mesure sur les 14 IR place celui-ci vers 17 kHz. C'est cette valeur-la qu'il faut
# implementer, sinon on coupe 8 dB d'air que la machine laisse passer.
MUR_HZ_LXP = 17000.0
# Frequence a laquelle `ratio_hf` est DEFINI. Ce n'est pas un choix libre : c'est la
# bande haute de `analyse_ir.py`, donc celle ou la cible T60(8 kHz)/T60(500 Hz) est
# relevee sur les IR. Le filtre absorbant doit atteindre son rapport ICI et nulle part
# ailleurs, sinon le parametre et sa mesure ne parlent pas de la meme chose.
FREQ_AMORT_HZ = 8000.0
ORDRE_MUR = 6

# Pre/de-accentuation : U31 (LF353) accentue avant l'echantillonneur-bloqueur, C48/R43 et
# C56/R44 compensent apres le DAC. Astuce de 1987 pour gagner du rapport signal/bruit dans
# l'aigu sur 16 bits.
#
# ✅ MESURE dans la nomenclature du manuel de service (2026-09-07) :
#     R43, R44 = 6,8 kOhm  (RES,CF,5%,1/4W,6.8K OHM -> R43,44,61)
#     C48, C56 = 10 nF     (CAP,MYL,.01uF,100V,10%,RAD -> C48,56)
#     tau = R * C = 68 us,  soit un coin a 2 340 Hz.
#
# J'avais suppose 50 us (le standard CD de l'epoque). C'est 68. La difference n'est pas
# cosmetique : le coin descend de 3 180 a 2 340 Hz, donc l'accentuation mord plus bas et
# le bruit de quantification est mis en forme differemment.
#
# ⚠️ La seconde constante reste inconnue : elle depend d'une resistance de la meme maille
# que le schema n'a pas encore livree. Le rapport 3,33 (celui du couple 50/15) est garde
# faute de mieux, et fixe la hauteur du plateau. A confirmer sur la planche du schema.
EMPHASE_T1 = 68e-6
EMPHASE_T2 = EMPHASE_T1 / 3.33


def _emphase(fs, inverse=False):
    """Biquad de pre-accentuation (ou son inverse exact) par transformation bilineaire.

    En regime lineaire les deux se compensent exactement : ajouter l'emphase ne change
    RIEN au spectre mesure. Elle n'agit que sur ce qui se passe entre les deux — le bruit
    de quantification 16 bits et l'ecretage interne. C'est precisement la, et nulle part
    ailleurs, que se joue le grain de la machine.
    """
    num, den = ([EMPHASE_T1, 1.0], [EMPHASE_T2, 1.0])
    if inverse:
        num, den = den, num
    b, a = _sig.bilinear(num, den, fs)
    return _sig.tf2sos(b, a)


def _courbe_compensation(points):
    """Normalise une courbe de compensation pour qu'elle vaille EXACTEMENT 1,0 au centre.

    ⚠️ Sans ca, COLOR a 50 % ne redonne pas le moteur accorde. Les courbes sont mesurees
    par regression, donc leur point central sort a 1,01 ou 0,995 plutot qu'a 1,000 — assez
    pour decaler le T60 de 1 % au centre de la course. Inaudible, mais c'est la promesse
    du bouton qui tombe : « le son a position originale ne doit pas changer de nos mesures
    des IR ». Le centre est un point fixe, pas un resultat d'ajustement.
    """
    c = np.asarray(points, dtype=float)
    milieu = float(np.interp(0.5, np.linspace(0.0, 1.0, len(c)), c))
    return c / milieu if abs(milieu) > 1e-9 else c


def macro_color(params, color=0.5, compensation=None):
    """Applique la macro COLOR a un jeu de parametres accorde.

    Nommee COLOR (et non « Resonance ») parce que c'est la couleur de la reverbe qui
    change, pas une resonance particuliere.

    0,5 = le LXP-1 mesure, a l'identique. Vers 0 la reverbe devient plus cremeuse, vers 1
    plus metallique et synthetique.

    Les trois leviers vont dans le meme sens, ce qui est ce qui rend la macro lisible :

      taille          lignes courtes = modes espaces = spectre accidente = metallique
                      lignes longues = beaucoup de modes = spectre lisse = cremeux
      mixage          peu de melange = echos distincts (« choppier » chez Lexicon)
      mod_profondeur  la modulation aleatoire lisse ; on la retire pour durcir

    ⚠️ COMPENSATION : changer `taille` change la longueur de boucle, donc le T60 si on n'y
    prend pas garde. Le gain est recalcule par ligne a partir de `t60_s` et de la longueur
    reelle (cf. `regler_t60`), donc la duree ne bouge pas — c'est ce qui rend le bouton
    utilisable. Sans ca, tourner Resonance rallongerait ou raccourcirait la reverbe en meme
    temps qu'il changerait son caractere, et personne ne voudrait s'en servir.
    """
    r = float(np.clip(color, 0.0, 1.0))
    u = (r - 0.5) * 2.0                      # -1 = cremeux, 0 = reference, +1 = metallique
    q = dict(params)
    # La courbe de compensation voyage AVEC le jeu de parametres (elle est calibree par
    # programme et rangee dans programmes.json), mais ce n'est pas un parametre du moteur :
    # on la retire ici pour que `q` reste directement utilisable comme **kwargs.
    if compensation is None:
        compensation = q.get("compensation_color")
    comp_lp = q.get("compensation_lp")
    q.pop("compensation_color", None)
    q.pop("compensation_lp", None)

    def vers(actuel, but, k):
        """Interpole de la valeur accordee vers une extremite. PAS une multiplication.

        La premiere version multipliait les parametres accordes. Elle ne marchait pas :
        l'accordeur les a deja pousses aux bords (mixage a 0,05, taille contre son
        plafond), donc il ne restait aucune marge et la course sortait non monotone.
        Interpoler vers une cible garantit que chaque extremite est atteignable.
        """
        return float(actuel + (but - actuel) * k)

    k = abs(u)
    if u < 0:                                # --- vers le cremeux
        q["taille"] = float(np.clip(vers(q.get("taille", 1.0),
                                         q.get("taille", 1.0) * 2.6, k), 0.06, 3.2))
        q["mixage"] = vers(q.get("mixage", 0.5), 1.0, k)
        q["mod_profondeur"] = vers(q.get("mod_profondeur", 8.0), 26.0, k)
        q["n_lignes"] = 8
    else:                                    # --- vers le metallique
        # ⚠️ Bornes serrees du cote metallique. Sans elles, `small2` tombait a 0,84 s de
        # T60 pour 1,20 vise : avec 3 lignes tres courtes et presque aucun melange, la
        # structure modale devient trop clairsemee pour soutenir la decroissance demandee,
        # et la compensation lache. Un utilisateur entendrait la reverbe RACCOURCIR en
        # tournant Resonance, ce qui n'est pas ce que le bouton annonce.
        q["taille"] = float(np.clip(vers(q.get("taille", 1.0),
                                         max(q.get("taille", 1.0) * 0.45, 0.30), k),
                                    0.06, 3.2))
        q["mixage"] = vers(q.get("mixage", 0.5), 0.06, k)
        q["mod_profondeur"] = vers(q.get("mod_profondeur", 8.0), 0.0, k)
        q["n_lignes"] = int(round(8 - k * 4.0))

    # ⚠️ COMPENSATION DE DUREE, calibree par programme.
    #
    # Le T60 flechit reellement vers le metallique — confirme a l'ecoute, pas seulement
    # mesure : « en mode metallique on perd un peu de longueur et de corps ». Avec moins de
    # lignes et des lignes plus courtes, la structure modale ne soutient plus la meme
    # decroissance, et la formule de gain par ligne ne suffit pas a le rattraper.
    #
    # `compensation` est une courbe de 5 points (COLOR = 0, 0,25, 0,5, 0,75, 1) mesuree une
    # fois par `accorder.py`. Interpolee ici, elle coute une multiplication au moment de
    # construire le moteur — rien en temps reel.
    if compensation:
        c = _courbe_compensation(compensation)
        q["t60_s"] = float(q.get("t60_s", 1.0)
                           * np.interp(r, np.linspace(0.0, 1.0, len(c)), c))

    # ⚠️ COMPENSATION SPECTRALE, meme principe.
    #
    # Changer le nombre et la longueur des lignes deplace aussi la couleur tonale : « la
    # clarte est moins presente, ca manque d'informations dans les aigus ». Un utilisateur
    # devrait rattraper au shelf a chaque mouvement du bouton — ce qui reviendrait a dire
    # que le bouton n'est pas fini.
    #
    # COLOR doit changer le CARACTERE (densite modale, grain), pas le TIMBRE. La coupure du
    # filtre de sortie est donc corrigee par la meme courbe a 5 points, calibree une fois.
    if comp_lp:
        q["lp_sortie_hz"] = float(np.clip(
            q.get("lp_sortie_hz", 8000.0)
            * np.interp(r, np.linspace(0.0, 1.0, len(comp_lp)),
                        _courbe_compensation(comp_lp)),
            1000.0, 22000.0))
    return q


def _bloqueur(x, n):
    """Bloqueur d'ordre zero : remontee 31,25 -> 48 kHz a la maniere du DAC.

    Volontairement PAS un reechantillonnage propre. Un interpolateur a bande limitee
    supprime tout au-dessus de 15,6 kHz, alors que le PCM54 du LXP-1 maintient chaque
    echantillon jusqu'au suivant et produit donc des IMAGES a 31250-f, que le filtre
    analogique n'enleve qu'en partie. Le contenu mesure entre 15 et 17 kHz sur les IR
    d'origine vient de la, et de nulle part ailleurs : avec un resample_poly le moteur
    sortait 8 dB trop sombre a 16 kHz — l'air qui manquait a l'ecoute.
    """
    idx = (np.arange(n) * len(x)) // n
    return x[np.minimum(idx, len(x) - 1)]


def _correction_sinc(fs_hote, fs_interne, n_taps=127):
    """FIR compensant l'enveloppe sin(x)/x du bloqueur d'ordre zero.

    Un maintien d'echantillon attenue par sinc(f/fs_interne) : -3,6 dB des 15 kHz pour
    31,25 kHz. Tout etage de reconstruction de DAC corrige cela ; sans la correction, le
    moteur perdait 3 dB d'air a 14-15 kHz en plus de ce que coupe le filtre.
    """
    f = np.linspace(0.0, 1.0, 512)
    x = np.pi * f * (fs_hote / 2.0) / fs_interne
    g = np.ones_like(x)
    g[1:] = x[1:] / np.sin(np.clip(x[1:], 1e-9, np.pi * 0.999))
    g = np.clip(g, 1.0, 6.0)
    g[f > 0.80] = g[np.searchsorted(f, 0.80)]      # pas d'explosion pres de Nyquist
    return _sig.firwin2(n_taps, f, g)


def _ajuster(y, n):
    """Ramene y a exactement n echantillons (troncature ou zero-padding)."""
    if len(y) == n:
        return y
    if len(y) > n:
        return y[:n]
    return np.concatenate([y, np.zeros(n - len(y))])


def quantifier(x, bits):
    """Quantification uniforme sur `bits`, pleine echelle a +/-1.

    Le LXP-1 est 16 bits lineaires : DAC PCM54HP, et un ADC a approximations successives
    dans le Lexichip qui reutilise ce meme DAC en contre-reaction.
    """
    if not bits:
        return x
    q = 2.0 ** (bits - 1)
    # Dither TPDF d'un LSB crete-a-crete. Les IR d'origine ont 99 % d'echantillons non
    # nuls jusqu'a la fin du fichier — un vrai plancher de convertisseur 16 bits — la ou
    # une quantification seche tombe au zero numerique absolu et fait disparaitre la queue
    # d'un coup au lieu de la laisser se fondre.
    r = np.random.default_rng(20260907)
    d = (r.random(len(x)) - r.random(len(x)))
    return np.round(x * q + d) / q

# Longueurs de Dattorro 1997, en echantillons a 29 761 Hz. Conservees telles quelles pour
# que la source reste verifiable ; la transposition se fait a la construction.
FS_DATTORRO = 29761.0
DIFFUSEUR_LONGUEURS = (142, 107, 379, 277)
DIFFUSEUR_COEFFS = (0.75, 0.75, 0.625, 0.625)
TANK_A = (672, 4453, 1800, 3720)     # (allpass module, delai, allpass, delai)
TANK_B = (908, 4217, 2656, 3163)

# --- Tank FDN a 8 lignes -------------------------------------------------------
#
# Remplace le figure-8 a deux bras, qui butait sur une limite structurelle : il ne savait
# pas produire a la fois un spectre creuse (rugosite) et une montee lente (temps de
# melange). Les deux dependaient de la meme longueur de lignes, et il fallait choisir.
#
# Un FDN separe les deux : la LONGUEUR des lignes porte la densite modale, la FORCE DU
# MELANGE de la matrice porte la vitesse de diffusion. Deux commandes pour deux mesures.
#
# Longueurs premieres entre elles, a 31,25 kHz, rapport 1:2,7. Total 15 820 echantillons a
# taille 1,0 — tres en dessous des 65 536 de la DRAM du LXP-1 (quatre 64K x 4), donc la
# machine pouvait largement se payer huit lignes, et sans doute davantage.
FDN_LONGUEURS = (1123, 1319, 1543, 1789, 2003, 2333, 2699, 3011)
N_FDN = len(FDN_LONGUEURS)


def _transpose(n, fs, taille=1.0):
    return max(int(round(n * fs / FS_DATTORRO * taille)), 4)


# --------------------------------------------------------------------- elements

class Delai:
    """Ligne a retard simple, retard fixe egal a la taille du tampon."""

    __slots__ = ("buf", "n", "i")

    def __init__(self, n):
        self.n = int(n)
        self.buf = np.zeros(self.n)
        self.i = 0

    def pousser(self, x):
        y = self.buf[self.i]
        self.buf[self.i] = x
        self.i = (self.i + 1) % self.n
        return y


class Allpass:
    """Allpass de Schroeder : d = buf[i] ; w = x + k*d ; buf[i] = w ; y = d - k*w.

    Passe-tout au sens strict : le module est plat, seule la phase change. C'est ce qui
    permet d'etaler une impulsion en une bouffee dense SANS colorer — la brique de base de
    toute la lignee.
    """

    __slots__ = ("buf", "n", "i", "k")

    def __init__(self, n, k):
        self.n = int(n)
        self.buf = np.zeros(self.n)
        self.i = 0
        self.k = float(k)

    def traiter(self, x):
        d = self.buf[self.i]
        w = x + self.k * d
        self.buf[self.i] = w
        self.i = (self.i + 1) % self.n
        return d - self.k * w


class AllpassModule:
    """Allpass dont le retard de lecture est module autour d'une valeur nominale.

    Desaccorde en permanence les modes stationnaires du tank : sans lui la queue se fige
    et devient metallique. Le tampon est plus long que le retard nominal pour laisser la
    place a l'excursion, et la lecture est interpolee lineairement.
    """

    __slots__ = ("buf", "n", "i", "k", "nominal", "_yi")

    def __init__(self, nominal, k, marge):
        self.nominal = int(nominal)
        self.n = int(nominal + marge + 4)
        self.buf = np.zeros(self.n)
        self.i = 0
        self.k = float(k)
        self._yi = 0.0          # etat de l'interpolateur allpass

    def traiter(self, x, decalage):
        """Lecture a retard fractionnaire par interpolation ALLPASS du 1er ordre.

        Pas lineaire : l'interpolation lineaire est un passe-bas, discret sur un passage
        mais qui se compose a CHAQUE tour de boucle. Mesure sur `gate` : la queue perdait
        11,8 dB a 16 kHz avec interpolation lineaire, 3,8 dB a profondeur reduite, et rien
        du tout sans modulation — c'est-a-dire que l'interpolateur, et non l'amortissement,
        etait la cause du « trop de mids » entendu. Un allpass est plat en module par
        construction : il ne deplace que la phase, ce qui est exactement ce qu'on veut
        dans une boucle recirculante.
        """
        p = self.nominal + decalage
        i0 = int(p)
        f = p - i0
        if f < 0.5:                       # l'allpass du 1er ordre se comporte mieux
            i0 -= 1                       # pour une fraction dans [0.5, 1.5)
            f += 1.0
        a = (self.i - i0) % self.n
        b = (self.i - i0 - 1) % self.n
        eta = (1.0 - f) / (1.0 + f)
        d = eta * self.buf[a] + self.buf[b] - eta * self._yi
        self._yi = d
        w = x + self.k * d
        self.buf[self.i] = w
        self.i = (self.i + 1) % self.n
        return d - self.k * w


class DelaiModule:
    """Ligne a retard a lecture fractionnaire, SANS contre-reaction interne.

    C'est ce qu'un FDN attend a chaque branche. Un allpass a la place — ce que j'avais
    d'abord mis — apporte sa propre boucle de gain 0,7 : la decroissance sortait trois fois
    trop longue quel que soit le gain applique par le reseau, parce que l'energie
    recirculait a deux endroits au lieu d'un.

    L'interpolation est un allpass du 1er ordre, plate en module : dans une boucle
    recirculante, une interpolation lineaire coute jusqu'a 12 dB d'aigu dans la queue.
    """

    __slots__ = ("buf", "n", "i", "nominal", "_yi", "prises")

    def __init__(self, nominal, marge,
                 fractions=(0.06, 0.13, 0.24, 0.41, 0.63, 0.85)):
        self.nominal = int(nominal)
        self.n = int(nominal + marge + 4)
        self.buf = np.zeros(self.n)
        self.i = 0
        self._yi = 0.0
        # Prises intermediaires le long de la ligne. Le brevet EMT US4181820 (Blesser &
        # Bader, figure 2) montre exactement ca : UNE longue ligne a retard portant des
        # prises A2...An, chacune amortie puis resommee. C'est ainsi qu'on obtient une
        # densite d'arrivees elevee avec peu de memoire — la contrainte de 1977 comme
        # celle de 1987.
        #
        # Sans elles, huit lignes donnent huit arrivees par tour : bien trop peu pour
        # remplir le palier d'un hall, et l'enveloppe sortait en pointes isolees separees
        # de creux a -120 dB la ou l'original a un lit diffus continu a -25 dB.
        # Fractions descendues jusqu'a 0,06 : avec un plancher a 0,27 la premiere arrivee
        # tombait a 18 ms sur les halls, et les 17 premieres millisecondes restaient
        # parfaitement muettes.
        self.prises = [max(int(nominal * f), 1) for f in fractions]

    def traiter(self, x, decalage):
        p = self.nominal + decalage
        i0 = int(p)
        f = p - i0
        if f < 0.5:
            i0 -= 1
            f += 1.0
        a = (self.i - i0) % self.n
        b = (self.i - i0 - 1) % self.n
        eta = (1.0 - f) / (1.0 + f)
        d = eta * self.buf[a] + self.buf[b] - eta * self._yi
        self._yi = d
        self.buf[self.i] = x
        self.i = (self.i + 1) % self.n
        return d

    def somme_prises(self, poids):
        """Somme ponderee des prises intermediaires, a lecture entiere.

        Les poids montent avec la position de la prise. Dans une salle, la densite
        d'arrivees CROIT avec le temps ; a poids egaux, les prises donnaient une densite
        constante et l'enveloppe atteignait son plateau des 15 ms, la ou l'original tient
        son palier bas jusqu'a 60 ms.
        """
        t = 0.0
        n, i, b = self.n, self.i, self.buf
        for k, p in enumerate(self.prises):
            t += poids[k] * b[(i - p) % n]
        return t


# ----------------------------------------------------------------------- moteur

def _pole_pour_rapport(rap, cos_w):
    """Pole du premier ordre dont le module vaut `rap` A LA PULSATION VISEE.

    Le filtre absorbant est y = g(1-b).x + b.y[-1], de module normalise

        |H(w)| / g = (1 - b) / sqrt(1 - 2 b cos w + b^2)

    Poser |H| = rap donne une quadratique en b dont les deux racines sont b et
    1/b — on garde donc celle de module inferieur a 1, seule stable.

    A cos_w = -1 (Nyquist) la solution redonne exactement (1-rap)/(1+rap) :
    l'ancienne formule est le cas particulier, et c'est pour ca qu'elle passait
    inapercue.
    """
    r2 = rap * rap
    a = 1.0 - r2
    if abs(a) < 1e-12:                    # rap = 1 : filtre plat, pas de pole
        return 0.0
    b_lin = 2.0 * (r2 * cos_w - 1.0)
    disc = b_lin * b_lin - 4.0 * a * a
    if disc <= 0.0:
        return 0.0
    rac = np.sqrt(disc)
    for b in ((-b_lin - rac) / (2.0 * a), (-b_lin + rac) / (2.0 * a)):
        if abs(b) < 1.0:
            return float(b)
    return 0.0


class Moteur:
    """Reverbe algorithmique parametrable.

    Parametres, et la mesure que chacun vise :

      predelai_ms       -> pre-delai mesure
      taille            -> echelle des lignes du tank ; motif des premieres reflexions
      diffusion         -> coefficient du diffuseur ; RUGOSITE et temps de melange
      n_diffuseurs      -> nombre d'etages de diffusion (0-4) ; effet faible
      k_tank, k_tank2   -> coefficients des allpass DU TANK ; c'est eux qui portent la
                           RUGOSITE, donc le cote metallique
      t60_s             -> gain de boucle ; T60 global
      treble_hz         -> « Treble » : coupure HF a l'entree, DANS la boucle de pre-delai
      predelai_fb       -> « Predelay Feedback » : contre-reaction autour de Treble +
                           Diffusion + Predelay
      ratio_bass        -> « Bass Multiply » : T60(grave)/T60(medium)
      cross_bass_hz     -> « Crossover » : ou le grave cede la place au medium. Sur le
                           Lexicon 224 c'est un parametre balayable de 100 Hz a 11 kHz,
                           pas une constante — il n'y a donc pas de « vraie » valeur a
                           retrouver. Sans effet sur les 12 programmes, dont le grave est
                           mesure plat (T60 grave/medium = 1,00 en moyenne)
      hp_sortie_hz      -> « Low Cut Filter » de sortie
      stereo            -> largeur ; 1,0 = l'image mesuree du LXP-1
      preampli_db       -> pot d'entree R52 en dB ; None = etage d'entree desactive
      tilt              -> TILT en dB : negatif = plus de corps, positif = plus d'air
      tilt_pivot_hz     -> frequence de bascule (700 Hz par defaut)
      ratio_hf          -> T60(8 kHz)/T60(500 Hz) VISE ; pilote directement les filtres
                           absorbants par ligne, sans coupure intermediaire a chercher
      lp_sortie_hz      -> pente douce ; coin HF a -3 dB
      mur_hz            -> filtre anti-image ; coupure absolue (16,9 kHz mesures)
      mod_profondeur    -> profondeur de modulation du tank, en echantillons
      mod_hz            -> vitesse de modulation
      reflexions        -> pics mesures < 60 ms ; le PALIER de l'enveloppe
      niveau_er         -> dosage des reflexions primaires
      retard_tank_ms    -> retard d'entree du tank seul ; la MARCHE de l'enveloppe
      prise_tot         -> dosage des prises precoces ; VITESSE d'etablissement
      mixage            -> force de la matrice de Householder ; TEMPS DE MELANGE,
                           independamment de la longueur des lignes
      gain_prises       -> « DIFFUSION » du PCM 70 : densite des arrivees PRECOCES
      fenetre_diff_ms   -> etendue de cette periode initiale
      n_lignes          -> nombre de lignes actives (2-8) ; DENSITE MODALE. Le levier de
                           RESONANCE, parce qu'il garde sa marge quel que soit l'accordage
      mixage            -> « DEFINITION » : densite de la partie TARDIVE, decouplee
      pente_prises      -> dB entre premiere et derniere prise ; PENTE DE LA MONTEE
      etalement         -> etalement des injections, en fraction du plus court retard de
                           boucle ; le MOTIF des premieres arrivees et la MONTEE
      forme             -> « Shape » de Lexicon : 0 = le son entre au DEBUT de la fenetre
                           d'injection, 1 = a la FIN. Avec `etalement` (« Spread »), forme
                           l'enveloppe de la partie precoce

    Le mur et la pente douce sont deux etages distincts parce que la mesure dit qu'ils le
    sont : la pente varie par programme (2,9-7,5 kHz), le mur est constant sur les 14.
    """

    def __init__(self, fs=FS, predelai_ms=0.0, taille=1.0, diffusion=0.75, t60_s=2.0,
                 ratio_hf=0.7, lp_sortie_hz=6000.0, mur_hz=MUR_HZ_LXP,
                 mod_profondeur=8.0, mod_hz=0.7, bits=16, emphase=True,
                 n_diffuseurs=4, k_tank=0.7, k_tank2=0.5, correction_fir=None,
                 reflexions=None, niveau_er=0.0, retard_tank_ms=0.0, prise_tot=0.0,
                 mixage=1.0, gain_prises=0.0, etalement=0.0, n_lignes=N_FDN,
                 forme=0.0, pente_prises=0.0, fenetre_diff_ms=60.0,
                 treble_hz=20000.0,
                 predelai_fb=0.0, ratio_bass=1.0, cross_bass_hz=400.0,
                 hp_sortie_hz=0.0, tilt=0.0, tilt_pivot_hz=700.0,
                 stereo=1.0, preampli_db=None):
        self.fs_hote = fs
        # Tout l'interieur travaille a la frequence de la machine.
        fs = self.fs = FS_INTERNE
        self.bits = bits
        self.emphase = bool(emphase)
        self.mod_hz = float(mod_hz)
        self.taille = float(taille)
        # La profondeur est RELATIVE a la taille du tank. En absolu, 8 echantillons sur
        # l'allpass d'un grand hall (1442 ech.) font 0,55 % ; sur une petite salle
        # (347 ech.) ils font 2,3 %, soit quatre fois plus de desaccord — ce qui
        # s'entendait comme un flanger sur les programmes courts.
        self.mod_profondeur = float(mod_profondeur) * self.taille

        # --- Etage d'entree, dans l'ORDRE du manuel LXP-15 :
        #     « Predelay […] is located after Treble and Diffusion and before the reverb »
        #     « Predelay Feedback : a feedback loop around Treble, Diffusion, and Predelay »
        #
        # J'avais pre-delai -> diffuseur -> tank. C'est l'inverse, et la boucle manquait
        # entierement. C'est elle qui fait que « la diffusion augmente a chaque repetition »
        # et que l'attenuation d'aigu s'accumule passe apres passe.
        self.predelai = Delai(max(int(round(predelai_ms * 1e-3 * fs)), 1))
        self.k_treble = self._coef_lp(treble_hz)
        self.predelai_fb = float(np.clip(predelai_fb, -0.95, 0.95))

        # --- Reflexions primaires, en PARALLELE du tank.
        #
        # Sans elles le tank est attaque directement et se remplit des le premier
        # echantillon : l'enveloppe rampe continument, ce qui s'entend comme une montee de
        # reverbe. La machine fait autre chose — l'enveloppe mesuree reste PLATE autour de
        # -24 dB pendant 70 ms, puis monte d'un coup quand la queue arrive. Un palier suivi
        # d'une marche ne s'entend pas comme une montee ; une rampe, si.
        #
        # Les positions et amplitudes viennent des pics mesures sur l'IR d'origine
        # (`premieres_reflexions`) : c'est un element CLONE, pas invente. Les prises sont
        # reparties alternativement a gauche et a droite, la correlation mesuree de la
        # queue etant nulle.
        self.niveau_er = float(niveau_er)
        self.er_g, self.er_d = None, None
        if reflexions and self.niveau_er > 0:
            n_er = max(int(fs * 0.09), 8)
            g, d = np.zeros(n_er), np.zeros(n_er)
            for k, (t_ms, amp_db) in enumerate(reflexions):
                i = int(round(t_ms * 1e-3 * fs))
                if 0 <= i < n_er:
                    (g if k % 2 == 0 else d)[i] += 10.0 ** (amp_db / 20.0)
            # Normalisation en energie : les amplitudes mesurees sont relatives au pic
            # des 60 premieres ms de l'IR d'origine, donc de l'ordre de l'unite, et il y
            # en a jusqu'a 43. Sans cette normalisation la section ecrase le tank meme au
            # plus petit dosage teste, et `niveau_er` ne veut plus rien dire.
            norme = np.sqrt(np.sum(g ** 2) + np.sum(d ** 2))
            if norme > 0:
                g, d = g / norme, d / norme
            self.er_g, self.er_d = g, d

        # Retard d'entree du TANK seul : c'est lui qui cree la marche, en decalant
        # l'arrivee de la partie diffuse apres le palier des reflexions.
        self.retard_tank = max(int(round(retard_tank_ms * 1e-3 * fs)), 1)

        # Dosage des prises PRECOCES, en debut de bras. Avec les seules prises de fin, le
        # signal doit traverser un bras entier avant d'atteindre la sortie : le moteur
        # mettait 65 ms a monter la ou l'original de `plate_b` est plein en 10 ms. Les
        # programmes courts n'ont d'ailleurs aucun palier (-2,7 dB des le depart) : ils
        # sont la tout de suite, et seule une prise precoce peut faire ca.
        self.prise_tot = float(prise_tot)

        # Le diffuseur decide de la RUGOSITE de la queue, et donc du cote « metallique ».
        # Deux reglages, pas un : le coefficient (combien chaque impulsion s'etale) et le
        # NOMBRE d'etages (combien de fois on recommence). Quatre etages a coefficient
        # minimal lissent encore trop — c'est ce qui rendait le moteur 21 a 39 % plus lisse
        # que la machine sur toutes les mesures de rugosite.
        self.n_diffuseurs = int(np.clip(n_diffuseurs, 0, 4))
        self.diff = [Allpass(_transpose(n, fs), min(diffusion * (c / 0.75), 0.92))
                     for n, c in zip(DIFFUSEUR_LONGUEURS[:self.n_diffuseurs],
                                     DIFFUSEUR_COEFFS[:self.n_diffuseurs])]

        # Ce sont CES coefficients-la qui decident du cote metallique, pas le diffuseur
        # d'entree : a zero etage de diffusion la rugosite plafonnait encore a 0,73 pour
        # une cible de 1,10. Le lissage vient du tank, ou chaque allpass repasse a chaque
        # tour de boucle — donc son effet s'accumule, contrairement au diffuseur qui ne
        # sert qu'une fois.
        # --- Les huit lignes du FDN, chacune un allpass module (interpolation allpass,
        # plate en module : une interpolation lineaire dans une boucle recirculante coute
        # jusqu'a 12 dB d'aigu dans la queue, mesure sur `gate`).
        marge = int(self.mod_profondeur) + 2
        # Nombre de lignes ACTIVES. C'est le levier le plus direct sur la densite modale :
        # huit lignes donnent un spectre dense et lisse, trois donnent des modes espaces et
        # un timbre sonnant. Contrairement a `taille` ou `mixage`, il garde toute sa marge
        # quel que soit l'accordage — ceux-la sont deja pousses aux bords par l'accordeur.
        self.n_lignes = int(np.clip(n_lignes, 2, N_FDN))
        self.lignes = [DelaiModule(_transpose(L, fs, taille), marge)
                       for L in FDN_LONGUEURS[:self.n_lignes]]
        # Signes alternes sur les prises de sortie : deux sorties decorrelees sans ajouter
        # de retard, la mesure donnant une correlation L/R nulle sur les 14 programmes.
        self.signes = [1.0 if (k // 2) % 2 == 0 else -1.0 for k in range(self.n_lignes)]
        self.mixage = float(np.clip(mixage, 0.0, 1.0))
        # Dosage des prises intermediaires : 8 lignes x 3 prises = 24 arrivees par tour
        # au lieu de 8, sans un octet de memoire supplementaire.
        # --- DIFFUSION en amont, sur un trajet DIRECT — pas sur les lignes recirculantes.
        #
        # Le PCM 70 distingue deux controles de densite (manuel, « Reverb Parameters ») :
        #   DIFFUSION  : « degree to which echo density increases over time. High settings
        #                 result in high INITIAL buildup [...] After the initial period,
        #                 density continues to rise at a preset rate. »
        #   DEFINITION : « affects the echo density buildup rate during the LATTER part of
        #                 the decay period. »
        #
        # Ce sont deux regimes distincts. Mes prises etaient posees sur les lignes du FDN,
        # donc elles debitaient a CHAQUE tour : la queue restait dense en permanence et le
        # temps de melange sortait 2 a 4 fois trop court sur les grands programmes, sans
        # qu'aucun reglage n'y change rien.
        #
        # Corrige : les prises deviennent un banc de retards en PARALLELE alimente par
        # l'entree diffusee (structure du decorrelateur de la FIG. 4 du brevet EMT). Elles
        # ne produisent donc que des arrivees precoces, et la densite tardive redevient
        # l'affaire de la seule matrice — c'est-a-dire DEFINITION.
        self.gain_prises = float(gain_prises)
        n_ff = 12
        fen = float(np.clip(fenetre_diff_ms, 5.0, 400.0)) * 1e-3 * fs
        self.ff_taps = [max(int(fen * ((k + 1) / n_ff) ** 1.3), 1) for k in range(n_ff)]
        self.ff_max = max(self.ff_taps) + 2
        self.ff_buf = np.zeros(self.ff_max)
        self.ff_i = 0
        # Pente en dB entre la premiere et la derniere prise. Positive = arrivees precoces
        # attenuees, donc enveloppe qui monte.
        n_p = 12
        self.poids_prises = [10.0 ** (-pente_prises * (n_p - 1 - k) / (n_p - 1) / 20.0)
                             for k in range(n_p)]

        # --- Injection retardee, une par ligne (brevet Jot US5491754, FIG. 2d, bloc
        # « FIRST ECHOS CONTROL »). L'entree n'arrive pas simultanement dans les huit
        # lignes : chaque ligne la recoit avec son propre retard t_i et son propre gain
        # b_i. Les premieres arrivees en sortie sont donc directement ces (t_i, b_i) —
        # c'est le motif de reflexions primaires, obtenu sans section separee.
        #
        # Contrainte portee sur la figure : |t_N - t_i| < min(tau_i). L'etalement doit
        # rester sous le plus court des retards de boucle, faute de quoi l'injection
        # empiete sur la queue au lieu de la preceder.
        tau_min = min(l.nominal for l in self.lignes)
        etal = float(np.clip(etalement, 0.0, 0.95)) * tau_min
        nl = self.n_lignes
        self.injections = [Delai(max(int(round(etal * k / max(nl - 1, 1))), 1))
                           for k in range(nl)]
        # --- SHAPE, au sens ou Lexicon l'emploie dans sa propre documentation
        # (manuel PCM Native Hall, p. 33) :
        #
        #   « Spread est une fenetre de temps pendant laquelle le signal est injecte dans
        #     la reverbe. Shape controle COMMENT le signal est injecte pendant cette
        #     fenetre. Une valeur basse : le son entre au DEBUT de la fenetre. Une valeur
        #     haute : l'essentiel entre a la FIN. Ensemble, les deux parametres creent
        #     l'enveloppe de la partie precoce de la reverbe. »
        #
        # `etalement` est donc Spread, et `forme` est Shape. La version precedente ne
        # savait qu'attenuer les injections tardives — toujours front-loaded — alors qu'un
        # hall demande exactement l'inverse : l'energie doit arriver EN FIN de fenetre pour
        # que l'enveloppe monte au lieu de partir pleine. C'est ce qui bloquait `hall_b`.
        f = float(np.clip(forme, 0.0, 1.0))
        self.b_inj = []
        for k in range(self.n_lignes):
            u = k / max(self.n_lignes - 1, 1)                       # 0 = injection precoce, 1 = tardive
            # Poids triangulaire centre sur `forme` : a 0 tout entre au debut, a 1 tout
            # entre a la fin, entre les deux la repartition glisse continument.
            self.b_inj.append(float(max(1.0 - abs(u - f) * 2.2, 0.03)))

        # Longueur moyenne d'une ligne : convertit un T60 en gain de boucle.
        self.boucle = float(np.mean([l.nominal for l in self.lignes]))
        self.ratio_hf = float(ratio_hf)
        self.cos_w_hf = float(np.cos(2.0 * np.pi * FREQ_AMORT_HZ / self.fs))
        self.ratio_bass = float(ratio_bass)
        # Plage du 224 : 100 Hz - 11 kHz.
        self.cross_bass_hz = float(np.clip(cross_bass_hz, 100.0, 11000.0))
        self.regler_t60(t60_s)

        self.lp_out = self._coef_lp(lp_sortie_hz)
        # Mur anti-image : Butterworth d'ordre 8, applique HORS de la boucle donc en une
        # passe vectorisee.
        #
        # La version precedente cascadait 4 passe-bas du 1er ordre a 16,9 kHz. Ce n'etait
        # pas un mur : quatre poles doux commencent a mordre des 5 kHz et le moteur
        # sortait 10 dB trop sombre a 14-16 kHz — exactement le manque de brillant et de
        # metallique entendu a l'ecoute. La mesure decrit une falaise verticale a 17 kHz
        # avec une reponse plate en dessous : il faut un filtre d'ordre eleve, pas une
        # cascade de pentes douces.
        # Anti-repliement (entree) et anti-image (sortie), tous deux a 15 kHz, chacun a la
        # frequence ou il travaille reellement.
        # L'anti-repliement doit rester sous Nyquist interne (15,6 kHz) mais sans manger
        # la bande utile : la machine passe 15 kHz a -3,8 dB seulement.
        self.aa_sos = _sig.butter(6, min(15200.0 / (self.fs_hote / 2), 0.985),
                                  btype="lowpass", output="sos")
        self.mur_sos = _sig.butter(ORDRE_MUR, min(mur_hz / (self.fs_hote / 2), 0.985),
                                   btype="lowpass", output="sos")
        self.sinc_fir = _correction_sinc(self.fs_hote, FS_INTERNE)
        # Voicing de sortie : FIR court derive du RESIDU mesure entre le moteur et la
        # machine, et non d'une topologie de filtre choisie a l'avance. Trois essais
        # successifs (mur plus haut, bloqueur d'ordre zero, correction sinc) ont chacun
        # deplace le residu sans le supprimer : il restait une bosse de +2 dB vers 10-12
        # kHz et un creux de -7 dB a 16 kHz. Inverser directement la courbe mesuree est
        # plus honnete que de continuer a deviner quelle cascade la produirait.
        self.correction_fir = (np.asarray(correction_fir, dtype=float)
                               if correction_fir is not None else None)
        self.hp_sortie_hz = float(hp_sortie_hz)

        # --- TILT : un seul bouton, de « corps » a « air ».
        #
        # Deux plateaux opposes autour d'un pivot : ce qu'on donne a l'aigu, on l'enleve au
        # grave. L'energie totale bouge donc peu, et l'oreille entend un basculement
        # d'assiette plutot qu'un changement de volume.
        #
        # ⚠️ A ne pas confondre avec la compensation de COLOR. La compensation est INTERNE
        # et invisible : elle garantit que COLOR ne deplace pas le timbre. TILT est une
        # commande artistique PAR-DESSUS. S'appuyer sur TILT pour rattraper COLOR
        # obligerait l'utilisateur a corriger a chaque mouvement du bouton, ce qui
        # reviendrait a livrer un bouton non fini.
        # --- STEREO : 100 % = l'image du LXP-1 telle que mesuree.
        #
        # Travaille en milieu/cote. En dessous de 100 % on reduit le cote vers le mono ;
        # au-dessus on l'amplifie.
        #
        # ⚠️ Ce n'est PAS de l'opposition de phase — en mono le cote s'annule et il reste
        # le milieu intact, donc rien ne s'effondre. Mais sur une reverbe dont les deux
        # sorties sont deja totalement decorrelees (correlation mesuree 0,00 a 0,02),
        # milieu et cote portent la meme energie : amplifier le cote fait perdre en mono
        # une part croissante du signal. La plage est bornee pour cette raison, et la
        # compatibilite mono est mesuree dans le banc plutot que supposee.
        self.stereo = float(np.clip(stereo, 0.0, 1.6))

        # --- ETAGE D'ENTREE.
        #
        # `None` = pas de preampli du tout, et c'est le DEFAUT : les 12 programmes ont ete
        # accordes sans lui, et l'introduire par defaut deplacerait la reference validee a
        # l'oreille. Le passer a une valeur en dB insere l'etage neutre calibre sur le
        # manuel (THD 0,07 % a +15 dBV, harmoniques impaires seulement) — voir preampli.py.
        self.preampli = None if preampli_db is None else Preampli(gain_db=preampli_db)

        self.tilt_db = float(np.clip(tilt, -12.0, 12.0))
        if abs(self.tilt_db) > 0.01:
            # Forme classique du tilt, du 1er ordre :
            #     H(s) = (1/G + s/w) / (1 + s/(G*w))
            # gain 1/G en continu, G a l'infini, bascule autour de w. Symetrique par
            # construction : ce qu'on ajoute d'un cote, on l'enleve exactement de l'autre.
            #
            # Ma premiere version cascadait deux plateaux ecrits a la main et sortait
            # dissymetrique — a +9 dB elle coupait 6,3 dB de grave pour ne monter que
            # 2,2 dB d'aigu. Une seule section bien posee vaut mieux que deux bricolees.
            G = 10.0 ** (self.tilt_db / 40.0)
            w = 2.0 * np.pi * float(np.clip(tilt_pivot_hz, 100.0, 4000.0))
            b, a = _sig.bilinear([1.0 / w, 1.0 / G], [1.0 / (G * w), 1.0], self.fs_hote)
            self.tilt_sos = _sig.tf2sos(b, a)
        else:
            self.tilt_sos = None

        self.pre_sos = _emphase(self.fs_hote, False)
        self.de_sos = _emphase(self.fs_hote, True)

    def _coef_lp(self, fc):
        """Coefficient d'un passe-bas du 1er ordre : y += k*(x-y)."""
        if fc >= self.fs * 0.49:
            return 1.0
        return float(1.0 - np.exp(-2.0 * np.pi * fc / self.fs))

    def regler_t60(self, t60_s):
        """Gain de boucle donnant la decroissance voulue.

        Dans un FDN a matrice unitaire, chaque tour d'une ligne subit g une fois et dure
        sa propre longueur. Avec `boucle` = longueur moyenne, perdre 60 dB en t60_s impose
        g = 10^(-3*boucle/(t60*fs)).
        """
        self.t60_s = float(t60_s)
        # --- Filtre absorbant par ligne, h_i(Z) du brevet Jot (US5491754, FIG. 2b).
        #
        # Deux choses a la fois, et c'est ce qui le rend juste :
        #  - un gain PAR LIGNE fonction de sa longueur. Un gain unique donne a chaque ligne
        #    la meme perte par TOUR, donc une perte par SECONDE d'autant plus faible que la
        #    ligne est longue : la queue finit gouvernee par la plus longue.
        #  - une pente HF PAR LIGNE, elle aussi proportionnelle a la longueur, pour que
        #    toutes les lignes decroissent au meme rythme a chaque frequence.
        #
        # Le parametre expose n'est plus une frequence de coupure a chercher, mais le
        # RAPPORT T60(8 kHz)/T60(500 Hz) — c'est-a-dire une cible mesuree directement sur
        # les IR du LXP-1, sans intermediaire.
        #
        # ⚠️ ET IL DOIT ETRE ATTEINT A 8 kHz. L'ecriture naive b = (1-rap)/(1+rap) pose le
        # rapport a NYQUIST, soit 15,6 kHz ici : a 8 kHz le premier ordre n'a fait qu'une
        # partie de sa descente. Mesure de l'ecart, a t60 = 2,2 s :
        #
        #     demande 0,50 -> obtenu 0,638      demande 0,70 -> obtenu 0,812
        #     demande 0,58 -> obtenu 0,714      demande 0,80 -> obtenu 0,883
        #
        # C'est l'origine de la derive constatee sur les douze programmes (0,87-0,91
        # obtenu pour 0,51-0,80 vise) : pas un accordage qui converge mal, une
        # correspondance fausse entre le parametre et la grandeur qui le mesure.
        r = float(np.clip(self.ratio_hf, 0.1, 2.0))
        t60_hf = max(t60_s * r, 1e-3)
        # « Bass Multiply : provides control over bass reverb time. » Exemple du manuel :
        # Decay a 2 s + Bass Multiply -> 4 s dans le grave. C'est un MULTIPLICATEUR du
        # temps de decroissance sous la coupure, pas une attenuation.
        rb = float(np.clip(self.ratio_bass, 0.25, 4.0))
        self.k_cross = self._coef_lp(self.cross_bass_hz)
        self.b_abs, self.gains, self.gain_bass = [], [], []
        for l in self.lignes:
            if t60_s <= 0:
                self.b_abs.append(0.0)
                self.gains.append(0.0)
                continue
            g_dc = min(float(10.0 ** (-3.0 * l.nominal / (t60_s * self.fs))), 0.9999)
            g_hf = min(float(10.0 ** (-3.0 * l.nominal / (t60_hf * self.fs))), 0.9999)
            rap = float(np.clip(g_hf / max(g_dc, 1e-12), 1e-6, 0.999))
            b = _pole_pour_rapport(rap, self.cos_w_hf)
            self.b_abs.append(float(np.clip(b, -0.95, 0.95)))
            self.gains.append(g_dc)
            # Gain supplementaire applique a la seule bande grave, pour que le T60 y soit
            # rb fois celui du medium.
            g_lf = min(float(10.0 ** (-3.0 * l.nominal / (max(t60_s * rb, 1e-3) * self.fs))),
                       0.9999)
            self.gain_bass.append(float(g_lf / max(g_dc, 1e-12) - 1.0))
        self.g = float(np.mean(self.gains)) if self.lignes else 0.0

    # ---------------------------------------------------------------- traitement

    def rendre(self, x):
        """Traite un signal mono a la frequence de l'hote, renvoie (gauche, droite).

        Chaine complete du LXP-1, dans l'ordre du manuel de service :

            preampli -> PRE-ACCENTUATION -> anti-repliement 15 kHz -> ADC 16 bits a
            31,25 kHz -> [tank] -> DAC 16 bits -> anti-image 15 kHz -> DE-ACCENTUATION

        Le reechantillonnage n'est pas une commodite : la machine travaille reellement a
        31,25 kHz, et c'est ce qui borne sa bande a 15,6 kHz par construction.
        """
        n_hote = len(x)
        if self.preampli is not None:
            x = self.preampli(x)
        if self.emphase:
            x = _sig.sosfilt(self.pre_sos, x)
        x = _sig.sosfilt(self.aa_sos, x)
        # 48000 -> 31250 = 125/192
        x = _sig.resample_poly(x, 125, 192)
        x = quantifier(x, self.bits)          # ADC

        if self.retard_tank > 1:
            xt = np.concatenate([np.zeros(self.retard_tank), x])[:len(x)]
        else:
            xt = x
        L, R = self._tank(xt)
        if self.er_g is not None:
            # Section feed-forward : une simple convolution, pas besoin de la boucle.
            L = L + self.niveau_er * _sig.fftconvolve(x, self.er_g)[:len(L)]
            R = R + self.niveau_er * _sig.fftconvolve(x, self.er_d)[:len(R)]

        L, R = quantifier(L, self.bits), quantifier(R, self.bits)      # DAC
        L = _sig.sosfilt(self.mur_sos, np.convolve(
            _bloqueur(L, len(L) * 192 // 125), self.sinc_fir, "same"))
        R = _sig.sosfilt(self.mur_sos, np.convolve(
            _bloqueur(R, len(R) * 192 // 125), self.sinc_fir, "same"))
        if abs(self.stereo - 1.0) > 1e-3:
            mid = (L + R) * 0.5
            cote = (L - R) * 0.5 * self.stereo
            L, R = mid + cote, mid - cote
        if self.tilt_sos is not None:
            L = _sig.sosfilt(self.tilt_sos, L)
            R = _sig.sosfilt(self.tilt_sos, R)
        if self.correction_fir is not None:
            L = np.convolve(L, self.correction_fir, "same")
            R = np.convolve(R, self.correction_fir, "same")
        if self.emphase:
            L = _sig.sosfilt(self.de_sos, L)
            R = _sig.sosfilt(self.de_sos, R)
        # L'aller-retour 48000 -> 31250 -> 48000 ne retombe pas toujours sur le compte
        # exact : on rend rigoureusement la longueur recue, sinon tout code appelant qui
        # melange l'entree et la sortie casse.
        return _ajuster(L, n_hote), _ajuster(R, n_hote)

    def _tank(self, x):
        """Reseau a retards boucles (FDN) a 8 lignes, matrice de Householder.

        La matrice unitaire de Householder melange les huit lignes en O(N) : chaque sortie
        recoit `y_i = v_i - (2/N) * somme(v)`. Unitaire donc tous les poles gardent le meme
        module, ce qui evite qu'un mode particulier survive aux autres et colore la queue.

        `mixage` interpole entre l'identite (aucun melange : huit lignes independantes,
        spectre tres creuse, timbre metallique) et Householder complet (melange maximal,
        queue lisse). C'est ce parametre-la, et non la longueur des lignes, qui porte
        desormais le temps de melange.
        """
        n = len(x)
        outL = np.empty(n)
        outR = np.empty(n)

        # --- Modulation ALEATOIRE, pas sinusoidale.
        #
        # « CHORUSING randomizes delay times and introduces modulation to make
        #   reverberation sound less metallic. »  (manuel PCM 70)
        # « For naturalness the output of each reverberation delay unit is randomly
        #   damped. »  (brevet EMT US4181820, resume)
        #
        # Les deux sources disent la meme chose : c'est du hasard filtre, pas un LFO. Un
        # sinus fait respirer toute la queue au meme rythme et laisse entendre sa periode ;
        # un bruit lent desaccorde les modes sans jamais se repeter.
        NL = self.n_lignes
        lfos = []
        rng = np.random.default_rng(20260907)
        tau = max(self.fs / max(self.mod_hz, 0.01) / 6.0, 2.0)
        a = float(np.exp(-1.0 / tau))
        for k in range(NL):
            b = rng.standard_normal(n)
            y = _sig.lfilter([1.0 - a], [1.0, -a], b)
            ec = np.std(y)
            y = y / ec if ec > 1e-12 else y
            lfos.append(np.clip(y, -2.5, 2.5) * self.mod_profondeur * 0.4
                        + self.mod_profondeur * 0.5)

        pre = self.predelai
        diff = self.diff
        lignes = self.lignes
        gains, b_abs, lp_out, tot = self.gains, self.b_abs, self.lp_out, self.prise_tot
        gp, wp = self.gain_prises, self.poids_prises
        ffb, ffn, ff_taps = self.ff_buf, self.ff_max, self.ff_taps
        ffi = self.ff_i
        k_tr, pdfb = self.k_treble, self.predelai_fb
        e_tr = fb_etat = 0.0
        k_cr, g_bass = self.k_cross, self.gain_bass
        e_bass = [0.0] * NL
        injections, b_inj = self.injections, self.b_inj
        inj = [0.0] * NL
        mix = self.mixage
        signes = self.signes

        etats = [0.0] * NL          # etats des filtres d'amortissement
        sorties = [0.0] * NL
        sL = sR = hL = hR = 0.0
        k_hp = self._coef_lp(self.hp_sortie_hz) if self.hp_sortie_hz > 0 else 0.0

        for i in range(n):
            # Treble -> Diffusion -> Predelay, avec contre-reaction autour des trois.
            u = x[i] + fb_etat * pdfb
            e_tr += k_tr * (u - e_tr)
            v = e_tr
            for ap in diff:
                v = ap.traiter(v)
            v = pre.pousser(v)
            fb_etat = v
            for k in range(NL):
                inj[k] = b_inj[k] * injections[k].pousser(v)

            # Melange : Householder attenue par `mix`.
            somme = 0.0
            for k in range(NL):
                somme += sorties[k]
            f_mix = mix * 2.0 / NL * somme
            for k in range(NL):
                e = (sorties[k] - f_mix) + inj[k]
                d = lignes[k].traiter(e, lfos[k][i])
                # h_i(Z) : passe-bas du 1er ordre a gain continu g_dc et pente propre.
                etats[k] = gains[k] * (1.0 - b_abs[k]) * d + b_abs[k] * etats[k]
                # Etage de grave : un plateau du 1er ordre qui ajoute g_bass fois la
                # partie basse. A ratio 1,0 le terme est nul et rien ne change.
                e_bass[k] += k_cr * (etats[k] - e_bass[k])
                sorties[k] = etats[k] + g_bass[k] * e_bass[k]

            gch = drt = 0.0
            for k in range(0, NL - 1, 2):
                gch += signes[k] * sorties[k]
                drt += signes[k + 1] * sorties[k + 1]
            # Banc de retards direct : arrivees precoces uniquement.
            ffb[ffi] = v
            ffi = (ffi + 1) % ffn
            for t in range(12):
                e_ff = ffb[(ffi - ff_taps[t]) % ffn] * wp[t]
                if t % 2:
                    gch += gp * e_ff
                else:
                    drt += gp * e_ff
            # Prise instantanee sur la sortie du diffuseur : c'est le seul point qui ne
            # traverse aucune ligne, donc le seul vraiment precoce.
            gch += tot * v
            drt -= tot * v

            sL += lp_out * (gch - sL)
            sR += lp_out * (drt - sR)
            if k_hp > 0.0:              # « Low Cut Filter », absent jusqu'ici
                hL += k_hp * (sL - hL)
                hR += k_hp * (sR - hR)
                outL[i] = sL - hL
                outR[i] = sR - hR
            else:
                outL[i] = sL
                outR[i] = sR

        return outL, outR

    def reponse_impulsionnelle(self, duree_s=4.0):
        x = np.zeros(int(self.fs_hote * duree_s))
        x[0] = 1.0
        return self.rendre(x)


def enveloppe_gate(h, fs, profil_db, pas_ms=5.0):
    """Applique un profil d'enveloppe mesure (liste [t_ms, dB]) a une queue.

    Gate et Inverse ne sont pas des reverbes courtes : ce sont des tanks normaux suivis
    d'un etage d'enveloppe. Leur R2 de decroissance (0,59 et 0,76 contre 0,99+ ailleurs)
    le prouve — la forme dans le temps ne suit pas une exponentielle, elle est imposee.
    """
    t = np.array([p[0] for p in profil_db]) * 1e-3
    a = 10.0 ** (np.array([p[1] for p in profil_db]) / 20.0)
    ti = np.arange(len(h)) / fs
    env = np.interp(ti, t, a, left=a[0], right=0.0)
    return h * env


if __name__ == "__main__":
    # Verification : le T60 obtenu doit correspondre au T60 demande. C'est le seul point
    # ou le moteur peut etre valide sans aucune cible du LXP-1.
    import time
    from analyse_ir import schroeder, temps_decroissance

    # Deux colonnes, et c'est le point du test : a taille figee a 1,0 un bras fait 181 ms,
    # donc un T60 court ne dispose pas d'assez de tours de boucle pour decroitre et sort
    # tres long. Avec la taille contrainte par le T60 vise (ce que fait `accorder.py`),
    # l'ecart rentre. Le premier chiffre n'est pas un bug, c'est la limite a connaitre.
    from accorder import taille_pour

    print(f"{'t60 demande':>12s} {'taille 1.0':>11s} {'taille liee':>12s}")
    for cible in (0.5, 1.0, 2.0, 3.0):
        ligne = f"{cible:10.2f} s "
        for taille in (1.0, taille_pour(cible)):
            m = Moteur(t60_s=cible, taille=taille, diffusion=0.75)
            L, R = m.reponse_impulsionnelle(max(cible * 1.6, 1.5))
            obtenu, _ = temps_decroissance(schroeder((L + R) / 2.0), FS)
            ligne += f" {obtenu:5.2f}s {obtenu/cible - 1:+6.1%}"
        print(ligne)
