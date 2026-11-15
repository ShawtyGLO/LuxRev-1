# LuxRev-1

Réverbe algorithmique de lignée Lexicon, accordée sur des mesures faites sur les 14 IR du
Lexicon LXP-1. Entre le clone et la conception personnelle : les programmes sont clonés
depuis des cibles mesurées, le comportement des paramètres est conçu depuis les sources
publiques (Griesinger, Dattorro, Gardner).

Nom de travail. `LuxRev-1` évite la proximité de marque avec « LXP-1 » ; la forme finale
(`LuxRev`, `Lux Rev 1`) reste à trancher.

**Dossier de travail de ce projet.** Les consignes permanentes, le vault Obsidian et
pluginval sont dans `~/.claude/CLAUDE.md`, chargé dans toutes les sessions.

## À lire en début de session

1. [`CONCEPTION.md`](CONCEPTION.md) — l'architecture, le partage clone/ADN, les décisions ouvertes
2. [`_proto/mesures/CIBLES.md`](_proto/mesures/CIBLES.md) — les 14 fiches de cibles mesurées
3. Note de conception du vault : `C:\Users\llg53\Documents\Obsidian Vault\LuxRev-1\LuxRev-1.md`

## Règle non négociable

Les IR et les modèles NAM sont sous **licence T3K** : l'usage dans un logiciel et la
publication des sorties sont autorisés, la **redistribution des fichiers ne l'est pas**.
Ils vivent dans `Impulse Responce LXP-1/` et servent uniquement de **cibles de mesure**.
Aucun `.wav` ni `.nam` ne doit entrer dans `LuxRev-1/`, ni dans un build, ni dans un
dépôt public. Le plugin est algorithmique, pas un convolueur.

## Structure

    LuxRev-1/                   le plugin JUCE (vide — le prototype vit dans _proto/)
    _proto/                     bancs Python
      gen_signaux.py            génère les signaux de mesure du préampli
      analyse_ir.py             banc d'analyse des 14 IR -> mesures/
      moteur.py                 le moteur de reverbe (prototype Python)
      accorder.py               derive les parametres depuis les cibles et verifie
      PROTOCOLE_MESURE.md       marche à suivre pour la session de rendu
      signaux/                  signaux de test générés
      rendus/                   rendus à rapatrier depuis FL Studio (vide)
      mesures/                  CIBLES.md, 14 JSON, planches/*.png
      accord/                   ACCORD.md (ecarts), programmes.json (parametres v1)
    Impulse Responce LXP-1/     données source, ne sortent jamais d'ici
      Reverb IR/                14 IR, 4 canaux true-stereo, 48 kHz float32
      Préamp IR NAM/            3 captures WaveNet de l'étage d'entrée
    _outils/                    outillage (pluginval…)
    maquettes/                  maquettes HTML d'interface

## Bancs Python

numpy, scipy, matplotlib. Pas de `soundfile` : la lecture et l'écriture WAV sont faites à
la main dans les scripts.

```bash
python _proto/analyse_ir.py     # recalcule toutes les cibles depuis les IR
python _proto/accorder.py       # accorde le moteur sur les cibles et mesure les écarts
python _proto/moteur.py         # vérifie que le T60 obtenu suit le T60 demandé
python _proto/gen_signaux.py    # régénère les signaux de test du préampli
```

**v1 = 12 programmes.** `lxp1_room_delay1` et `delay2` sont reportés : ils ne se mélangent
jamais (densité finale 0,80 et 0,20), c'est un moteur d'une autre nature.

Ne jamais accorder une valeur à la main dans `moteur.py` : il n'expose que des paramètres,
et `accorder.py` est le seul endroit où une cible du LXP-1 entre dans la conception.
