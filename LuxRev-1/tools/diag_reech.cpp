// Diagnostic du reechantillonnage : la marge entre ce que le tank a produit et
// ce que la sortie consomme. Si elle passe sous zero, le bloqueur relit le
// dernier echantillon disponible et la sortie se fige — ce qui s'entend comme
// un gresillement periodique au rythme du bloc.
#include "../Source/Dsp/Moteur.h"
#include <cstdio>
int main()
{
    using namespace luxrev;
    Moteur m;
    m.preparer (48000.0);
    Reglages r = reglagesDe (kProgrammes[1]);
    m.appliquer (r, &kProgrammes[1]);
    m.vider();

    const int n = 48000 * 2;
    std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
    for (int i = 0; i < n; ++i) x[(size_t) i] = 0.2f * std::sin (i * 0.05f);

    for (int bloc : { 512, 512, 512, 64, 1024, 128, 512 })
    {
        int k = 0;
        while (k + bloc <= n)
        {
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], bloc);
            k += bloc;
        }
        std::printf ("  bloc %4d : marge minimale %lld echantillons internes\n",
                     bloc, m.margeMinimale);
        m.margeMinimale = 1 << 30;
    }
    return 0;
}
