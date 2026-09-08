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
// ⚠️ CORRECTION (2026-09-08), apres lecture du manuel de service du LXP-5. Celui-ci
// separe « THD wet < 0,02 % » et « THD dry < 0,01 % » : le chiffre du manuel est
// donc une THD du CHEMIN TRAITE ENTIER, pas de l'etage d'entree seul. Les 0,07 %
// du LXP-1 sont de la meme famille. L'etage d'entree reel est donc PLUS PROPRE que
// ce qu'on modelise ici. Consequence pratique nulle — 0,07 % est inaudible, et
// DRIVE a zero reste la reference — mais le raisonnement etait faux et il fallait
// le dire plutot que de laisser un commentaire qui affirme le contraire.
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
    /** DRIVE, de 0 a 1.

        ⚠️ Ce n'est plus un gain d'entree, et c'est une decision de produit, pas
        une correction. Un gain d'entree ne sert plus a rien depuis que le fondu
        sec/humide est a puissance constante : il ne fait que deplacer le niveau.
        Un drive, lui, POUSSE dans l'etage puis defait le gain juste apres — le
        niveau ne bouge pas, seule la quantite d'harmoniques change.

        ⚠️ Et il faut le dire : au-dela de quelques pour cent, ce n'est plus le
        LXP-1. Le manuel donne moins de 0,07 % de THD sur TOUTE la plage
        d'entree. A drive nul on est exactement sur cette valeur ; au-dela, on
        est dans une commande de couleur, au meme titre que COLOR ou TILT.
    */
    void poserDrive (double drive) noexcept
    {
        // +18 dB au maximum : la THD croit comme le carre du niveau, donc de
        // 0,07 % a environ 4,5 %. Au-dela l'etage devient une distorsion, pas
        // une couleur.
        drive = std::clamp (drive, 0.0, 1.0);
        gain = std::pow (10.0, drive * 18.0 / 20.0);
        compensation = 1.0 / gain;
    }

    /** Le niveau reellement presente a l'etage, en multiple de la pleine echelle.
        C'est lui que le bandeau de headroom doit afficher : sans le drive, le
        temoin de plafond ne s'allumerait jamais. */
    double facteurNiveau() const noexcept { return gain; }

    void activer (bool oui) noexcept { actif = oui; }
    bool estActif() const noexcept { return actif; }

    inline float traiter (float x) const noexcept
    {
        if (! actif) return x;
        // Ecretage doux symetrique. tanh est le profil d'un etage a
        // contre-reaction : lineaire au centre, compression progressive, jamais
        // de coin franc.
        const double v = (double) x * gain * kAmpPleineEchelle;
        return (float) (std::tanh (kEcretageK * v)
                          / (kEcretageK * kAmpPleineEchelle) * compensation);
    }

private:
    double gain = 1.0, compensation = 1.0;
    bool actif = true;
};

} // namespace luxrev
