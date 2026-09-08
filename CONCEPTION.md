# LuxRev-1 — conception

Réverbe algorithmique de lignée Lexicon, accordée sur des mesures faites sur les 14 IR du
LXP-1. Ni un convolueur, ni une invention libre : un moteur conçu depuis les sources
publiques, dont chaque constante est fixée par un nombre mesuré quand un nombre existe.

État : prototype Python accordé sur les cibles. Mesures IR ✅, moteur ✅, préampli ⏳, JUCE 💤.

**v1 = 12 programmes** (décidé 2026-09-05) : les deux Delay sont reportés.

---

## 1. Le partage clone / ADN, tranché par la donnée

Les 14 captures sont toutes **aux réglages par défaut**. On a donc 14 points dans l'espace
des *programmes* et zéro dans l'espace des *paramètres*. Ce n'est pas un arbitrage de
goût, c'est ce que la donnée permet :

| | Statut | Source |
|---|---|---|
| Les 14 programmes | **Clonés** | Une cible mesurée chacun (`_proto/mesures/CIBLES.md`) |
| Coupure absolue à 16,9 kHz | **Clonée** | Mesurée, constante sur les 14 |
| Étage d'entrée | **Cloné** | 3 modèles NAM, à mesurer (§ 6) |
| Comportement de Decay, Size, Treble Decay | **Conçu** | Griesinger / Dattorro / Gardner |
| Modulation du tank | **Réglée à l'oreille** | Invisible dans une IR (§ 7) |

---

## 2. Ce que les mesures disent de la machine

Chiffres complets dans [`_proto/mesures/CIBLES.md`](_proto/mesures/CIBLES.md), une planche
PNG par programme dans `_proto/mesures/planches/`.

**Un mur à 16,9 kHz, et rien au-dessus.** Coupure mesurée entre 15,8 et 17,6 kHz sur les
14 programmes — moyenne 16,9 kHz, dispersion ±5 %. C'est le filtre anti-image de la
machine, le seul chiffre qui la date directement, et le plus reproductible de tous.
Au-dessus, la falaise est verticale.

**Mais la pente commence bien plus bas.** Le coin à −3 dB va de 2,9 kHz (`hall_d`) à
7,5 kHz (`small2`) selon le programme. Donc deux filtres distincts, et non un seul : une
pente douce *variable par programme* dans le médium-aigu, puis un mur *fixe*.

**Les aigus meurent plus vite que le médium, toujours.** T60(8 kHz)/T60(500 Hz) entre
**0,50** (`small1`) et **0,93** (`delay1`), une seule exception à 1,07 (`gate`, dont
l'enveloppe est de toute façon imposée de l'extérieur). C'est exactement la plage que
l'amortissement dans la boucle devra couvrir.

**Le temps de mélange s'ordonne avec la taille.** 48 ms (`small2`) → 84 (`small1`) →
164 (`plate_b`) → 270 (`medium1`) → 326 (`large`) → 666 ms (`hall_b`). Un facteur 14 entre
les extrêmes : **la diffusion ne peut pas être une constante du moteur**, elle est
programmée. C'est la mesure la plus structurante de tout le lot.

**La queue est totalement décorrélée.** Corrélation L/R entre −0,06 et +0,02 sur tous les
programmes de réverbe. Les deux sorties doivent être prises en des points suffisamment
éloignés du tank pour n'avoir rien en commun.

**Le taux d'accumulation, lui, varie peu** : 8 à 15,6 pics par 10 ms. La machine remplit
vite ; ce qui change d'un programme à l'autre, c'est le temps qu'elle met à devenir
statistiquement lisse, pas la vitesse des premiers échos.

**Trois programmes ne sont pas des réverbes.** `delay2` a une densité finale de 0,20 — il
ne se mélange jamais, et ses pics sont régulièrement espacés de 1,46 ms. `delay1` monte à
0,80 avec un pré-délai de 166 ms. `gate` et `inverse` ont un R² de décroissance de 0,59 et
0,76 contre 0,99+ partout ailleurs : leur enveloppe n'est pas exponentielle, elle est
imposée. Ces quatre-là demandent un traitement à part (§ 4).

## ⛔ Les IR n'ont PAS été capturées aux réglages nominaux (2026-09-07)

Le manuel utilisateur du LXP-1 (source Harman) donne la **Program table** avec les valeurs
nominales de chaque programme. Confrontées aux mesures :

| Programme | RT60 nominal | mesuré | écart | Pré-délai nominal | mesuré |
|---|--:|--:|--:|--:|--:|
| Small 1 | 0,8 s | 1,70 s | **+112 %** | 16 ms | 0,0 |
| Small 2 | 1,2 s | 0,84 s | −30 % | 16 ms | 0,0 |
| Medium 1 | 1,2 s | 2,05 s | +71 % | 33 ms | 0,0 |
| Medium 2 | 1,6 s | 1,61 s | +1 % | 33 ms | 0,0 |
| Large 1 | 2,2 s | 2,96 s | +35 % | 33 ms | 3,5 |
| Large 2 | 2,4 s | 2,97 s | +24 % | 33 ms | 0,0 |
| Hall D | 1,6 s | 2,25 s | +41 % | 33 ms | 26,4 |
| Hall B | 2,6 s | 3,42 s | +32 % | 33 ms | 0,5 |
| Plate D | 1,8 s | 1,28 s | −29 % | 33 ms | 0,0 |
| Plate B | 1,7 s | 2,61 s | +54 % | 0 ms | 0,0 |

**L'auteur des captures a écrit « default settings ». Mais sur le LXP-1, DECAY et DELAY sont
des boutons physiques — il n'existe aucune position par défaut mémorisée.** Les captures ont
été faites là où les boutons se trouvaient.

Les preuves convergent :
- **Le pré-délai mesuré est ~0 sur 8 programmes sur 10**, alors que le nominal est 16 ou
  33 ms. Bouton DELAY au minimum.
- **Les T60 sont dispersés des deux côtés** (+112 % à −30 %), ce qui exclut un biais de
  mesure systématique.
- Le bouton DECAY était en moyenne à **64 % de course** contre ~56 % au nominal, avec
  13 points d'écart-type — donc ni au nominal, ni à une position fixe.

⚠️ **Conséquence sur les cibles.** Le T60 et le pré-délai mesurés décrivent *ces captures-là*,
pas les programmes du LXP-1. Ils sont **remplacés par les valeurs du manuel**
(`_proto/specs_lxp1.json`).

✅ **Ce qui reste valide** : tout ce que les deux boutons ne contrôlent pas — le spectre, la
rugosité, le rapport d'amortissement HF, la forme d'enveloppe, la corrélation stéréo, la
coupure anti-image. C'est-à-dire l'essentiel du caractère.

Et le manuel donne en prime les **plages** de chaque programme (Small 1 : 0,08–2,4 s ;
Hall B : 0,65–9,0 s ; pré-délai 0–246 ms partout), ce qui définit exactement le
comportement du bouton Decay — la zone d'ombre la plus ancienne du projet.

⚠️ **Une réserve sur les captures.** Les dix programmes de réverbe décroissent en droite
quasi parfaite (R² ≥ 0,991) jusqu'à −60 dB et au-delà, avec un plancher à −120 dB. Une
capture matérielle réelle s'écarte de la droite en arrivant sur son bruit de fond. Cette
perfection suggère une queue débruitée ou prolongée par l'outil de capture. **Sans
conséquence sur les T60**, qui sont ajustés entre −5 et −35 dB. Mais la toute fin de queue
n'est pas une mesure et ne servira pas de cible.

---

## 3. Ce que le manuel de service a donné (2026-09-06)

Manuel de service LXP-1, Lexicon réf. 070-06952. Une lecture a corrigé plus de choses que
trois jours de mesure — parce qu'une mesure dit ce que l'appareil *fait*, jamais **avec
quoi** ni **pourquoi**.

| Fait | Conséquence |
|---|---|
| **Word clock à 31,25 kHz** (quartz 16 MHz) | Le tank tourne à cette fréquence, pas à 48 kHz. Nyquist à 15,6 kHz par construction. |
| **Filtres 15 kHz en modules** — U32 anti-repliement, U25/U29 anti-image | Le mur mesuré à 16,9 kHz était le −20 dB de ce filtre. 15 kHz est la valeur de conception. |
| ⚠️ **Pré/dé-accentuation** — U31 (LF353) avant l'échantillonneur, C48/R43 et C56/R44 après le DAC | **Invisible à la mesure** : les deux se compensent exactement en linéaire. N'agit que sur le bruit de quantification et l'écrêtage interne — c'est-à-dire précisément le grain. |
| **16 bits linéaires**, DAC PCM54HP, ADC à approximations successives réutilisant ce DAC | Quantification à l'entrée et à la sortie du tank. |
| **Aucun transformateur d'entrée** : 1/4" asymétrique 50 kΩ, pot double R52, ampli-op uPC4570 | L'étage d'entrée est un ampli-op propre. |
| **THD < 0,07 % en wet**, plage d'entrée −20 à +15 dBV | ⚠️ La capture NAM mesurait **36 %** — facteur 500. Elle ne décrit pas le LXP-1. |
| **Plage dynamique 85 dB** | Confirme que les IR à 120 dB ont été débruitées ou prolongées par l'outil de capture. |

Détail : les longueurs de Dattorro sont données à 29 761 Hz, soit à 5 % de la fréquence
réelle de la machine.

Sources et méthode de recherche : note `Où trouver les sources techniques` du vault.

## 3bis. Ce que le manuel de service du LXP-5 a confirmé (2026-09-08)

Manuel de service LXP-5, Lexicon réf. 070-07300, 69 pages avec description de circuit,
spécifications, nomenclature et schémas. Trouvé sur `vintagesynthparts.com`, scan sans OCR —
lu en rendant les planches en images, méthode notée dans `Où trouver les sources techniques`.

⚠️ **Il ne corrige rien, il CONFIRME** — et c'est précieux : nos valeurs les plus
structurantes venaient d'un seul manuel, celui du LXP-1. Une seconde machine de la même
famille qui donne les mêmes chiffres les fait passer de « lu quelque part » à « constant de
la gamme ».

| Ce que le LXP-5 dit | Notre valeur | Verdict |
|---|---|---|
| « **31.25 kHz sample rate** », écrit en toutes lettres | 31 250 Hz | ✅ confirmé, et cette fois explicitement plutôt que déduit d'un quartz |
| Quartz **16 MHz** sur le Lexichip, divisé en interne | 16 MHz | ✅ identique |
| **16-bit linear PCM** | 16 bits | ✅ confirmé |
| DAC **PCM54HP** (nomenclature, U51) | PCM54HP | ✅ **le même composant** |
| ADC à approximations successives réutilisant le DAC + comparateur externe | idem | ✅ même architecture |
| Mémoire audio : quatre 64K × 4 bits → **64K × 16 bits** | idem | ✅ identique |
| **Plage dynamique 85 dB**, 20 Hz – 20 kHz | 85 dB | ✅ identique |

### ✅ Ce qu'il apporte de neuf

**Un seul convertisseur, partagé en trois temps.** Le diagramme *A/D SAR Conversion Timing*
montre l'ordre exact d'une période d'échantillonnage : **conversion A/N, puis voie DROITE,
puis voie GAUCHE**. Les deux sorties ne sont donc pas converties au même instant — il y a un
décalage d'un tiers de période entre L et R, soit **10,7 µs** à 31,25 kHz.

✅ **Modélisé depuis le 2026-09-09, en dur et sans interrupteur** (décidé avec l'utilisateur :
« il devrait y être comme signature et pas comme proposition »).

⚠️ **Avec un écart assumé : c'est la GAUCHE qui mène**, alors que le manuel donne l'ordre
inverse (`kGaucheEnPremier`, décidé 2026-09-09). Le coût est nul à l'oreille — un écart de
10,7 µs décorrèle identiquement dans les deux sens, et corrélation, somme mono et spectre
sont indiscernables. Ce qui change, c'est ce qu'on peut *affirmer* : sur ce point précis le
plugin ne reproduit plus la machine, il la reflète. La machine n'avait pas le
choix, le plugin non plus. Implémenté comme un **décalage du bloqueur** et non comme un
retard fractionnaire filtré : la gauche tient simplement l'échantillon précédent pendant le
premier tiers de période, ce qui est exactement ce que fait un échantillonneur-bloqueur, et
ce qui ne coûte rien.

Mesuré par `tools/diag_convertisseur.cpp`, corrélation moyenne entre voies :

| | sans | avec |
|---|--:|--:|
| programme court n° 10 | +0,104 | **+0,081** |
| programme court n° 11 | −0,180 | **−0,102** |
| `plate_b` (queue longue) | −0,003 | −0,004 |

⚠️ **Et il ne fait rien sur les queues longues** — c'était prévisible et il fallait le
vérifier plutôt que de l'annoncer : une queue de réverbe est déjà décorrélée par
construction. Le décalage n'agit que là où il reste de la corrélation, c'est-à-dire dans le
champ précoce et sur les programmes courts, où il en retire 20 à 45 %. La somme mono est
invariante (0,004 dB sur `plate_b`).

**La bande passante en wet est spécifiée** : `20 Hz – 15 kHz, +1,0 / −1,5 dB` (le sec allant
à 20 kHz ±0,1 dB). Cela confirme du côté conception ce que les filtres 15 kHz laissaient
supposer, et cela n'entre pas en conflit avec notre mur à 17 kHz : la spec ne dit rien de ce
qui se passe **au-dessus** de 15 kHz, et c'est justement là que nos mesures d'IR placent la
falaise.

### ⚠️ Et une lecture à corriger de mon côté

Le LXP-5 sépare **THD wet < 0,02 %** et **THD dry < 0,01 %**. Le manuel du LXP-1 ne donne
qu'un chiffre, `< 0,07 %`, et il est de la même famille : c'est une THD **du chemin traité
entier**, pas de l'étage d'entrée seul.

J'ai calibré `preampli.py` en attribuant ces 0,07 % au seul étage d'entrée. **L'étage réel
est donc plus propre que ça.** Conséquence pratique nulle — 0,07 % est inaudible, et DRIVE à
zéro reste la référence — mais le raisonnement écrit dans le code était faux et il est
corrigé.

⛔ **Le manuel de service du LXP-15 n'a pas été trouvé** : ni sur vintagesynthparts, ni sur
synthxl, ni via manualslib. Seul son manuel utilisateur est accessible, et il ne donne pas
d'architecture.

## 4. Architecture du moteur

Topologie de la lignée Griesinger/Dattorro, dimensionnée par les mesures ET par le manuel.

```
entrée (48 kHz) ─▶ pré-accentuation ─▶ anti-repliement 15 kHz ─▶ ADC 16 bits
                ─▶ [ TANK à 31,25 kHz ] ─▶ DAC 16 bits
                ─▶ anti-image 15 kHz ─▶ dé-accentuation ─▶ sortie (48 kHz)
```

```
entrée ─▶ [préampli LXP] ─▶ [pré-délai 0-200 ms] ─▶ [diffuseur : 4 allpass série]
                                                              │
                              ┌───────────────────────────────┘
                              ▼
       ┌──── TANK figure-8 (2 bras croisés) ─────────────────────────┐
       │  bras A : allpass modulé → délai → LP amortissement         │
       │           → allpass → délai ────────────┐                   │
       │  bras B : idem, longueurs premières entre elles             │
       │           ◀────────────────────────────┘  (croisement)      │
       └──────────┬──────────────────────┬───────────────────────────┘
                  │ prises L             │ prises R   (points éloignés)
                  ▼                      ▼
            [enveloppe programmée : Gate / Inverse uniquement]
                  │
                  ▼
       [LP variable 2,9-7,5 kHz] ─▶ [mur fixe 16,9 kHz] ─▶ [mix] ─▶ sortie
```

**Les constantes, et d'où elles viennent :**

| Élément | Valeur | Origine |
|---|---|---|
| Pré-délai | 0 – 200 ms | Mesuré 0,0 – 166 ms |
| Diffusion (nb d'étages actifs + coefficient) | programmée | Temps de mélange mesuré, 48 – 666 ms |
| Gain de boucle du tank | par programme | T60 mesuré, 0,16 – 3,44 s |
| LP d'amortissement dans la boucle | par programme | Rapport T60(8k)/T60(500), 0,50 – 0,93 |
| LP de sortie | 2,9 – 7,5 kHz | Coin −3 dB mesuré |
| Mur anti-image | **16,9 kHz fixe**, pente raide | Mesuré, constant sur les 14 |
| Écart des prises L/R | maximal | Corrélation mesurée ≈ 0 |
| Longueurs de lignes | nombres premiers entre eux | Dattorro 1997 — **pas mesuré** |
| Profondeur / vitesse de modulation | à régler | **Pas mesurable** (§ 7) |

---

## 5. Les quatre programmes atypiques

**`gate` et `inverse`** ne sont pas des réverbes courtes, ce sont des réverbes dont
l'enveloppe est réécrite. Leur R² de décroissance (0,59 et 0,76) le prouve : la forme dans
le temps ne suit pas une exponentielle.

⚠️ **Et ce ne sont PAS des portes déclenchées.** Je les avais implémentées comme une
enveloppe relancée à chaque attaque du signal, avec détection et seuil. C'est une erreur de
nature, et deux manuels de l'époque le disent chacun de leur côté :

> **AMS RMX16**, programme NONLIN 2 — « the sound decays only very slightly for a period of
> time (set by the decay control), and then dies away very rapidly ». REVERSE 1 — « the
> reverberating sound builds up for a period of time then suddenly stops ».

> **Lexicon**, paramètre *Slope* de l'algorithme Gate — « Determines **how the amplitude
> builds over time** (Size). Low Slope settings result in a higher amplitude at the
> beginning; high Slope settings result in a higher amplitude at the end. »

Aucun seuil, aucun déclencheur. La forme **est la réponse impulsionnelle du programme**, et
la commande de durée en règle la **longueur** — pas une vitesse de décroissance. C'est un
système linéaire et invariant : le même motif répond à chaque échantillon d'entrée, ce qui
est exactement pourquoi la machine n'a rien à régler.

**Implémentation retenue** : une ligne à retard alimentée par l'entrée **diffusée**, portant
**192 prises** dont les gains suivent le profil relevé, à espacement pseudo-aléatoire. Le
réseau bouclé n'intervient pas sur ces deux programmes — une queue exponentielle par-dessus
émousserait justement la falaise qui fait le programme. La densité vient du nombre de prises
et de la diffusion en amont, qui étale chaque prise en une bouffée.

| écart de forme à la machine | porte déclenchée | banc non linéaire |
|---|--:|--:|
| Gate | 1,1 dB | **1,0 dB** |
| Inverse | 2,5 dB | **1,4 dB** |

Et surtout : **plus rien à régler**, comme sur la machine.

**`delay1` et `delay2`** ne se mélangent jamais (densité finale 0,80 et 0,20). Les traiter
comme des réverbes serait une erreur de nature. `delay2` montre des pics régulièrement
espacés de 1,46 ms : c'est une ligne recirculante courte, pas un tank. **Décidé le
2026-09-05 : reportés hors v1.** Ils demandent un moteur d'une autre nature.

---

## 6. Paramètres — simple mais complet

Le LXP-1 avait un sélecteur de programme et une poignée de boutons. On garde cette
philosophie : ce n'est pas un compromis, c'est fidèle.

**Façade — 6 contrôles**

| Contrôle | Plage | Note |
|---|---|---|
| **Programme** | 14 (ou 10, cf. § 8) | Un jeu de cibles, pas un algorithme différent |
| **Decay** | ×0,25 – ×4 autour de la valeur du programme | Conçu, non mesuré |
| **Pre-Delay** | 0 – 200 ms | Mesuré |
| **Size** | échelle les longueurs de lignes | Conçu, non mesuré |
| **Treble Decay** | rapport 0,3 – 1,0 | Plage élargie autour du 0,50 – 0,93 mesuré |
| **Mix** | 0 – 100 % | |

**Capot avancé — 4 contrôles**

- **GRAIN** — axe unique 1987 ↔ aujourd'hui. À 100 : coupure 16,9 kHz, temps de mélange et
  rugosité aux valeurs mesurées. À 0 : mur repoussé à 20 kHz, diffusion augmentée, queue
  plus lisse. **Par défaut à 100**, contrairement à l'Intégrité de [[Digitape]] : ici le
  grain n'est pas une dégradation optionnelle, c'est l'identité du produit. Mais il reste
  rétractable, et le cœur doit être bon en dessous — la leçon du 15/07 tient.
- **Modulation** du tank — réglée à l'oreille.
- **Préampli LXP** — on/off + drive, adossé aux mesures NAM.
- **HP / LP** de sortie.

---


## 6 bis. Les commandes qui vont au-delà de la machine (décidé 2026-09-07)

Le clone est validé à l'oreille. Ce qui justifie le plugin au-delà du clone, c'est que
**la machine était bornée et nous ne le sommes pas** : 65 536 échantillons de RAM et un
Lexichip de 1987, avec des programmes gravés en ROM.

Deux axes retenus, tous deux construits sur le même principe — **le point de référence est
une position centrale, pas une extrémité**.

### RÉSONANCE — 50 % au centre

50 % = le LXP-1 mesuré. Vers le bas, plus crémeux. Vers le haut, plus « métallique
synthétique ».

Leviers, du plus au moins efficace : longueur des lignes, force de la matrice de
Householder, rugosité au-dessus de 5 kHz, profondeur de la modulation aléatoire.

⚠️ **Contrainte de construction** : trois de ces leviers déplacent aussi le T60 et le
timbre — exactement ce qui vient d'être validé. La macro doit être **compensée** : quand
elle raccourcit les lignes, elle rattrape le gain de boucle pour que la durée ne bouge pas.
Sinon tourner Résonance changera la longueur de la réverbe, ce qui n'est pas l'intention.

### STÉRÉO — 100 % au centre  ✅ implémenté 2026-09-07

100 % = l'image du LXP-1 telle que mesurée. À 100 % pile, la commande est **court-circuitée
dans le code** : le rendu est bit à bit identique au moteur validé à l'oreille. Vérifié,
écart maximum 0,000e+00.

Le mécanisme retenu est **milieu/côté** : on décompose en `M = (L+R)/2` et `C = (L−R)/2`,
on multiplie le côté par la largeur, on recompose. Plage 0 % à 160 %.

| largeur | corrélation L/R | mono |
|---:|---:|---:|
| 0 % | +1,000 | inchangé |
| 50 % | +0,551 | inchangé |
| **100 %** | **−0,074** | **inchangé** |
| 130 % | −0,324 | inchangé |
| 160 % | −0,496 | inchangé |

**⚠️ Correction d'une erreur d'analyse antérieure.** J'avais écarté le milieu/côté en
l'assimilant à de l'opposition de phase et en affirmant qu'il « s'effondre en mono ».
**C'est faux, et la mesure le montre** : en mono le côté s'annule et il reste `M` intact,
donc la somme mono est *rigoureusement invariante* sur toute la plage — 0,0 dB d'écart de
0 % à 160 %. Amplifier le côté n'enlève rien au mono ; ça n'y **ajoute** simplement rien.
La vraie conséquence de la largeur est que la part du signal qui n'existe qu'en stéréo
grandit : à 160 %, le mono ne porte plus que 6,0 dB sous le niveau stéréo, contre 3,3 dB à
la référence. On perd de l'ampleur en mono, pas de la matière.

C'est pour ça que la plage s'arrête à 160 % (corrélation −0,50) et pas plus haut : au-delà,
l'écart entre l'écoute casque et l'écoute mono devient assez grand pour piéger un mixage.

**La route des sous-ensembles de lignes disjoints n'a pas été prise.** Elle était correcte
mais inutile ici : le FDN distribue *déjà* les lignes paires à gauche et impaires à droite,
donc les deux canaux sont déjà disjoints — c'est ce qui produit la corrélation de −0,07 à
la référence, soit le plancher de la décorrélation. Aller plus loin demanderait de donner à
un canal les lignes courtes et à l'autre les longues, ce qui **déséquilibre le timbre entre
les deux canaux** (un côté brillant, un côté sombre). Le milieu/côté élargit sans toucher
au timbre de chaque canal. Gardé en réserve si la plage haute déçoit à l'oreille.

### 🔜 Piste reportée — élargissement par modulation (« chorussy »)

Proposée par l'utilisateur le 2026-09-07, après validation de STÉRÉO : *« on pourra aussi
ajouter une fonction chorussy pour donner une sensation plus large, ou simplement un mode
à part ».*

**Reportée hors v1**, conformément au principe de périmètre borné — la façade est complète
et validée, on ne rouvre pas.

⚠️ **À ne pas confondre avec CHORUSING, qui existe déjà dans le moteur.** Le vocabulaire
Lexicon appelle CHORUSING la modulation qui *randomise les temps de retard* à l'intérieur
du tank (`mod_profondeur`, `mod_hz`) — elle sert à casser les modes, pas à élargir. Ce que
propose l'utilisateur est autre chose : une modulation **en sortie**, désaccordée entre L
et R, qui crée de la décorrélation *variable dans le temps* là où le milieu/côté ne fait
que doser une décorrélation statique.

C'est d'ailleurs le bon complément à la limite identifiée sur STÉRÉO : à 100 % la
corrélation est déjà au plancher, donc élargir davantage ne peut venir que d'un mécanisme
qui *fabrique* de la décorrélation, pas d'un qui la dose.

**Question de forme à trancher le jour venu** : bouton sur la façade, ou mode à part comme
le suggère l'utilisateur. Un mode à part a un avantage — il assume que ce n'est plus le
LXP-1, alors qu'un bouton de façade suggère que si.

### ⛔ ÉPOQUE — écarté, et pas seulement reporté

Un axe qui aurait fait varier ensemble la fréquence interne, le mur anti-image, la
quantification et la pré-accentuation — de 1987 à « sans limites ».

**Écarté sur argument produit (utilisateur, 2026-09-07)** : le but de cette réverbe est
d'être **typée**. Un plugin vintage dont le second bouton dit « rends-la moins vintage » se
saborde : à zéro, LuxRev devient une réverbe algorithmique propre de plus, sur un terrain
où elle n'a aucun avantage. La contrainte de 1987 n'est pas un défaut désactivable, c'est
le produit.

Le mécanisme **reste dans le moteur** et ne bouge pas — ce n'est pas une commande, c'est
l'identité. Si l'axe revient un jour, ce sera de la machine vers **un peu plus ouvert**,
jamais jusqu'à l'absence de limites.

### Onglet Advanced (v2)

Façade fidèle et simple, plus un onglet développé façon Arturia LX-24, avec une
**représentation graphique** de la réverbe et de ses paramètres. Le vocabulaire est prêt
(Size, Diffusion, Definition, Treble Decay, Bass Multiply, Crossover, High/Low Cut, Shape,
Spread, Chorusing) — voir la note `LuxRev-1 - Vocabulaire Lexicon` du vault. **Hors v1.**

## 7. Étage d'entrée — ✅ tranché 2026-09-07

**Décision utilisateur** : *« on part sur une version custom neutre avec le bon THD ».*
Les trois captures NAM sont abandonnées comme source. Quatre lots de rendus ont donné des
fichiers **identiques au bit près** sur les trois positions de gain, et la seule mesure
exploitable donnait **36 % de THD contre 0,07 % au manuel** — un facteur 500. Elles ne
décrivent pas le LXP-1.

`_proto/preampli.py` construit donc un étage conforme à ce que le manuel de service **dit**
de l'étage réel, sans modéliser aucune capture.

**Deux déductions, et leur justification :**

1. **Distorsion symétrique, harmoniques impaires seulement.** Le manuel dit « aucun
   transformateur d'entrée », pot double R52, ampli-op uPC4570. Pas de transformateur, pas
   de tube, pas d'étage asymétrique : rien qui produise du rang 2. Ajouter une « chaleur »
   de rang 2 ici reviendrait à inventer un composant que le manuel dit absent.
2. **Calibration au haut de la plage.** « THD < 0,07 %, plage d'entrée −20 à +15 dBV » se
   lit comme une seule ligne : sur toute la plage, la THD reste sous 0,07 %. On cale donc
   0,07 % à **+15 dBV**, ce qui rend l'étage plus propre partout en dessous.

| entrée | THD | rang 2 | rang 3 |
|---:|---:|---:|---:|
| −20 dBV | 0,0000 % | 0,0000 % | 0,0000 % |
| 0 dBV | 0,0022 % | 0,0000 % | 0,0022 % |
| **+15 dBV** | **0,0700 %** | 0,0000 % | 0,0700 % |
| +20 dBV | 0,2204 % | 0,0000 % | 0,2204 % |

⚠️ **L'étage est désactivé par défaut** (`preampli_db=None`). Les 12 programmes ont été
accordés sans lui ; l'activer par défaut déplacerait la référence validée à l'oreille.
Vérifié : écart 0,000e+00 avec le rendu de référence.

⚠️ **Et il est inaudible à niveau normal — c'est correct.** Le grain du LXP-1 ne vient pas
de son préampli, il vient de la quantification 16 bits et de la pré-accentuation dans le
tank. Un préampli qui « s'entend » serait une faute de clone.

### ⛔ Ce qu'on ne peut PAS mesurer : la THD en wet

Le manuel spécifie « THD < 0,07 % **en wet** ». Impossible à reproduire comme test :

| condition | THD mesurée |
|---|---:|
| chaîne complète, **préampli désactivé** | 0,572 % |
| préampli actif, entrée 0 dBV | 0,572 % |
| préampli actif, entrée +15 dBV | 0,585 % |
| préampli actif, entrée +20 dBV | 0,737 % |

Le plancher de 0,57 % est là **sans préampli**, et il ne bouge pas avec le niveau. Ce n'est
donc pas de la distorsion : c'est l'énergie propre de la réverbe aux fréquences
harmoniques, produite par la modulation aléatoire des lignes. Vérifié aussi que ce n'est
pas la quantification — 24 et 32 bits donnent le même chiffre.

**Conclusion : mesurer une THD à travers une réverbe modulée ne mesure pas la distorsion.**
La seule mesure valide de l'étage est celle faite **en isolation**, tableau ci-dessus. La
contribution du préampli ne devient visible qu'au-dessus de +15 dBV, c'est-à-dire au-delà
de la plage d'entrée du manuel — exactement ce qu'on attend d'un étage conforme.

## 8. Ce qu'aucune mesure ne donnera

- **La modulation du tank.** Une IR est un instantané figé, et une capture au sweep étale
  la modulation jusqu'à la rendre invisible. Réglage à l'oreille, sur du matériau à graine
  fixe (`_proto/signaux/30_bruit_rose.wav`) pour que l'A/B soit reproductible.
- **Le comportement des boutons.** Aucune des 14 captures ne dit ce que fait Decay ou Size.
- **Les longueurs de lignes exactes.** Le motif des premières réflexions les contraint,
  il ne les détermine pas.

---

## 9. Le prototype — où il en est

`_proto/moteur.py` implémente la topologie du § 3 ; `_proto/accorder.py` en dérive les
paramètres depuis les cibles, rend une IR, **la remesure avec le même banc que les IR
d'origine** et corrige. Cible et résultat passent par le même code : sinon l'écart
contiendrait autant de différence de méthode que de différence de son.

Résultats sur les 12 programmes (détail : [`_proto/accord/ACCORD.md`](_proto/accord/ACCORD.md)) :

| Cible | Écart obtenu |
|---|---|
| **T60 global** | **±1 % sur les 12** |
| **Amortissement HF** | **±4 % sur les 10 réverbes** |
| **Temps de mélange** | ±8 % sur 6 réverbes ; −36 % (`hall_d`), +25 % (`large1`), −20 % (`plate_d`) |

**Deux leçons du prototypage**, toutes deux inscrites dans le code :

1. **Une boucle longue ne peut pas décroître vite.** À `taille` 1,0 un bras fait 181 ms ;
   demander un T60 de 0,5 s revient à 2,7 tours de boucle, et la mesure sortait 76 % trop
   longue. `taille` est donc contrainte par le T60 visé (≥ 14 tours), ce qui est aussi
   physiquement juste : petite salle = lignes courtes.
2. **La diffusion se règle par bissection, pas par correction multiplicative.** Diffusion
   et taille agissent toutes deux sur le mélange ; les faire bouger ensemble tournait en
   rond. Taille figée + bissection exploite la seule chose dont on soit sûr : plus de
   diffusion = mélange plus rapide.

Les trois écarts résiduels sur le mélange sont une **limite de topologie, pas de réglage** :
`hall_d` finit à une diffusion de 0,46, contre un plancher à 0,45, et mélange encore trop
vite. Il faudra un nombre d'étages de diffusion variable par programme, pas un meilleur
coefficient. À traiter au portage.

## 10. Décision ouverte

Une seule reste : le **nombre d'étages de diffusion**, à rendre variable pour rattraper
`hall_d`, `large1` et `plate_d`.

**Pistes reportées** — programmes Delay (décidé) ; chorus/pitch dans le tank
(territoire LXP-15) ; ducking sur sidechain ; presets utilisateur ; modes true-stereo
exploitant les 4 canaux des IR (la matrice L→L / L→R / R→L / R→R est dans les fichiers,
non exploitée en v1).

---

## 11. Sources

- ⛔ **Brevets Griesinger/Lexicon sur la réverbe : ILS N'EXISTENT PAS.** Vérifié le
  2026-09-07 sur Google Patents. `inventor:(David Griesinger)` donne 21 résultats, tous
  Harman (2002+) sauf **EP0323904B1** (Lexicon Inc., priorité 1988) — qui porte sur une
  matrice de reproduction surround, pas sur la réverbération. `assignee:Lexicon` renvoie
  3 017 brevets de *Lexicon **Pharmaceutical*** : une autre société.
  **Lexicon a gardé ses algorithmes en secret industriel**, ce qui est cohérent — breveter
  impose de divulguer, et du microcode en ROM est déjà difficile à rétro-concevoir.
  ⚠️ Cette note corrige ce qui était écrit ici depuis le premier jour du projet : les
  brevets Griesinger avaient été présentés comme « la source la plus proche d'un code
  source qui existe légalement ». C'était faux.
- ✅ **US4181820, Blesser & Bader (EMT), 1977** — même lignée, et il *contient* les
  schémas. C'est la meilleure source de structure dont on dispose. Voir § 3.
- **Jon Dattorro**, *Effect Design Part 1: Reverberator and Other Filters*, JAES 45(9),
  1997 — topologie de plate dérivée du travail de Griesinger chez Lexicon, longueurs de
  lignes et coefficients publiés.
- **William Gardner**, *Reverberation Algorithms*, in *Applications of DSP to Audio and
  Acoustics* — allpass imbriqués, structures de room, montée de densité.
- **Abel & Huang**, *A Simple, Robust Measure of Reverberation Echo Density*, AES 2006 —
  la métrique de temps de mélange utilisée par le banc.
- **Les 14 IR**, licence T3K : usage dans un logiciel et publication des sorties autorisés
  sans redevance ; redistribution du fichier interdite. Elles ne quittent pas `_proto/`.
