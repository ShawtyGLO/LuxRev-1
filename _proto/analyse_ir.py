#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LuxRev-1 — banc d'analyse des 14 IR du Lexicon LXP-1.

Extrait, pour chaque programme, la fiche de cibles que le moteur algorithmique devra
atteindre. Aucune IR n'est embarquee dans le plugin : ce script les lit, en tire des
nombres, et ce sont ces nombres qui pilotent la conception (licence T3K : usage autorise,
redistribution du fichier interdite).

Mesures produites, et pourquoi chacune :

  pre-delai            temps avant la premiere energie -> parametre Pre-Delay
  T30 / EDT par octave decroissance par bande -> Decay + amortissement HF ("Treble Decay")
  bande passante       coin HF a -3 et -10 dB -> le plafond de 1987, coeur du grain
  premieres reflexions positions et amplitudes des pics < 60 ms -> longueurs des lignes
  coupure absolue      le mur au-dessus duquel il n'y a plus rien -> filtre anti-image
  densite d'echos      metrique Abel & Huang -> temps de melange, LA mesure du grain
  correlation L/R      largeur stereo a reproduire
  rugosite spectrale   dispersion du spectre de queue -> lisse ou sonnant
  enveloppe            profil en dB dans le temps -> indispensable pour Gate et Inverse

Entree  : Impulse Responce LXP-1/Reverb IR/*.wav   (4 canaux true-stereo, 48 kHz, float32)
Sortie  : _proto/mesures/*.json, CIBLES.md, planches/*.png
Deps    : numpy, scipy, matplotlib

Usage : python analyse_ir.py
"""

import json
import struct
from pathlib import Path

import numpy as np
from scipy import signal

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RACINE = Path(__file__).resolve().parent.parent
DOSSIER_IR = RACINE / "Impulse Responce LXP-1" / "Reverb IR"
SORTIE = Path(__file__).parent / "mesures"
PLANCHES = SORTIE / "planches"

# Bandes d'octave analysees. On monte a 16 k pour voir ou le LXP-1 s'arrete vraiment.
BANDES = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]


# ------------------------------------------------------------------ lecture WAV

def lire_wav(chemin):
    """Lecteur RIFF minimal -> (donnees [n, canaux], fs). Gere PCM 16/24/32 et float32."""
    b = chemin.read_bytes()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise ValueError(f"{chemin.name} : pas un WAV RIFF")
    off, fmt, data = 12, None, None
    while off + 8 <= len(b):
        cid = b[off:off + 4]
        taille = struct.unpack("<I", b[off + 4:off + 8])[0]
        corps = b[off + 8:off + 8 + taille]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", corps[:16])
        elif cid == b"data":
            data = corps
        off += 8 + taille + (taille & 1)
    if fmt is None or data is None:
        raise ValueError(f"{chemin.name} : chunk fmt ou data manquant")
    tag, ch, fs, _, _, bits = fmt
    if tag == 3 and bits == 32:
        x = np.frombuffer(data, dtype="<f4").astype(np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(data, dtype="<i2").astype(np.float64) / 32768.0
    elif tag == 1 and bits == 24:
        raw = np.frombuffer(data, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16)
        v = np.where(v & 0x800000, v - 0x1000000, v)
        x = v.astype(np.float64) / 8388608.0
    elif tag == 1 and bits == 32:
        x = np.frombuffer(data, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"{chemin.name} : format non gere (tag={tag}, bits={bits})")
    return x.reshape(-1, ch), fs


# ------------------------------------------------------------------- primitives

def db(x, plancher=-120.0):
    return np.maximum(20.0 * np.log10(np.maximum(np.abs(x), 1e-300)), plancher)


def rms_glissant(h, fs, pas_ms=100.0):
    n = max(int(fs * pas_ms / 1000.0), 1)
    k = len(h) // n
    return np.sqrt(np.mean(h[:k * n].reshape(k, n) ** 2, axis=1))


def plancher_bruit(h, fs):
    """RMS minimal sur des fenetres de 100 ms.

    Pas la fin du fichier : sur ces IR la queue descend encore a 9,5 s, donc mesurer le
    plancher sur les dernieres 10 % le contaminait avec de la reverbe.
    """
    r = rms_glissant(h, fs)
    return float(r.min()) if len(r) else 0.0


def temps_a_moins_60db(h, fs):
    """Instant ou l'enveloppe RMS passe sous -60 dB de son maximum, et n'y remonte plus.

    Duree « audible » comparable d'un programme a l'autre, contrairement a une duree
    calculee par rapport au plancher de bruit, qui depend de la capture.
    """
    r = rms_glissant(h, fs, 5.0)
    if not len(r) or r.max() <= 0:
        return 0.0
    g = db(r / r.max())
    au_dessus = np.flatnonzero(g > -60.0)
    return float((au_dessus[-1] + 1) * 0.005) if len(au_dessus) else 0.0


def linearite_decroissance(edc):
    """R2 d'un ajustement lineaire de l'EDC entre -5 et -60 dB.

    Diagnostic de capture, pas de son : une IR reelle s'ecarte de la droite en arrivant
    sur son plancher de bruit. Un R2 quasi parfait sur 55 dB signale une queue debruitee
    ou prolongee synthetiquement par l'outil de capture — auquel cas la fin de la queue
    n'est plus une mesure et ne doit pas servir de cible.
    """
    i0 = int(np.argmax(edc <= -5.0))
    i1 = int(np.argmax(edc <= -60.0))
    if i1 <= i0 + 100:
        return None
    y = edc[i0:i1]
    x = np.arange(len(y))
    p = np.polyfit(x, y, 1)
    resid = y - np.polyval(p, x)
    var = np.var(y)
    return round(float(1.0 - np.var(resid) / var), 4) if var > 0 else None


def onset(h, seuil_db=-40.0):
    """Premier echantillon depassant seuil_db sous la crete : le pre-delai."""
    seuil = np.max(np.abs(h)) * 10.0 ** (seuil_db / 20.0)
    idx = np.flatnonzero(np.abs(h) >= seuil)
    return int(idx[0]) if len(idx) else 0


def schroeder(h):
    """Courbe de decroissance d'energie : integrale inverse de h^2, en dB, normalisee.

    Preferee a l'enveloppe brute parce qu'elle est monotone : une droite s'y ajuste,
    alors qu'une enveloppe instantanee est trop bruitee pour un T60 fiable.
    """
    e = np.cumsum(h[::-1] ** 2)[::-1]
    if e[0] <= 0:
        return np.full(len(h), -120.0)
    return 10.0 * np.log10(np.maximum(e / e[0], 1e-30))


def schroeder_multi(canaux):
    """EDC calculee en sommant les ENERGIES des canaux, jamais leurs formes d'onde.

    Les deux sorties d'une reverbe stereo sont decorrelees par construction, et sur ces
    captures elles sont meme en partie en opposition de phase : les sommer en mono
    annulait 2,4 a 3,0 dB d'energie precoce, ce qui fabriquait une montee de reverbe qui
    n'existe pas. Le moteur accorde sur cette mesure reproduisait fidelement l'artefact.
    """
    e = np.zeros(len(canaux[0]))
    for c in canaux:
        e += np.cumsum(c[::-1] ** 2)[::-1]
    if e[0] <= 0:
        return np.full(len(e), -120.0)
    return 10.0 * np.log10(np.maximum(e / e[0], 1e-30))


def spectre_multi(canaux, fs, n_pts=240):
    """Spectre moyenne en PUISSANCE sur les canaux — meme raison que ci-dessus."""
    n = 1 << int(np.ceil(np.log2(len(canaux[0]))))
    P = np.zeros(n // 2 + 1)
    for c in canaux:
        P += np.abs(np.fft.rfft(c, n)) ** 2
    H = np.sqrt(P / len(canaux))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    fc = np.geomspace(20.0, min(20000.0, fs / 2 * 0.99), n_pts)
    mag = np.empty(n_pts)
    for i, c in enumerate(fc):
        m = (f >= c / 2 ** (1 / 6)) & (f <= c * 2 ** (1 / 6))
        mag[i] = np.sqrt(np.mean(H[m] ** 2)) if m.any() else np.nan
    ref = np.nanmean(mag[(fc >= 500) & (fc <= 2000)])
    return fc, 20.0 * np.log10(np.maximum(mag / ref, 1e-12))


def _montee(t_ms, env_db, marge=6.0):
    """Instant ou l'enveloppe depasse durablement le palier des premieres reflexions.

    C'est la « marche » : le moment ou la queue diffuse arrive par-dessus les reflexions.
    Reglee sur ce nombre, l'entree du tank cesse de produire une rampe continue.
    """
    n = max(int(60 / (t_ms[1] - t_ms[0])) if len(t_ms) > 1 else 12, 2)
    seuil = float(np.median(env_db[:n])) + marge
    au = np.flatnonzero(env_db > seuil)
    return float(t_ms[au[0]]) if len(au) else float(t_ms[min(n, len(t_ms) - 1)])


def enveloppe_multi(canaux, fs, pas_ms=5.0):
    """Enveloppe RMS en dB, energie sommee sur les canaux."""
    n = int(fs * pas_ms / 1000.0)
    k = len(canaux[0]) // n
    e = np.zeros(k)
    for c in canaux:
        e += np.mean(c[:k * n].reshape(k, n) ** 2, axis=1)
    r = np.sqrt(e)
    return np.arange(k) * pas_ms, db(r / max(r.max(), 1e-30))


def temps_decroissance(edc, fs):
    """T30 extrapole a 60 dB (ajustement -5 -> -35 dB).

    Repli sur T20 (-5 -> -25) puis EDT (0 -> -10) quand la plage utile manque, ce qui
    arrive sur les programmes courts. Retourne (valeur_s, methode).
    """
    for haut, bas, nom, fact in ((-5.0, -35.0, "T30", 2.0),
                                 (-5.0, -25.0, "T20", 3.0),
                                 (0.0, -10.0, "EDT", 6.0)):
        i0 = np.argmax(edc <= haut)
        i1 = np.argmax(edc <= bas)
        if i1 > i0 + 10:
            t = np.arange(i0, i1) / fs
            pente = np.polyfit(t, edc[i0:i1], 1)[0]
            if pente < -1e-6:
                # (haut - bas) dB parcourus a `pente` dB/s, puis extrapolation a 60 dB.
                return float(abs((haut - bas) / pente) * fact), nom
    return float("nan"), "insuffisant"


def bande(h, fs, fc, ordre=4):
    """Filtre d'octave a phase nulle (sosfiltfilt) : ne deplace pas la decroissance."""
    f1, f2 = fc / np.sqrt(2.0), fc * np.sqrt(2.0)
    nyq = fs / 2.0
    if f2 >= nyq * 0.99:
        sos = signal.butter(ordre, f1 / nyq, btype="highpass", output="sos")
    else:
        sos = signal.butter(ordre, [f1 / nyq, f2 / nyq], btype="bandpass", output="sos")
    return signal.sosfiltfilt(sos, h)


def spectre_lisse(h, fs, n_pts=240):
    """Magnitude moyennee en tiers d'octave, 20 Hz - 20 kHz, referencee a 500-2000 Hz."""
    n = 1 << int(np.ceil(np.log2(len(h))))
    H = np.abs(np.fft.rfft(h, n))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    fc = np.geomspace(20.0, min(20000.0, fs / 2 * 0.99), n_pts)
    mag = np.empty(n_pts)
    for i, c in enumerate(fc):
        m = (f >= c / 2 ** (1 / 6)) & (f <= c * 2 ** (1 / 6))
        mag[i] = np.sqrt(np.mean(H[m] ** 2)) if m.any() else np.nan
    ref = np.nanmean(mag[(fc >= 500) & (fc <= 2000)])
    return fc, 20.0 * np.log10(np.maximum(mag / ref, 1e-12))


def coupure_absolue(h, fs, seuil_rel=0.1):
    """Frequence au-dela de laquelle il ne reste plus rien (< -20 dB de la bande 1-4 kHz).

    Distincte du coin a -3 dB : le LXP-1 a une pente douce dans le medium-aigu PUIS un mur
    net, qui est celui de son filtre anti-image. C'est ce mur qui date la machine, et il
    est remarquablement constant d'un programme a l'autre.
    """
    n = 1 << 18
    seg = h[:int(fs * 1.0)]
    H = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    ref = np.sqrt(np.mean(H[(f > 1000) & (f < 4000)] ** 2))
    lisse = np.convolve(H, np.ones(400) / 400, "same")
    au = np.flatnonzero((lisse > ref * seuil_rel) & (f > 2000))
    return float(f[au[-1]]) if len(au) else 0.0


def coin_hf(fc, mag_db, chute):
    """Derniere frequence au-dessus de 1 kHz ou la magnitude passe sous -chute dB."""
    m = fc >= 1000.0
    f, g = fc[m], mag_db[m]
    sous = np.flatnonzero(g <= -chute)
    return float(f[sous[0]]) if len(sous) else float(f[-1])


def premieres_reflexions(h, fs, duree=0.06, seuil_db=-20.0):
    """Pics de l'enveloppe dans les 60 premieres ms, au-dessus de -20 dB.

    Ce motif est la signature des longueurs de lignes a retard de la machine : c'est ce
    qu'on reglera en premier dans le moteur, avant meme le temps de decroissance.

    Fenetre volontairement courte et seuil haut : au-dela, sur un programme dense, « les
    reflexions » ne sont plus des evenements discrets mais la queue elle-meme, et un
    detecteur de pics y renvoie autant de resultats qu'on lui en demande — ce qui ne
    decrit plus rien.
    """
    seg = np.abs(h[:int(fs * duree)])
    if not len(seg) or seg.max() <= 0:
        return []
    env = signal.savgol_filter(seg, 31, 2) if len(seg) > 31 else seg
    pics, _ = signal.find_peaks(env, height=env.max() * 10 ** (seuil_db / 20.0),
                                distance=int(fs * 0.001))
    return [{"t_ms": round(p / fs * 1000.0, 3),
             "amp_db": round(float(db(env[p] / env.max())), 2)} for p in pics]


def taux_accumulation(h, fs, duree=0.05, seuil_db=-35.0):
    """Nombre de pics par 10 ms dans les 50 premieres ms.

    Mesure la vitesse a laquelle la machine construit sa densite d'echos, la ou le temps
    de melange ne donne qu'un instant final. Faible = attaque clairsemee, on entend les
    reflexions une par une ; eleve = ca se remplit tout de suite.
    """
    seg = np.abs(h[:int(fs * duree)])
    if not len(seg) or seg.max() <= 0:
        return None
    env = signal.savgol_filter(seg, 31, 2) if len(seg) > 31 else seg
    pics, _ = signal.find_peaks(env, height=env.max() * 10 ** (seuil_db / 20.0),
                                distance=int(fs * 0.0005))
    return round(len(pics) / (duree * 100.0), 1)


def densite_echos(h, fs, fenetre_ms=20.0, pas_ms=2.0):
    """Densite d'echos normalisee (Abel & Huang, 2006).

    Fraction d'echantillons de la fenetre depassant son ecart-type, divisee par la valeur
    attendue pour un bruit gaussien (erfc(1/sqrt2) = 0.3173). Vaut ~1 quand la reverbe est
    devenue statistiquement lisse. Le temps pour y arriver = temps de melange : c'est la
    difference mesurable entre une machine de 1987 et une reverbe moderne dense.

    Renvoie aussi le niveau RMS de chaque fenetre : la metrique n'a de sens que la ou il
    y a du signal, et les fenetres tombees dans le plancher de bruit donnent des valeurs
    aberrantes (> 2) qu'il faut ecarter.
    """
    n = int(fs * fenetre_ms / 1000.0)
    pas = max(int(fs * pas_ms / 1000.0), 1)
    ref = 0.3173
    t, d, niv = [], [], []
    for i in range(0, max(len(h) - n, 0), pas):
        w = h[i:i + n]
        s = np.std(w)
        if s <= 0:
            continue
        t.append((i + n / 2) / fs * 1000.0)
        d.append(float(np.mean(np.abs(w) > s) / ref))
        niv.append(s)
    niv = np.array(niv)
    niv_db = db(niv / niv.max()) if len(niv) and niv.max() > 0 else niv
    return np.array(t), np.array(d), niv_db


def temps_melange(t_ms, dens, cible=0.9, fenetre_min_ms=300.0):
    """Instant ou la densite d'echos atteint la cible en montant.

    La recherche demarre au MINIMUM de la courbe dans les premieres centaines de ms, pas
    a t=0 : les toutes premieres fenetres chevauchent l'attaque, ou le signal n'est ni
    sparse ni gaussien et ou la metrique remonte artificiellement au-dessus de 1. Sans ce
    garde-fou le detecteur declenchait sur la premiere fenetre et annoncait 10 ms partout.
    """
    if len(dens) < 20:
        return float("nan")
    # Lissage sur ~20 ms avant le seuillage. La courbe brute est dentelee, et un
    # franchissement de seuil sur une courbe dentelee saute d'un facteur 4 pour une
    # variation de parametre negligeable : la mesure devient inexploitable pour accorder
    # quoi que ce soit. On cherche la tendance, pas la fluctuation fenetre a fenetre.
    k = 11
    noyau = np.ones(k) / k
    lisse = np.convolve(dens, noyau, "same")
    lisse[:k] = dens[:k]
    lisse[-k:] = dens[-k:]

    fin_debut = int(np.searchsorted(t_ms, fenetre_min_ms)) or len(lisse)
    i_min = int(np.argmin(lisse[:fin_debut]))
    for i in range(i_min, len(lisse) - 5):
        if lisse[i] >= cible and np.all(lisse[i:i + 5] >= cible * 0.95):
            return float(t_ms[i])
    return float("nan")


def rugosite_spectrale(h, fs, t0_ms=100.0):
    """Ecart-type, en dB, du spectre de la queue lisse au 1/12 d'octave (200 Hz - 5 kHz).

    Remplace un comptage de pics spectraux, qui ne mesurait que la resolution de la FFT :
    avec 9 s de queue les bins font 0,1 Hz et « les pics » se comptent par centaines quel
    que soit le programme. L'ecart-type d'un spectre lisse est stable et se lit
    directement : faible = queue dense et lisse ; eleve = queue clairsemee, timbre
    sonnant et metallique.
    """
    i0 = int(fs * t0_ms / 1000.0)
    q = h[i0:]
    if len(q) < 8192:
        return None
    n = 1 << int(np.ceil(np.log2(len(q))))
    H = np.abs(np.fft.rfft(q, n))
    f = np.fft.rfftfreq(n, 1.0 / fs)
    fc = np.geomspace(200.0, 5000.0, 160)
    mag = np.empty(len(fc))
    for i, c in enumerate(fc):
        m = (f >= c / 2 ** (1 / 24)) & (f <= c * 2 ** (1 / 24))
        mag[i] = np.sqrt(np.mean(H[m] ** 2)) if m.any() else np.nan
    g = 20.0 * np.log10(np.maximum(mag, 1e-30))
    g = g[np.isfinite(g)]
    # On retire la pente generale : c'est la rugosite qui interesse, pas le tilt, deja
    # mesure par ailleurs.
    x = np.arange(len(g))
    g = g - np.polyval(np.polyfit(x, g, 1), x)
    return round(float(np.std(g)), 2)


def enveloppe_db(h, fs, pas_ms=5.0):
    """Enveloppe RMS en dB. Seul moyen de lire correctement Gate et Inverse, dont la
    forme dans le temps est justement ce qui les definit."""
    n = int(fs * pas_ms / 1000.0)
    k = len(h) // n
    r = np.sqrt(np.mean(h[:k * n].reshape(k, n) ** 2, axis=1))
    return np.arange(k) * pas_ms, db(r / max(r.max(), 1e-30))


# ---------------------------------------------------------------------- analyse

def analyser(chemin):
    x, fs = lire_wav(chemin)
    n, nch = x.shape
    nom = chemin.stem

    # Matrice true-stereo, ordre etabli par correlation croisee : ch0 = L->L,
    # ch1 = L->R, ch2 = R->R, ch3 = R->L. La machine est quasi symetrique (ch0 et ch2
    # correles a 0,96, residu 11 dB dessous), donc la paire (ch0, ch1) — la reponse
    # stereo a une entree gauche — porte toute l'information utile.
    if nch >= 4:
        gauche, droite = x[:, 0], x[:, 1]
    elif nch == 2:
        gauche, droite = x[:, 0], x[:, 1]
    else:
        gauche = droite = x[:, 0]
    canaux = [gauche, droite]
    # Signal de reference pour les mesures qui portent sur la FORME d'onde (onset,
    # premieres reflexions, densite d'echos) : un canal reel, pas une somme.
    h = gauche

    crete = float(np.max(np.abs(h)))
    bruit = plancher_bruit(h, fs)
    plage_db = round(float(db(crete) - db(bruit)), 1)
    i_onset = onset(h)

    # Toutes les mesures temporelles partent de l'onset, pas du debut du fichier :
    # sinon le pre-delai fausse la pente de decroissance.
    hq = h[i_onset:]

    canaux_q = [c[i_onset:] for c in canaux]
    edc = schroeder_multi(canaux_q)
    t60, methode = temps_decroissance(edc, fs)

    bandes = {}
    for fc in BANDES:
        if fc > fs / 2 * 0.9:
            continue
        cb = [bande(c, fs, fc) for c in canaux_q]
        tb, mb = temps_decroissance(schroeder_multi(cb), fs)
        bandes[str(fc)] = {"t60_s": None if np.isnan(tb) else round(tb, 3),
                           "methode": mb,
                           "energie_db": round(float(db(np.sqrt(
                               np.mean(np.concatenate(cb) ** 2)))), 1)}

    fc_sp, mag = spectre_multi(canaux_q, fs)
    t_dens, dens, niv_dens = densite_echos(hq, fs)
    t_env, env = enveloppe_multi(canaux_q, fs)

    # Correlation L/R sur la queue seule : les premieres reflexions sont correlees par
    # construction, elles masqueraient la vraie largeur de la queue.
    i_q = int(fs * 0.1)
    if len(gauche) > i_q + 1000:
        g, d_ = gauche[i_q:], droite[i_q:]
        denom = np.sqrt(np.sum(g ** 2) * np.sum(d_ ** 2))
        iacc = float(np.sum(g * d_) / denom) if denom > 0 else float("nan")
    else:
        iacc = float("nan")

    # La densite d'echos ne vaut que la ou il y a du signal : au-dela de -55 dB sous la
    # crete, les fenetres tombent dans le plancher et renvoient des valeurs > 2.
    utile = (niv_dens > -55.0) & (niv_dens < -1.0)
    dens_fin = (round(float(np.median(dens[utile][-40:])), 2)
                if utile.sum() > 40 else None)

    res = {
        "nom": nom,
        "fs": fs, "canaux": nch, "duree_fichier_s": round(n / fs, 3),
        "crete": round(crete, 5),
        "plage_dynamique_db": plage_db,
        "predelai_ms": round(i_onset / fs * 1000.0, 2),
        "duree_60db_s": round(temps_a_moins_60db(hq, fs), 3),
        "t60_global_s": None if np.isnan(t60) else round(t60, 3),
        "t60_methode": methode,
        "linearite_decroissance_r2": linearite_decroissance(edc),
        "t60_par_bande": bandes,
        "amortissement_hf": None,
        "coin_hf_3db": round(coin_hf(fc_sp, mag, 3.0), 0),
        "coin_hf_10db": round(coin_hf(fc_sp, mag, 10.0), 0),
        "coupure_absolue_hz": round(coupure_absolue(hq, fs), 0),
        "tilt_lf_hf_db": round(float(np.nanmean(mag[(fc_sp >= 100) & (fc_sp <= 300)])
                                    - np.nanmean(mag[(fc_sp >= 4000) & (fc_sp <= 8000)])), 1),
        "temps_melange_ms": None,
        "densite_finale": dens_fin,
        "rugosite_spectrale_db": rugosite_spectrale(hq, fs),
        "correlation_lr": None if np.isnan(iacc) else round(iacc, 3),
        "energie_canaux_db": [round(float(db(np.sqrt(np.mean(x[:, c] ** 2)))), 1)
                              for c in range(nch)],
        # Spectre echantillonne sur une grille, pour servir de cible directe a
        # l'accordage du filtre de sortie. Le coin a -3 dB ne suffit pas : l'amortissement
        # dans la boucle assombrit aussi le spectre, donc le filtre de sortie doit etre
        # ajuste sur la courbe entiere, pas pose sur un point.
        "spectre_db": [[int(f), round(float(np.interp(f, fc_sp, mag)), 2)]
                       for f in (250, 500, 1000, 2000, 3000, 4000, 6000, 8000,
                                 10000, 12000, 14000, 16000, 17000, 18000)],
        # Spectres SEPARES du debut et de la queue. Une seule courbe globale ne suffit
        # pas : sur `gate` le debut est juste a 1 dB pres pendant que la queue est a
        # -13 dB a 16 kHz, et la moyenne des deux tombait juste. L'oreille, elle, les
        # entend separement — « on entend bien les high » ET « trop de mids ».
        "spectre_debut_db": [[int(f), round(float(np.interp(
            f, *spectre_multi([c[:int(fs * 0.08)] for c in canaux_q], fs))), 2)]
            for f in (250, 500, 1000, 2000, 4000, 6000, 8000, 12000, 16000)],
        "spectre_queue_db": [[int(f), round(float(np.interp(
            f, *spectre_multi([c[int(fs * 0.15):] for c in canaux_q], fs))), 2)]
            for f in (250, 500, 1000, 2000, 4000, 6000, 8000, 12000, 16000)],
        "crete_ms": round(float(np.argmax(
            np.sqrt(np.mean(canaux_q[0][:int(fs * 0.5) // 240 * 240].reshape(-1, 240) ** 2,
                            axis=1))) * 240 / fs * 1000), 1),
        # Forme de la montee, lue directement sur l'enveloppe. Deux nombres suffisent a
        # decrire ce que l'oreille entend comme « pas de montee » : le niveau du PALIER de
        # reflexions primaires, et l'instant ou la partie diffuse arrive par-dessus.
        "plateau_db": round(float(np.median(env[:max(int(60 / 5), 2)])), 1),
        "montee_ms": round(float(_montee(t_env, env)), 1),
        "taux_accumulation_pics_10ms": taux_accumulation(hq, fs),
        "premieres_reflexions": premieres_reflexions(hq, fs),
        "enveloppe_db": [[round(float(a), 1), round(float(b), 1)]
                         for a, b in zip(t_env, env)][:600],
    }

    tm = temps_melange(t_dens, dens)
    res["temps_melange_ms"] = None if np.isnan(tm) else round(tm, 1)

    # Amortissement HF : rapport T60(8k)/T60(500). C'est exactement ce que le bouton
    # "Treble Decay" de Lexicon controle.
    b8, b5 = bandes.get("8000", {}).get("t60_s"), bandes.get("500", {}).get("t60_s")
    if b8 and b5:
        res["amortissement_hf"] = round(b8 / b5, 3)

    planche(res, fc_sp, mag, t_dens, dens, edc, fs, t_env, env)
    return res


# ---------------------------------------------------------------------- planches

def planche(r, fc, mag, t_dens, dens, edc, fs, t_env, env):
    PLANCHES.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(2, 2, figsize=(13, 8))
    fig.suptitle(f"{r['nom']}   —   T60 {r['t60_global_s']} s ({r['t60_methode']})   "
                 f"pre-delai {r['predelai_ms']} ms   coin HF {r['coin_hf_3db']:.0f} Hz",
                 fontsize=12)

    t = np.arange(len(edc)) / fs
    ax[0, 0].plot(t, edc, lw=1.0)
    ax[0, 0].axhline(-60, ls=":", c="r", lw=0.8)
    ax[0, 0].set(title="Decroissance (Schroeder)", xlabel="s", ylabel="dB",
                 ylim=(-80, 2), xlim=(0, min(t[-1], 6)))
    ax[0, 0].grid(alpha=.3)

    ax[0, 1].semilogx(fc, mag, lw=1.2)
    for chute, c in ((3, "orange"), (10, "r")):
        ax[0, 1].axhline(-chute, ls=":", c=c, lw=.8)
    ax[0, 1].set(title="Spectre (1/3 oct., ref. 500-2k)", xlabel="Hz", ylabel="dB",
                 ylim=(-45, 15), xlim=(20, 20000))
    ax[0, 1].grid(alpha=.3, which="both")

    pr = r["premieres_reflexions"]
    if pr:
        ax[1, 0].stem([p["t_ms"] for p in pr], [p["amp_db"] for p in pr],
                      basefmt=" ", markerfmt=".")
    ax[1, 0].set(title=f"Premieres reflexions ({len(pr)} pics > -20 dB, "
                       f"{r['taux_accumulation_pics_10ms']} pics/10ms)",
                 xlabel="ms", ylabel="dB", xlim=(0, 60))
    ax[1, 0].grid(alpha=.3)

    ax[1, 1].plot(t_dens, dens, lw=1.0, label="densite d'echos")
    ax[1, 1].axhline(1.0, ls=":", c="g", lw=.8)
    if r["temps_melange_ms"]:
        ax[1, 1].axvline(r["temps_melange_ms"], ls="--", c="g", lw=.8,
                         label=f"melange {r['temps_melange_ms']:.0f} ms")
    a2 = ax[1, 1].twinx()
    a2.plot(t_env, env, lw=.9, c="grey", alpha=.7)
    a2.set_ylabel("enveloppe dB", color="grey")
    a2.set_ylim(-70, 3)
    ax[1, 1].set(title="Densite d'echos + enveloppe", xlabel="ms", ylim=(0, 1.4),
                 xlim=(0, min(600, t_dens[-1] if len(t_dens) else 600)))
    ax[1, 1].legend(fontsize=8, loc="lower right")
    ax[1, 1].grid(alpha=.3)

    fig.tight_layout()
    fig.savefig(PLANCHES / f"{r['nom']}.png", dpi=100)
    plt.close(fig)


# -------------------------------------------------------------------- rapport

def famille(nom):
    for cle, lib in (("hall", "Hall"), ("plate", "Plate"), ("inverse", "Inverse"),
                     ("gate", "Gate"), ("delay", "Delay"), ("small", "Room court"),
                     ("medium", "Room moyen"), ("large", "Room long")):
        if cle in nom:
            return lib
    return "?"


def rapport(tous):
    L = ["# LuxRev-1 — cibles mesurees sur les 14 IR du LXP-1", "",
         "Genere par `analyse_ir.py`. Chaque ligne est un programme du LXP-1 capture a ses",
         "reglages par defaut. Ces nombres sont les cibles que le moteur doit atteindre ;",
         "les IR elles-memes ne quittent pas `_proto/`.", "",
         "## Tableau de synthese", "",
         "| Programme | Famille | Pre-dél. | T60 | Durée −60dB | Coin HF −3dB | Coupure | Amort. HF | Mélange | Dens. fin. | Rugosité | Corr. L/R |",
         "|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|"]
    for r in tous:
        f = lambda v, u="", d=2: "—" if v is None else f"{v:.{d}f}{u}"
        L.append(
            f"| `{r['nom']}` | {famille(r['nom'])} | {r['predelai_ms']:.1f} ms | "
            f"{f(r['t60_global_s'], ' s')} | {r['duree_60db_s']:.2f} s | "
            f"{r['coin_hf_3db']:.0f} Hz | {r['coupure_absolue_hz']:.0f} Hz | "
            f"{f(r['amortissement_hf'], '', 2)} | "
            f"{f(r['temps_melange_ms'], ' ms', 0)} | {f(r['densite_finale'], '', 2)} | "
            f"{f(r['rugosite_spectrale_db'], ' dB', 1)} | {f(r['correlation_lr'], '', 2)} |")

    L += ["", "## T60 par bande d'octave (secondes)", "",
          "L'evolution du T60 avec la frequence est ce que le bouton *Treble Decay* de",
          "Lexicon controle. Une colonne 8k nettement plus courte que la colonne 500 =",
          "amortissement HF marque dans le tank.", "",
          "| Programme | " + " | ".join(f"{b} Hz" for b in BANDES) + " |",
          "|---" * (len(BANDES) + 1) + "|"]
    for r in tous:
        cells = []
        for b in BANDES:
            v = r["t60_par_bande"].get(str(b), {}).get("t60_s")
            cells.append("—" if v is None else f"{v:.2f}")
        L.append(f"| `{r['nom']}` | " + " | ".join(cells) + " |")

    L += ["", "## Premieres reflexions (ms, ordre chronologique)", "",
          "Le motif temporel des premiers pics est la signature des longueurs de lignes a",
          "retard de la machine. C'est ce qu'on regle en premier dans le moteur.", ""]
    for r in tous:
        pr = r["premieres_reflexions"][:12]
        L.append(f"- **`{r['nom']}`** — " +
                 (", ".join(f"{p['t_ms']:.1f} ms ({p['amp_db']:.0f} dB)" for p in pr)
                  if pr else "aucun pic detecte"))

    L += ["", "## Lecture", "",
          "- **Pre-délai** — temps avant la premiere energie. Cible directe du parametre Pre-Delay.",
          "- **T60** — decroissance extrapolee a 60 dB. La methode reellement utilisee "
          "(T30/T20/EDT) figure dans les JSON : sur les programmes courts la plage utile "
          "ne permet pas un T30, et la valeur est plus fragile.",
          "- **Coin HF −3 dB** — ou la reponse decroche au-dessus de 1 kHz. C'est le "
          "plafond de 1987, et une grande partie de ce qu'on reconnait a l'oreille.",
          "- **Coupure** — mur au-dessus duquel il ne reste plus rien (-20 dB). "
          "Constant sur les 14 programmes : c'est le filtre anti-image de la machine, "
          "et le seul chiffre qui la date directement.",
          "- **Amort. HF** — rapport T60(8 kHz) / T60(500 Hz). En dessous de 1, les aigus "
          "meurent plus vite que le medium : c'est le comportement attendu.",
          "- **Mélange** — instant ou la densite d'echos atteint 0,9 (Abel & Huang). "
          "Tard = queue granuleuse et clairsemee, tot = queue lisse. **La mesure du grain.**",
          "- **Rugosité** — ecart-type du spectre de la queue lisse au 1/12 d'octave, "
          "pente retiree. Faible = queue dense et lisse ; eleve = clairsemee et sonnante.",
          "- **Corr. L/R** — proche de 0 : queue large et decorrelee. Proche de 1 : mono.",
          "",
          "## Ce que ces mesures ne disent pas", "",
          "- **La modulation du tank.** Une IR est un instantane fige ; si la capture a ete "
          "faite au sweep, la modulation est etalee et invisible. Ce parametre se reglera "
          "a l'oreille, pas sur un chiffre.",
          "- **Le comportement des boutons.** Les 14 captures sont toutes aux reglages par "
          "defaut : on a 14 points dans l'espace des *programmes*, zero dans l'espace des "
          "*parametres*. Decay, Size et Treble Decay seront conçus depuis Griesinger et "
          "Dattorro, pas mesures.",
          "- **La non-linearite de l'etage d'entree**, qui est dans les modeles NAM, pas ici.",
          ""]
    (SORTIE / "CIBLES.md").write_text("\n".join(L), encoding="utf-8")


def main():
    SORTIE.mkdir(parents=True, exist_ok=True)
    fichiers = sorted(DOSSIER_IR.glob("*.wav"))
    if not fichiers:
        raise SystemExit(f"Aucun WAV dans {DOSSIER_IR}")

    print(f"{len(fichiers)} IR dans {DOSSIER_IR}\n")
    tous = []
    for p in fichiers:
        r = analyser(p)
        tous.append(r)
        (SORTIE / f"{r['nom']}.json").write_text(
            json.dumps(r, indent=2, ensure_ascii=False), encoding="utf-8")
        print(f"  {r['nom']:22s} T60 {str(r['t60_global_s']):>6s} s  "
              f"pre {r['predelai_ms']:6.1f} ms  HF {r['coin_hf_3db']:6.0f} Hz  "
              f"melange {str(r['temps_melange_ms']):>6s} ms  "
              f"corr {str(r['correlation_lr']):>6s}")

    rapport(tous)
    print(f"\n-> {SORTIE / 'CIBLES.md'}")
    print(f"-> {len(tous)} JSON + {len(tous)} planches PNG dans {SORTIE}")


if __name__ == "__main__":
    main()
