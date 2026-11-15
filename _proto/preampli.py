"""Etage d'entree du LXP-1 — version neutre, calibree sur le manuel de service.

DECISION (utilisateur, 2026-09-07) : « on part sur une version custom neutre avec le bon
thd ». Les trois captures NAM sont abandonnees comme source — quatre lots de rendus ont
donne des fichiers identiques au bit pres sur les trois positions de gain, et la seule
mesure exploitable donnait 36 % de THD contre 0,07 % au manuel, soit un facteur 500. Elles
ne decrivent pas le LXP-1.

Ce module ne modelise donc pas une capture : il construit un etage conforme a ce que le
manuel de service DIT de l'etage reel.

CE QUE LE MANUEL DONNE
  - « Aucun transformateur d'entree » : 1/4" asymetrique, 50 kOhm, pot double R52,
    ampli-op uPC4570.
  - THD < 0,07 %, plage d'entree -20 a +15 dBV.
  - Plage dynamique 85 dB.

CE QUE J'EN DEDUIS, ET POURQUOI
  1. La distorsion est SYMETRIQUE, donc harmoniques IMPAIRES. C'est de la contre-reaction
     d'ampli-op bipolaire : pas de transformateur, pas de tube, pas d'etage asymetrique —
     rien qui produise du rang 2. Modeliser une chaleur de rang 2 ici serait inventer un
     composant que le manuel dit absent.
  2. Le point de calibration est le HAUT de la plage. « THD < 0,07 %, plage -20 a +15 dBV »
     se lit comme une seule ligne : sur toute la plage, la THD reste sous 0,07 %. On cale
     donc 0,07 % a +15 dBV, ce qui rend l'etage plus propre partout en dessous.

⚠️ Consequence assumee : a niveau normal cet etage est INAUDIBLE, et c'est correct. Le
grain du LXP-1 ne vient pas de son preampli, il vient de la quantification 16 bits et de
la pre-accentuation dans le tank. Un preampli qui « s'entend » serait une faute de clone.
"""

import numpy as np

FS = 48000
DBV_MAX = 15.0        # haut de la plage d'entree, manuel de service
THD_CIBLE = 0.0007    # 0,07 %
PLAGE_DYN_DB = 85.0


def dbv_vers_amp(dbv):
    """dBV -> amplitude crete d'une sinusoide (0 dBV = 1 V efficace)."""
    return 10.0 ** (dbv / 20.0) * np.sqrt(2.0)


def thd(y, fs=FS, f0=1000.0, n_harm=10):
    """THD d'un signal sinusoidal, rapport des harmoniques 2..N sur le fondamental."""
    n = len(y)
    fen = np.hanning(n)
    sp = np.abs(np.fft.rfft(y * fen))
    frq = np.fft.rfftfreq(n, 1.0 / fs)

    def raie(f):
        k = int(round(f * n / fs))
        return float(np.max(sp[max(k - 3, 0):k + 4]))

    fond = raie(f0)
    if fond <= 0:
        return 0.0
    h = np.sqrt(sum(raie(f0 * m) ** 2 for m in range(2, n_harm + 1)
                    if f0 * m < fs / 2 - 100))
    return float(h / fond)


class Preampli:
    """Etage d'entree neutre : gain d'entree, ecretage doux symetrique, plafond.

    `gain_db` est le pot R52 — il n'existe pas de reglage de « drive » separe sur la
    machine, le seul moyen d'entrer dans l'ecretage est de monter le niveau, comme sur
    l'appareil.
    """

    def __init__(self, gain_db=0.0, k=None, fs=FS):
        self.fs = float(fs)
        self.gain = 10.0 ** (float(gain_db) / 20.0)
        self.k = float(k) if k is not None else K_CALIBRE

    def __call__(self, x):
        x = np.asarray(x, dtype=float) * self.gain
        # Ecretage doux symetrique. tanh est le profil d'un etage a contre-reaction :
        # lineaire au centre, compression progressive, jamais de coin franc.
        return np.tanh(self.k * x) / self.k


def calibrer(dbv=DBV_MAX, cible=THD_CIBLE, fs=FS, f0=1000.0):
    """Cherche le k qui donne `cible` de THD a `dbv`. Bissection : THD croit avec k."""
    t = np.arange(int(fs * 0.5)) / fs
    x = dbv_vers_amp(dbv) * np.sin(2.0 * np.pi * f0 * t)
    bas, haut = 1e-6, 10.0
    for _ in range(80):
        m = 0.5 * (bas + haut)
        if thd(np.tanh(m * x) / m, fs, f0) < cible:
            bas = m
        else:
            haut = m
    return 0.5 * (bas + haut)


K_CALIBRE = 0.011536683636704733   # calibre par calibrer(), fige pour la reproductibilite


if __name__ == "__main__":
    k = calibrer()
    print(f"k calibre : {k!r}   (fige dans le module : {K_CALIBRE!r})\n")
    print("THD de l'etage sur la plage d'entree du manuel :\n")
    print(f"  {'niveau':>10s} {'THD':>9s} {'rang 2':>9s} {'rang 3':>9s}")
    t = np.arange(int(FS * 0.5)) / FS
    pre = Preampli(k=k)
    for dbv in (-20, -10, 0, 5, 10, 15, 20, 24):
        x = dbv_vers_amp(dbv) * np.sin(2.0 * np.pi * 1000.0 * t)
        y = pre(x)
        n = len(y); sp = np.abs(np.fft.rfft(y * np.hanning(n)))

        def raie(f):
            i = int(round(f * n / FS))
            return float(np.max(sp[i - 3:i + 4]))

        f1 = raie(1000.0)
        d = thd(y) * 100.0
        marq = "   <- calibration" if dbv == DBV_MAX else ""
        print(f"  {dbv:+7.0f} dBV {d:8.4f}% {raie(2000.0)/f1*100:8.4f}% "
              f"{raie(3000.0)/f1*100:8.4f}%{marq}")
    print("\n  Le rang 2 est nul : l'ecretage est symetrique, comme un ampli-op.")
