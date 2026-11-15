# LuxRev-1 — accord du moteur sur les cibles mesurees

Genere par `accorder.py`. Pour chaque programme : la cible mesuree sur l'IR du
LXP-1, la valeur obtenue par le moteur, et l'ecart. Cible et resultat sont
mesures par le meme code (`analyse_ir.py`) — sinon l'ecart contiendrait autant de
difference de methode que de difference de son.

## Ecarts

| Programme | T60 cible | obtenu | Δ | Rugosité cible | obtenue | Δ | Mélange cible | obtenu | AmortHF cible | obtenue |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_hall_b` | 2.60 s | 2.72 s | -21% | 0.82 | 1.22 | +49% | 934 ms | 398 ms | 0.80 | 0.87 |
| `lxp1_hall_d` | 1.60 s | 1.60 s | -29% | 1.32 | 1.68 | +27% | 468 ms | 260 ms | 0.69 | 0.87 |
| `lxp1_room_gate` *(env.)* | 0.51 s | 0.25 s | -51% | 2.69 | 2.73 | +1% | 52 ms | 216 ms | 1.07 | 0.92 |
| `lxp1_room_inverse` *(env.)* | 0.69 s | 0.23 s | -67% | 2.40 | 2.52 | +5% | 136 ms | 92 ms | 2.72 | 1.02 |
| `lxp1_room_large1` | 2.20 s | 2.21 s | -25% | 1.31 | 1.36 | +4% | 374 ms | 294 ms | 0.58 | 0.91 |
| `lxp1_room_large2` | 2.40 s | 2.38 s | -20% | 1.24 | 1.08 | -13% | 454 ms | 392 ms | 0.76 | 0.78 |
| `lxp1_room_medium1` | 1.20 s | 1.19 s | -42% | 1.36 | 1.65 | +21% | 398 ms | 320 ms | 0.66 | 0.90 |
| `lxp1_room_medium2` | 1.60 s | 1.60 s | -1% | 1.47 | 1.41 | -4% | 322 ms | 320 ms | 0.80 | 0.91 |
| `lxp1_room_plate_b` | 1.70 s | 1.71 s | -35% | 1.10 | 1.45 | +32% | 344 ms | 282 ms | 0.68 | 0.91 |
| `lxp1_room_plate_d` | 1.80 s | 1.80 s | +41% | 1.47 | 1.40 | -5% | 120 ms | 234 ms | 0.70 | 0.83 |
| `lxp1_room_small1` | 0.80 s | 0.81 s | -52% | 2.03 | 2.04 | +0% | 96 ms | 124 ms | 0.51 | 0.70 |
| `lxp1_room_small2` | 1.20 s | 1.20 s | +43% | 2.48 | 2.60 | +5% | 60 ms | 62 ms | 0.80 | 0.84 |

*(env.)* = programme a enveloppe imposee. Le T60 y decrit l'enveloppe plaquee en sortie, pas le tank : ce n'est pas une cible d'accordage du tank.

## Parametres retenus

| Programme | Pré-délai | Taille | Mixage | T60 tank | Ratio HF | LP sortie | Prises | Étalement |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_hall_b` | 33.0 ms | 2.92 | 0.05 | 2.64 s | 1.25 | 7262 Hz | 0.50 | 0.60 |
| `lxp1_hall_d` | 33.0 ms | 1.48 | 0.05 | 1.52 s | 1.60 | 1519 Hz | 0.50 | 0.60 |
| `lxp1_room_gate` | 0.1 ms | 1.11 | 0.60 | 1.20 s | 1.60 | 5674 Hz | 0.50 | 0.60 |
| `lxp1_room_inverse` | 22.7 ms | 0.42 | 0.60 | 1.20 s | 1.60 | 10717 Hz | 0.50 | 0.60 |
| `lxp1_room_large1` | 33.0 ms | 1.80 | 0.05 | 2.23 s | 0.97 | 2011 Hz | 0.50 | 0.60 |
| `lxp1_room_large2` | 33.0 ms | 2.05 | 0.05 | 2.64 s | 0.71 | 8584 Hz | 0.50 | 0.60 |
| `lxp1_room_medium1` | 33.0 ms | 1.34 | 0.05 | 1.17 s | 0.91 | 2813 Hz | 0.50 | 0.60 |
| `lxp1_room_medium2` | 33.0 ms | 1.80 | 0.05 | 1.66 s | 1.60 | 5144 Hz | 0.50 | 0.60 |
| `lxp1_room_plate_b` | 0.0 ms | 1.53 | 0.05 | 1.66 s | 1.16 | 3725 Hz | 0.50 | 0.60 |
| `lxp1_room_plate_d` | 33.0 ms | 1.80 | 0.53 | 1.94 s | 1.29 | 2564 Hz | 0.50 | 0.60 |
| `lxp1_room_small1` | 16.0 ms | 1.11 | 1.00 | 0.81 s | 0.65 | 2350 Hz | 0.50 | 0.60 |
| `lxp1_room_small2` | 16.0 ms | 0.39 | 1.00 | 1.30 s | 0.72 | 8565 Hz | 0.50 | 0.60 |

## Lecture

- **T60** et **AmortHF** sont derives puis corriges ; un ecart de quelques pourcents est sans consequence audible.
- **Mélange** est le plus dur a atteindre : c'est le seul parametre ou la topologie elle-meme peut manquer de marge. Un ecart important signale qu'il faudra plus d'etages de diffusion, pas un meilleur reglage.
- Le **mur anti-image** est fixe a 15 000 Hz pour tous, et n'apparait donc pas dans le tableau.
- La **modulation** n'est pas accordee ici : elle est invisible dans une IR. Valeurs de depart 8 echantillons / 0,7 Hz, a regler a l'oreille.
