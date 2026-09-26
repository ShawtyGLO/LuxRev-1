// LuxRev-1 — le moteur de reverbe.
//
// Transcription temps reel de _proto/moteur.py. Le prototype Python reste la
// reference : toute divergence est un bug, pas une optimisation.
//
// La chaine, dans l'ordre du manuel de service du LXP-1 :
//
//   pre-accentuation 68 us -> anti-repliement 15,2 kHz -> ADC 16 bits a
//   31,25 kHz -> [ FDN 8 lignes ] -> DAC 16 bits -> bloqueur d'ordre zero ->
//   correction sinc -> mur anti-image 17 kHz -> STEREO -> TILT -> FIR de
//   voicing -> de-accentuation
//
// ⚠️ Le reechantillonnage n'est pas une commodite. La machine travaille
// reellement a 31,25 kHz et c'est ce qui borne sa bande a 15,6 kHz par
// construction. Le passage retour est un BLOQUEUR D'ORDRE ZERO, pas un
// interpolateur propre : le PCM54 maintient chaque echantillon jusqu'au suivant
// et produit donc des images a 31250-f que le filtre analogique n'enleve qu'en
// partie. Le contenu mesure entre 15 et 17 kHz sur les IR d'origine vient de la,
// et de nulle part ailleurs — avec un reechantillonnage propre, le prototype
// sortait 8 dB trop sombre a 16 kHz.
//
// ⚠️ DEUX PHASES, et c'est structurel :
//   preparer()  alloue une fois, pour le PIRE cas (COLOR au maximum de taille).
//   appliquer() pose les reglages SANS jamais allouer.
// Sans cette separation, chaque mouvement de COLOR reconstruirait huit lignes a
// retard dans le fil audio.

#pragma once

#include "Conception.h"
#include "Preampli.h"
#include "../Generated/Programmes.h"

namespace luxrev
{

// Bornes de la macro COLOR sur `taille`. Elles fixent la memoire a reserver.
inline constexpr float kTailleMax = 3.2f;
inline constexpr float kProfondeurMax = 26.0f;   // vers le cremeux
inline constexpr float kPredelaiMaxMs = 246.0f;  // Program table du manuel
inline constexpr float kFenetreDiffMaxMs = 400.0f;
// Ecart entre les deux bloqueurs, en periodes d'echantillonnage interne. Le
// convertisseur unique du LXP-5 sert l'entree, puis une voie, puis l'autre : un
// tiers de periode, soit 10,7 us a 31,25 kHz. Manuel de service, diagramme
// « A/D SAR Conversion Timing ».
// ⚠️ Redefinissable A LA COMPILATION seulement, et uniquement pour que les bancs de
// tools/ puissent mesurer l'avant et l'apres. Ce n'est PAS un reglage : le plugin le
// compile toujours a 1/3, parce que la machine n'avait pas le choix non plus.
#ifndef LUXREV_TIERS_CONVERTISSEUR
 #define LUXREV_TIERS_CONVERTISSEUR (1.0 / 3.0)
#endif
inline constexpr double kTiersConvertisseur = LUXREV_TIERS_CONVERTISSEUR;

// ⚠️ QUELLE VOIE ARRIVE EN PREMIER — et ici on s'ecarte du document.
//
// Le manuel de service donne l'ordre : A/N, puis DROITE, puis GAUCHE. Sur la
// machine c'est donc la droite qui arrive la premiere. Le projet a choisi
// l'inverse (decide 2026-09-09) : c'est la GAUCHE qui mene.
//
// Ce que ca coute est nul a l'oreille et non nul sur le papier. Un ecart de
// 10,7 us decorrele exactement pareil dans les deux sens — aucune mesure ne
// les distingue, corrolation, somme mono et spectre sont identiques. Ce qui
// change, c'est ce qu'on peut AFFIRMER : sur ce point precis le plugin ne
// reproduit plus la machine, il la reflete. Repasser a false suffit a revenir.
inline constexpr bool kGaucheEnPremier = true;

// ⚠️ QUANTIFICATION DANS LA BOUCLE — essai, desactive par defaut.
//
// La memoire audio du LXP-1 fait 64K x 16 bits (les deux manuels de service le
// disent). Dans la machine, le contenu des lignes a retard EST donc quantifie a
// chaque tour, pas seulement en entree et en sortie du tank. C'est ce que ce
// drapeau ajoute : `e`, ce qui est ECRIT dans chaque ligne, passe par le
// convertisseur.
//
// ⛔ ET LA MESURE NE PEUT PAS ARBITRER CONTRE LA MACHINE. Les 14 IR ont un
// plancher a -137 dB avec des queues qui decroissent proprement jusqu'au bout :
// la deconvolution du sweep a efface le bruit du LXP-1. Une machine 16 bits qui
// annonce 85 dB ne peut pas produire 140 dB de plage. Les IR ne portent donc
// AUCUNE information sur son grain.
//
// La decision se prend donc sur l'ecart AUDIBLE entre les deux versions, pas sur
// la fidelite de procede : l'erreur recircule et s'amplifie par le gain de
// boucle, ce qui releve le plancher des queues longues et peut creer des cycles
// limites — pour un benefice qui reste a etablir.
#ifndef LUXREV_QUANT_BOUCLE
 #define LUXREV_QUANT_BOUCLE 0
#endif

//==============================================================================
/** Tous les parametres du moteur. Miroir des arguments de Moteur.__init__.

    ⚠️ Aucune valeur accordee ici : ce sont des reglages, pas des cibles. Les
    valeurs viennent de Generated/Programmes.h, produites par accorder.py — seul
    endroit ou une cible mesuree sur le LXP-1 entre dans la conception. Meme
    regle qu'en Python, et pour la meme raison.
*/
struct Reglages
{
    float predelaiMs    = 0.0f;
    float taille        = 1.0f;
    float diffusion     = 0.75f;
    float t60S          = 2.0f;
    float ratioHf       = 0.7f;
    float lpSortieHz    = 6000.0f;
    float murHz         = 17000.0f;
    float modProfondeur = 8.0f;
    float modHz         = 0.7f;
    int   bits          = 16;
    bool  emphase       = true;
    int   nDiffuseurs   = 4;
    float priseTot      = 0.0f;
    float mixage        = 1.0f;
    float gainPrises    = 0.0f;
    float etalement     = 0.0f;
    int   nLignes       = kNFdn;
    float forme         = 0.0f;
    float pentePrises   = 0.0f;
    float fenetreDiffMs = 60.0f;
    float trebleHz      = 20000.0f;
    float predelaiFb    = 0.0f;
    float ratioBass     = 1.0f;
    float crossBassHz   = 400.0f;
    float hpSortieHz    = 0.0f;
    float tiltDb        = 0.0f;
    float tiltPivotHz   = 700.0f;
    float stereo        = 1.0f;
    float drive         = 0.0f;      // 0 = l'etage du manuel, 1 = +18 dB dedans
    bool  preampliActif = true;
};

/** Charge les reglages d'un programme accorde. */
inline Reglages reglagesDe (const Programme& p)
{
    Reglages r;
    r.predelaiMs    = p.predelaiMs;    r.lpSortieHz    = p.lpSortieHz;
    r.murHz         = p.murHz;         r.taille        = p.taille;
    r.t60S          = p.t60S;          r.diffusion     = p.diffusion;
    r.mixage        = p.mixage;        r.ratioHf       = p.ratioHf;
    r.ratioBass     = p.ratioBass;     r.crossBassHz   = p.crossBassHz;
    r.gainPrises    = p.gainPrises;    r.etalement     = p.etalement;
    r.forme         = p.forme;         r.pentePrises   = p.pentePrises;
    r.modProfondeur = p.modProfondeur; r.modHz         = p.modHz;
    r.priseTot      = p.priseTot;      r.fenetreDiffMs = p.fenetreDiffMs;
    r.trebleHz      = p.trebleHz;      r.predelaiFb    = p.predelaiFb;
    r.hpSortieHz    = p.hpSortieHz;
    return r;
}

//==============================================================================
/** Courbe de compensation, normalisee pour valoir EXACTEMENT 1,0 au centre.

    ⚠️ Sans cette normalisation, COLOR a 50 % ne redonne pas le moteur accorde.
    Les courbes sont obtenues par regression, donc leur point central sort a 1,01
    ou 0,995 — assez pour decaler le T60 de 1 % au milieu de la course. Inaudible,
    mais c'est la promesse du bouton qui tombe : le son a position d'origine ne
    doit pas s'ecarter des mesures des IR. Le centre est un point fixe, pas un
    resultat d'ajustement.
*/
inline float lireCompensation (const float* points, float r) noexcept
{
    const float milieu = points[2];                        // COLOR = 0,5
    if (std::abs (milieu) < 1e-9f) return 1.0f;

    const float x = std::clamp (r, 0.0f, 1.0f) * 4.0f;     // 5 points, 4 intervalles
    const int   k = std::min ((int) x, 3);
    const float f = x - (float) k;
    return (points[k] + (points[k + 1] - points[k]) * f) / milieu;
}

/** COLOR : 50 % = le LXP-1 mesure, en dessous plus cremeux, au-dessus plus
    « metallique synthetique ».

    ⚠️ Par INTERPOLATION vers des extremes, pas par multiplication. L'accordeur
    ne laisse aucune marge — `mixage` sort a 0,05 et `taille` touche son plafond —
    donc multiplier ne bougeait quasiment rien d'un cote et saturait de l'autre.

    Les deux compensations garantissent que COLOR change le CARACTERE (densite
    modale, grain) sans toucher a la DUREE ni au TIMBRE. C'est verifiable a
    l'ecran : tourner COLOR ne doit pas deplacer la reponse en frequence.
*/
inline Reglages macroColor (const Reglages& base, float color, const Programme& p)
{
    const float r = std::clamp (color, 0.0f, 1.0f);
    const float u = (r - 0.5f) * 2.0f;      // -1 cremeux, 0 reference, +1 metallique
    const float k = std::abs (u);

    Reglages q = base;
    auto vers = [k] (float actuel, float but) { return actuel + (but - actuel) * k; };

    if (u < 0.0f)                            // vers le cremeux
    {
        q.taille        = std::clamp (vers (base.taille, base.taille * 2.6f), 0.06f, kTailleMax);
        q.mixage        = vers (base.mixage, 1.0f);
        q.modProfondeur = vers (base.modProfondeur, kProfondeurMax);
        q.nLignes       = kNFdn;
    }
    else                                     // vers le metallique
    {
        q.taille        = std::clamp (vers (base.taille, std::max (base.taille * 0.45f, 0.30f)),
                                      0.06f, kTailleMax);
        q.mixage        = vers (base.mixage, 0.06f);
        q.modProfondeur = vers (base.modProfondeur, 0.0f);
        q.nLignes       = (int) std::lround (8.0f - k * 4.0f);
    }

    q.t60S      *= lireCompensation (p.compensationColor, r);
    q.lpSortieHz = std::clamp (q.lpSortieHz * lireCompensation (p.compensationLp, r),
                               1000.0f, 22000.0f);
    return q;
}

//==============================================================================
class Moteur
{
public:
    static constexpr int kNPrisesFf = 12;
    /** Avance de l'anneau interne sur la lecture, en echantillons internes.

        ⚠️ ELLE DOIT COUVRIR LE REGARD EN AVANT DU DECIMATEUR. Le noyau lit
        kTaps echantillons d'hote autour du point vise, donc la production ne
        peut demarrer qu'une fois ce contexte disponible — elle prend kTaps
        echantillons de retard une fois pour toutes, au premier bloc. Si la
        lecture part sans cette avance, elle double la production et n'y revient
        JAMAIS : le bloqueur relit alors le dernier echantillon disponible sur la
        fin de chaque bloc.

        Mesure du defaut, avant correction : marge de -38 echantillons internes,
        stable quelle que soit la taille de bloc (64 a 1024). Cela figeait ~11 %
        de chaque bloc — inaudible comme repetition, puisque le palier traverse
        ensuite les filtres de reconstruction qui le rendent variable, mais
        parfaitement audible comme gresillement au rythme du bloc.
    */
    int slack = 64;

    /** Court-circuite le tank : seule la chaine de conversion travaille.

        ⚠️ Crochet de VERIFICATION, pas une fonctionnalite. Il sert a repondre a
        une question qu'on ne peut pas trancher en regardant le rendu complet :
        un ecart de timbre vient-il de la conversion (emphase, anti-repliement,
        decimation, bloqueur, correction sinc, mur) ou du reseau lui-meme ?
        Sans ce partage, on corrige au hasard. */
    bool contournerTank = false;

    /** ⚠️ LA PORTE DECLENCHEE A ETE RETIREE, et c'etait une erreur de nature.

        J'avais implemente Gate et Inverse comme une enveloppe relancee a chaque
        attaque du signal d'entree, avec detection et seuil. Deux manuels de
        l'epoque disent que ce n'est pas ca — voir BancNonLineaire dans
        Elements.h. La forme EST la reponse impulsionnelle du programme : systeme
        lineaire et invariant, meme motif pour chaque echantillon d'entree. C'est
        exactement pour cette raison que la machine n'a aucun seuil a regler, et
        pourquoi le mien ne donnait jamais un resultat propre quel que soit son
        reglage.
    */
    /** Recueille le signal a la frequence INTERNE, juste apres decimation.
        Crochet de verification : il coupe la chaine en deux, ce qui permet de
        dire si un ecart vient de la descente vers 31,25 kHz ou de la remontee. */
    std::vector<float>* sondeInterne = nullptr;

    /** Coupe l'etage d'enveloppe. Crochet de verification : il sert a mesurer ce
        que le tank fait SEUL, ce dont on a besoin pour calculer le profil
        correctif de Gate et Inverse. */
    bool porteActive = true;

    /** Marge minimale observee entre production et consommation, en echantillons
        internes. Crochet de diagnostic : si elle atteint zero, le bloqueur relit
        le dernier echantillon et la sortie se fige. */
    long long margeMinimale = 1 << 30;

    //--------------------------------------------------------------------------
    /** Alloue tout, pour le pire cas. Jamais appele depuis le fil audio. */
    void preparer (double sampleRateHote)
    {
        fsHote = sampleRateHote;
        ratio  = fsHote / kFsInterne;

        interp.preparer (ratio);
        // le retard pris une fois par le regard en avant du decimateur, plus une
        // marge pour la gigue de taille de bloc
        slack = (int) std::ceil ((double) Decimateur::kTaps / ratio) + 16;
        anneauIn.assign ((size_t) kTailleAnneau, 0.0f);
        anneauL.assign  ((size_t) kTailleAnneau, 0.0f);
        anneauR.assign  ((size_t) kTailleAnneau, 0.0f);

        preSos = emphase (fsHote, false);
        deSosL = deSosR = emphase (fsHote, true);

        // Anti-repliement (entree) et anti-image (sortie), chacun a la frequence
        // ou il travaille reellement. L'anti-repliement doit rester sous Nyquist
        // interne (15,6 kHz) sans manger la bande utile : la machine passe
        // 15 kHz a -3,8 dB seulement.
        Biquad tmp[Chaine::kMaxSections];
        aa.poser (tmp, butterworthPasseBas (tmp, 6, 15200.0, fsHote));

        // Deux passe-tout etales sur le spectre : le dephasage varie avec la
        // frequence, ce qui se lit comme de la largeur au lieu d'un trou.
        const Biquad ap[2] = { passeTout1 (320.0, fsHote), passeTout1 (2600.0, fsHote) };
        apCote.poser (ap, 2);

        const auto sinc = correctionSinc (fsHote);
        sincL.preparer ((int) sinc.size()); sincL.poser (sinc.data(), (int) sinc.size());
        sincR.preparer ((int) sinc.size()); sincR.poser (sinc.data(), (int) sinc.size());
        voicingL.preparer (kTailleFirVoicing);
        voicingR.preparer (kTailleFirVoicing);

        // --- allocations dimensionnees au PIRE cas, une fois pour toutes
        const double fs = kFsInterne;
        // ⚠️ Le pre-delai est une ligne a lecture fractionnaire, pas un simple
        // tampon circulaire : le bouton Pre-Delay se tourne en continu, et une
        // longueur qui saute d'un echantillon a l'autre claque a chaque bloc.
        predelai.preparer ((int) std::lround (kPredelaiMaxMs * 1e-3 * fs) + 8, 4);

        for (int k = 0; k < 4; ++k)
            diff[(size_t) k].preparer (transpose (kDiffuseurLongueurs[k], fs), 0.0f);

        // ⚠️ La marge doit couvrir l'EXCURSION REELLE du modulateur, pas la
        // profondeur nominale. Le decalage vaut
        //     clamp(bruit, -2,5, +2,5) x profondeur x 0,4  +  profondeur x 0,5
        // soit une plage de -0,5 a +1,5 fois la profondeur — donc la lecture peut
        // partir a une fois et demie la profondeur AU-DELA du retard nominal.
        //
        // A COLOR = 0, `taille` et la modulation sont toutes deux au maximum :
        // profondeur = 26 x 3,2 = 83,2 echantillons, excursion jusqu'a +124,8,
        // pour 85 de marge allouee. La lecture depassait donc de 40 echantillons
        // dans du tampon perime — ce qui craque, et seulement a ce reglage.
        const int margeMax = (int) (1.5 * kProfondeurMax * kTailleMax) + 8;
        // ~80 ms de glissement : assez lent pour ne pas claquer, assez rapide
        // pour que le bouton reponde.
        const double kGliss = 1.0 - std::exp (-1.0 / (0.080 * fs));
        predelai.poserGlissement (kGliss);
        for (int k = 0; k < kNFdn; ++k)
        {
            lignes[(size_t) k].preparer (transpose (kFdnLongueurs[k], fs, kTailleMax), margeMax);
            lignes[(size_t) k].poserGlissement (kGliss);
        }

        // L'etalement reste sous le plus court des retards de boucle (contrainte
        // portee sur la FIG. 2d du brevet Jot), donc borne par la ligne la plus
        // courte a la taille maximale.
        const int tauMax = transpose (kFdnLongueurs[0], fs, kTailleMax);
        for (int k = 0; k < kNFdn; ++k)
            injections[(size_t) k].preparer (std::max ((int) (tauMax * 0.95), 1));

        // le banc non lineaire couvre 3 s de profil a la frequence interne
        // 2x la duree du profil : Decay peut le dilater jusque-la
        banc.preparer ((int) (kPasEnveloppeMs * 1e-3 * fs * 600.0 * 2.0) + 16);

        ffN = (int) (kFenetreDiffMaxMs * 1e-3 * fs) + 2;
        ffBuf.assign ((size_t) ffN, 0.0);

        vider();
    }

    void vider()
    {
        std::fill (anneauIn.begin(), anneauIn.end(), 0.0f);
        std::fill (anneauL.begin(),  anneauL.end(),  0.0f);
        std::fill (anneauR.begin(),  anneauR.end(),  0.0f);
        ecritsIn = 0; ecritsInterne = slack; posLecture = 0.0; posSortie = 0.0;
        margeMinimale = 1 << 30;

        preSos.vider(); deSosL.vider(); deSosR.vider();
        aa.vider(); murL.vider(); murR.vider(); tiltL.vider(); tiltR.vider(); apCote.vider();
        sincL.vider(); sincR.vider(); voicingL.vider(); voicingR.vider();

        predelai.vider(); predelai.caler();
        for (auto& d : diff)       d.vider();
        for (auto& l : lignes)     { l.vider(); l.caler(); }
        for (auto& d : injections) d.vider();
        std::fill (ffBuf.begin(), ffBuf.end(), 0.0); ffI = 0;
        for (int k = 0; k < kNFdn; ++k) { etats[k] = sorties[k] = eBass[k] = lfoEtat[k] = 0.0; }
        eTreble = fbEtat = sL = sR = hL = hR = 0.0;
        banc.vider();
    }

    //--------------------------------------------------------------------------
    /** Pose un jeu de reglages SANS allouer. Appelable depuis le fil audio. */
    void appliquer (const Reglages& r, const Programme* prog)
    {
        const bool nouveauProgramme = (prog != programme);
        reg = r;
        programme = prog;

        const double fs = kFsInterne;
        nLignes = std::clamp (reg.nLignes, 2, kNFdn);

        // ⚠️ Profondeur RELATIVE a la taille. En absolu, 8 echantillons sur la
        // ligne d'un grand hall font 0,55 % et sur une petite salle 2,3 %, soit
        // quatre fois plus de desaccord — ce qui s'entendait comme un flanger
        // sur les programmes courts.
        profondeur = reg.modProfondeur * reg.taille;

        predelai.poserNominal (std::max ((int) std::lround (reg.predelaiMs * 1e-3 * fs), 1));
        kTreble = coefLp (reg.trebleHz, fs);

        nDiff = std::clamp (reg.nDiffuseurs, 0, 4);
        for (int k = 0; k < nDiff; ++k)
            diff[(size_t) k].poserCoef (std::min (reg.diffusion * (kDiffuseurCoeffs[k] / 0.75f), 0.92f));

        for (int k = 0; k < nLignes; ++k)
            lignes[(size_t) k].poserNominal (transpose (kFdnLongueurs[k], fs, reg.taille));

        // Signes alternes sur les prises : deux sorties decorrelees sans ajouter
        // de retard — la mesure donne une correlation L/R nulle sur les 14 IR.
        for (int k = 0; k < nLignes; ++k)
            signes[k] = ((k / 2) % 2 == 0) ? 1.0 : -1.0;

        // Injection retardee, une par ligne (brevet Jot US5491754, FIG. 2d, bloc
        // « FIRST ECHOS CONTROL »). L'entree n'arrive pas simultanement dans les
        // huit lignes : les premieres arrivees en sortie sont donc directement ce
        // motif, obtenu sans section de reflexions separee.
        int tauMin = lignes[0].nominalEch();
        for (int k = 0; k < nLignes; ++k) tauMin = std::min (tauMin, lignes[(size_t) k].nominalEch());
        const double etal = std::clamp (reg.etalement, 0.0f, 0.95f) * tauMin;
        for (int k = 0; k < nLignes; ++k)
            injections[(size_t) k].poserLongueur (
                std::max ((int) std::lround (etal * k / std::max (nLignes - 1, 1)), 1));

        // SHAPE au sens ou Lexicon l'emploie : « Spread est une fenetre de temps
        // pendant laquelle le signal est injecte dans la reverbe ; Shape controle
        // COMMENT il l'est. » Poids triangulaire centre sur `forme` : a 0 tout
        // entre au debut, a 1 tout entre a la fin. Un hall demande l'inverse d'un
        // front-loaded — c'est ce qui bloquait `hall_b`.
        const float f = std::clamp (reg.forme, 0.0f, 1.0f);
        for (int k = 0; k < nLignes; ++k)
        {
            const double u = (double) k / (double) std::max (nLignes - 1, 1);
            bInj[k] = std::max (1.0 - std::abs (u - (double) f) * 2.2, 0.03);
        }

        // Banc de retards en PARALLELE, douze prises sur la fenetre de diffusion.
        {
            const double fen = std::clamp (reg.fenetreDiffMs, 5.0f, kFenetreDiffMaxMs) * 1e-3 * fs;
            for (int k = 0; k < kNPrisesFf; ++k)
                ffTaps[k] = std::min (std::max ((int) (fen * std::pow ((k + 1.0) / kNPrisesFf, 1.3)), 1),
                                      ffN - 1);
            // Pente en dB entre la premiere et la derniere prise. Positive =
            // arrivees precoces attenuees, donc enveloppe qui monte : dans une
            // salle, la densite d'arrivees CROIT avec le temps.
            for (int k = 0; k < kNPrisesFf; ++k)
                poidsPrises[k] = std::pow (10.0,
                    -reg.pentePrises * (kNPrisesFf - 1 - k) / (kNPrisesFf - 1) / 20.0);
        }

        reglerT60();

        lpOut = coefLp (reg.lpSortieHz, fs);
        kHp   = reg.hpSortieHz > 0.0f ? coefLp (reg.hpSortieHz, fs) : 0.0f;

        Biquad tmp[Chaine::kMaxSections];
        const int n = butterworthPasseBas (tmp, 6, reg.murHz, fsHote);
        murL.poser (tmp, n);
        murR.poser (tmp, n);

        gainInterne = (programme != nullptr && programme->gainInterne > 0.0f)
                        ? programme->gainInterne : 1.0f;

        preampli.poserDrive (reg.drive);
        preampli.activer (reg.preampliActif);

        nonLineaire = (programme != nullptr && programme->enveloppePorte != nullptr);
        if (nonLineaire)
        {
            // Decay dilate le motif : au reglage d'usine le facteur vaut 1 et le
            // profil est celui qu'on a releve sur la machine.
            const double dilatation =
                std::clamp ((double) reg.t60S / std::max (programme->t60S, 0.01f), 0.25, 2.0);
            banc.poserProfil (programme->enveloppePorte, programme->nEnveloppePorte,
                              kPasEnveloppeMs * 1e-3 * kFsInterne, 1.0, dilatation);
        }

        // ⚠️ Le dosage plafonne a 0,5, et ce n'est pas de la prudence : melanger
        // le cote a MOITIE avec sa version dephasee donne un peigne, donc une
        // correlation qui varie avec la frequence. A dosage 1,0 on remplace le
        // cote par un passe-tout PUR — meme module dans chaque bande, donc
        // exactement la meme correlation qu'avant : l'effet disparait. Mesure
        // faite, l'ecart-type par bande retombait de 0,26 a 0,02.
        dosageLargeur = 0.5f * std::clamp ((reg.stereo - 1.0f) / 0.6f, 0.0f, 1.0f);

        tiltActif = std::abs (reg.tiltDb) > 0.01f;
        if (tiltActif)
        {
            const Biquad b = tilt (reg.tiltDb, reg.tiltPivotHz, fsHote);
            tiltL.poser (&b, 1);
            tiltR.poser (&b, 1);
        }
        else { tiltL.effacer(); tiltR.effacer(); }

        if (programme != nullptr && nouveauProgramme)
        {
            voicingL.poser (programme->firVoicing, kTailleFirVoicing);
            voicingR.poser (programme->firVoicing, kTailleFirVoicing);
        }

        // Modulation : bruit blanc filtre par un pole a tau = fs / mod_hz / 6.
        // ⚠️ Le prototype normalise l'ecart-type sur le bloc rendu ; en flux
        // continu on ne peut pas, donc on corrige analytiquement — l'ecart-type
        // en regime etabli d'un pole `a` sur du bruit unitaire vaut
        // sqrt((1-a)/(1+a)).
        {
            const double tau = std::max (fs / std::max ((double) reg.modHz, 0.01) / 6.0, 2.0);
            const double a   = std::exp (-1.0 / tau);
            aLfo    = (float) (1.0 - a);
            gainLfo = (float) std::sqrt ((1.0 + a) / std::max (1.0 - a, 1e-9));
        }
    }

    int latenceEchantillons() const noexcept
    {
        return sincL.latence() + voicingL.latence() + (int) std::ceil (ratio * slack);
    }

    //--------------------------------------------------------------------------
    /** Traite un bloc mono vers deux sorties, a la frequence de l'hote. */
    void traiter (const float* entree, float* sortieL, float* sortieR, int n) noexcept
    {
        // --- 1. etage d'entree a la frequence de l'hote
        for (int i = 0; i < n; ++i)
        {
            float x = preampli.traiter (entree[i]);
            if (reg.emphase) x = preSos.traiter (x);
            anneauIn[(size_t) (ecritsIn & kMasque)] = aa.traiter (x);
            ++ecritsIn;
        }

        // --- 2. descente vers 31,25 kHz, puis le tank
        while (posLecture + (double) Decimateur::kTaps < (double) ecritsIn)
        {
            const float xi = lireInterpole (posLecture);
            posLecture += ratio;

            const float q = quantifier (xi, reg.bits, bruit);      // ADC
            if (sondeInterne != nullptr) sondeInterne->push_back (q);
            Ech g = 0.0, d = 0.0;
            if (contournerTank)      { g = d = q; }
            else if (nonLineaire)    { tankNonLineaire (q, g, d); }
            else                     { tank (q, g, d); }

            // ⚠️ Le signal est monte AVANT le convertisseur et redescendu juste
            // apres. Il n'est pas modifie — seule sa POSITION dans la grille
            // 16 bits change. Sans ce calage, le tank sort a des niveaux qui
            // varient de 14 dB d'un programme a l'autre, donc le grain de
            // quantification s'entend d'autant plus fort que le programme est
            // discret : mesure avant correction, de 71 a 95 dB de plage dynamique
            // pour 85 dB annonces au manuel.
            anneauL[(size_t) (ecritsInterne & kMasque)] =
                quantifier ((float) (g * gainInterne), reg.bits, bruit) / gainInterne;
            anneauR[(size_t) (ecritsInterne & kMasque)] =
                quantifier ((float) (d * gainInterne), reg.bits, bruit) / gainInterne;
            ++ecritsInterne;                                        // DAC
        }

        // --- 3. remontee par BLOQUEUR D'ORDRE ZERO, puis reconstruction
        const double pas = 1.0 / ratio;
        for (int i = 0; i < n; ++i)
        {
            long long k = (long long) posSortie;
            margeMinimale = std::min (margeMinimale, ecritsInterne - 1 - k);

            // ⚠️ LE CONVERTISSEUR EST PARTAGE, ET LES DEUX VOIES NE SORTENT PAS
            // AU MEME INSTANT.
            //
            // Manuel de service du LXP-5, diagramme « A/D SAR Conversion
            // Timing » : un seul convertisseur fait trois taches par periode
            // d'echantillonnage, dans cet ordre — conversion A/N, puis voie
            // DROITE, puis voie GAUCHE. Le bloqueur de la gauche bascule donc
            // un TIERS DE PERIODE apres celui de la droite, soit 10,7 us a
            // 31,25 kHz.
            //
            // Ce n'est pas une option et ce n'est pas un defaut a corriger :
            // c'est une source de decorrelation NATIVE de la machine, presente
            // sur chaque exemplaire sorti d'usine, et elle participe a la
            // largeur qu'on entend. On la modelise donc en dur.
            //
            // Modelise comme un DECALAGE DU BLOQUEUR, pas comme un retard
            // fractionnaire filtre : la voie en retard tient simplement
            // l'echantillon precedent pendant le premier tiers de periode.
            // C'est exactement ce que fait l'echantillonneur-bloqueur, et ca ne
            // coute rien.
            //
            // ⚠️ kGaucheEnPremier inverse les roles par rapport au manuel : voir
            // la declaration de la constante.
            long long kTard = (long long) std::floor (posSortie - kTiersConvertisseur);

            k     = std::clamp (k,     0LL, ecritsInterne - 1);
            kTard = std::clamp (kTard, 0LL, ecritsInterne - 1);
            posSortie += pas;

            const long long iL = kGaucheEnPremier ? k : kTard;
            const long long iR = kGaucheEnPremier ? kTard : k;

            float L = murL.traiter (sincL.traiter (anneauL[(size_t) (iL & kMasque)]));
            float R = murR.traiter (sincR.traiter (anneauR[(size_t) (iR & kMasque)]));

            if (std::abs (reg.stereo - 1.0f) > 1e-3f)
            {
                const float mid = (L + R) * 0.5f;
                float cote = (L - R) * 0.5f * reg.stereo;

                // ⚠️ Au-dessus de 100 %, on ne se contente pas d'amplifier le
                // cote. Sur une queue deja totalement decorrelee — correlation
                // mesuree 0,00 a 0,02 — l'amplifier ne fabrique aucune largeur :
                // l'oreille lit un creux, pas un elargissement. On ajoute donc un
                // dephasage qui VARIE avec la frequence, ce qui se lit comme de
                // la largeur. Le milieu n'est pas touche, donc la somme mono non
                // plus. A 100 % pile, dosage nul : la reference est intacte.
                if (dosageLargeur > 0.0f)
                    cote += dosageLargeur * (apCote.traiter (cote) - cote);

                L = mid + cote; R = mid - cote;
            }
            if (tiltActif) { L = tiltL.traiter (L); R = tiltR.traiter (R); }
            L = voicingL.traiter (L);
            R = voicingR.traiter (R);
            if (reg.emphase) { L = deSosL.traiter (L); R = deSosR.traiter (R); }

            sortieL[i] = L;
            sortieR[i] = R;
        }

        // On evite que les compteurs derivent vers l'infini sur une session longue.
        if (ecritsIn > (1LL << 40))
        {
            const long long dIn = ecritsIn - (long long) posLecture;
            ecritsIn = dIn; posLecture = 0.0;
            const long long dOut = ecritsInterne - (long long) posSortie;
            ecritsInterne = dOut; posSortie = 0.0;
        }
    }

private:
    static constexpr int kTailleAnneau = 1 << 15;
    static constexpr long long kMasque = kTailleAnneau - 1;

    //--------------------------------------------------------------------------
    /** Lecture fractionnaire de l'anneau d'entree.

        Le vrai anti-repliement est fait juste avant par le Butterworth d'ordre 6
        a 15,2 kHz — celui de la machine. L'interpolateur n'a plus qu'a placer
        l'echantillon au bon instant, mais il doit le faire SANS assombrir :
        voir InterpolateurSinc, un Lagrange cubique coutait ici 2,2 dB d'aigu.
    */
    inline float lireInterpole (double pos) const noexcept
    {
        return interp.lire (anneauIn.data(), kMasque, pos);
    }

    //--------------------------------------------------------------------------
    /** Un echantillon du reseau a retards boucles, matrice de Householder.

        La matrice unitaire melange les huit lignes en O(N) : y_i = v_i - (2/N) somme(v).
        Unitaire, donc tous les poles gardent le meme module — aucun mode ne
        survit aux autres pour colorer la queue.

        `mixage` interpole entre l'identite (huit lignes independantes, spectre
        creuse, timbre metallique) et Householder complet (queue lisse). C'est ce
        parametre, et non la longueur des lignes, qui porte le temps de melange —
        les deux etaient confondus dans la version figure-8, et il fallait
        choisir entre un spectre juste et une montee juste.
    */
    inline void tank (Ech x, Ech& gaucheOut, Ech& droiteOut) noexcept
    {
        const int NL = nLignes;

        // Treble -> Diffusion -> Predelay, avec contre-reaction autour des trois.
        // ⚠️ Cet ordre vient du manuel LXP-15 : « Predelay is located after Treble
        // and Diffusion and before the reverb », et « a feedback loop around
        // Treble, Diffusion, and Predelay ». Le prototype avait d'abord l'inverse,
        // et la boucle manquait entierement.
        const Ech u = x + fbEtat * reg.predelaiFb;
        eTreble += kTreble * (u - eTreble);
        Ech v = eTreble;
        for (int k = 0; k < nDiff; ++k) v = diff[(size_t) k].traiter (v);
        v = predelai.traiter (v, 0.0);
        fbEtat = v;

        Ech somme = 0.0;
        for (int k = 0; k < NL; ++k) somme += sorties[k];
        const Ech fMix = reg.mixage * 2.0 / (double) NL * somme;

        for (int k = 0; k < NL; ++k)
        {
            // Modulation ALEATOIRE, pas sinusoidale : « CHORUSING randomizes
            // delay times » (manuel PCM 70), « randomly damped » (brevet EMT).
            // Un sinus fait respirer toute la queue au meme rythme et laisse
            // entendre sa periode ; un bruit lent desaccorde les modes sans
            // jamais se repeter.
            lfoEtat[k] += aLfo * (bruit.gaussien() - lfoEtat[k]);
            const double lfo = std::clamp (lfoEtat[k] * gainLfo, -2.5, 2.5)
                                * profondeur * 0.4 + profondeur * 0.5;

            const Ech inj = bInj[k] * injections[(size_t) k].pousser (v);
            Ech e = (sorties[k] - fMix) + inj;
#if LUXREV_QUANT_BOUCLE
            // `e` est ce qui est ECRIT dans la ligne, donc ce que la machine range
            // dans ses 64K x 16 bits. Le calage passe par gainInterne, comme au
            // DAC, sans quoi le grain varierait de 14 dB d'un programme a l'autre.
            //
            // ⚠️ Generateur de bruit SEPARE, et c'est necessaire a l'experience :
            // le dither et la modulation puisent dans la meme source, donc
            // partager le flux donnerait aux deux versions des trajectoires de LFO
            // differentes, et on ne comparerait plus la seule chose qu'on veut
            // comparer.
            e = quantifier ((float) (e * gainInterne), reg.bits, bruitBoucle) / gainInterne;
#endif
            const Ech d = lignes[(size_t) k].traiter (e, lfo);

            // h_i(Z) du brevet Jot : passe-bas du 1er ordre a gain continu g_dc et
            // pente propre a la ligne. Gain ET pente proportionnels a la longueur,
            // sans quoi la perte par SECONDE serait d'autant plus faible que la
            // ligne est longue, et la queue finirait gouvernee par la plus longue.
            etats[k] = gains[k] * (1.0f - bAbs[k]) * d + bAbs[k] * etats[k];
            // « Bass Multiply provides control over bass reverb time » : un
            // MULTIPLICATEUR du temps de decroissance sous la coupure, pas une
            // attenuation. A ratio 1,0 le terme est nul et rien ne change.
            eBass[k] += kCross * (etats[k] - eBass[k]);
            sorties[k] = etats[k] + gainBass[k] * eBass[k];
        }

        Ech gch = 0.0, drt = 0.0;
        for (int k = 0; k + 1 < NL; k += 2)
        {
            gch += signes[k]     * sorties[k];
            drt += signes[k + 1] * sorties[k + 1];
        }

        // Banc de retards en PARALLELE, alimente par l'entree diffusee.
        // ⚠️ Ces prises etaient d'abord posees SUR les lignes du FDN, donc elles
        // debitaient a CHAQUE tour : la queue restait dense en permanence et le
        // temps de melange sortait 2 a 4 fois trop court, sans qu'aucun reglage
        // n'y change rien. En parallele, elles ne produisent que des arrivees
        // precoces — DIFFUSION — et la densite tardive redevient l'affaire de la
        // seule matrice — DEFINITION. Le PCM 70 distingue bien les deux.
        ffBuf[(size_t) ffI] = v;
        if (++ffI >= ffN) ffI = 0;
        for (int t = 0; t < kNPrisesFf; ++t)
        {
            int j = ffI - ffTaps[t]; if (j < 0) j += ffN;
            const Ech e = ffBuf[(size_t) j] * poidsPrises[t];
            if (t & 1) gch += reg.gainPrises * e;
            else       drt += reg.gainPrises * e;
        }

        // Prise instantanee en sortie du diffuseur : le seul point qui ne traverse
        // aucune ligne, donc le seul vraiment precoce. Sans elle, le signal doit
        // traverser un bras entier avant d'atteindre la sortie.
        gch += reg.priseTot * v;
        drt -= reg.priseTot * v;

        sL += lpOut * (gch - sL);
        sR += lpOut * (drt - sR);

        if (kHp > 0.0)                     // « Low Cut Filter »
        {
            hL += kHp * (sL - hL);
            hR += kHp * (sR - hR);
            gaucheOut = sL - hL;
            droiteOut = sR - hR;
        }
        else { gaucheOut = sL; droiteOut = sR; }
    }

    //--------------------------------------------------------------------------
    /** Pole du premier ordre dont le module vaut `rap` A 8 kHz.

        Le filtre absorbant est y = g(1-b).x + b.y[-1], de module normalise

            |H(w)| / g = (1 - b) / sqrt(1 - 2 b cos w + b^2)

        ⚠️ POSER b = (1-rap)/(1+rap) atteint le rapport A NYQUIST, soit 15,6 kHz
        ici. Or `ratioHf` est defini comme T60(8 kHz)/T60(500 Hz), releve a 8 kHz
        sur les IR : a cette frequence le premier ordre n'a fait qu'une partie de
        sa descente, et le rapport obtenu etait

            demande 0,50 -> 0,638      demande 0,70 -> 0,812
            demande 0,58 -> 0,714      demande 0,80 -> 0,883

        d'ou les 0,87-0,91 mesures sur les douze programmes pour 0,51-0,80 vises.
        Le parametre et la grandeur qui le mesure ne parlaient pas de la meme
        frequence. Meme correction dans _proto/moteur.py.

        Poser |H| = rap donne une quadratique en b dont les deux racines sont b
        et 1/b — on garde celle de module inferieur a 1, seule stable.
    */
    static double polePourRapport (double rap) noexcept
    {
        // 8 kHz a la frequence interne, la bande haute d'analyse_ir.py.
        // ⚠️ Pi en dur, PAS juce::MathConstants : ces en-tetes DSP se compilent aussi
        // hors JUCE — c'est ce qui permet aux bancs de tools/ de mesurer exactement le
        // meme code que le plugin. Le compilateur l'a rappele tout de suite.
        static const double cosW = std::cos (2.0 * 3.14159265358979323846
                                             * 8000.0 / kFsInterne);
        const double r2 = rap * rap;
        const double a  = 1.0 - r2;
        if (std::abs (a) < 1e-12) return 0.0;            // filtre plat
        const double bl = 2.0 * (r2 * cosW - 1.0);
        const double disc = bl * bl - 4.0 * a * a;
        if (disc <= 0.0) return 0.0;
        const double rc = std::sqrt (disc);
        const double b1 = (-bl - rc) / (2.0 * a);
        if (std::abs (b1) < 1.0) return b1;
        const double b2 = (-bl + rc) / (2.0 * a);
        return (std::abs (b2) < 1.0) ? b2 : 0.0;
    }

    //--------------------------------------------------------------------------
    /** Gain de boucle donnant la decroissance voulue.

        Dans un FDN a matrice unitaire, chaque tour d'une ligne subit g une fois
        et dure sa propre longueur : perdre 60 dB en t60 impose
        g = 10^(-3 * longueur / (t60 * fs)).
    */
    void reglerT60() noexcept
    {
        const double fs = kFsInterne;
        const double r  = std::clamp ((double) reg.ratioHf, 0.1, 2.0);
        const double t60Hf = std::max (reg.t60S * r, 1e-3);
        const double rb = std::clamp ((double) reg.ratioBass, 0.25, 4.0);
        kCross = coefLp (reg.crossBassHz, fs);

        for (int k = 0; k < kNFdn; ++k) { gains[k] = 0.0; bAbs[k] = 0.0; gainBass[k] = 0.0; }
        if (reg.t60S <= 0.0f) return;

        for (int k = 0; k < nLignes; ++k)
        {
            const double L = lignes[(size_t) k].nominalEch();
            const double gDc = std::min (std::pow (10.0, -3.0 * L / (reg.t60S * fs)), 0.9999);
            const double gHf = std::min (std::pow (10.0, -3.0 * L / (t60Hf * fs)), 0.9999);
            const double rap = std::clamp (gHf / std::max (gDc, 1e-12), 1e-6, 0.999);

            gains[k] = gDc;
            bAbs[k]  = std::clamp (polePourRapport (rap), -0.95, 0.95);

            const double gLf = std::min (
                std::pow (10.0, -3.0 * L / (std::max ((double) reg.t60S * rb, 1e-3) * fs)), 0.9999);
            gainBass[k] = gLf / std::max (gDc, 1e-12) - 1.0;
        }
    }

    //--------------------------------------------------------------------------
    Reglages reg;
    const Programme* programme = nullptr;

    double fsHote = 48000.0, ratio = 48000.0 / kFsInterne;
    long long ecritsIn = 0, ecritsInterne = 0;
    double posLecture = 0.0, posSortie = 0.0;
    std::vector<float> anneauIn, anneauL, anneauR;
    Decimateur interp;

    Biquad preSos, deSosL, deSosR;
    Chaine aa, murL, murR, tiltL, tiltR, apCote;
    float dosageLargeur = 0.0f;
    Fir sincL, sincR, voicingL, voicingR;
    bool tiltActif = false;

    LigneModulee predelai;
    Allpass diff[4];
    LigneModulee lignes[kNFdn];
    Delai injections[kNFdn];
    std::vector<Ech> ffBuf;
    int ffTaps[kNPrisesFf] {};
    double poidsPrises[kNPrisesFf] {};
    int ffN = 8, ffI = 0, nLignes = kNFdn, nDiff = 4;

    double signes[kNFdn] {}, bInj[kNFdn] {}, gains[kNFdn] {}, bAbs[kNFdn] {}, gainBass[kNFdn] {};
    Ech etats[kNFdn] {}, sorties[kNFdn] {}, eBass[kNFdn] {}, lfoEtat[kNFdn] {};

    double kTreble = 1.0, kCross = 0.0, lpOut = 1.0, kHp = 0.0;
    double profondeur = 0.0, aLfo = 0.0, gainLfo = 1.0;
    Ech eTreble = 0.0, fbEtat = 0.0, sL = 0.0, sR = 0.0, hL = 0.0, hR = 0.0;

    //--------------------------------------------------------------------------
    /** Gate et Inverse : l'entree diffusee traverse le banc de prises, et c'est
        tout. Pas de reseau boucle — une queue exponentielle par-dessus emousserait
        justement la falaise qui fait le programme. */
    inline void tankNonLineaire (Ech x, Ech& gaucheOut, Ech& droiteOut) noexcept
    {
        const Ech u = x + fbEtat * reg.predelaiFb;
        eTreble += kTreble * (u - eTreble);
        Ech v = eTreble;
        for (int k = 0; k < nDiff; ++k) v = diff[(size_t) k].traiter (v);
        v = predelai.traiter (v, 0.0);
        fbEtat = v;

        Ech g = 0.0, d = 0.0;
        banc.traiter (v, g, d);

        sL += lpOut * (g - sL);
        sR += lpOut * (d - sR);
        gaucheOut = sL;
        droiteOut = sR;
    }

    BancNonLineaire banc;
    bool nonLineaire = false;

    float gainInterne = 1.0f;
    Bruit bruitBoucle { 20260927u };   // dither de la boucle, flux propre
    Preampli preampli;

    Bruit bruit { 20260907u };
};

} // namespace luxrev
