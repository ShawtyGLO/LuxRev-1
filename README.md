# LuxRev-1

Réverbe algorithmique en C++ / JUCE, conçue à partir de mesures faites sur les réponses d'impulsion d’une réverbe matérielle de référence (Lexicon LXP-1).

## L'idée

Les quatorze programmes de la machine sont **clonés sur cible mesurée**. Le comportement des
réglages, lui, est **conçu** à partir de la littérature (Griesinger, Dattorro) : aucune mesure ne le contraint, et le projet ne prétend pas le contraire.

## La démarche de validation

C'est le cœur du projet, et ce qui en fait autre chose qu'un essai.

1. **Protocole de mesure écrit avant de coder** (`_proto/PROTOCOLE_MESURE.md`) : deux sources de
   données, deux méthodes, à ne pas mélanger.
2. **Cibles chiffrées** (`_proto/mesures/CIBLES.md`) : pour chacun des quatorze programmes,
   pré-délai, T60, coin HF, amortissement, densité, corrélation L/R.
3. **Critère d'acceptation chiffré** : une tolérance et un poids par grandeur, plus une
   validation à l'écoute, comparaisons égalisées en niveau.
4. **Point de sauvegarde de non-régression** : le jeu de réglages validé le 07/09/2026 est figé
   dans `_proto/points_de_sauvegarde/`, avec sa recette complète.

### Limite assumée

Les mesures ne couvrent que les réglages par défaut : quatorze points dans l'espace des
*programmes*, zéro dans l'espace des *paramètres*. La campagne ne dit donc rien du comportement
des boutons. C'est écrit dans le protocole plutôt que passé sous silence.

## Les dix grandeurs mesurées

Pour chacun des quatorze programmes, `analyse_ir.py` extrait dix grandeurs de la réponse
impulsionnelle. Le tableau complet est dans `_proto/mesures/CIBLES.md`.

| Grandeur | Ce qu'elle mesure |
|---|---|
| Pré-délai | Temps avant la première énergie |
| T60 | Décroissance extrapolée à 60 dB |
| Durée −60 dB | La durée réellement observée, à côté du T60 extrapolé |
| Coin HF −3 dB | Où la réponse décroche au-dessus de 1 kHz |
| Coupure | Le mur au-dessus duquel il ne reste plus rien (−20 dB) |
| Amortissement HF | Rapport T60(8 kHz) / T60(500 Hz) |
| Mélange | Instant où la densité d'échos atteint 0,9 (Abel & Huang) — la mesure du grain |
| Densité finale | La densité atteinte en fin de queue |
| Rugosité | Écart-type du spectre de la queue au 1/12 d'octave, pente retirée |
| Corrélation L/R | Proche de 0 : queue large et décorrélée. Proche de 1 : mono |

Deux niveaux de détail s'y ajoutent : le **T60 par bande d'octave** (63 Hz à 16 kHz) et le
**motif des premières réflexions**, signature des longueurs de lignes à retard de la machine.

## Tolérances et poids

L'accordage (`_proto/accorder.py`) ne compare pas deux états à l'œil : il les note. Chaque écart
compte comme (écart ÷ tolérance)², multiplié par un poids. **La tolérance dit à partir de quand
un écart devient grave, le poids dit combien il compte.**

| Grandeur | Tolérance | Poids |
|---|--:|--:|
| T60 | 4 % | 5,0 |
| Équilibre spectral | 1,5 dB | 3,0 |
| Rugosité | 15 % | 2,5 |
| Amortissement HF | 12 % | 2,0 |
| Enveloppe | 6 | 1,0 |
| Mélange | 30 % | 0,5 |

Les poids viennent des retours d'écoute, pas d'une intuition : l'équilibre de bande et le côté
métallique étaient signalés à chaque tour, le temps de mélange jamais.

Un premier jeu de poids donnait 1,5 au T60 contre 3,0 au spectre, et l'accordage sacrifiait
volontiers le premier — `plate_b` sortait à +24 % de T60 pour gagner 1 dB de spectre. Le T60 et
le pré-délai viennent du manuel : ce sont des **spécifications**, pas des préférences. Ils ont
donc été repondérés.

## Écarts obtenus

Tenus ou non, ils sont dans `_proto/accord/ACCORD.md` :

- **T60** à 1 % de la cible sur la plupart des programmes, dans sa tolérance de 4 %.
- **Rugosité** au-delà de sa tolérance sur les programmes longs, jusqu'à +70 % sur `hall_b`.
- **Mélange** souvent manqué : l'écart signale une limite de topologie et non un défaut de
  réglage. Le corriger demanderait un étage de diffusion de plus.

## Contenu

| Dossier | Rôle |
|---|---|
| `LuxRev-1/` | le plugin : source C++, projet JUCE |
| `_proto/` | prototype Python, protocole, mesures, points de sauvegarde |

Les réponses impulsionnelles qui ont servi de référence sont sous licence TONE3000 : leur
redistribution est interdite. Elles restent hors du dépôt, et le `.gitignore` les exclut.

## Construire

Ouvrir `LuxRev-1/LuxRev-1.jucer` dans Projucer, puis compiler sous Visual Studio 2022.
Cible : VST3, Windows 64 bits. Validation avec [pluginval](https://github.com/Tracktion/pluginval).

## Note

Conception, protocole de test, mesures et validation sont de moi. L'écriture du code s'appuie
sur une assistance IA.

## Droits

© Lucas Lallier-Ginoux. **Tous droits réservés.** Ce dépôt est publié en consultation
uniquement, comme portfolio. Aucune licence d'utilisation, de modification, de redistribution
ou d'exploitation commerciale n'est accordée.
