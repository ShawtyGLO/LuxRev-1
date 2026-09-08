// Mesure de la largeur : la correlation L/R BANDE PAR BANDE.
//
// ⚠️ La correlation large bande ne suffit pas. Amplifier le cote la fait
// descendre partout de la meme facon, ce qui ne s'entend pas comme de la
// largeur mais comme un creux. Ce qui s'entend comme de la largeur, c'est une
// correlation qui VARIE d'une bande a l'autre — d'ou l'ecart-type.
#include "../Source/Dsp/Moteur.h"
#include <cstdio>
int main (int argc, char** argv)
{
    using namespace luxrev;
    const float w = (argc > 1) ? (float) std::atof (argv[1]) : 1.0f;
    const int prog = 6;                                   // Hall B
    Moteur m;
    m.preparer (48000.0);
    Reglages r = macroColor (reglagesDe (kProgrammes[prog]), 0.5f, kProgrammes[prog]);
    r.stereo = w;
    m.appliquer (r, &kProgrammes[prog]);
    m.vider();

    const int n = 48000 * 3;
    std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
    x[200] = 1.0f;
    for (int k = 0; k < n; k += 512)
        m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (512, n - k));

    // correlation par bande, sur la queue seule
    const int deb = (int) (48000 * 0.25);
    const double bandes[6] = { 250, 500, 1000, 2000, 4000, 8000 };
    double c[6], moy = 0.0;
    for (int b = 0; b < 6; ++b)
    {
        Biquad lo[3], hi[3];
        const int nl = butterworthPasseBas (lo, 4, bandes[b] * 1.26, 48000.0);
        const int nh = butterworthPasseBas (hi, 4, bandes[b] / 1.26, 48000.0);
        Chaine fgl, fdl, fgh, fdh;
        fgl.poser (lo, nl); fdl.poser (lo, nl); fgh.poser (hi, nh); fdh.poser (hi, nh);
        double sgg = 0, sdd = 0, sgd = 0;
        for (int i = 0; i < n; ++i)
        {
            const float a = fgl.traiter (g[(size_t) i]) - fgh.traiter (g[(size_t) i]);
            const float e = fdl.traiter (d[(size_t) i]) - fdh.traiter (d[(size_t) i]);
            if (i < deb) continue;
            sgg += a * a; sdd += e * e; sgd += a * e;
        }
        c[b] = sgd / std::sqrt (sgg * sdd + 1e-30);
        moy += c[b];
    }
    moy /= 6.0;
    double var = 0.0;
    for (int b = 0; b < 6; ++b) var += (c[b] - moy) * (c[b] - moy);
    std::printf ("  largeur %4.0f %%  correlation moyenne %+6.3f  ecart-type %.3f   [",
                 w * 100.0f, moy, std::sqrt (var / 6.0));
    for (int b = 0; b < 6; ++b) std::printf (" %+.2f", c[b]);
    std::printf (" ]\n");
    return 0;
}
