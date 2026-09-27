// La quantification DANS la boucle peut-elle SERVIR de dither ?
//
// L'idee, posee par l'utilisateur le 2026-09-27 : notre dither TPDF de sortie est
// un bruit AJOUTE A LA MAIN pour imiter un plancher de convertisseur. Si la
// quantification en boucle fabrique deja un plancher du bon ordre — et la mesure
// dit qu'elle en fabrique un, jusqu'a +4,5 dB — alors elle pourrait ETRE le
// dither. On remplacerait un bruit decoratif par un bruit MODELISE, issu du meme
// mecanisme que dans la machine.
//
// ⚠️ LE PIEGE A VERIFIER. L'erreur de quantification disparait quand le signal
// disparait. Plus de signal, plus de dither, et la fin de queue se fait TRONQUER
// au lieu de se dissoudre — exactement ce que le dither devait empecher. Le
// critere vient des IR d'origine : elles ont 99 % d'echantillons non nuls jusqu'a
// la fin du fichier.
//
// Deux grandeurs, et il faut les deux :
//   1. TAUX D'ECHANTILLONS NON NULS a la toute fin de la queue. C'est le critere
//      releve sur les IR. Une troncature seche le fait tomber a zero d'un coup.
//   2. DISTORSION A BAS NIVEAU. Un quantificateur non dithere produit une erreur
//      CORRELEE au signal, donc de la distorsion harmonique et non du bruit. On
//      mesure le residu hors de la fondamentale sur un sinus tres faible, la ou
//      le defaut se voit le mieux.
//
// Compilation, quatre fois :
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=0 /DLUXREV_DITHER=1 ... /Fe:tools\ad_actuel.exe
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=1 /DLUXREV_DITHER=1 ... /Fe:tools\ad_deux.exe
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=1 /DLUXREV_DITHER=0 ... /Fe:tools\ad_auto.exe
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=0 /DLUXREV_DITHER=0 ... /Fe:tools\ad_sec.exe

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

    std::printf ("  boucle %s   dither %s\n\n",
                 LUXREV_QUANT_BOUCLE ? "OUI" : "non", LUXREV_DITHER ? "OUI" : "non");

    // ---- 1. fin de queue : reste-t-il du signal, ou est-ce tronque ?
    std::printf ("  %-12s %10s %12s %12s\n",
                 "programme", "plancher", "non nuls", "plus faible");
    for (int i : { 1, 5, 6 })                    // Small_2, Large_2, Hall_B
    {
        const Programme& p = kProgrammes[i];
        const int n = (int) (fs * 14.0);
        std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);
        x[200] = 1.0f;

        Moteur m;
        m.preparer (fs);
        m.appliquer (macroColor (reglagesDe (p), 0.5f, p), &p);
        m.vider();
        for (int k = 0; k < n; k += bloc)
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (bloc, n - k));

        // la DERNIERE seconde : bien apres l'extinction de tous les programmes
        const int deb = (int) (fs * 13.0);
        double s = 0.0, mini = 1.0;
        int nz = 0;
        for (int k = deb; k < n; ++k)
        {
            const double v = std::abs ((double) g[k]);
            s += v * v;
            if (v > 0.0) { ++nz; mini = std::min (mini, v); }
        }
        std::printf ("  %-12s %9.1f %11.1f%% %11.1f\n", p.nom,
                     db (std::sqrt (s / (n - deb))),
                     100.0 * nz / (n - deb),
                     nz ? db (mini) : 0.0);
    }

    // ---- 2. distorsion a bas niveau : l'erreur est-elle du bruit ou du signal ?
    //
    // Sinus a -60 dBFS, tank CONTOURNE : on isole le convertisseur de sortie, sans
    // quoi la queue de reverbe masquerait tout. Le residu hors fondamentale dit si
    // l'erreur est decorrelee (du bruit, bien) ou non (de la distorsion, mal).
    {
        const Programme& p = kProgrammes[6];
        const int n = 1 << 16;
        std::vector<float> x ((size_t) n), g ((size_t) n), d ((size_t) n);
        const double f0 = 997.0;
        for (int k = 0; k < n; ++k)
            x[(size_t) k] = (float) (0.001 * std::sin (2.0 * M_PI * f0 * k / fs));

        Moteur m;
        m.preparer (fs);
        m.contournerTank = true;
        m.appliquer (macroColor (reglagesDe (p), 0.5f, p), &p);
        m.vider();
        for (int k = 0; k < n; k += bloc)
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], std::min (bloc, n - k));

        // Goertzel sur la fondamentale et ses sept premiers harmoniques.
        auto raie = [&] (double f)
        {
            double c = 0.0, s = 0.0;
            for (int k = (int) (fs * 0.2); k < n; ++k)
            {
                const double a = 2.0 * M_PI * f * k / fs;
                c += g[(size_t) k] * std::cos (a);
                s += g[(size_t) k] * std::sin (a);
            }
            return std::sqrt (c * c + s * s);
        };
        const double f = raie (f0);
        double h = 0.0;
        for (int k = 2; k <= 8; ++k) { const double v = raie (f0 * k); h += v * v; }
        std::printf ("\n  sinus -60 dBFS, tank contourne :\n");
        std::printf ("    harmoniques 2 a 8, sous la fondamentale : %.1f dB\n",
                     db (std::sqrt (h) / std::max (f, 1e-30)));
    }
    return 0;
}
