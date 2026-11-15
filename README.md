# LuxRev-1

Réverbe algorithmique en C++ / JUCE, conçue à partir de mesures faites sur une réverbe matérielle de référence (Lexicon LXP-1).

## L'idée

Les quatorze programmes de la machine sont **clonés sur cible mesurée**. Le comportement des
réglages, lui, est **conçu** à partir de la littérature (Griesinger, Dattorro) : aucune mesure
ne le contraint, et le projet ne prétend pas le contraire.

## La démarche de validation

C'est le cœur du projet, et ce qui en fait autre chose qu'un essai.

1. **Protocole de mesure écrit avant de coder** (`_proto/PROTOCOLE_MESURE.md`) : deux sources de
   données, deux méthodes, à ne pas mélanger.
2. **Cibles chiffrées** (`_proto/mesures/CIBLES.md`) : pour chacun des quatorze programmes,
   pré-délai, T60, coin HF, amortissement, densité, corrélation L/R.
3. **Critère d'acceptation à l'écoute**, comparaisons égalisées en niveau.
4. **Point de sauvegarde de non-régression** : le jeu de réglages validé le 07/09/2026 est figé
   dans `_proto/points_de_sauvegarde/`, avec sa recette complète.

### Limite assumée

Les mesures ne couvrent que les réglages par défaut : quatorze points dans l'espace des
*programmes*, zéro dans l'espace des *paramètres*. La campagne ne dit donc rien du comportement
des boutons. C'est écrit dans le protocole plutôt que passé sous silence.

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
