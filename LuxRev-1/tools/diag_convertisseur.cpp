// Ce que le convertisseur partage du LXP-5 fait vraiment au champ stereo.
//
// Le manuel de service donne l'ordre d'une periode d'echantillonnage : conversion
// A/N, voie DROITE, voie GAUCHE. Un tiers de periode separe donc les deux
// bloqueurs, soit 10,7 us a 31,25 kHz. Ce banc mesure ce que ce tiers change.
//
// Deux questions, et deux seulement :
//   1. la somme MONO est-elle intacte ? (un decalage de 10,7 us met son premier
//      creux de peigne a 46,7 kHz, donc elle doit l'etre — a verifier, pas a
//      supposer)
//   2. la correlation entre voies baisse-t-elle, et dans quelles bandes ?
//
// Compilation, deux fois, depuis un invite developpeur x64 :
//   cl /O2 /EHsc /std:c++17 /DLUXREV_TIERS_CONVERTISSEUR=0.0 tools\diag_convertisseur.cpp /Fe:tools\diag_conv_sans.exe
//   cl /O2 /EHsc /std:c++17                                  tools\diag_convertisseur.cpp /Fe:tools\diag_conv_avec.exe

#include "../Source/Dsp/Moteur.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace luxrev;

int main (int argc, char** argv)
{
    // ⚠️ Le programme se choisit, et ce n'est pas un detail de confort : sur une queue
    // de reverbe les deux voies sont deja decorrelees par construction (0,00 a 0,03),
    // donc un decalage de 10,7 us n'y change rien. C'est dans le champ PRECOCE, encore
    // correle, qu'il peut compter — donc sur les programmes courts.
    const int iProg = (argc > 1) ? std::atoi (argv[1]) : 6;
    const double fenetre = (argc > 2) ? std::atof (argv[2]) : 0.0;   // s ; 0 = tout
    constexpr double fs = 48000.0;
    constexpr int    n  = (int) (fs * 8.0);
    constexpr int    bloc = 256;

    // Bruit blanc stationnaire : la seule facon de mesurer une correlation par
    // bande sans dependre de la phase d'une impulsion (cf. l'erreur de mesure du
    // 2026-09-06 — le bloqueur rend une IR unique dependante de sa phase).
    std::mt19937 rng (12345);
    std::normal_distribution<float> loi (0.0f, 0.25f);
    std::vector<float> x ((size_t) n);
    for (auto& v : x) v = loi (rng);

    const Programme& p = kProgrammes[iProg];
    Moteur m;
    m.preparer (fs);
    Reglages r = macroColor (reglagesDe (p), 0.5f, p);
    r.stereo = 1.0f;
    m.appliquer (r, &p);
    m.vider();

    std::vector<float> L ((size_t) n), R ((size_t) n);
    for (int i = 0; i < n; i += bloc)
    {
        const int k = std::min (bloc, n - i);
        m.traiter (&x[(size_t) i], &L[(size_t) i], &R[(size_t) i], k);
    }

    // On jette la premiere seconde : etablissement du tank.
    const int deb = (int) fs;
    // Fenetre d'analyse : tout par defaut, ou les premieres `fenetre` secondes apres
    // l'etablissement pour ne regarder que le champ precoce.
    const int fin = (fenetre > 0.0) ? std::min (n, deb + (int) (fs * fenetre)) : n;

    // --- somme mono, en dB
    double em = 0.0;
    for (int i = deb; i < fin; ++i) { const double s = 0.5 * (L[i] + R[i]); em += s * s; }
    em = 10.0 * std::log10 (em / (fin - deb) + 1e-30);

    // --- correlation par bande, six bandes d'octave par filtrage biquad
    const double centres[] = { 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 };
    printf ("  decalage L/R applique : %.4f echantillon interne (%.1f us)\n",
            kTiersConvertisseur, kTiersConvertisseur / kFsInterne * 1e6);
    printf ("  somme mono : %+.3f dB\n", em);
    printf ("  correlation par bande :\n");
    for (double fc : centres)
    {
        // passe-bande resonant simple, Q = 2
        const double w = 2.0 * 3.14159265358979323846 * fc / fs, q = 2.0;
        const double a = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + a;
        const double b0 = a / a0, b2 = -a / a0, a1 = -2.0 * c / a0, a2 = (1.0 - a) / a0;
        // Forme directe II transposee, b1 = 0 sur un passe-bande a jupes constantes.
        double l1 = 0, l2 = 0, r1 = 0, r2 = 0;
        double sll = 0, srr = 0, slr = 0;
        for (int i = 0; i < n; ++i)
        {
            const double yl = b0 * L[i] + l1;
            l1 = -a1 * yl + l2;
            l2 = b2 * L[i] - a2 * yl;
            const double yr = b0 * R[i] + r1;
            r1 = -a1 * yr + r2;
            r2 = b2 * R[i] - a2 * yr;
            if (i >= deb && i < fin) { sll += yl*yl; srr += yr*yr; slr += yl*yr; }
        }
        printf ("    %5.0f Hz   %+.4f\n", fc, slr / (std::sqrt (sll * srr) + 1e-30));
    }
    return 0;
}
