# LuxRev-1 — cibles mesurees sur les 14 IR du LXP-1

Genere par `analyse_ir.py`. Chaque ligne est un programme du LXP-1 capture a ses
reglages par defaut. Ces nombres sont les cibles que le moteur doit atteindre ;
les IR elles-memes ne quittent pas `_proto/`.

## Tableau de synthese

| Programme | Famille | Pre-dél. | T60 | Durée −60dB | Coin HF −3dB | Coupure | Amort. HF | Mélange | Dens. fin. | Rugosité | Corr. L/R |
|---|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| `lxp1_hall_b` | Hall | 0.5 ms | 3.42 s | 3.80 s | 6479 Hz | 17398 Hz | 0.80 | 934 ms | 0.99 | 0.8 dB | 0.00 |
| `lxp1_hall_d` | Hall | 26.4 ms | 2.25 s | 2.60 s | 2884 Hz | 16511 Hz | 0.69 | 468 ms | 1.04 | 1.3 dB | -0.01 |
| `lxp1_room_delay1` | Delay | 171.3 ms | 2.34 s | 2.26 s | 7932 Hz | 16864 Hz | 0.99 | 178 ms | 0.87 | 2.2 dB | 0.29 |
| `lxp1_room_delay2` | Delay | 206.9 ms | 2.10 s | 1.86 s | 16816 Hz | 17687 Hz | 1.00 | 12 ms | 0.18 | 0.2 dB | -0.01 |
| `lxp1_room_gate` | Gate | 0.1 ms | 0.51 s | 1.55 s | 6294 Hz | 17123 Hz | 1.07 | 52 ms | 1.02 | 2.7 dB | -0.01 |
| `lxp1_room_inverse` | Inverse | 22.7 ms | 0.69 s | 1.56 s | 7486 Hz | 17317 Hz | 2.72 | 136 ms | 1.10 | 2.4 dB | -0.07 |
| `lxp1_room_large1` | Room long | 3.5 ms | 2.96 s | 3.35 s | 3145 Hz | 16781 Hz | 0.58 | 374 ms | 0.99 | 1.3 dB | -0.01 |
| `lxp1_room_large2` | Room long | 0.0 ms | 2.97 s | 3.28 s | 6115 Hz | 17307 Hz | 0.76 | 454 ms | 1.01 | 1.2 dB | 0.01 |
| `lxp1_room_medium1` | Room moyen | 0.0 ms | 2.05 s | 2.38 s | 3964 Hz | 16844 Hz | 0.66 | 398 ms | 1.02 | 1.4 dB | 0.01 |
| `lxp1_room_medium2` | Room moyen | 0.0 ms | 1.61 s | 1.80 s | 6115 Hz | 17148 Hz | 0.80 | 322 ms | 1.04 | 1.5 dB | 0.01 |
| `lxp1_room_plate_b` | Plate | 0.0 ms | 2.61 s | 2.83 s | 4995 Hz | 17017 Hz | 0.68 | 344 ms | 1.06 | 1.1 dB | 0.00 |
| `lxp1_room_plate_d` | Plate | 0.0 ms | 1.28 s | 1.47 s | 3741 Hz | 16783 Hz | 0.70 | 120 ms | 1.01 | 1.5 dB | 0.01 |
| `lxp1_room_small1` | Room court | 0.0 ms | 1.70 s | 1.86 s | 3634 Hz | 16060 Hz | 0.51 | 96 ms | 1.04 | 2.0 dB | 0.01 |
| `lxp1_room_small2` | Room court | 0.0 ms | 0.84 s | 0.93 s | 7273 Hz | 16904 Hz | 0.80 | 60 ms | 1.02 | 2.5 dB | 0.02 |

## T60 par bande d'octave (secondes)

L'evolution du T60 avec la frequence est ce que le bouton *Treble Decay* de
Lexicon controle. Une colonne 8k nettement plus courte que la colonne 500 =
amortissement HF marque dans le tank.

| Programme | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1000 Hz | 2000 Hz | 4000 Hz | 8000 Hz | 16000 Hz |
|---|---|---|---|---|---|---|---|---|---|
| `lxp1_hall_b` | 3.37 | 3.56 | 3.59 | 3.68 | 3.74 | 3.70 | 3.44 | 2.96 | 2.63 |
| `lxp1_hall_d` | 2.50 | 2.58 | 2.54 | 2.45 | 2.30 | 2.18 | 1.96 | 1.69 | 1.56 |
| `lxp1_room_delay1` | 2.29 | 2.27 | 2.29 | 2.30 | 2.32 | 2.34 | 2.47 | 2.28 | 2.22 |
| `lxp1_room_delay2` | 2.04 | 2.15 | 2.30 | 2.13 | 2.31 | 2.15 | 2.25 | 2.13 | 1.87 |
| `lxp1_room_gate` | 0.22 | 0.22 | 0.24 | 0.21 | 0.23 | 4.50 | 0.22 | 0.22 | 0.61 |
| `lxp1_room_inverse` | 0.13 | 0.12 | 0.15 | 0.13 | 0.15 | 8.23 | 0.84 | 0.34 | 0.62 |
| `lxp1_room_large1` | 3.30 | 3.46 | 3.46 | 3.31 | 3.02 | 2.82 | 2.44 | 1.93 | 1.69 |
| `lxp1_room_large2` | 3.02 | 3.02 | 3.29 | 3.27 | 3.20 | 3.18 | 2.96 | 2.50 | 2.13 |
| `lxp1_room_medium1` | 2.58 | 2.37 | 2.48 | 2.27 | 2.18 | 2.02 | 1.84 | 1.50 | 1.32 |
| `lxp1_room_medium2` | 1.70 | 1.58 | 1.71 | 1.75 | 1.77 | 1.73 | 1.63 | 1.40 | 1.25 |
| `lxp1_room_plate_b` | 3.03 | 2.85 | 3.02 | 2.91 | 2.84 | 2.70 | 2.42 | 1.97 | 1.67 |
| `lxp1_room_plate_d` | 1.53 | 1.51 | 1.48 | 1.42 | 1.33 | 1.28 | 1.15 | 0.99 | 0.92 |
| `lxp1_room_small1` | 2.19 | 2.09 | 1.96 | 1.87 | 1.73 | 1.55 | 1.27 | 0.95 | 0.82 |
| `lxp1_room_small2` | 0.64 | 0.77 | 0.85 | 0.89 | 0.96 | 0.92 | 0.85 | 0.71 | 0.71 |

## Premieres reflexions (ms, ordre chronologique)

Le motif temporel des premiers pics est la signature des longueurs de lignes a
retard de la machine. C'est ce qu'on regle en premier dans le moteur.

- **`lxp1_hall_b`** — 0.1 ms (-4 dB), 2.4 ms (-7 dB), 3.8 ms (-6 dB), 4.8 ms (-9 dB), 6.2 ms (-7 dB), 7.9 ms (-5 dB), 9.8 ms (-10 dB), 10.9 ms (-4 dB), 12.2 ms (-11 dB), 13.8 ms (-5 dB), 15.1 ms (-5 dB), 17.4 ms (-9 dB)
- **`lxp1_hall_d`** — 35.5 ms (-20 dB), 41.8 ms (-4 dB), 45.4 ms (-20 dB), 47.1 ms (-19 dB), 48.8 ms (-16 dB), 50.1 ms (-2 dB), 55.1 ms (-2 dB), 56.4 ms (-18 dB), 58.6 ms (0 dB)
- **`lxp1_room_delay1`** — 1.0 ms (0 dB), 2.1 ms (-1 dB), 3.2 ms (-1 dB), 4.2 ms (-3 dB), 5.6 ms (-4 dB), 7.2 ms (-5 dB), 8.3 ms (-5 dB), 9.7 ms (-4 dB), 11.0 ms (-4 dB), 12.4 ms (-5 dB), 13.4 ms (-7 dB), 15.2 ms (-9 dB)
- **`lxp1_room_delay2`** — 0.2 ms (0 dB)
- **`lxp1_room_gate`** — 0.5 ms (-20 dB), 1.5 ms (-17 dB), 3.6 ms (-19 dB), 5.6 ms (-17 dB), 7.6 ms (-15 dB), 11.6 ms (-12 dB), 13.2 ms (-14 dB), 14.6 ms (-16 dB), 16.3 ms (-4 dB), 17.8 ms (-8 dB), 19.3 ms (-8 dB), 20.6 ms (-11 dB)
- **`lxp1_room_inverse`** — 0.4 ms (-10 dB), 1.4 ms (-14 dB), 2.4 ms (-10 dB), 4.2 ms (-10 dB), 6.0 ms (-12 dB), 7.1 ms (-11 dB), 8.7 ms (-12 dB), 9.9 ms (-10 dB), 12.0 ms (-17 dB), 13.0 ms (-9 dB), 14.8 ms (-14 dB), 15.8 ms (-15 dB)
- **`lxp1_room_large1`** — 0.6 ms (-4 dB), 1.9 ms (-7 dB), 3.5 ms (-12 dB), 4.5 ms (-8 dB), 6.3 ms (-8 dB), 7.4 ms (-8 dB), 9.6 ms (-0 dB), 10.9 ms (-2 dB), 12.1 ms (-12 dB), 14.0 ms (-8 dB), 15.9 ms (-9 dB), 17.0 ms (-10 dB)
- **`lxp1_room_large2`** — 0.9 ms (-18 dB), 2.0 ms (-4 dB), 3.0 ms (-10 dB), 4.4 ms (-9 dB), 6.2 ms (-9 dB), 7.2 ms (-2 dB), 8.7 ms (-7 dB), 10.4 ms (-9 dB), 11.8 ms (-6 dB), 13.3 ms (-9 dB), 15.0 ms (-9 dB), 16.1 ms (-5 dB)
- **`lxp1_room_medium1`** — 0.5 ms (-19 dB), 10.8 ms (-18 dB), 13.7 ms (-19 dB), 25.8 ms (-18 dB), 30.9 ms (-20 dB), 34.4 ms (-20 dB), 36.8 ms (-20 dB), 39.0 ms (-19 dB), 41.5 ms (-16 dB), 42.8 ms (-3 dB), 45.1 ms (-20 dB), 47.8 ms (-2 dB)
- **`lxp1_room_medium2`** — 10.1 ms (-20 dB), 12.2 ms (-18 dB), 14.5 ms (-20 dB), 19.4 ms (-19 dB), 22.4 ms (-19 dB), 26.9 ms (-19 dB), 33.1 ms (-18 dB), 35.7 ms (-17 dB), 37.6 ms (-18 dB), 40.3 ms (-2 dB), 41.6 ms (-18 dB), 44.6 ms (-3 dB)
- **`lxp1_room_plate_b`** — 4.9 ms (-20 dB), 12.7 ms (-8 dB), 13.9 ms (-8 dB), 15.1 ms (-12 dB), 16.2 ms (-16 dB), 17.8 ms (-8 dB), 19.0 ms (-9 dB), 20.6 ms (-8 dB), 21.8 ms (-8 dB), 22.9 ms (-8 dB), 24.0 ms (-10 dB), 25.7 ms (-8 dB)
- **`lxp1_room_plate_d`** — 11.5 ms (-6 dB), 13.3 ms (-12 dB), 15.2 ms (-8 dB), 17.5 ms (-9 dB), 19.1 ms (-10 dB), 21.2 ms (-11 dB), 23.0 ms (-4 dB), 24.7 ms (-10 dB), 26.7 ms (-4 dB), 28.9 ms (-7 dB), 30.5 ms (-4 dB), 31.5 ms (-10 dB)
- **`lxp1_room_small1`** — 18.4 ms (-7 dB), 19.5 ms (-8 dB), 20.6 ms (-6 dB), 21.9 ms (-3 dB), 24.0 ms (-6 dB), 25.1 ms (-8 dB), 26.2 ms (-3 dB), 27.3 ms (-5 dB), 29.3 ms (-1 dB), 30.8 ms (-1 dB), 31.9 ms (-7 dB), 32.9 ms (-13 dB)
- **`lxp1_room_small2`** — 2.6 ms (-18 dB), 14.5 ms (-5 dB), 15.9 ms (-2 dB), 17.0 ms (-4 dB), 18.5 ms (-2 dB), 19.6 ms (-4 dB), 20.9 ms (-2 dB), 22.1 ms (-7 dB), 23.4 ms (0 dB), 25.2 ms (-3 dB), 27.2 ms (-0 dB), 28.6 ms (-2 dB)

## Lecture

- **Pre-délai** — temps avant la premiere energie. Cible directe du parametre Pre-Delay.
- **T60** — decroissance extrapolee a 60 dB. La methode reellement utilisee (T30/T20/EDT) figure dans les JSON : sur les programmes courts la plage utile ne permet pas un T30, et la valeur est plus fragile.
- **Coin HF −3 dB** — ou la reponse decroche au-dessus de 1 kHz. C'est le plafond de 1987, et une grande partie de ce qu'on reconnait a l'oreille.
- **Coupure** — mur au-dessus duquel il ne reste plus rien (-20 dB). Constant sur les 14 programmes : c'est le filtre anti-image de la machine, et le seul chiffre qui la date directement.
- **Amort. HF** — rapport T60(8 kHz) / T60(500 Hz). En dessous de 1, les aigus meurent plus vite que le medium : c'est le comportement attendu.
- **Mélange** — instant ou la densite d'echos atteint 0,9 (Abel & Huang). Tard = queue granuleuse et clairsemee, tot = queue lisse. **La mesure du grain.**
- **Rugosité** — ecart-type du spectre de la queue lisse au 1/12 d'octave, pente retiree. Faible = queue dense et lisse ; eleve = clairsemee et sonnante.
- **Corr. L/R** — proche de 0 : queue large et decorrelee. Proche de 1 : mono.

## Ce que ces mesures ne disent pas

- **La modulation du tank.** Une IR est un instantane fige ; si la capture a ete faite au sweep, la modulation est etalee et invisible. Ce parametre se reglera a l'oreille, pas sur un chiffre.
- **Le comportement des boutons.** Les 14 captures sont toutes aux reglages par defaut : on a 14 points dans l'espace des *programmes*, zero dans l'espace des *parametres*. Decay, Size et Treble Decay seront conçus depuis Griesinger et Dattorro, pas mesures.
- **La non-linearite de l'etage d'entree**, qui est dans les modeles NAM, pas ici.
