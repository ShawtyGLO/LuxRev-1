# LuxRev-1 — accord du moteur sur les cibles mesurees

Genere par `accorder.py`. Pour chaque programme : la cible mesuree sur l'IR du
LXP-1, la valeur obtenue par le moteur, et l'ecart. Cible et resultat sont
mesures par le meme code (`analyse_ir.py`) — sinon l'ecart contiendrait autant de
difference de methode que de difference de son.

## Ecarts

| Programme | T60 cible | obtenu | Δ | Rugosité cible | obtenue | Δ | Mélange cible | obtenu | AmortHF cible | obtenue |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_hall_b` | 2.60 s | 2.58 s | -1% | 0.82 | 1.08 | +32% | 934 ms | 366 ms | 0.80 | 0.80 |
| `lxp1_hall_d` | 1.60 s | 1.59 s | -1% | 1.32 | 1.71 | +30% | 468 ms | 268 ms | 0.69 | 0.71 |
| `lxp1_room_gate` *(env.)* | 0.51 s | 0.24 s | -54% | 2.69 | 2.60 | -3% | 52 ms | 244 ms | 1.07 | 1.02 |
| `lxp1_room_inverse` *(env.)* | 0.69 s | 0.21 s | -70% | 2.40 | 2.57 | +7% | 136 ms | 92 ms | 2.72 | 1.06 |
| `lxp1_room_large1` | 2.20 s | 2.18 s | -1% | 1.31 | 1.37 | +5% | 374 ms | 268 ms | 0.58 | 0.55 |
| `lxp1_room_large2` | 2.40 s | 2.40 s | -0% | 1.24 | 1.24 | +0% | 454 ms | 418 ms | 0.76 | 0.77 |
| `lxp1_room_medium1` | 1.20 s | 1.20 s | +0% | 1.36 | 1.38 | +1% | 398 ms | 222 ms | 0.66 | 0.68 |
| `lxp1_room_medium2` | 1.60 s | 1.60 s | +0% | 1.47 | 1.42 | -3% | 322 ms | 322 ms | 0.80 | 0.81 |
| `lxp1_room_plate_b` | 1.70 s | 1.70 s | +0% | 1.10 | 1.45 | +32% | 344 ms | 312 ms | 0.68 | 0.69 |
| `lxp1_room_plate_d` | 1.80 s | 1.80 s | +0% | 1.47 | 1.43 | -3% | 120 ms | 232 ms | 0.70 | 0.71 |
| `lxp1_room_small1` | 0.80 s | 0.81 s | +1% | 2.03 | 2.07 | +2% | 96 ms | 120 ms | 0.51 | 0.54 |
| `lxp1_room_small2` | 1.20 s | 1.20 s | +0% | 2.48 | 2.59 | +4% | 60 ms | 78 ms | 0.80 | 0.81 |

*(env.)* = programme a enveloppe imposee. Le T60 y decrit l'enveloppe plaquee en sortie, pas le tank : ce n'est pas une cible d'accordage du tank.

## Parametres retenus

| Programme | Pré-délai | Taille | Mixage | T60 tank | Ratio HF | LP sortie | Prises | Étalement |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_hall_b` | 33.0 ms | 1.99 | 0.05 | 2.85 s | 0.85 | 15567 Hz | 0.50 | 0.60 |
| `lxp1_hall_d` | 33.0 ms | 1.45 | 0.05 | 1.65 s | 0.71 | 2434 Hz | 0.50 | 0.60 |
| `lxp1_room_gate` | 0.1 ms | 1.35 | 0.60 | 0.99 s | 1.60 | 5675 Hz | 0.50 | 0.60 |
| `lxp1_room_inverse` | 22.7 ms | 0.51 | 0.60 | 0.99 s | 1.60 | 10733 Hz | 0.50 | 0.60 |
| `lxp1_room_large1` | 33.0 ms | 2.13 | 0.07 | 2.49 s | 0.47 | 3814 Hz | 0.50 | 0.60 |
| `lxp1_room_large2` | 33.0 ms | 1.68 | 0.05 | 2.65 s | 0.81 | 8639 Hz | 0.50 | 0.60 |
| `lxp1_room_medium1` | 33.0 ms | 1.66 | 0.07 | 1.30 s | 0.66 | 3381 Hz | 0.50 | 0.60 |
| `lxp1_room_medium2` | 33.0 ms | 1.80 | 0.05 | 1.82 s | 0.82 | 6248 Hz | 0.50 | 0.60 |
| `lxp1_room_plate_b` | 0.0 ms | 1.25 | 0.03 | 1.89 s | 0.65 | 4849 Hz | 0.50 | 0.60 |
| `lxp1_room_plate_d` | 33.0 ms | 1.80 | 0.68 | 2.09 s | 0.76 | 2839 Hz | 0.50 | 0.60 |
| `lxp1_room_small1` | 16.0 ms | 1.11 | 1.00 | 0.84 s | 0.57 | 2464 Hz | 0.50 | 0.60 |
| `lxp1_room_small2` | 16.0 ms | 0.45 | 1.00 | 1.32 s | 0.79 | 7966 Hz | 0.50 | 0.60 |

## Lecture

- **T60** et **AmortHF** sont derives puis corriges ; un ecart de quelques pourcents est sans consequence audible.
- **Mélange** est le plus dur a atteindre : c'est le seul parametre ou la topologie elle-meme peut manquer de marge. Un ecart important signale qu'il faudra plus d'etages de diffusion, pas un meilleur reglage.
- Le **mur anti-image** est fixe a 15 000 Hz pour tous, et n'apparait donc pas dans le tableau.
- La **modulation** n'est pas accordee ici : elle est invisible dans une IR. Valeurs de depart 8 echantillons / 0,7 Hz, a regler a l'oreille.
