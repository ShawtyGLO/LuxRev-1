// LuxRev-1 — etage d'entree.
//
// Transcription de _proto/preampli.py. Ce module ne modelise PAS une capture :
// il construit un etage conforme a ce que le manuel de service DIT de l'etage
// reel. Les trois captures NAM ont ete abandonnees comme source — quatre lots de
// rendus identiques au bit pres sur les trois positions de gain, et la seule
// mesure exploitable donnait 36 % de THD contre 0,07 % au manuel, soit un
// facteur 500. Elles ne decrivent pas le LXP-1.
//
// CE QUE LE MANUEL DONNE
//   - « Aucun transformateur d'entree » : 1/4" asymetrique, 50 kOhm, pot double
//     R52, ampli-op uPC4570.
//   - THD < 0,07 %, plage d'entree -20 a +15 dBV.
//
// CE QUE J'EN DEDUIS, ET POURQUOI
//   1. La distorsion est SYMETRIQUE, donc harmoniques IMPAIRES. C'est de la
//      contre-reaction d'ampli-op bipolaire : pas de transformateur, pas de tube,
//      pas d'etage asymetrique — rien qui produise du rang 2. Modeliser une
//      chaleur de rang 2 ici serait inventer un composant que le manuel dit
//      absent.
//   2. Le point de calibration est le HAUT de la plage. « THD < 0,07 %, plage
//      -20 a +15 dBV » se lit comme une seule ligne : sur toute la plage, la THD
//      reste sous 0,07 %. On cale donc 0,07 % a +15 dBV, ce qui rend l'etage plus
//      propre partout en dessous.
//
// ⚠️ A niveau normal cet etage est INAUDIBLE, et c'est correct. Le grain du
// LXP-1 ne vient pas de son preampli, il vient de la quantification 16 bits et
// de la pre-accentuation dans le tank. Un preampli qui « s'entend » serait une
// faute de clone.

#pragma once

#include <algorithm>
#include <cmath>

namespace luxrev
{

/** Coefficient d'ecretage, calibre par _proto/preampli.py::calibrer().

    Il donne exactement 0,07 % de THD a +15 dBV. Fige ici plutot que recalcule :
    la valeur vient d'une bissection sur une mesure de THD, et la refaire au
    demarrage du plugin couterait du temps pour retomber sur le meme nombre. */
inline constexpr double kEcretageK = 0.011536683636704733;

/** Amplitude crete correspondant a +15 dBV, le haut de la plage du manuel.

    ⚠️ Convention de niveau, et elle merite d'etre explicite : la pleine echelle
    numerique (1,0) est posee sur le HAUT de la plage d'entree de la machine.
    C'est ce qui rend l'etage fidele — pousser le plugin a 0 dBFS revient a
    pousser le LXP-1 a +15 dBV, donc a l'endroit ou son etage d'entree commence
    tout juste a ecreter. Sans cette convention, on aurait un ecretage soit
    inatteignable, soit permanent. */
inline constexpr double kAmpPleineEchelle = 7.9482229; // 10^(15/20) * sqrt(2)

//==============================================================================
class Preampli
{
public:
    /** `gainDb` est le pot d'entree R52. Il n'y a pas de reglage de « drive »
        separe sur la machine : le seul moyen d'entrer dans l'ecretage est de
        monter le niveau, comme sur l'appareil. */
    void poserGain (double gainDb) noexcept
    {
        gain = std::pow (10.0, gainDb / 20.0);
    }

    void activer (bool oui) noexcept { actif = oui; }
    bool estActif() const noexcept { return actif; }

    inline float traiter (float x) const noexcept
    {
        if (! actif) return x;
        // Ecretage doux symetrique. tanh est le profil d'un etage a
        // contre-reaction : lineaire au centre, compression progressive, jamais
        // de coin franc.
        const double v = (double) x * gain * kAmpPleineEchelle;
        return (float) (std::tanh (kEcretageK * v) / (kEcretageK * kAmpPleineEchelle));
    }

private:
    double gain = 1.0;
    bool actif = true;
};

} // namespace luxrev
