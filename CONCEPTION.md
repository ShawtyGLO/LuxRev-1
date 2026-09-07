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
le temps ne suit pas une exponentielle. Implémentation : le tank tourne normalement, un
étage d'enveloppe programmée est appliqué en sortie. Le profil exact est déjà dans le
champ `enveloppe_db` de leur JSON — c'est une cible directe, à recopier.

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

## 7. Étage d'entrée — à reprendre

Trois captures NAM (`Input Gain 2 / 5 / 7`) dans
`Impulse Responce LXP-1/Préamp IR NAM/`. Ce sont des **WaveNet**, donc non linéaires :
trois positions de gain ne se capturent que s'il y a de la saturation à capturer.

**Décision : ne pas réimplémenter le réseau.** L'architecture est une variante récente
(`SlimmableContainer`, 2 sous-modèles sélectionnés par niveau, `kernel_sizes` variables,
`bottleneck`, `layer1x1`) dont l'ordre d'aplatissement des poids n'est pas documenté dans
le fichier. Une implémentation numpy serait invérifiable — sans référence pour la comparer,
une erreur subtile produirait des courbes plausibles et fausses, qui empoisonneraient
silencieusement les cibles. On mesure au rendu, comme on mesurerait du matériel.

Protocole complet : [`_proto/PROTOCOLE_MESURE.md`](_proto/PROTOCOLE_MESURE.md).
Signaux déjà générés dans `_proto/signaux/`. 12 rendus à faire, ~1 h.

---

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
