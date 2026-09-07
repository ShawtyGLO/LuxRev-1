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
        buf.assign ((size_t) capacite, 0.0f);
        taille = capacite;
        i = 0;
    }

    /** Change le retard sans allouer (pre-delai : 0 a 246 ms sur la machine). */
    void poserLongueur (int n)
    {
        const int m = std::clamp (n, 1, capacite);
        if (m != taille) { taille = m; i = 0; }
    }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0f); i = 0; }

    inline float pousser (float x) noexcept
    {
        const float y = buf[(size_t) i];
        buf[(size_t) i] = x;
        if (++i >= taille) i = 0;
        return y;
    }

private:
    std::vector<float> buf;
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
        buf.assign ((size_t) taille, 0.0f);
        i = 0;
        k = coef;
    }

    /** Le coefficient bouge avec Diffusion ; la longueur, elle, ne depend pas de
        `taille` — le diffuseur d'entree est transpose une fois pour toutes. */
    void poserCoef (float coef) noexcept { k = coef; }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0f); i = 0; }

    inline float traiter (float x) noexcept
    {
        const float d = buf[(size_t) i];
        const float w = x + k * d;
        buf[(size_t) i] = w;
        if (++i >= taille) i = 0;
        return d - k * w;
    }

private:
    std::vector<float> buf;
    int taille = 1, i = 0;
    float k = 0.0f;
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
        buf.assign ((size_t) taille, 0.0f);
        nominal = std::max (nominalMax, 4);
        i = 0;
        yi = 0.0f;
        calculerPrises();
    }

    /** Change le retard SANS allouer — COLOR balaie `taille` en continu, et
        reconstruire les huit lignes a chaque mouvement du bouton allouerait
        dans le fil audio. */
    void poserNominal (int n)
    {
        nominal = std::clamp (n, 4, taille - margeMax - 4);
        calculerPrises();
    }

    void calculerPrises()
    {

        // Prises intermediaires le long de la ligne — la structure de la FIG. 2
        // du brevet EMT US4181820 (Blesser & Bader) : UNE longue ligne portant
        // des prises A2..An. Densite d'arrivees elevee avec peu de memoire.
        static constexpr float fractions[kNPrises] = { 0.06f, 0.13f, 0.24f, 0.41f, 0.63f, 0.85f };
        for (int k = 0; k < kNPrises; ++k)
            prises[k] = std::max ((int) (nominal * fractions[k]), 1);
    }

    void vider() { std::fill (buf.begin(), buf.end(), 0.0f); i = 0; yi = 0.0f; }

    inline float traiter (float x, float decalage) noexcept
    {
        const float p = (float) nominal + decalage;
        int   i0 = (int) p;
        float f  = p - (float) i0;
        if (f < 0.5f) { i0 -= 1; f += 1.0f; }   // l'allpass du 1er ordre se tient
                                                // mieux pour f dans [0,5 ; 1,5)
        int a = (i - i0) % taille; if (a < 0) a += taille;
        int b = (i - i0 - 1) % taille; if (b < 0) b += taille;

        const float eta = (1.0f - f) / (1.0f + f);
        const float d = eta * buf[(size_t) a] + buf[(size_t) b] - eta * yi;
        yi = d;

        buf[(size_t) i] = x;
        if (++i >= taille) i = 0;
        return d;
    }

    int nominalEch() const noexcept { return nominal; }

private:
    std::vector<float> buf;
    int taille = 8, i = 0, nominal = 4, margeMax = 0;
    int prises[kNPrises] { 1, 1, 1, 1, 1, 1 };
    float yi = 0.0f;
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
/** Interpolateur a retard fractionnaire, sinus cardinal fenetre.

    ⚠️ Il remplace un Lagrange a 4 points, et ce n'etait pas un detail de qualite :
    le Lagrange cubique s'affaisse dans l'aigu bien avant Nyquist. Mesure contre le
    prototype, la descente vers 31,25 kHz coutait 2,2 dB entre 2 et 16 kHz,
    systematiquement dans le meme sens. C'est exactement la famille d'erreur que
    l'oreille attrape en premier sur ce projet — « pas assez de brillance » — et
    qui a deja coute plusieurs tours au prototype.

    Table de phases precalculee : le cout en temps reel est de kTaps
    multiplications par echantillon interne, soit environ 0,5 M op/s. Rien.
*/
class InterpolateurSinc
{
public:
    static constexpr int kTaps = 16;
    static constexpr int kPhases = 512;

    void preparer()
    {
        if (! table.empty()) return;
        table.assign ((size_t) (kPhases * kTaps), 0.0f);
        constexpr int demi = kTaps / 2;
        for (int p = 0; p < kPhases; ++p)
        {
            const double frac = (double) p / kPhases;
            double somme = 0.0;
            for (int k = 0; k < kTaps; ++k)
            {
                const double x = (double) (k - demi + 1) - frac;
                const double s = (std::abs (x) < 1e-9) ? 1.0
                                   : std::sin (3.14159265358979323846 * x)
                                     / (3.14159265358979323846 * x);
                // fenetre de Blackman : lobes secondaires assez bas pour que la
                // bande utile reste plate a mieux que 0,05 dB
                const double u = (double) k / (kTaps - 1);
                const double w = 0.42 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * u)
                                      + 0.08 * std::cos (4.0 * 3.14159265358979323846 * u);
                const double v = s * w;
                table[(size_t) (p * kTaps + k)] = (float) v;
                somme += v;
            }
            // gain unite en continu, phase par phase : sans ca l'interpolateur
            // module legerement le niveau au rythme de la phase
            if (std::abs (somme) > 1e-9)
                for (int k = 0; k < kTaps; ++k)
                    table[(size_t) (p * kTaps + k)] = (float) (table[(size_t) (p * kTaps + k)] / somme);
        }
    }

    /** `lire(anneau, masque, pos)` : pos en echantillons, partie fractionnaire
        interpolee. L'anneau doit avoir au moins kTaps echantillons d'avance. */
    inline float lire (const float* anneau, long long masque, double pos) const noexcept
    {
        const long long i0 = (long long) pos;
        const int p = (int) ((pos - (double) i0) * kPhases) & (kPhases - 1);
        const float* h = &table[(size_t) (p * kTaps)];
        constexpr int demi = kTaps / 2;
        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k)
            acc += h[k] * anneau[(size_t) ((i0 + k - demi + 1) & masque)];
        return acc;
    }

private:
    std::vector<float> table;
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
