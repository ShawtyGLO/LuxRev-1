#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Exporte les 12 programmes accordes vers Source/Generated/Programmes.h.

⚠️ LICENCE T3K — ce script ne transporte QUE des coefficients derives.
Aucun .wav, aucun .nam, aucune donnee d'IR n'entre dans le plugin. Ce qui passe
ici, ce sont les parametres du moteur trouves par `accorder.py` et le FIR de
voicing, c'est-a-dire des nombres issus d'une mesure — pas la mesure elle-meme,
et encore moins le fichier source. Le plugin reste algorithmique.

Relancer apres tout nouvel accordage :

    python "LuxRev-1/tools/generer_programmes.py"
"""

import json
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parent.parent                      # « LuxRev-1 VST »
ACCORD = RACINE / "_proto" / "accord" / "programmes.json"
MESURES = RACINE / "_proto" / "mesures"
SORTIE = ICI.parent / "Source" / "Generated" / "Programmes.h"

# Ordre d'affichage a la facade. Les cles sont celles de l'accordeur.
ORDRE = [
    ("lxp1_room_small1",  "Small 1"),
    ("lxp1_room_small2",  "Small 2"),
    ("lxp1_room_medium1", "Medium 1"),
    ("lxp1_room_medium2", "Medium 2"),
    ("lxp1_room_large1",  "Large 1"),
    ("lxp1_room_large2",  "Large 2"),
    ("lxp1_hall_b",       "Hall B"),
    ("lxp1_hall_d",       "Hall D"),
    ("lxp1_room_plate_b", "Plate B"),
    ("lxp1_room_plate_d", "Plate D"),
    ("lxp1_room_gate",    "Gate"),
    ("lxp1_room_inverse", "Inverse"),
]

# Plages Decay et pre-delai d'usine, Program table du manuel LXP-1.
# (t60_min, t60_max, predelai_ms)
MANUEL = {
    "Small 1":  (0.08, 2.4, 16), "Small 2":  (0.12, 2.5, 16),
    "Medium 1": (0.30, 4.0, 33), "Medium 2": (0.40, 8.0, 33),
    "Large 1":  (0.45, 6.5, 33), "Large 2":  (0.55, 8.5, 33),
    "Hall B":   (0.65, 9.0, 33), "Hall D":   (0.60, 8.0, 33),
    "Plate B":  (0.25, 6.5,  0), "Plate D":  (0.40, 6.0, 33),
    "Gate":     (0.10, 2.0,  0), "Inverse":  (0.10, 2.0,  0),
}

SCALAIRES = [
    ("predelaiMs",     "predelai_ms",     33.0),
    ("lpSortieHz",     "lp_sortie_hz",    8000.0),
    ("murHz",          "mur_hz",          17000.0),
    ("taille",         "taille",          1.0),
    ("t60S",           "t60_s",           2.0),
    ("diffusion",      "diffusion",       0.75),
    ("mixage",         "mixage",          0.5),
    ("ratioHf",        "ratio_hf",        0.7),
    ("ratioBass",      "ratio_bass",      1.0),
    ("crossBassHz",    "cross_bass_hz",   400.0),
    ("gainPrises",     "gain_prises",     0.0),
    ("etalement",      "etalement",       0.0),
    ("forme",          "forme",           0.0),
    ("pentePrises",    "pente_prises",    0.0),
    ("modProfondeur",  "mod_profondeur",  8.0),
    ("modHz",          "mod_hz",          0.7),
    ("priseTot",       "prise_tot",       0.0),
    ("fenetreDiffMs",  "fenetre_diff_ms", 60.0),
    ("trebleHz",       "treble_hz",       20000.0),
    ("predelaiFb",     "predelai_fb",     0.0),
    ("hpSortieHz",     "hp_sortie_hz",    0.0),
]


def litteral(v):
    """Litteral flottant C++ valide.

    ⚠️ `%g` peut rendre « 0 » ou « -7 », et « 0f » / « -7f » ne compilent pas :
    en C++ le suffixe f ne s'applique qu'a un litteral qui a deja un point ou un
    exposant. Il faut donc forcer la partie decimale.
    """
    t = f"{float(v):.9g}"
    if "e" not in t and "." not in t:
        t += ".0"
    return t + "f"


def flottants(valeurs, par_ligne=6, indent="        "):
    lignes, tampon = [], []
    for v in valeurs:
        tampon.append(litteral(v))
        if len(tampon) == par_ligne:
            lignes.append(indent + ", ".join(tampon) + ",")
            tampon = []
    if tampon:
        lignes.append(indent + ", ".join(tampon) + ",")
    return "\n".join(lignes)


N_ENV = 100
N_SP = 12
SP_BORNES = (250, 16000)


def lire_mesures():
    """Extrait l'enveloppe et la reponse mesurees, pour l'affichage de la facade.

    ⚠️ Ce sont des COURBES, pas de l'audio. 100 valeurs en dB sur 3 s d'un cote,
    12 valeurs en dB de l'autre : impossible d'en reconstruire une IR. Rien de ce
    que la licence T3K interdit de redistribuer ne passe ici.
    """
    out = {}
    for cle, nom in ORDRE:
        d = json.loads((MESURES / f"{cle}.json").read_text(encoding="utf-8"))
        env = d["enveloppe_db"]
        pas = max(len(env) // N_ENV, 1)
        # maximum de chaque paquet : une enveloppe se sous-echantillonne par le
        # haut, sinon on rate les pics et la courbe s'affaisse
        env100 = [round(max(v for _, v in env[i*pas:(i+1)*pas]), 1) for i in range(N_ENV)]
        sp = [v for f, v in d["spectre_db"] if SP_BORNES[0] <= f <= SP_BORNES[1]][:N_SP]
        while len(sp) < N_SP:
            sp.append(sp[-1] if sp else 0.0)
        out[nom] = {"env": env100, "sp": sp,
                    "t60": d["t60_global_s"], "mel": d["temps_melange_ms"]}
    return out


def main():
    acc = json.loads(ACCORD.read_text(encoding="utf-8"))
    mesures = lire_mesures()
    manquants = [c for c, _ in ORDRE if c not in acc]
    if manquants:
        raise SystemExit(f"programmes absents de l'accordage : {manquants}")

    n_fir = len(acc[ORDRE[0][0]]["correction_fir"])
    for cle, nom in ORDRE:
        if len(acc[cle]["correction_fir"]) != n_fir:
            raise SystemExit(f"{nom} : FIR de longueur {len(acc[cle]['correction_fir'])}, "
                             f"attendu {n_fir} — le format suppose une longueur commune")

    t = []
    t.append("// ⚠️ FICHIER GENERE — ne pas editer a la main.")
    t.append("// Produit par LuxRev-1/tools/generer_programmes.py depuis")
    t.append("// _proto/accord/programmes.json, seul endroit ou une cible mesuree")
    t.append("// sur le LXP-1 entre dans la conception.")
    t.append("//")
    t.append("// LICENCE T3K : ce fichier ne contient que des COEFFICIENTS derives des")
    t.append("// mesures. Aucune donnee d'IR, aucun .wav, aucun .nam. Le plugin est")
    t.append("// algorithmique, pas un convolueur.")
    t.append("")
    t.append("#pragma once")
    t.append("")
    t.append("namespace luxrev {")
    t.append("")
    t.append(f"inline constexpr int kNombreProgrammes = {len(ORDRE)};")
    t.append(f"inline constexpr int kTailleFirVoicing = {n_fir};")
    t.append("")
    t.append("// L'ecran de la facade affiche l'enveloppe MESUREE du programme : la courbe")
    t.append("// reagit aux commandes, mais elle part d'un releve, pas d'un dessin.")
    t.append("//")
    t.append("// ⚠️ 100 valeurs en dB sur 3 s, une par 30 ms. C'est une COURBE D'ENERGIE,")
    t.append("// pas de l'audio : elle ne permet pas de reconstruire l'IR, et ne tombe donc")
    t.append("// pas sous l'interdiction de redistribution de la licence T3K.")
    t.append(f"inline constexpr int kTailleEnveloppe = {N_ENV};")
    t.append(f"inline constexpr float kDureeEnveloppeS = 3.0f;")
    t.append(f"inline constexpr int kTailleSpectre = {N_SP};")
    t.append("")
    t.append("struct Programme")
    t.append("{")
    t.append("    const char* nom;")
    for champ, _, _ in SCALAIRES:
        t.append(f"    float {champ};")
    t.append("    float t60Min, t60Max;          // plage Decay, Program table du manuel")
    t.append("    float predelaiUsineMs;         // pre-delai d'usine, meme source")
    t.append("    const float* firVoicing;       // kTailleFirVoicing coefficients")
    t.append("    const float* compensationColor;  // 5 points, COLOR 0 / 0,25 / 0,5 / 0,75 / 1")
    t.append("    const float* compensationLp;     // idem, pour la coupure de sortie")
    t.append("    const float* enveloppeDb;        // kTailleEnveloppe points, releve sur l'IR")
    t.append("    const float* spectreDb;          // kTailleSpectre points, releve sur l'IR")
    t.append("    float t60MesureS;                // ce que la machine fait vraiment")
    t.append("    float melangeMs;                 // temps de melange mesure")
    t.append("};")
    t.append("")

    for i, (cle, nom) in enumerate(ORDRE):
        p = acc[cle]
        t.append(f"// --- {nom}")
        t.append(f"inline constexpr float kFir{i}[kTailleFirVoicing] = {{")
        t.append(flottants(p["correction_fir"]))
        t.append("};")
        t.append(f"inline constexpr float kCompColor{i}[5] = {{ "
                 + ", ".join(litteral(v) for v in p.get("compensation_color", [1]*5)) + " };")
        t.append(f"inline constexpr float kCompLp{i}[5] = {{ "
                 + ", ".join(litteral(v) for v in p.get("compensation_lp", [1]*5)) + " };")
        m = mesures[nom]
        t.append(f"inline constexpr float kEnv{i}[kTailleEnveloppe] = {{")
        t.append(flottants(m["env"], 8))
        t.append("};")
        t.append(f"inline constexpr float kSpectre{i}[kTailleSpectre] = {{")
        t.append(flottants(m["sp"], 8))
        t.append("};")
        t.append("")

    t.append("inline constexpr Programme kProgrammes[kNombreProgrammes] = {")
    for i, (cle, nom) in enumerate(ORDRE):
        p = acc[cle]
        vmin, vmax, pre = MANUEL[nom]
        champs = ", ".join(litteral(p.get(src, defaut)) for _, src, defaut in SCALAIRES)
        t.append(f'    {{ "{nom}", {champs},')
        t.append(f"      {litteral(vmin)}, {litteral(vmax)}, {litteral(pre)},")
        m = mesures[nom]
        t.append(f"      kFir{i}, kCompColor{i}, kCompLp{i}, kEnv{i}, kSpectre{i},")
        t.append(f"      {litteral(m['t60'])}, {litteral(m['mel'])} }},")
    t.append("};")
    t.append("")
    t.append("} // namespace luxrev")
    t.append("")

    SORTIE.parent.mkdir(parents=True, exist_ok=True)
    SORTIE.write_text("\n".join(t), encoding="utf-8")
    print(f"{SORTIE.relative_to(RACINE)} : {len(ORDRE)} programmes, "
          f"FIR de {n_fir} coefficients")


if __name__ == "__main__":
    main()
