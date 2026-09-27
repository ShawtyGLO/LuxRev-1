// LuxRev-1 — conception des filtres.
//
// Transcription des appels scipy du prototype :
//   _sig.butter(N, fc, 'lowpass', output='sos')  -> butterworthPasseBas
//   _sig.bilinear([...], [...], fs)              -> bilineaire1
//   _correction_sinc(...) via firwin2            -> correctionSinc
//
// Tout est calcule a prepareToPlay, jamais dans la boucle audio.

#pragma once

#include "Elements.h"
#include <complex>

namespace luxrev
{

inline constexpr double kPi = 3.14159265358979323846;

//==============================================================================
/** Passe-bas de Butterworth d'ordre pair, par transformation bilineaire.

    Ecrit dans `sections` (au moins ordre/2 elements) et renvoie le nombre de
    sections. ⚠️ Pas de vector en retour : cette fonction est appelee quand un
    parametre bouge, donc potentiellement depuis le fil audio.

    ⚠️ Il faut un ordre eleve, pas une cascade de pentes douces. Le prototype a
    d'abord cascade 4 passe-bas du 1er ordre a 16,9 kHz : quatre poles doux
    commencent a mordre des 5 kHz et le moteur sortait 10 dB trop sombre a
    14-16 kHz — exactement le manque de brillant entendu a l'ecoute. La mesure
    decrit une falaise a 17 kHz avec une reponse plate en dessous.
*/
inline int butterworthPasseBas (Biquad* sections, int ordre, double fc, double fs)
{
    if (ordre < 2 || fc <= 0.0) return 0;

    const double nyq = fs * 0.5;
    const double w   = std::min (fc / nyq, 0.985);          // meme borne qu'en Python
    const double K   = std::tan (kPi * w * 0.5);            // prewarp bilineaire
    const double K2  = K * K;

    // Poles conjugues du prototype de Butterworth, pris deux par deux.
    int n = 0;
    for (int k = 0; k < ordre / 2; ++k)
    {
        const double theta = kPi * (2.0 * k + 1.0) / (2.0 * ordre);
        const double q2    = 2.0 * std::sin (theta);        // = 1/Q de la paire
        const double nrm   = 1.0 / (1.0 + q2 * K + K2);

        Biquad b;
        b.b0 = K2 * nrm;
        b.b1 = 2.0 * b.b0;
        b.b2 = b.b0;
        b.a1 = 2.0 * (K2 - 1.0) * nrm;
        b.a2 = (1.0 - q2 * K + K2) * nrm;
        sections[n++] = b;
    }
    return n;
}

//==============================================================================
/** Bilineaire d'une fraction du 1er ordre en s : (n1 s + n0) / (d1 s + d0). */
inline Biquad bilineaire1 (double n1, double n0, double d1, double d0, double fs)
{
    const double c = 2.0 * fs;                 // scipy.signal.bilinear, sans prewarp
    Biquad b;
    const double den = d1 * c + d0;
    b.b0 = (n1 * c + n0) / den;
    b.b1 = (n0 - n1 * c) / den;
    b.b2 = 0.0;
    b.a1 = (d0 - d1 * c) / den;
    b.a2 = 0.0;
    return b;
}

/** Pre-accentuation 68 us, ou son inverse exact.

    ⚠️ En regime lineaire les deux se compensent EXACTEMENT : ajouter l'emphase
    ne change rien au spectre. Elle n'agit que sur ce qui se passe entre les
    deux — le bruit de quantification 16 bits et l'ecretage interne. C'est
    precisement la, et nulle part ailleurs, que se joue le grain de la machine.
*/
inline Biquad emphase (double fs, bool inverse)
{
    if (! inverse) return bilineaire1 (kEmphaseT1, 1.0, kEmphaseT2, 1.0, fs);
    return bilineaire1 (kEmphaseT2, 1.0, kEmphaseT1, 1.0, fs);
}

/** TILT du 1er ordre : H(s) = (s/w + 1/G) / (s/(G w) + 1).

    Gain 1/G en continu, G a l'infini, bascule autour de w. Symetrique par
    construction : ce qu'on ajoute d'un cote, on l'enleve exactement de l'autre.
    ⚠️ Une premiere version cascadait deux plateaux ecrits a la main et sortait
    dissymetrique — a +9 dB elle coupait 6,3 dB de grave pour ne monter que
    2,2 dB d'aigu. Une seule section bien posee vaut mieux que deux bricolees.
*/
inline Biquad tilt (double db, double pivotHz, double fs)
{
    const double G = std::pow (10.0, db / 40.0);
    const double w = 2.0 * kPi * std::clamp (pivotHz, 100.0, 4000.0);
    return bilineaire1 (1.0 / w, 1.0 / G, 1.0 / (G * w), 1.0, fs);
}

/** Passe-tout du 1er ordre : plat en module, il ne deplace que la phase.

    Sert a DECORRELER le canal de cote. ⚠️ Ce n'est pas cosmetique : au-dela
    d'une correlation nulle, amplifier le cote ne rend pas le son plus large, il
    le rend plus creux — l'oreille ne lit plus de largeur, elle lit un trou. Un
    dephasage qui varie avec la frequence, lui, se lit comme de la largeur, et il
    laisse le milieu intact donc la somme mono aussi.
*/
inline Biquad passeTout1 (double fc, double fs)
{
    const double t = std::tan (kPi * std::clamp (fc, 20.0, fs * 0.45) / fs);
    const double a = (t - 1.0) / (t + 1.0);
    Biquad b;
    b.b0 = a; b.b1 = 1.0; b.b2 = 0.0;
    b.a1 = a; b.a2 = 0.0;
    return b;
}

//==============================================================================
/** FIR compensant l'enveloppe sin(x)/x du bloqueur d'ordre zero.

    Un maintien d'echantillon attenue par sinc(f / fs_interne) : -3,6 dB des
    15 kHz pour 31,25 kHz. Tout etage de reconstruction de DAC corrige cela ;
    sans la correction, le moteur perd 3 dB d'air a 14-15 kHz en plus de ce que
    coupe le filtre.

    Equivalent de scipy firwin2 : reponse desiree echantillonnee sur une grille
    lineaire, transformee de Fourier inverse, fenetre de Hamming. Fait une fois
    a prepareToPlay — la somme directe (512 x 127) coute une fraction de
    milliseconde et evite d'embarquer une FFT.
*/
inline std::vector<float> correctionSinc (double fsHote, int nTaps = 127)
{
    constexpr int nGrille = 512;
    std::vector<double> g ((size_t) nGrille);

    int iGel = nGrille - 1;
    for (int k = 0; k < nGrille; ++k)
    {
        const double f = (double) k / (nGrille - 1);          // 0 .. 1 (x Nyquist)
        const double x = kPi * f * (fsHote * 0.5) / kFsInterne;
        double v = 1.0;
        if (k > 0)
        {
            const double s = std::sin (std::clamp (x, 1e-9, kPi * 0.999));
            v = std::clamp (x / s, 1.0, 6.0);
        }
        g[(size_t) k] = v;
        if (f <= 0.80) iGel = k;
    }
    // pas d'explosion pres de Nyquist : au-dela de 0,80 on gele la valeur
    for (int k = iGel + 1; k < nGrille; ++k) g[(size_t) k] = g[(size_t) iGel];

    const int M = (nTaps - 1) / 2;
    std::vector<float> h ((size_t) nTaps);
    for (int n = -M; n <= M; ++n)
    {
        double acc = 0.0;
        for (int k = 0; k < nGrille; ++k)
        {
            const double f = (double) k / (nGrille - 1);
            const double p = (k == 0 || k == nGrille - 1) ? 0.5 : 1.0;  // trapezes
            acc += p * g[(size_t) k] * std::cos (kPi * f * n);
        }
        acc /= (double) (nGrille - 1);
        // fenetre de Hamming, celle que firwin2 pose par defaut
        const double w = 0.54 - 0.46 * std::cos (2.0 * kPi * (n + M) / (nTaps - 1));
        h[(size_t) (n + M)] = (float) (acc * w);
    }

    // Normalisation a gain unite en continu : la grille et la fenetre laissent
    // une derive de quelques pour cent, et un FIR de voicing suit derriere.
    double somme = 0.0;
    for (float v : h) somme += v;
    if (std::abs (somme) > 1e-9)
        for (auto& v : h) v = (float) (v / somme);

    return h;
}

//==============================================================================
/** Quantification uniforme sur `bits`, pleine echelle a +/-1, dither TPDF.

    Le LXP-1 est 16 bits lineaires : DAC PCM54HP, et un ADC a approximations
    successives dans le Lexichip qui reutilise ce meme DAC en contre-reaction.

    ⚠️ Le dither n'est pas cosmetique. Les IR d'origine ont 99 % d'echantillons
    non nuls jusqu'a la fin du fichier — un vrai plancher de convertisseur — la
    ou une quantification seche tombe au zero numerique absolu et fait
    disparaitre la queue d'un coup au lieu de la laisser se fondre.
*/
// ⚠️ Drapeau d'ESSAI, actif par defaut. A zero, le quantificateur tronque sans
// dither — ce qui sert a repondre a une question precise : la quantification DANS
// la boucle du FDN, si on l'active, suffit-elle a dithering le convertisseur de
// sortie ? Autrement dit, peut-on remplacer un bruit ajoute a la main par un
// bruit MODELISE, issu du meme mecanisme que dans la machine ?
//
// Le piege a verifier est net : l'erreur de quantification disparait quand le
// signal disparait. Plus de signal, plus de dither, et la fin de queue se fait
// tronquer — exactement ce que le dither devait empecher.
#ifndef LUXREV_DITHER
 #define LUXREV_DITHER 1
#endif

inline float quantifier (float x, int bits, Bruit& bruit) noexcept
{
    if (bits <= 0) return x;
    const float q = (float) (1 << (bits - 1));
#if LUXREV_DITHER
    const float d = bruit.uniforme() - bruit.uniforme();     // TPDF, 1 LSB c-a-c
#else
    const float d = 0.0f;
    (void) bruit;
#endif
    return std::round (x * q + d) / q;
}

} // namespace luxrev
