# LuxRev-1 — cibles de l'etage d'entree du LXP-1

Genere par `analyse_preamp.py` depuis 12 rendus des trois captures NAM.
Les modeles ne sont pas reimplementes : ils sont mesures comme du materiel.

## Reponse en frequence a bas niveau (-40 dBFS, dB rel. 500-2000 Hz)

C'est le comportement LINEAIRE de l'etage — ce qu'un IIR doit reproduire.

| Gain | 50 Hz | 100 Hz | 200 Hz | 500 Hz | 1000 Hz | 2000 Hz | 4000 Hz | 8000 Hz | 12000 Hz | 16000 Hz | 20000 Hz |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | +0.8 | +0.1 | -0.0 | +0.1 | +0.2 | -0.2 | +0.2 | +0.1 | +0.5 | -0.2 | -13.5 |
| 5 | +0.8 | +0.1 | -0.0 | +0.1 | +0.2 | -0.2 | +0.2 | +0.1 | +0.5 | -0.2 | -13.5 |
| 7 | +0.8 | +0.1 | -0.0 | +0.1 | +0.2 | -0.2 | +0.2 | +0.1 | +0.5 | -0.2 | -13.5 |

## Deformation de la reponse sous le niveau

Ecart entre la reponse a 0 dBFS et celle a -40 dBFS. Un ecart non nul signifie
que la saturation ne se contente pas d'ajouter des harmoniques : elle change
aussi la couleur. C'est ce qu'un simple waveshaper statique ne reproduirait pas.

| Gain | 50 Hz | 100 Hz | 200 Hz | 500 Hz | 1000 Hz | 2000 Hz | 4000 Hz | 8000 Hz | 12000 Hz | 16000 Hz | 20000 Hz |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 2 | +0.4 | -0.0 | -0.0 | +0.5 | -0.3 | +0.7 | +0.4 | +1.1 | +0.6 | +1.3 | +4.5 |
| 5 | +0.4 | -0.0 | -0.0 | +0.5 | -0.3 | +0.7 | +0.4 | +1.1 | +0.6 | +1.3 | +4.5 |
| 7 | +0.4 | -0.0 | -0.0 | +0.5 | -0.3 | +0.7 | +0.4 | +1.1 | +0.6 | +1.3 | +4.5 |

## Gain et compression

| Gain | Niveau abs. a -40 dBFS | a 0 dBFS | Compression |
|---|--:|--:|--:|
| 2 | 45.7 dB | 67.6 dB | -18.1 dB |
| 5 | 45.7 dB | 67.6 dB | -18.1 dB |
| 7 | 45.7 dB | 67.6 dB | -18.1 dB |

*Compression* = ecart au comportement lineaire. 0 dB = lineaire ; une valeur
negative = l'etage cede quand on le pousse.

## Harmoniques a 1 kHz

| Gain | Entree | THD | H2 | H3 | H4 | H5 |
|---|--:|--:|--:|--:|--:|--:|
| 2 | -60 dBFS | 0.00 % | -120 dB | -110 dB | -117 dB | -106 dB |
| 2 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 2 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 2 | -15 dBFS | 21.86 % | -47 dB | -13 dB | -51 dB | -29 dB |
| 2 | 0 dBFS | 36.31 % | -23 dB | -10 dB | -38 dB | -15 dB |
| 5 | -60 dBFS | 0.00 % | -120 dB | -110 dB | -117 dB | -106 dB |
| 5 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 5 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 5 | -15 dBFS | 21.86 % | -47 dB | -13 dB | -51 dB | -29 dB |
| 5 | 0 dBFS | 36.31 % | -23 dB | -10 dB | -38 dB | -15 dB |
| 7 | -60 dBFS | 0.00 % | -120 dB | -110 dB | -117 dB | -106 dB |
| 7 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 7 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 7 | -15 dBFS | 21.86 % | -47 dB | -13 dB | -51 dB | -29 dB |
| 7 | 0 dBFS | 36.31 % | -23 dB | -10 dB | -38 dB | -15 dB |

## Harmoniques a 100 Hz

| Gain | Entree | THD | H2 | H3 | H4 | H5 |
|---|--:|--:|--:|--:|--:|--:|
| 2 | -60 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 2 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 2 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 2 | -15 dBFS | 21.51 % | -41 dB | -14 dB | -49 dB | -30 dB |
| 2 | 0 dBFS | 35.18 % | -27 dB | -10 dB | -44 dB | -15 dB |
| 5 | -60 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 5 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 5 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 5 | -15 dBFS | 21.51 % | -41 dB | -14 dB | -49 dB | -30 dB |
| 5 | 0 dBFS | 35.18 % | -27 dB | -10 dB | -44 dB | -15 dB |
| 7 | -60 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 7 | -45 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 7 | -30 dBFS | 0.00 % | -120 dB | -120 dB | -120 dB | -120 dB |
| 7 | -15 dBFS | 21.51 % | -41 dB | -14 dB | -49 dB | -30 dB |
| 7 | 0 dBFS | 35.18 % | -27 dB | -10 dB | -44 dB | -15 dB |

## Lecture pour la conception

- La **reponse a bas niveau** est le comportement lineaire : elle se reproduit
  exactement par un IIR ajuste, sans approximation.
- La **deformation sous le niveau** decide de l'architecture : si elle est nulle,
  un waveshaper statique precede d'un filtre suffit. Si elle ne l'est pas, il faut
  une non-linearite dependante de la frequence (filtre dans la boucle de
  contre-reaction, ou plusieurs bandes).
- Le **rapport H2/H3** dit la symetrie : H2 dominant = non-linearite asymetrique
  (un seul cote de l'onde ecrete) ; H3 dominant = symetrique.
- Les **trois positions de gain** donnent la trajectoire de la non-linearite, ce
  qu'une seule capture ne pourrait pas donner : c'est ce qui permet un bouton
  Drive continu plutot que trois presets.
