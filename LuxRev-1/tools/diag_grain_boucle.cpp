// Quantifier DANS la boucle du FDN change-t-il quelque chose de mesurable ?
//
// La memoire audio du LXP-1 fait 64K x 16 bits : dans la machine, le contenu des
// lignes a retard est quantifie a chaque tour. Notre moteur ne quantifie qu'en
// entree et en sortie du tank.
//
// ⛔ LES IR NE PEUVENT PAS ARBITRER. Leur plancher est a -137 dB avec des queues
// qui decroissent proprement : la deconvolution du sweep a efface le bruit de la
// machine. Une machine 16 bits qui annonce 85 dB ne peut pas produire 140 dB de
// plage dynamique. La decision se prend donc sur l'ecart entre NOS deux versions.
//
// Ce que ce banc mesure, et c'est la signature meme d'une erreur qui recircule :
//   1. le PLANCHER de la queue, longtemps apres l'impulsion. Hors boucle il est
//      fixe ; dans la boucle il doit monter avec le gain de boucle, donc avec le
//      temps de decroissance.
//   2. la DECROISSANCE REELLE atteinte — si le plancher monte, la queue s'arrete
//      dessus et le T60 utile raccourcit.
//   3. le cout en CPU : huit quantifications de plus par echantillon interne.
//
// Compilation, deux fois, depuis un invite developpeur x64 :
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=0 tools\diag_grain_boucle.cpp /Fe:tools\diag_grain_hors.exe
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=1 tools\diag_grain_boucle.cpp /Fe:tools\diag_grain_dans.exe

#include "../Source/Dsp/Moteur.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace luxrev;

static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-30)); }

int main()
{
    constexpr double fs = 48000.0;
    constexpr double duree = 20.0;        // ⚠️ long : a 6 s on mesurait encore la queue
    constexpr int bloc = 256;
    const int n = (int) (fs * duree);

    std::printf ("  quantification dans la boucle : %s\n\n",
                 LUXREV_QUANT_BOUCLE ? "OUI" : "non");
    std::printf ("  %-12s %7s %10s %10s %9s %8s\n",
                 "programme", "t60", "crete", "plancher", "plage", "CPU");

    std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);

    for (int i = 0; i < kNombreProgrammes; ++i)
    {
        const Programme& p = kProgrammes[i];
        Moteur m;
        m.preparer (fs);
        Reglages r = macroColor (reglagesDe (p), 0.5f, p);
        m.appliquer (r, &p);
        m.vider();

        std::fill (x.begin(), x.end(), 0.0f);
        x[200] = 1.0f;

        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < n; k += bloc)
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k],
                       std::min (bloc, n - k));
        const double sec = std::chrono::duration<double> (
                               std::chrono::steady_clock::now() - t0).count();

        double crete = 0.0;
        for (int k = 0; k < n; ++k) crete = std::max (crete, (double) std::abs (g[k]));

        // Plancher : les cinq dernieres secondes. Meme un t60 de 12 s y est eteint.
        double s = 0.0;
        const int deb = (int) (fs * (duree - 5.0));
        for (int k = deb; k < n; ++k) s += (double) g[k] * g[k];
        const double pl = std::sqrt (s / (n - deb));

        std::printf ("  %-12s %6.2fs %9.1f %9.1f %8.1f %7.1f%%\n",
                     p.nom, (double) r.t60S, db (crete), db (pl),
                     db (crete / std::max (pl, 1e-30)), sec / duree * 100.0);
    }
    return 0;
}
