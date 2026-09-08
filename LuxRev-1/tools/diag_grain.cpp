// Deux questions, une mesure chacune.
//
// 1. PLANCHER DE QUANTIFICATION. Le manuel du LXP-1 annonce 85 dB de plage
//    dynamique. Si notre plancher est plus haut, on entend le grain plus fort
//    que la machine ne le faisait — et ce n'est pas de la fidelite, c'est un
//    niveau interne mal cale.
//
// 2. DECLENCHEMENT DE LA PORTE. On envoie trois bouffees separees par du
//    silence et on releve l'enveloppe de sortie : si la porte ne se rouvre pas,
//    ca se voit.
#include "../Source/Dsp/Moteur.h"
#include <cstdio>

using namespace luxrev;

static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-30)); }

int main (int argc, char** argv)
{
    const int mode = (argc > 1) ? std::atoi (argv[1]) : 0;
    const double fs = 48000.0;

    if (mode == 2)
    {
        // Niveau de sortie du tank, programme par programme, pour un meme signal
        // d'entree. Sert a calculer le gain interne : c'est lui qui decide quelle
        // part de la grille 16 bits chaque programme occupe, donc a quel point on
        // entend le grain de quantification.
        for (int i = 0; i < kNombreProgrammes; ++i)
        {
            Moteur m; m.preparer (fs);
            Reglages r = macroColor (reglagesDe (kProgrammes[i]), 0.5f, kProgrammes[i]);
            m.appliquer (r, &kProgrammes[i]);
            m.vider();
            const int n = (int) (fs * 3.0);
            std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
            unsigned int e = 99u;
            for (int k = 0; k < n; ++k)
            {
                e = e * 1664525u + 1013904223u;
                x[(size_t) k] = (float) (0.25 * ((double) ((e >> 8) & 0xFFFFFF) / 8388608.0 - 1.0));
            }
            for (int k = 0; k < n; k += 512)
                m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (512, n - k));
            double c = 0.0;
            for (int k = (int) (fs * 0.5); k < n; ++k)
                c = std::max (c, std::max ((double) std::abs (g[(size_t) k]),
                                           (double) std::abs (d[(size_t) k])));
            std::printf ("%s  %.6f\n", kProgrammes[i].nom, c);
        }
        return 0;
    }

    if (mode == 0)
    {
        std::printf ("Plancher de quantification — manuel du LXP-1 : 85 dB de plage dynamique\n\n");
        std::printf ("  %-10s %10s %12s %12s\n", "programme", "crete", "plancher", "plage");
        for (int i : { 1, 6, 8, 10 })
        {
            Moteur m; m.preparer (fs);
            Reglages r = macroColor (reglagesDe (kProgrammes[i]), 0.5f, kProgrammes[i]);
            m.appliquer (r, &kProgrammes[i]);
            m.vider();

            // ⚠️ 20 s, pas 6. Avec un T60 de 2,6 s, la queue de Hall B est
            // encore bien au-dessus du bruit a 6 s : on mesurait de la reverbe
            // et on l'appelait plancher de quantification.
            const int n = (int) (fs * 20.0);
            std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
            // ⚠️ Bruit soutenu a -12 dBFS pendant 3 s, pas une impulsion : c'est
            // le niveau que le tank voit vraiment en usage, et c'est lui qui dit
            // combien des 16 bits on utilise.
            unsigned int e = 99u;
            for (int k = 0; k < (int) (fs * 3.0); ++k)
            {
                e = e * 1664525u + 1013904223u;
                x[(size_t) k] = (float) (0.25 * ((double) ((e >> 8) & 0xFFFFFF) / 8388608.0 - 1.0));
            }
            for (int k = 0; k < n; k += 512)
                m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (512, n - k));

            double crete = 0.0;
            for (int k = 0; k < (int) (fs * 3.0); ++k)
                crete = std::max (crete, (double) std::abs (g[(size_t) k]));
            // plancher : energie efficace de la derniere seconde, ou la queue est
            // largement eteinte et ou il ne reste que le bruit du convertisseur
            double s = 0.0;
            const int deb = n - (int) (fs * 0.5);
            for (int k = deb; k < n; ++k) s += (double) g[(size_t) k] * g[(size_t) k];
            const double plancher = std::sqrt (s / (double) (n - deb));
            std::printf ("  %-10s %9.2f %11.1f %11.1f dB\n", kProgrammes[i].nom,
                         crete, db (plancher), db (crete) - db (plancher));
        }
        return 0;
    }

    // --- declenchement de la porte
    for (int i : { 10, 11 })
    {
        Moteur m; m.preparer (fs);
        Reglages r = macroColor (reglagesDe (kProgrammes[i]), 0.5f, kProgrammes[i]);
        m.appliquer (r, &kProgrammes[i]);
        m.vider();

        const int n = (int) (fs * 4.0);
        std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
        // trois bouffees de 120 ms a 1 s d'intervalle, comme des coups de caisse
        unsigned int e = 7u;
        for (int b = 0; b < 3; ++b)
            for (int k = 0; k < (int) (fs * 0.12); ++k)
            {
                e = e * 1664525u + 1013904223u;
                const double env = std::exp (-k / (fs * 0.03));
                x[(size_t) ((int) (fs * (0.2 + b)) + k)] =
                    (float) (0.5 * env * ((double) ((e >> 8) & 0xFFFFFF) / 8388608.0 - 1.0));
            }
        for (int k = 0; k < n; k += 512)
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (512, n - k));

        std::printf ("\n  %s — enveloppe de sortie, une valeur par 50 ms\n   ", kProgrammes[i].nom);
        for (int t = 0; t < 70; ++t)
        {
            double s = 0.0;
            const int a = (int) (t * fs * 0.05), b2 = (int) ((t + 1) * fs * 0.05);
            for (int k = a; k < b2 && k < n; ++k) s += (double) g[(size_t) k] * g[(size_t) k];
            const double v = db (std::sqrt (s / (fs * 0.05)));
            std::printf ("%c", v > -30 ? '#' : (v > -45 ? '+' : (v > -60 ? '.' : ' ')));
        }
        std::printf ("\n    (# fort, + moyen, . faible, espace silence ; les coups sont a 0,2 / 1,2 / 2,2 s)\n");
    }
    return 0;
}
