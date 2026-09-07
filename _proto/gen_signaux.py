#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LuxRev-1 — generateur des signaux de mesure.

Fabrique les fichiers a faire passer dans TONE3000 (modeles NAM "Lxp1 Input Gain 2/5/7")
pour caracteriser l'etage d'entree du LXP-1, qui est NON LINEAIRE : sa reponse depend du
niveau, donc un seul sweep ne suffit pas.

Les 14 IR de reverbe n'ont PAS besoin de ces signaux : elles se telechargent directement
depuis tone3000 et se posent dans _proto/ir_source/. On garde quand meme un impulse et un
sweep de reference pour mesurer n'importe quelle chaine lineaire au besoin.

Sortie : _proto/signaux/*.wav (48 kHz, mono, float32) + manifest.json
Dependances : numpy uniquement (l'ecriture WAV est faite a la main).

Usage :  python gen_signaux.py
"""

import json
import struct
from pathlib import Path

import numpy as np

FS = 48000                      # les modeles NAM tournent a 48 kHz : on s'aligne dessus
DOSSIER = Path(__file__).parent / "signaux"

# Niveaux de sweep, en dBFS. -40 donne le comportement quasi lineaire (= la vraie reponse
# en frequence), 0 donne le comportement sature. La trajectoire entre les deux est
# exactement ce qu'on cherche a reproduire.
NIVEAUX_SWEEP = [-40, -24, -12, -6, 0]

# Paliers pour la courbe de transfert, en dBFS.
NIVEAUX_PALIERS = list(range(-60, 1, 5))

MARQUEUR_AMP = 0.1              # -20 dBFS : reperable en correlation, sans cogner l'entree
MARQUEUR_SILENCE = 0.5          # secondes de silence apres le marqueur


# --------------------------------------------------------------------------- WAV

def ecrire_wav(chemin, x, fs=FS):
    """WAV RIFF float32, mono ou multicanal. Ecrit a la main pour ne dependre que de numpy.

    ⚠️ Le nombre de canaux est deduit de la forme de `x`, il n'est PAS code en dur.
    Un tableau [n] donne du mono, un tableau [n, canaux] donne du multicanal entrelace.
    Cette fonction a longtemps declare 1 canal quoi qu'on lui passe : un rendu stereo
    ressortait alors en mono de longueur double, avec L et R alternes — inecoutable, et
    difficile a reperer parce que le fichier s'ouvre et sonne « presque » juste.
    """
    x = np.asarray(x)
    if x.ndim == 1:
        canaux = 1
    elif x.ndim == 2:
        canaux = x.shape[1]
    else:
        raise ValueError(f"ecrire_wav : forme {x.shape} non geree")
    d = np.ascontiguousarray(x, dtype="<f4").tobytes()
    bloc = 4 * canaux
    en_tete = b"RIFF" + struct.pack("<I", 36 + len(d)) + b"WAVE"
    en_tete += b"fmt " + struct.pack("<IHHIIHH", 16, 3, canaux, fs, fs * bloc, bloc, 32)
    en_tete += b"data" + struct.pack("<I", len(d))
    chemin.write_bytes(en_tete + d)


def db(x):
    return 10.0 ** (x / 20.0)


def marqueur():
    """Impulsion unique + silence, en tete de chaque fichier.

    Sert a realigner le rendu sur la source : la latence du plugin hote est inconnue et
    varie d'un rendu a l'autre. Sans ce repere, decouper les segments a la main est faux.
    """
    m = np.zeros(int(FS * MARQUEUR_SILENCE) + 1, dtype=np.float64)
    m[0] = MARQUEUR_AMP
    return m


def fondu(x, ms=5.0):
    """Raised-cosine aux deux bouts : evite les clics, qui polluent le spectre."""
    n = int(FS * ms / 1000.0)
    if n * 2 >= len(x):
        return x
    f = 0.5 * (1.0 - np.cos(np.pi * np.arange(n) / n))
    y = x.copy()
    y[:n] *= f
    y[-n:] *= f[::-1]
    return y


# ------------------------------------------------------------------- generateurs

def sweep_log(duree, f1=20.0, f2=20000.0):
    """Sweep exponentiel de Farina.

    Choisi plutot qu'un sweep lineaire parce que la distorsion harmonique se replie dans
    les temps NEGATIFS de la deconvolution : on peut separer la reponse lineaire des
    harmoniques dans le meme rendu. C'est ce qui rend un seul sweep exploitable sur un
    etage sature.
    """
    t = np.arange(int(FS * duree)) / FS
    k = np.log(f2 / f1)
    return np.sin(2.0 * np.pi * f1 * duree / k * (np.exp(t * k / duree) - 1.0))


def sinus(freq, duree, amp):
    t = np.arange(int(FS * duree)) / FS
    return amp * np.sin(2.0 * np.pi * freq * t)


def silence(duree):
    return np.zeros(int(FS * duree))


def bruit_rose(duree, graine=1987):
    """Bruit rose par mise en forme spectrale (1/sqrt(f)). Graine fixe = reproductible."""
    n = int(FS * duree)
    rng = np.random.default_rng(graine)
    X = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1.0 / FS)
    g = np.ones_like(f)
    g[1:] = 1.0 / np.sqrt(f[1:])
    g[0] = 0.0
    y = np.fft.irfft(X * g, n)
    return y / np.max(np.abs(y))


# ------------------------------------------------------------------------ fichiers

def main():
    DOSSIER.mkdir(parents=True, exist_ok=True)
    manifest = {"fs": FS, "marqueur_amp": MARQUEUR_AMP,
                "marqueur_silence_s": MARQUEUR_SILENCE, "fichiers": {}}

    def poser(nom, blocs, segments, note):
        """blocs : tableaux concatenes apres le marqueur.
        segments : description exploitable telle quelle par le banc d'analyse."""
        x = np.concatenate([marqueur()] + blocs)
        crete = float(np.max(np.abs(x)))
        ecrire_wav(DOSSIER / nom, x)
        manifest["fichiers"][nom] = {"note": note, "duree_s": round(len(x) / FS, 3),
                                     "crete": round(crete, 4), "segments": segments}
        print("  {:28s} {:6.2f} s   crete {:.3f}".format(nom, len(x) / FS, crete))

    print("Ecriture dans {}\n".format(DOSSIER))

    # -- 01 : impulsion seule ------------------------------------------------
    # Sur une chaine LINEAIRE, le rendu de ce fichier EST la reponse impulsionnelle,
    # sans aucune deconvolution. Sert de recours et d'etalon de latence.
    poser("01_impulse.wav",
          [np.array([1.0]), silence(5.0)],
          [{"t0_s": MARQUEUR_SILENCE, "type": "impulse", "amp_dbfs": 0.0}],
          "Impulsion pleine echelle + 5 s de queue. Chaine lineaire uniquement.")

    # -- 02 : sweep de reference --------------------------------------------
    T = 8.0
    poser("02_sweep_ref.wav",
          [fondu(sweep_log(T)) * db(-6), silence(3.0)],
          [{"t0_s": MARQUEUR_SILENCE, "type": "sweep", "duree_s": T,
            "f1": 20.0, "f2": 20000.0, "amp_dbfs": -6.0}],
          "Sweep log 20-20k, 8 s, -6 dBFS. Reference lineaire.")

    # -- 10 : sweeps a 5 niveaux (LE fichier important) ----------------------
    # Un rendu de ce fichier par modele NAM. Les 5 sweeps donnent la reponse en frequence
    # a chaque niveau : leur ecart EST la non-linearite en fonction du niveau.
    blocs, segs, t = [], [], MARQUEUR_SILENCE
    for lvl in NIVEAUX_SWEEP:
        blocs += [fondu(sweep_log(T)) * db(lvl), silence(2.0)]
        segs.append({"t0_s": round(t, 4), "type": "sweep", "duree_s": T,
                     "f1": 20.0, "f2": 20000.0, "amp_dbfs": float(lvl)})
        t += T + 2.0
    poser("10_sweep_niveaux.wav", blocs, segs,
          "5 sweeps log a -40/-24/-12/-6/0 dBFS. Le fichier cle pour les NAM.")

    # -- 20/21 : paliers de sinus -------------------------------------------
    # Niveau constant par palier => FFT propre => rangs harmoniques mesurables.
    # 1 kHz et 100 Hz parce qu'un etage d'entree ne sature pas pareil en bas et en haut
    # (transfo, condensateurs de liaison, contre-reaction dependante de la frequence).
    for freq, nom in ((1000.0, "20_paliers_1k.wav"), (100.0, "21_paliers_100hz.wav")):
        blocs, segs, t = [], [], MARQUEUR_SILENCE
        for lvl in NIVEAUX_PALIERS:
            blocs += [fondu(sinus(freq, 1.5, db(lvl)), 20.0), silence(0.5)]
            segs.append({"t0_s": round(t, 4), "type": "palier", "freq": freq,
                         "duree_s": 1.5, "amp_dbfs": float(lvl)})
            t += 2.0
        poser(nom, blocs, segs,
              "Sinus {:.0f} Hz par paliers de 5 dB, -60 -> 0 dBFS. "
              "Profil harmonique par niveau.".format(freq))

    # -- 22 : rampe continue -------------------------------------------------
    # Balayage lent du niveau : donne la courbe de transfert en continu et revele une
    # eventuelle compression (le niveau de sortie qui cesse de suivre l'entree).
    duree_rampe = 30.0
    n = int(FS * duree_rampe)
    env = db(np.linspace(-60.0, 0.0, n))
    poser("22_rampe_1k.wav",
          [fondu(sinus(1000.0, duree_rampe, 1.0) * env, 20.0), silence(1.0)],
          [{"t0_s": MARQUEUR_SILENCE, "type": "rampe", "freq": 1000.0,
            "duree_s": duree_rampe, "dbfs_debut": -60.0, "dbfs_fin": 0.0}],
          "Sinus 1 kHz, niveau de -60 a 0 dBFS en 30 s. Courbe de transfert continue.")

    # -- 30 : bruit rose -----------------------------------------------------
    # Pas pour la mesure : pour l'oreille. Un materiau neutre et repetable pour comparer
    # a l'aveugle le moteur maison et la reference.
    blocs, segs, t = [], [], MARQUEUR_SILENCE
    for lvl in (-24, -12):
        blocs += [fondu(bruit_rose(1.0) * db(lvl), 20.0), silence(3.0)]
        segs.append({"t0_s": round(t, 4), "type": "bruit_rose",
                     "duree_s": 1.0, "amp_dbfs": float(lvl)})
        t += 4.0
    poser("30_bruit_rose.wav", blocs, segs,
          "Salves de bruit rose (graine fixe). Pour l'A/B a l'oreille, pas la mesure.")

    (DOSSIER / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    print("\n  manifest.json                {} fichiers decrits".format(
        len(manifest["fichiers"])))
    print("\nRendus attendus : 3 modeles NAM x (10, 20, 21, 22) = 12 fichiers")
    print("a deposer dans _proto/rendus/ sous la forme  <nam>__<signal>.wav")
    print("ex : gain7__10_sweep_niveaux.wav")


if __name__ == "__main__":
    main()
