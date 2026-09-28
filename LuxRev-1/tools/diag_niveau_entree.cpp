// Le bruit du convertisseur suit-il le niveau d'entree, ou reste-t-il fixe ?
//
// ⚠️ ASYMETRIE CONSTATEE DANS LE MOTEUR. `gainInterne` calibre le quantificateur
// de SORTIE — le signal est monte dans la grille 16 bits puis redescendu, ce qui
// rend le grain uniforme d'un programme a l'autre. Le quantificateur d'ENTREE,
// lui, n'a aucune calibration : il quantifie ce qui arrive, tel quel.
//
// Consequence a verifier : si la source est faible, le grain d'entree ne baisse
// pas avec elle et devient d'autant plus present. C'est exactement ce que le
// manuel du LXP-5 demande d'eviter en reglant l'entree — « INPUT LEVEL should be
// set to the level where the PEAK LED flashes briefly on the loudest peaks only ».
//
// Le banc mesure le rapport signal/bruit de sortie pour plusieurs niveaux
// d'entree. Si le rapport suit le niveau un pour un, le grain est fixe et une
// commande d'entree sert vraiment ; s'il ne bouge pas, elle est superflue.
//
//   cl /O2 /EHsc /std:c++17 tools\diag_niveau_entree.cpp /Fe:tools\diag_niveau_entree.exe

#include "../Source/Dsp/Moteur.h"
#include <cmath>
#include <cstdio>
#include <vector>

using namespace luxrev;
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-30)); }

int main()
{
    constexpr double fs = 48000.0;
    constexpr int bloc = 256;
    const int n = (int) (fs * 14.0);

    std::printf ("  Rapport signal/bruit en sortie, selon le niveau d'entree\n\n");
    std::printf ("  %-10s %10s %10s %10s %10s\n",
                 "programme", "entree", "crete", "plancher", "S/B");

    for (int i : { 1, 6 })                        // Small 2, Hall B
    {
        const Programme& p = kProgrammes[i];
        for (double dbIn : { 0.0, -12.0, -24.0, -36.0 })
        {
            const double amp = std::pow (10.0, dbIn / 20.0);
            std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
            x[200] = (float) amp;

            Moteur m;
            m.preparer (fs);
            m.appliquer (macroColor (reglagesDe (p), 0.5f, p), &p);
            m.vider();
            for (int k = 0; k < n; k += bloc)
                m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k],
                           std::min (bloc, n - k));

            double crete = 0.0;
            for (int k = 0; k < n; ++k) crete = std::max (crete, (double) std::abs (g[k]));

            const int deb = (int) (fs * 13.0);    // bien apres l'extinction
            double s = 0.0;
            for (int k = deb; k < n; ++k) s += (double) g[k] * g[k];
            const double pl = std::sqrt (s / (n - deb));

            std::printf ("  %-10s %9.0f %10.1f %10.1f %10.1f\n",
                         p.nom, dbIn, db (crete), db (pl), db (crete / pl));
        }
        std::printf ("\n");
    }
    return 0;
}
