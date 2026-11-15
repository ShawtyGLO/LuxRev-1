# LuxRev-1 — accord du moteur sur les cibles mesurees

Genere par `accorder.py`. Pour chaque programme : la cible mesuree sur l'IR du
LXP-1, la valeur obtenue par le moteur, et l'ecart. Cible et resultat sont
mesures par le meme code (`analyse_ir.py`) — sinon l'ecart contiendrait autant de
difference de methode que de difference de son.

## Ecarts

| Programme | T60 cible | obtenu | Δ | Rugosité cible | obtenue | Δ | Mélange cible | obtenu | AmortHF cible | obtenue |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_room_plate_b` | 1.70 s | 2.34 s | -10% | 1.10 | 1.44 | +31% | 344 ms | 366 ms | 0.68 | 0.86 |

*(env.)* = programme a enveloppe imposee. Le T60 y decrit l'enveloppe plaquee en sortie, pas le tank : ce n'est pas une cible d'accordage du tank.

## Parametres retenus

| Programme | Pré-délai | Taille | Mixage | T60 tank | Ratio HF | LP sortie | Prises | Étalement |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_room_plate_b` | 0.0 ms | 2.79 | 0.03 | 2.21 s | 1.16 | 2865 Hz | 0.50 | 0.60 |

## Lecture

- **T60** et **AmortHF** sont derives puis corriges ; un ecart de quelques pourcents est sans consequence audible.
- **Mélange** est le plus dur a atteindre : c'est le seul parametre ou la topologie elle-meme peut manquer de marge. Un ecart important signale qu'il faudra plus d'etages de diffusion, pas un meilleur reglage.
- Le **mur anti-image** est fixe a 15 000 Hz pour tous, et n'apparait donc pas dans le tableau.
- La **modulation** n'est pas accordee ici : elle est invisible dans une IR. Valeurs de depart 8 echantillons / 0,7 Hz, a regler a l'oreille.
