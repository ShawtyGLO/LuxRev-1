#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Calibre le niveau interne de chaque programme devant le convertisseur 16 bits.

⚠️ POURQUOI. Le LXP-1 est 16 bits lineaires et son manuel annonce 85 dB de plage
dynamique. Notre portage sortait entre 71 et 95 dB selon le programme, parce que
le niveau de sortie du tank varie de 14 dB de l'un a l'autre : chacun occupe donc
une portion differente de la grille 16 bits, et le grain de quantification
s'entend d'autant plus fort que le programme est discret.

Ce n'est pas une question de nombre de bits — c'est une question de CALAGE. On
place donc chaque programme au meme niveau devant le convertisseur, et on defait
le gain juste apres : le signal est inchange, seule sa position dans la grille
change.

    python _proto/niveau_interne.py
    python "LuxRev-1/tools/generer_programmes.py"
"""

import json
import subprocess
import sys
from pathlib import Path

ICI = Path(__file__).parent
RACINE = ICI.parent
BANC = RACINE / "LuxRev-1" / "tools" / "diag_grain.exe"
SORTIE = ICI / "accord" / "niveau_interne.json"

# -3 dBFS : assez haut pour user la grille, assez bas pour qu'une pointe de
# programme ne parte pas au-dela de ce que le convertisseur represente.
CIBLE = 0.71
GAIN_MAX = 8.0

CLES = {
    "Small 1": "lxp1_room_small1", "Small 2": "lxp1_room_small2",
    "Medium 1": "lxp1_room_medium1", "Medium 2": "lxp1_room_medium2",
    "Large 1": "lxp1_room_large1", "Large 2": "lxp1_room_large2",
    "Hall B": "lxp1_hall_b", "Hall D": "lxp1_hall_d",
    "Plate B": "lxp1_room_plate_b", "Plate D": "lxp1_room_plate_d",
    "Gate": "lxp1_room_gate", "Inverse": "lxp1_room_inverse",
}


def main():
    if not BANC.exists():
        raise SystemExit(f"banc absent : {BANC}")
    r = subprocess.run([str(BANC), "2"], capture_output=True, text=True, check=True)

    out = {}
    print("Calage du niveau interne — cible " + f"{20*__import__('math').log10(CIBLE):.1f} dBFS\n")
    print(f"  {'programme':11s} {'niveau':>9s} {'gain':>8s}")
    for ligne in r.stdout.splitlines():
        if not ligne.strip():
            continue
        nom, val = ligne.rsplit("  ", 1)
        nom = nom.strip()
        if nom not in CLES:
            continue
        crete = float(val)
        g = min(CIBLE / max(crete, 1e-6), GAIN_MAX)
        out[CLES[nom]] = round(g, 6)
        print(f"  {nom:11s} {crete:9.3f} {g:8.2f}")

    SORTIE.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print(f"\n  ecrit : {SORTIE.relative_to(RACINE)}")


if __name__ == "__main__":
    main()
