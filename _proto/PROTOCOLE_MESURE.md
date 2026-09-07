# LuxRev-1 — protocole de mesure

Deux sources de données, deux méthodes différentes. Ne pas les mélanger.

---

## A. Les 14 IR de réverbe — téléchargement direct, aucun rendu

Pack : <https://www.tone3000.com/tones/lexicon-lxp1-ir-87851> (auteur `@nickstomp`)
« One for each program, default settings. »

Bouton **Download all** → déposer les `.wav` dans `_proto/ir_source/`.

| Fichier | Famille |
|---|---|
| `lxp1_room_small1` · `small2` | Room court |
| `lxp1_room_medium1` · `medium2` | Room moyen |
| `lxp1_room_large1` · `large2` | Room long |
| `lxp1_room_plate_b` · `plate_d` | Plate |
| `lxp1_hall_b` · `hall_d` | Hall |
| `lxp1_room_inverse` | Inverse |
| `lxp1_room_gate` | Gate |
| `lxp1_room_delay1` · `delay2` | Delay |

**Licence T3K.** Le texte autorise explicitement « use the data file in software and publish
the resulting outputs without royalties or restrictions », et interdit « upload, republish,
or distribute the data file ». Traduction pour ce projet : **mesurer ces IR et en tirer un
moteur algorithmique est autorisé ; embarquer les `.wav` dans le plugin ne l'est pas.**
Elles restent dans `_proto/`, jamais dans `LuxRev-1/`.

⚠️ **Limite structurelle : « default settings ».** On a 14 points dans l'espace des
*programmes*, et **zéro point** dans l'espace des *paramètres*. Aucune IR ne dit ce que fait
le bouton Decay ou Size. C'est ce qui trace la frontière du projet :

- **Les 14 programmes → clonés**, chacun sur une cible mesurée.
- **Le comportement des boutons → conçu**, depuis Griesinger / Dattorro. Aucune donnée
  ne le contraint, et il ne faut pas prétendre le contraire.

---

## B. Les 3 modèles NAM de préampli — session de rendu nécessaire

`Lxp1 Input Gain 2`, `Gain 5`, `Gain 7`. Ce sont des captures **neuronales**, donc
**non linéaires** : la réponse dépend du niveau d'entrée. Un seul sweep ne suffit pas, et
les fichiers `.nam` ne sont pas sur le disque (TONE3000 les streame en mémoire) — on ne peut
donc pas les analyser hors ligne. On les mesure comme on mesurerait du hardware.

### Générer les signaux

```bash
python "D:\1 Plugin Vibecodding\LuxRev-1 VST\_proto\gen_signaux.py"
```

Écrit 7 WAV mono 48 kHz float32 dans `_proto/signaux/`, plus `manifest.json` qui décrit
chaque segment (position, type, niveau) — c'est lui qui permettra au banc d'analyse de
découper les rendus tout seul.

### Réglages du rendu — non négociables

| Réglage | Valeur | Pourquoi |
|---|---|---|
| Fréquence d'échantillonnage | **48 000 Hz** | Le taux natif des modèles NAM. À 44,1 le plugin rééchantillonne et on mesure son resampler autant que le LXP-1. |
| Format de sortie | **WAV 32-bit float** | Pas de troncature, pas de dither : le dither ajouterait un plancher de bruit qui fausse la mesure d'harmoniques à -60 dBFS. |
| Gain du canal | **0.0 dB, exactement** | Les niveaux sont encodés dans les signaux eux-mêmes. Le moindre trim invalide toute la courbe de transfert. |
| Autres plugins | **aucun** | Ni EQ, ni limiteur sur le master. Surtout pas de limiteur. |
| Dans TONE3000 | **un seul modèle actif**, IR désactivée | On mesure l'étage d'entrée seul. |
| Qualité / suréchantillonnage NAM | **au maximum**, jamais le mode économe | ⚠️ Le mode basse consommation replie les harmoniques. Or la non-linéarité **est** l'objet de la mesure : on modéliserait les raccourcis du plugin au lieu du LXP-1. Confirmé sur le premier jeu — l'énergie non harmonique monte de ~25 dB entre −40 et 0 dBFS. |
| Input/Output du plugin | **neutres**, identiques aux 3 rendus | Sinon on mesure le bouton de TONE3000, pas le LXP-1. |

### Les 12 rendus

Pour **chacun** des 3 modèles NAM, rendre ces 4 signaux :

| Signal | Ce qu'il donne |
|---|---|
| `10_sweep_niveaux.wav` | Réponse en fréquence à 5 niveaux. L'écart entre les courbes **est** la non-linéarité. Le fichier le plus important. |
| `20_paliers_1k.wav` | Rangs harmoniques à 1 kHz, niveau par niveau (pairs vs impairs → topologie). |
| `21_paliers_100hz.wav` | Idem à 100 Hz. Un étage d'entrée ne sature pas pareil en bas et en haut. |
| `22_rampe_1k.wav` | Courbe de transfert continue + compression éventuelle. |

Nommage attendu dans `_proto/rendus/` :

```
gain2__10_sweep_niveaux.wav
gain2__20_paliers_1k.wav
gain2__21_paliers_100hz.wav
gain2__22_rampe_1k.wav
gain5__…   gain7__…
```

### Réalignement

Chaque signal commence par une impulsion à −20 dBFS suivie de 0,5 s de silence. La latence
du plugin est inconnue et peut varier d'un rendu à l'autre : le banc d'analyse retrouve ce
repère par corrélation et recale tout seul. **Ne pas rogner le début des rendus.**

### ⚠️ Vérification que le modèle a VRAIMENT changé

Le premier jeu de rendus (2026-09-06) est sorti **identique sur les trois positions de
gain** — écart de 1,5·10⁻⁸, l'epsilon du float32, c'est-à-dire le même audio réexporté.
Le modèle NAM n'avait pas changé entre les exports. Rien dans FL ne le signale.

Avant chaque série, faire ce test de 10 secondes :

1. Charger le modèle, jouer la fin de `20_paliers_1k.wav` (les paliers à −5 et 0 dBFS).
2. **Noter le niveau de sortie affiché.**
3. Charger le modèle suivant, rejouer le même passage.

Le niveau doit **changer visiblement** entre les trois positions : le bouton Input Gain
d'un LXP-1 place plus ou moins de gain analogique devant le convertisseur, donc il déplace
le seuil d'écrêtage. S'il ne bouge pas, le modèle n'a pas été rechargé — inutile d'exporter.

### Vérification avant de fermer FL

Ouvrir un rendu et confirmer :
- l'impulsion de tête est présente ;
- rien ne touche 0 dBFS en écrêtage franc sur le segment 0 dBFS (une saturation douce est
  attendue et voulue — un écrêtage carré signale un limiteur oublié dans la chaîne) ;
- la durée correspond à celle du signal source (à la latence près) ;
- **les trois fichiers d'un même signal ne sont pas identiques** — le plus simple est de
  comparer leur taille ET leur forme d'onde, pas leur seule taille.

### ⚠️ Et surtout : aucun limiteur, y compris dans TONE3000

La mesure du premier jeu montre un écrêtage **franc et symétrique** au-dessus de −25 dBFS
d'entrée (H3 à −10 dB, H5 à −15 dB, harmoniques paires bien plus basses). C'est le profil
d'un convertisseur de 1987 qui sature en butée — plausible et intéressant — mais c'est
**aussi** le profil d'un limiteur oublié dans la chaîne. Vérifier que TONE3000 n'a ni
limiteur ni normalisation de sortie active, sans quoi on modéliserait le plugin de mesure
au lieu du LXP-1.

---

## Ordre de travail

1. Télécharger les 14 IR → `ir_source/` *(pas de rendu, immédiat)*
2. Banc d'analyse des IR → cibles par programme *(à écrire)*
3. Session de rendu des 3 NAM → `rendus/` *(manuel, ~1 h)*
4. Banc d'analyse des NAM → courbe de transfert + profil harmonique *(à écrire)*
5. Moteur DSP accordé sur les cibles

L'étape 1 ne dépend de rien et débloque les étapes 2 et 5. La faire en premier.
