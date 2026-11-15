#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Signaux de test pour ACCORDER la reverbe — pas pour mesurer le preampli.

⚠️ POURQUOI CE FICHIER EXISTE, SEPARE DE gen_signaux.py. Celui-la sert le banc du
preampli : il prefixe chaque signal d'un marqueur et decrit des segments, une
convention que le banc d'analyse attend. Les signaux d'accordage n'ont pas besoin
de tout ca, et les melanger rendrait les deux illisibles.

Liste tiree de la note « Accorder une reverbe — notes de terrain » (Thomas
Guillory, Arturia, 2026-09-26). On en mesurait deux — bruit stationnaire et
impulsion — et il en manquait la moitie :

    carres courts et aigus   resonances et modulation
    impulsion                distribution des lignes, en temps et en stereo
    caisse claire            resonances et reflexions precoces
    boucle de batterie       spectre complet, transitoires, repere stereo
    voix                     impitoyable ; les sifflantes jugent l'amortissement

⛔ CE QUE CE SCRIPT NE PEUT PAS FAIRE. Une caisse claire ACOUSTIQUE et une VOIX
ne se synthetisent pas de facon credible, et c'est justement leur credibilite qui
en fait de bons juges. Celles-ci doivent venir de la bibliothequedu DAW. Ce que
l'on fabrique ici est le complement synthetique :

    carre_aigu     rafales de carre, courtes, de 1 a 4 kHz
    frappe         une frappe percussive large bande, SUBSTITUT de caisse claire
                   — utile pour exposer un plancher de bruit et des resonances,
                   pas pour juger un timbre

Usage : python _proto/gen_signaux_reverbe.py
"""

import numpy as np
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).parent))
from gen_signaux import FS, ecrire_wav          # noqa: E402

SORTIE = Path(__file__).parent / "signaux_reverbe"


def fondu(x, ms=2.0):
    n = max(int(FS * ms * 1e-3), 1)
    f = np.linspace(0.0, 1.0, n)
    y = x.copy()
    y[:n] *= f
    y[-n:] *= f[::-1]
    return y


def carre_aigu(duree=6.0):
    """Rafales de carre, courtes et aigues. Le signal qui revele les resonances.

    Quatre hauteurs, chacune en rafales de 120 ms separees de silence : le silence
    est essentiel, c'est lui qui laisse entendre la queue seule.
    """
    x = np.zeros(int(FS * duree))
    t_rafale, t_vide = 0.12, 0.38
    i = int(FS * 0.05)
    for f in (1000.0, 1600.0, 2500.0, 4000.0):
        for _ in range(3):
            n = int(FS * t_rafale)
            if i + n >= len(x):
                break
            t = np.arange(n) / FS
            x[i:i + n] = fondu(0.35 * np.sign(np.sin(2 * np.pi * f * t)), 3.0)
            i += n + int(FS * t_vide)
    return x


def frappe(duree=6.0):
    """Frappe percussive large bande — SUBSTITUT de caisse claire, pas une.

    Bruit filtre en cloche plus deux modes accordes, enveloppe rapide. Elle fait
    deux choses utiles : exposer le plancher de bruit apres l'extinction, et
    reveler les resonances metalliques du tank. Elle ne juge PAS un timbre —
    pour ca il faut une vraie prise.
    """
    x = np.zeros(int(FS * duree))
    rng = np.random.default_rng(1987)
    for k, depart in enumerate((0.05, 1.55, 3.05, 4.55)):
        i = int(FS * depart)
        n = int(FS * 0.22)
        if i + n >= len(x):
            break
        t = np.arange(n) / FS
        env = np.exp(-t * 38.0)
        b = rng.standard_normal(n) * env
        # deux modes, comme la peau et le timbre d'une caisse
        corps = (np.sin(2 * np.pi * 185.0 * t) * np.exp(-t * 26.0) * 0.6
                 + np.sin(2 * np.pi * 330.0 * t) * np.exp(-t * 32.0) * 0.3)
        s = 0.55 * b + 0.45 * corps
        s *= 0.8 / (np.max(np.abs(s)) + 1e-12)
        x[i:i + n] = fondu(s, 1.0)
    return x


def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    for nom, x in (("carre_aigu.wav", carre_aigu()), ("frappe.wav", frappe())):
        ecrire_wav(SORTIE / nom, x.astype(np.float32))
        print(f"  {nom:18s} {len(x)/FS:5.2f} s   crete {np.max(np.abs(x)):.3f}")
    print(f"\n  ecrit : {SORTIE}")
    print("  ATTENTION : une vraie caisse claire et une voix restent a "
          "fournir depuis la bibliotheque du DAW.")


if __name__ == "__main__":
    main()
