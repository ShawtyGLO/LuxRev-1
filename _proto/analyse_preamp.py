#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LuxRev-1 — banc d'analyse de l'etage d'entree du LXP-1.

Analyse les 12 rendus des trois modeles NAM (`Lxp1 Input Gain 2 / 5 / 7`) pour en tirer les
cibles du preampli. Ces modeles sont des reseaux WaveNet, donc NON LINEAIRES : leur reponse
depend du niveau. On ne les reimplemente pas — on les mesure comme du materiel, ce qui est
aussi le seul moyen verifiable (l'ordre d'aplatissement des poids n'est pas documente).

Trois mesures, trois signaux :

  10_sweep_niveaux   -> reponse en frequence a 5 niveaux, par deconvolution de Farina.
                        Un sweep exponentiel replie ses harmoniques dans les temps NEGATIFS
                        de la deconvolution : on recupere la reponse LINEAIRE seule, meme
                        quand l'etage sature. C'est ce qui rend un seul rendu exploitable.
  20/21_paliers      -> niveau des harmoniques 2 a 5 palier par palier, a 1 kHz et 100 Hz.
                        Le rapport pairs/impairs dit la forme de la non-linearite.
  22_rampe_1k        -> courbe de transfert continue et compression.

Entree  : _proto/signaux/RENDER Sweep Préamp/*.wav
Sortie  : _proto/preampli/PREAMPLI.md, *.json, planches/*.png

Usage : python analyse_preamp.py
"""

import json
import re
from pathlib import Path

import numpy as np
from scipy.signal import fftconvolve

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from analyse_ir import lire_wav, db

PROTO = Path(__file__).parent
RENDUS = PROTO / "signaux" / "RENDER Sweep Préamp"
SORTIE = PROTO / "preampli"
PLANCHES = SORTIE / "planches"

FS = 48000
NIVEAUX_SWEEP = [-40, -24, -12, -6, 0]
NIVEAUX_PALIERS = list(range(-60, 1, 5))
T_SWEEP, F1, F2 = 8.0, 20.0, 20000.0
GRILLE = [50, 100, 200, 500, 1000, 2000, 4000, 8000, 12000, 16000, 20000]


def gain_du_nom(nom):
    """Les rendus sont nommes '... 2 (consolidated).wav' : le chiffre est la position."""
    m = re.search(r"\s(\d)\s*\(", nom)
    return int(m.group(1)) if m else None


def charger(chemin):
    """Rendu -> canal mono recale sur le repere de tete.

    Chaque signal source commence par une impulsion a -20 dBFS suivie de 0,5 s de silence.
    On la retrouve comme maximum des 300 premieres ms : la latence de l'hote est ainsi
    compensee sans supposer sa valeur.
    """
    x, fs = lire_wav(chemin)
    h = x[:, 0] if x.ndim > 1 else x
    i = int(np.argmax(np.abs(h[:int(fs * 0.3)])))
    return h[i:], fs, i


# --------------------------------------------------------------- sweeps (Farina)

def sweep_source(duree=T_SWEEP, fs=FS):
    t = np.arange(int(fs * duree)) / fs
    k = np.log(F2 / F1)
    return np.sin(2.0 * np.pi * F1 * duree / k * (np.exp(t * k / duree) - 1.0))


def filtre_inverse(duree=T_SWEEP, fs=FS):
    """Sweep retourne dans le temps, pondere en -6 dB/octave.

    La ponderation compense la duree croissante passee dans chaque octave par le sweep
    exponentiel ; sans elle la reponse deconvoluee serait inclinee.
    """
    x = sweep_source(duree, fs)
    k = np.log(F2 / F1)
    t = np.arange(len(x)) / fs
    return x[::-1] * np.exp(-t * k / duree)


def reponse_sweep(y, duree=T_SWEEP, fs=FS):
    """Deconvolution de Farina -> reponse en frequence lineaire, en dB.

    La reponse lineaire se trouve a la fin de la convolution ; les harmoniques la
    precedent. On fenetre donc autour du pic terminal pour ne garder que le lineaire.
    """
    inv = filtre_inverse(duree, fs)
    # fftconvolve et pas np.convolve : 450 k x 384 k echantillons en direct, c'est 1,7e11
    # operations et le banc ne rendait jamais la main.
    plein = fftconvolve(y, inv, "full")
    i = int(np.argmax(np.abs(plein)))
    a, b = max(i - int(fs * 0.002), 0), min(i + int(fs * 0.05), len(plein))
    h = plein[a:b] * np.hanning(b - a) ** 0.25
    n = 1 << 16
    H = np.abs(np.fft.rfft(h, n))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    m = (f >= 20) & (f <= 20000)
    return f[m], 20.0 * np.log10(np.maximum(H[m], 1e-30))


# --------------------------------------------------------------------- paliers

def analyser_paliers(y, freq, fs=FS, t0=0.5, duree=1.5, pas=2.0):
    """Niveau du fondamental et des harmoniques 2 a 5, palier par palier."""
    res = []
    for j, lvl in enumerate(NIVEAUX_PALIERS):
        d = int(fs * (t0 + j * pas + 0.25))
        f = d + int(fs * (duree - 0.5))
        if f > len(y):
            break
        seg = y[d:f] * np.hanning(f - d)
        n = 1 << (int(np.ceil(np.log2(len(seg)))) + 1)
        H = np.abs(np.fft.rfft(seg, n))
        fr = np.fft.rfftfreq(n, 1.0 / fs)
        pics = {}
        for rang in range(1, 6):
            fc = freq * rang
            if fc >= fs / 2:
                continue
            m = (fr > fc * 0.97) & (fr < fc * 1.03)
            pics[rang] = float(np.max(H[m])) if m.any() else 0.0
        f0 = max(pics.get(1, 0.0), 1e-30)
        harm = np.sqrt(sum(v ** 2 for r, v in pics.items() if r > 1))
        res.append({
            "entree_dbfs": lvl,
            "sortie_db": round(float(db(f0 * 2.0 / len(seg))), 2),
            "thd_pct": round(float(harm / f0 * 100.0), 3),
            "harmoniques_db": {r: round(float(db(v / f0)), 1) for r, v in pics.items() if r > 1},
        })
    return res


# ----------------------------------------------------------------------- rampe

def analyser_rampe(y, fs=FS, t0=0.5, duree=30.0, fen_ms=100.0):
    """Courbe de transfert continue : niveau d'entree connu -> niveau de sortie mesure."""
    d = int(fs * t0)
    seg = y[d:d + int(fs * duree)]
    n = int(fs * fen_ms / 1000.0)
    k = len(seg) // n
    r = np.sqrt(np.mean(seg[:k * n].reshape(k, n) ** 2, axis=1))
    entree = np.linspace(-60.0, 0.0, k) - 3.01     # RMS d'un sinus = crete - 3 dB
    return entree, db(r)


# ---------------------------------------------------------------------- rapport

def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    PLANCHES.mkdir(parents=True, exist_ok=True)
    if not RENDUS.exists():
        raise SystemExit(f"Dossier de rendus introuvable : {RENDUS}")

    # Recherche recursive : les rendus sont ranges par lot (BATCH 1, BATCH 2...). En cas
    # de doublon, le fichier le plus recent gagne — c'est toujours la derniere serie qui
    # fait foi.
    tousfic = sorted(RENDUS.rglob("*.wav"), key=lambda q: q.stat().st_mtime)
    fichiers = {}
    for p in tousfic:
        g = gain_du_nom(p.name)
        n = p.name.upper()
        genre = ("sweep" if "SWEEP" in n else "rampe" if "RAMPE" in n
                 else "palier100" if "100K" in n else "palier1k" if "1K" in n else None)
        if g and genre:
            fichiers[(g, genre)] = p

    gains = sorted({g for g, _ in fichiers})
    print(f"{len(fichiers)} rendus, gains {gains}\n")
    tout = {}

    for g in gains:
        r = {"gain": g}

        # -- reponse en frequence a 5 niveaux
        if (g, "sweep") in fichiers:
            y, fs, lat = charger(fichiers[(g, "sweep")])
            r["latence_ms"] = round(lat / fs * 1000, 3)
            rep = {}
            for j, lvl in enumerate(NIVEAUX_SWEEP):
                d = int(fs * (0.5 + j * (T_SWEEP + 2.0)))
                seg = y[d:d + int(fs * (T_SWEEP + 1.5))]
                if len(seg) < fs:
                    continue
                f, mag = reponse_sweep(seg, T_SWEEP, fs)
                ref = np.mean(mag[(f >= 500) & (f <= 2000)])
                rep[lvl] = {"absolu_db": round(float(ref), 2),
                            "courbe": [[int(fq), round(float(np.interp(fq, f, mag) - ref), 2)]
                                       for fq in GRILLE]}
            r["reponse_par_niveau"] = rep

        # -- harmoniques
        for genre, freq, cle in (("palier1k", 1000.0, "paliers_1k"),
                                 ("palier100", 100.0, "paliers_100hz")):
            if (g, genre) in fichiers:
                y, fs, _ = charger(fichiers[(g, genre)])
                r[cle] = analyser_paliers(y, freq, fs)

        # -- courbe de transfert
        if (g, "rampe") in fichiers:
            y, fs, _ = charger(fichiers[(g, "rampe")])
            e, s = analyser_rampe(y, fs)
            r["transfert"] = [[round(float(a), 1), round(float(b), 2)]
                              for a, b in zip(e[::5], s[::5])]

        tout[g] = r
        (SORTIE / f"gain{g}.json").write_text(
            json.dumps(r, indent=2, ensure_ascii=False), encoding="utf-8")

        p1k = r.get("paliers_1k", [])
        crete = max((d["thd_pct"] for d in p1k), default=0.0)
        print(f"  gain {g} : latence {r.get('latence_ms', 0):.2f} ms, "
              f"THD max {crete:.1f} % a 1 kHz, {len(p1k)} paliers")

    planches(tout)
    rapport(tout)
    print(f"\n-> {SORTIE / 'PREAMPLI.md'}")


def planches(tout):
    gains = sorted(tout)
    fig, ax = plt.subplots(2, 2, figsize=(13, 8))
    fig.suptitle("LuxRev-1 — etage d'entree du LXP-1, mesure sur les captures NAM")

    for g in gains:
        rep = tout[g].get("reponse_par_niveau", {})
        for lvl in (-40, 0):
            if lvl in rep:
                c = np.array(rep[lvl]["courbe"], dtype=float)
                ax[0, 0].semilogx(c[:, 0], c[:, 1], lw=1.2,
                                  ls="-" if lvl == -40 else "--",
                                  label=f"g{g} {lvl} dBFS")
    ax[0, 0].set(title="Reponse en frequence (— bas niveau, -- pleine echelle)",
                 xlabel="Hz", ylabel="dB", xlim=(20, 20000), ylim=(-24, 12))
    ax[0, 0].grid(alpha=.3, which="both"); ax[0, 0].legend(fontsize=7, ncol=2)

    for g in gains:
        t = np.array(tout[g].get("transfert", []), dtype=float)
        if len(t):
            ax[0, 1].plot(t[:, 0], t[:, 1] - t[:, 1].max(), lw=1.3, label=f"gain {g}")
    ax[0, 1].plot([-63, -3], [-60, 0], ls=":", c="grey", lw=.8, label="lineaire")
    ax[0, 1].set(title="Courbe de transfert (rampe 1 kHz)",
                 xlabel="entree dBFS RMS", ylabel="sortie dB (normalisee)")
    ax[0, 1].grid(alpha=.3); ax[0, 1].legend(fontsize=8)

    for i, (cle, titre) in enumerate((("paliers_1k", "THD a 1 kHz"),
                                      ("paliers_100hz", "THD a 100 Hz"))):
        for g in gains:
            d = tout[g].get(cle, [])
            if d:
                ax[1, i].plot([x["entree_dbfs"] for x in d],
                              [x["thd_pct"] for x in d], marker=".", lw=1.2,
                              label=f"gain {g}")
        ax[1, i].set(title=titre, xlabel="entree dBFS", ylabel="THD %")
        ax[1, i].grid(alpha=.3); ax[1, i].legend(fontsize=8)

    fig.tight_layout()
    fig.savefig(PLANCHES / "preampli.png", dpi=100)
    plt.close(fig)


def rapport(tout):
    gains = sorted(tout)
    L = ["# LuxRev-1 — cibles de l'etage d'entree du LXP-1", "",
         "Genere par `analyse_preamp.py` depuis 12 rendus des trois captures NAM.",
         "Les modeles ne sont pas reimplementes : ils sont mesures comme du materiel.", "",
         "## Reponse en frequence a bas niveau (-40 dBFS, dB rel. 500-2000 Hz)", "",
         "C'est le comportement LINEAIRE de l'etage — ce qu'un IIR doit reproduire.", "",
         "| Gain | " + " | ".join(f"{f} Hz" for f in GRILLE) + " |",
         "|---" * (len(GRILLE) + 1) + "|"]
    for g in gains:
        rep = tout[g].get("reponse_par_niveau", {})
        c = dict(rep.get(-40, {}).get("courbe", []))
        L.append(f"| {g} | " + " | ".join(
            f"{c[f]:+.1f}" if f in c else "—" for f in GRILLE) + " |")

    L += ["", "## Deformation de la reponse sous le niveau", "",
          "Ecart entre la reponse a 0 dBFS et celle a -40 dBFS. Un ecart non nul signifie",
          "que la saturation ne se contente pas d'ajouter des harmoniques : elle change",
          "aussi la couleur. C'est ce qu'un simple waveshaper statique ne reproduirait pas.", "",
          "| Gain | " + " | ".join(f"{f} Hz" for f in GRILLE) + " |",
          "|---" * (len(GRILLE) + 1) + "|"]
    for g in gains:
        rep = tout[g].get("reponse_par_niveau", {})
        bas, haut = dict(rep.get(-40, {}).get("courbe", [])), dict(rep.get(0, {}).get("courbe", []))
        L.append(f"| {g} | " + " | ".join(
            f"{haut[f] - bas[f]:+.1f}" if f in bas and f in haut else "—"
            for f in GRILLE) + " |")

    L += ["", "## Gain et compression", "",
          "| Gain | Niveau abs. a -40 dBFS | a 0 dBFS | Compression |",
          "|---|--:|--:|--:|"]
    for g in gains:
        rep = tout[g].get("reponse_par_niveau", {})
        b = rep.get(-40, {}).get("absolu_db")
        h = rep.get(0, {}).get("absolu_db")
        comp = f"{(h - b) - 40:+.1f} dB" if b is not None and h is not None else "—"
        L.append(f"| {g} | {b if b is None else f'{b:.1f} dB'} | "
                 f"{h if h is None else f'{h:.1f} dB'} | {comp} |")
    L += ["", "*Compression* = ecart au comportement lineaire. 0 dB = lineaire ; une valeur",
          "negative = l'etage cede quand on le pousse.", ""]

    for cle, titre, freq in (("paliers_1k", "1 kHz", 1000), ("paliers_100hz", "100 Hz", 100)):
        L += [f"## Harmoniques a {titre}", "",
              "| Gain | Entree | THD | H2 | H3 | H4 | H5 |", "|---|--:|--:|--:|--:|--:|--:|"]
        for g in gains:
            for d in tout[g].get(cle, []):
                if d["entree_dbfs"] % 15 and d["entree_dbfs"] != 0:
                    continue
                h = d["harmoniques_db"]
                L.append(f"| {g} | {d['entree_dbfs']} dBFS | {d['thd_pct']:.2f} % | " +
                         " | ".join(f"{h.get(str(r), h.get(r, float('nan'))):.0f} dB"
                                    for r in (2, 3, 4, 5)) + " |")
        L.append("")

    L += ["## Lecture pour la conception", "",
          "- La **reponse a bas niveau** est le comportement lineaire : elle se reproduit",
          "  exactement par un IIR ajuste, sans approximation.",
          "- La **deformation sous le niveau** decide de l'architecture : si elle est nulle,",
          "  un waveshaper statique precede d'un filtre suffit. Si elle ne l'est pas, il faut",
          "  une non-linearite dependante de la frequence (filtre dans la boucle de",
          "  contre-reaction, ou plusieurs bandes).",
          "- Le **rapport H2/H3** dit la symetrie : H2 dominant = non-linearite asymetrique",
          "  (un seul cote de l'onde ecrete) ; H3 dominant = symetrique.",
          "- Les **trois positions de gain** donnent la trajectoire de la non-linearite, ce",
          "  qu'une seule capture ne pourrait pas donner : c'est ce qui permet un bouton",
          "  Drive continu plutot que trois presets.", ""]
    (SORTIE / "PREAMPLI.md").write_text("\n".join(L), encoding="utf-8")


if __name__ == "__main__":
    main()
