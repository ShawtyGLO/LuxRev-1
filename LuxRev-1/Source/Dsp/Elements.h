// LuxRev-1 — briques elementaires du moteur.
//
// Transcription directe de _proto/moteur.py. Le prototype Python reste la
// reference : toute divergence ici est un bug, pas une optimisation.
//
// ⚠️ Rien dans ce fichier ne connait le LXP-1. Les valeurs accordees vivent
// dans Generated/Programmes.h, produites par _proto/accorder.py — seul endroit
// ou une cible mesuree entre dans la conception. Meme regle qu'en Python.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace luxrev
{

// Frequence interne du LXP-1, donnee par son manuel de service : word clock a
// 31,25 kHz depuis un quartz de 16 MHz. Nyquist a 15,6 kHz. Le tank tourne
// vraiment a cette frequence — c'est structurel, pas cosmetique.
inline constexpr double kFsInterne = 31250.0;

// Longueurs de Dattorro 1997 (JAES 45(9)), donnees a 29 761 Hz. Conservees
// telles quelles pour que la source reste verifiable ; la transposition se fait
// a la construction.
inline constexpr double kFsDattorro = 29761.0;
inline constexpr int    kDiffuseurLongueurs[4] = { 142, 107, 379, 277 };
inline constexpr float  kDiffuseurCoeffs[4]    = { 0.75f, 0.75f, 0.625f, 0.625f };

// Huit lignes premieres entre elles, rapport 1:2,7. Total 15 820 echantillons a
// taille 1,0 — tres en dessous des 65 536 de la DRAM du LXP-1, donc la machine
// pouvait largement se payer huit lignes.
inline constexpr int kFdnLongueurs[8] = { 1123, 1319, 1543, 1789, 2003, 2333, 2699, 3011 };
inline constexpr int kNFdn = 8;

// Pre/de-accentuation : R43 = 6,8 kOhm, C48 = 10 nF, releves dans la nomenclature
// du manuel de service. tau = 68 us, coin a 2 340 Hz.
inline constexpr double kEmphaseT1 = 68.0e-6;
inline constexpr double kEmphaseT2 = kEmphaseT1 / 3.33;

// ⚠️ Le tank calcule en DOUBLE, comme le prototype Python.
//
// Ce n'est pas de la prudence de principe : dans un reseau qui recircule avec
// des gains proches de 0,999, l'erreur d'arrondi repasse par la boucle a chaque
// tour et s'accumule. Mesure contre le prototype, le simple float coutait 0,2 a
// 1,6 dB dans le grave selon le programme — un ecart qui ne se rattachait a
// aucun parametre, signature d'une erreur numerique et non d'un etage faux.
using Ech = double;

inline int transpose (int n, double fs, double taille = 1.0)
{
    return std::max ((int) std::lround (n * fs / kFsDattorro * taille), 4);
}

// Coefficient d'un passe-bas du 1er ordre : y += k * (x - y).
inline float coefLp (double fc, double fs)
{
    if (fc >= fs * 0.49) return 1.0f;
    if (fc <= 0.0)       return 0.0f;
    return (float) (1.0 - std::exp (-2.0 * 3.14159265358979323846 * fc / fs));
}

//==============================================================================
/** Ligne a retard simple, retard fixe egal a la taille du tampon. */
class Delai
{
public:
    /** Alloue pour le retard MAXIMAL, une fois. */
    void preparer (int nMax)
    {
        capacite = std::max (nMax, 1);
        buf.assign ((size_t) capacite, 0.0);
        taille = capacite;
        i = 0;
    }

    /** Change le retard sans allouer (pre-delai : 0 a 246 ms sur la machine). */
    /** ⚠️ On NE remet PAS l'index a zero. Le faire relance la lecture au debut
        du tampon, donc renvoie d'un coup un morceau de signal deja joue — un
        claquement franc a chaque changement de longueur. Laisser l'index ou il
        est ne coute qu'un saut de phase, sans rupture d'amplitude. */
    void poserLongueur (int n)
    {
        taille = std::clamp (n, 1, capacite);
        if (i >= taille) i = 0;
    }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0); i = 0; }

    inline Ech pousser (Ech x) noexcept
    {
        const Ech y = buf[(size_t) i];
        buf[(size_t) i] = x;
        if (++i >= taille) i = 0;
        return y;
    }

private:
    std::vector<Ech> buf;
    int taille = 1, capacite = 1, i = 0;
};

//==============================================================================
/** Allpass de Schroeder : d = buf[i] ; w = x + k*d ; buf[i] = w ; y = d - k*w.

    Passe-tout au sens strict : le module est plat, seule la phase change. C'est
    ce qui permet d'etaler une impulsion en une bouffee dense SANS colorer — la
    brique de base de toute la lignee.
*/
class Allpass
{
public:
    void preparer (int n, float coef)
    {
        taille = std::max (n, 1);
        buf.assign ((size_t) taille, 0.0);
        i = 0;
        k = coef;
    }

    /** Le coefficient bouge avec Diffusion ; la longueur, elle, ne depend pas de
        `taille` — le diffuseur d'entree est transpose une fois pour toutes. */
    void poserCoef (double coef) noexcept { k = coef; }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0); i = 0; }

    inline Ech traiter (Ech x) noexcept
    {
        const Ech d = buf[(size_t) i];
        const Ech w = x + k * d;
        buf[(size_t) i] = w;
        if (++i >= taille) i = 0;
        return d - k * w;
    }

private:
    std::vector<Ech> buf;
    int taille = 1, i = 0;
    Ech k = 0.0;
};

//==============================================================================
/** Ligne a retard a lecture fractionnaire, SANS contre-reaction interne.

    C'est ce qu'un FDN attend a chaque branche. Un allpass a la place apporte sa
    propre boucle de gain : la decroissance sort trois fois trop longue quel que
    soit le gain applique par le reseau, parce que l'energie recircule a deux
    endroits au lieu d'un.

    ⚠️ L'interpolation est un ALLPASS du 1er ordre, plate en module. Une
    interpolation lineaire est un passe-bas discret sur un passage, mais qui se
    compose a CHAQUE tour de boucle : mesure sur `gate`, la queue perdait 11,8 dB
    a 16 kHz. C'est l'interpolateur, et non l'amortissement, qui causait le
    « trop de mids » entendu.
*/
class LigneModulee
{
public:
    static constexpr int kNPrises = 6;

    /** Alloue pour le PIRE cas (taille maximale de COLOR), une fois. */
    void preparer (int nominalMax, int marge)
    {
        margeMax = std::max (marge, 0);
        taille   = std::max (nominalMax, 4) + margeMax + 4;
        buf.assign ((size_t) taille, 0.0);
        nominal = std::max (nominalMax, 4);
        i = 0;
        yi = 0.0f;
        calculerPrises();
    }

    /** Change le retard SANS allouer, et SANS SAUTER.

        ⚠️ COLOR balaie `taille` en continu : a chaque bloc, le retard vise une
        nouvelle valeur. Le poser d'un coup deplace le point de lecture d'un ou
        deux echantillons d'un bloc a l'autre, dans une boucle qui recircule —
        ce qui s'entend comme un gresillement tant qu'on tient le bouton. Le
        retard glisse donc vers sa cible en ~80 ms, ce qui revient a un leger
        glissement de hauteur : exactement ce qu'on attend en tournant la taille
        d'une salle. */
    void poserNominal (int n)
    {
        cible = (double) std::clamp (n, 4, taille - margeMax - 4);
        nominal = (int) std::lround (cible);
        calculerPrises();
    }

    /** Rejoint la cible SANS glisser.

        ⚠️ A appeler a chaque remise a zero. Sans ca, les lignes partent de leur
        longueur maximale — celle pour laquelle le tampon est dimensionne — et
        mettent 80 ms a rejoindre la longueur du programme : la reponse
        impulsionnelle mesuree juste apres serait celle d'une autre salle. */
    void caler() noexcept { courant = cible; }

    /** Coefficient de glissement, pose une fois a la preparation. */
    void poserGlissement (double k) noexcept { kGliss = k; }

    void calculerPrises()
    {

        // Prises intermediaires le long de la ligne — la structure de la FIG. 2
        // du brevet EMT US4181820 (Blesser & Bader) : UNE longue ligne portant
        // des prises A2..An. Densite d'arrivees elevee avec peu de memoire.
        static constexpr float fractions[kNPrises] = { 0.06f, 0.13f, 0.24f, 0.41f, 0.63f, 0.85f };
        for (int k = 0; k < kNPrises; ++k)
            prises[k] = std::max ((int) (nominal * fractions[k]), 1);
    }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0); i = 0; yi = 0.0; }

    inline Ech traiter (Ech x, double decalage) noexcept
    {
        courant += (cible - courant) * kGliss;
        const double p = courant + decalage;
        int    i0 = (int) p;
        double f  = p - (double) i0;
        if (f < 0.5) { i0 -= 1; f += 1.0; }     // l'allpass du 1er ordre se tient
                                                // mieux pour f dans [0,5 ; 1,5)
        int a = (i - i0) % taille; if (a < 0) a += taille;
        int b = (i - i0 - 1) % taille; if (b < 0) b += taille;

        const double eta = (1.0 - f) / (1.0 + f);
        const Ech d = eta * buf[(size_t) a] + buf[(size_t) b] - eta * yi;
        yi = d;

        buf[(size_t) i] = x;
        if (++i >= taille) i = 0;
        return d;
    }

    int nominalEch() const noexcept { return nominal; }

private:
    std::vector<Ech> buf;
    int taille = 8, i = 0, nominal = 4, margeMax = 0;
    double cible = 4.0, courant = 4.0, kGliss = 4.0e-4;
    int prises[kNPrises] { 1, 1, 1, 1, 1, 1 };
    Ech yi = 0.0;
};

//==============================================================================
/** Section biquad en forme directe II transposee, chainable. */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void vider() noexcept { z1 = z2 = 0.0; }

    inline float traiter (float x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

/** Chaine de sections, l'equivalent d'un `sos` scipy.

    ⚠️ Capacite FIXE, jamais de vector : poser() est appele quand un parametre
    bouge, donc potentiellement depuis le fil audio. Un ordre 6 tient en trois
    sections ; quatre laissent de la marge sans rien couter.
*/
class Chaine
{
public:
    static constexpr int kMaxSections = 4;

    void poser (const Biquad* s, int n)
    {
        const int m = std::min (n, kMaxSections);
        for (int k = 0; k < m; ++k)
        {
            const double z1 = sections[(size_t) k].z1, z2 = sections[(size_t) k].z2;
            sections[(size_t) k] = s[k];
            sections[(size_t) k].z1 = z1;      // on garde l'etat : changer les
            sections[(size_t) k].z2 = z2;      // coefficients ne doit pas claquer
        }
        nSections = m;
    }

    void effacer() { nSections = 0; }
    bool vide() const noexcept { return nSections == 0; }
    void vider() { for (auto& s : sections) s.vider(); }

    inline float traiter (float x) noexcept
    {
        for (int k = 0; k < nSections; ++k) x = sections[(size_t) k].traiter (x);
        return x;
    }

private:
    Biquad sections[kMaxSections];
    int nSections = 0;
};

//==============================================================================
/** Convolution FIR directe, a phase non compensee.

    ⚠️ Le prototype Python utilise np.convolve(..., "same"), qui CENTRE le
    resultat : il retire (N-1)/2 echantillons au debut. En temps reel on ne peut
    pas anticiper, donc on retarde de (N-1)/2 et on declare cette latence a
    l'hote. C'est la seule facon d'obtenir la meme reponse sans tricher sur le
    temps.
*/
class Fir
{
public:
    /** Alloue une fois, a prepareToPlay. */
    void preparer (int n)
    {
        coeffs.assign ((size_t) n, 0.0f);
        etat.assign ((size_t) n, 0.0f);
        i = 0;
    }

    /** Change les coefficients SANS allouer — appelable depuis le fil audio. */
    void poser (const float* h, int n)
    {
        std::copy (h, h + std::min (n, (int) coeffs.size()), coeffs.begin());
    }

    void vider() { std::fill (etat.begin(), etat.end(), 0.0f); i = 0; }
    int latence() const noexcept { return coeffs.empty() ? 0 : ((int) coeffs.size() - 1) / 2; }
    bool vide() const noexcept { return coeffs.empty(); }

    inline float traiter (float x) noexcept
    {
        const int n = (int) coeffs.size();
        if (n == 0) return x;
        etat[(size_t) i] = x;
        double acc = 0.0;
        int j = i;
        for (int k = 0; k < n; ++k)
        {
            acc += (double) coeffs[(size_t) k] * etat[(size_t) j];
            if (--j < 0) j = n - 1;
        }
        if (++i >= n) i = 0;
        return (float) acc;
    }

private:
    std::vector<float> coeffs, etat;
    int i = 0;
};

//==============================================================================
/** Decimateur polyphase : il place l'echantillon au bon instant ET borne la bande.

    ⚠️ Ce n'est pas un simple interpolateur, et c'est la correction la plus
    importante du portage. Une premiere version se contentait d'interpoler, en
    laissant l'anti-repliement au seul Butterworth d'ordre 6 a 15,2 kHz de la
    machine. Mesure contre les IR d'origine, moyennee sur huit phases
    d'impulsion : le moteur sortait alors 2,43 dB TROP BRILLANT a 16 kHz, la ou
    le prototype tombe a 0,33 dB de la machine.

    La coupure du noyau est donc posee a la frequence de Nyquist INTERNE
    (15,625 kHz), pas a celle de l'hote. C'est la meme regle que
    scipy.resample_poly, et c'est ce que le materiel fait : la bande est bornee
    par ce qui precede l'echantillonneur, pas par ce qui le suit.

    Table de phases precalculee : kTaps multiplications par echantillon interne,
    soit environ 1 M op/s. Negligeable.
*/
class Decimateur
{
public:
    // 64 points, pas 32 : a 32 la transition mordait avant 15,625 kHz et le
    // moteur ressortait 1,0 dB trop sombre. scipy.resample_poly en emploie pres
    // de 4000 — on ne peut pas se le payer en temps reel, mais 64 suffisent a
    // ramener l'ecart dans le bruit de la mesure.
    static constexpr int kTaps = 64;
    static constexpr int kPhases = 512;

    /** `ratio` = fsHote / fsInterne. Au-dessus de 1, le noyau doit couper. */
    void preparer (double ratio)
    {
        const double fc = 0.5 / std::max (ratio, 1.0);      // coupure, en x fsHote
        if (! table.empty() && std::abs (fc - coupure) < 1e-12) return;
        coupure = fc;
        table.assign ((size_t) (kPhases * kTaps), 0.0f);

        constexpr int demi = kTaps / 2;
        constexpr double pi = 3.14159265358979323846;
        const double beta = 9.0;                            // Kaiser : ~ -90 dB
        const double i0b = besselI0 (beta);

        for (int p = 0; p < kPhases; ++p)
        {
            const double frac = (double) p / kPhases;
            double somme = 0.0;
            for (int k = 0; k < kTaps; ++k)
            {
                const double x = (double) (k - demi + 1) - frac;
                const double a = 2.0 * fc * x;
                const double s = (std::abs (a) < 1e-9) ? 1.0 : std::sin (pi * a) / (pi * a);
                // fenetre de Kaiser centree sur le noyau, pas sur l'indice : le
                // decalage fractionnaire deplace le centre effectif
                const double u = std::clamp ((x + demi - 1.0) / (kTaps - 1.0), 0.0, 1.0);
                const double r = 2.0 * u - 1.0;
                const double w = besselI0 (beta * std::sqrt (std::max (1.0 - r * r, 0.0))) / i0b;
                const double v = 2.0 * fc * s * w;
                table[(size_t) (p * kTaps + k)] = (float) v;
                somme += v;
            }
            // gain unite en continu, phase par phase — sinon l'interpolateur
            // module le niveau au rythme de la phase
            if (std::abs (somme) > 1e-9)
                for (int k = 0; k < kTaps; ++k)
                    table[(size_t) (p * kTaps + k)] =
                        (float) (table[(size_t) (p * kTaps + k)] / somme);
        }
    }

    inline float lire (const float* anneau, long long masque, double pos) const noexcept
    {
        const long long i0 = (long long) pos;
        const int p = (int) ((pos - (double) i0) * kPhases) & (kPhases - 1);
        const float* h = &table[(size_t) (p * kTaps)];
        constexpr int demi = kTaps / 2;
        double acc = 0.0;
        for (int k = 0; k < kTaps; ++k)
            acc += (double) h[k] * anneau[(size_t) ((i0 + k - demi + 1) & masque)];
        return (float) acc;
    }

private:
    /** Bessel modifiee de 1re espece, ordre 0 — pour la fenetre de Kaiser. */
    static double besselI0 (double x)
    {
        double somme = 1.0, terme = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            terme *= (x * 0.5) / k;
            somme += terme * terme;
        }
        return somme;
    }

    std::vector<float> table;
    double coupure = -1.0;
};

//==============================================================================
/** Bruit blanc reproductible — meme role que default_rng(20260907) en Python.

    Le prototype fige la graine pour que deux rendus du meme reglage soient
    identiques au bit pres. Ici on fige aussi, mais par instance : deux
    occurrences du plugin ne doivent PAS produire exactement la meme modulation,
    sinon leurs queues se correlent et l'image stereo se resserre.
*/
class Bruit
{
public:
    explicit Bruit (unsigned int graine = 20260907u) : etat (graine ? graine : 1u) {}

    inline float uniforme() noexcept        // [0, 1)
    {
        etat ^= etat << 13; etat ^= etat >> 17; etat ^= etat << 5;
        return (float) (etat & 0x00FFFFFFu) / 16777216.0f;
    }

    inline float bipolaire() noexcept { return uniforme() * 2.0f - 1.0f; }

    /** Gaussien approche par somme de trois uniformes — assez pour de la
        modulation lente, et sans appel a std::normal_distribution qui alloue. */
    inline float gaussien() noexcept
    {
        return (bipolaire() + bipolaire() + bipolaire()) * 0.7071f;
    }

private:
    unsigned int etat;
};

} // namespace luxrev
