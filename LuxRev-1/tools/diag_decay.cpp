// Verifie que Decay dilate bien le motif de Gate et Inverse, au lieu de ne
// servir a rien : on releve la duree au-dessus de -20 dB pour trois reglages.
#include "../Source/Dsp/Moteur.h"
#include <cstdio>
using namespace luxrev;
int main()
{
    const double fs = 48000.0;
    std::printf ("Duree du motif au-dessus de -20 dB, selon Decay\n\n");
    std::printf ("  %-9s %10s %10s %10s\n", "programme", "Decay min", "Decay usine", "Decay max");
    for (int i : { 10, 11 })
    {
        const Programme& p = kProgrammes[i];
        std::printf ("  %-9s", p.nom);
        for (float dec : { p.t60Min, p.t60S, p.t60Max })
        {
            Moteur m; m.preparer (fs);
            Reglages r = macroColor (reglagesDe (p), 0.5f, p);
            r.t60S = std::clamp (dec, p.t60Min, p.t60Max);
            m.appliquer (r, &p); m.vider();

            const int n = (int) (fs * 5.0);
            std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
            x[200] = 1.0f;
            for (int k = 0; k < n; k += 512)
                m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (512, n - k));

            const int pas = (int) (fs * 0.005);
            double crete = 0.0;
            std::vector<double> e;
            for (int b = 0; b + pas <= n; b += pas)
            {
                double s = 0.0;
                for (int k = b; k < b + pas; ++k) s += g[(size_t) k] * g[(size_t) k];
                e.push_back (std::sqrt (s / pas));
                crete = std::max (crete, e.back());
            }
            int dernier = 0;
            for (size_t k = 0; k < e.size(); ++k)
                if (20.0 * std::log10 (std::max (e[k] / crete, 1e-12)) > -20.0) dernier = (int) k;
            std::printf (" %8.0f ms", dernier * 5.0);
        }
        std::printf ("\n");
    }
    return 0;
}
