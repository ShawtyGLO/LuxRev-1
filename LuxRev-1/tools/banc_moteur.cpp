// Banc de verification du portage : rend les reponses impulsionnelles du moteur
// C++ pour les comparer a celles du prototype Python.
//
// ⚠️ Pourquoi ce banc existe. pluginval valide la CONSTRUCTION du plugin — pas
// de plantage, pas de NaN, l'etat se recharge — et il a repondu SUCCESS a
// severite 10. Il n'a pas d'oreilles : un moteur qui compile, ne plante pas et
// sonne faux passe au vert. Fautes de construction et fautes de son sont deux
// familles disjointes.
//
// Le seul juge du portage est la comparaison avec le prototype, qui est lui-meme
// accorde sur les mesures des 14 IR du LXP-1. Ce banc n'inclut PAS JUCE : il
// compile les memes en-tetes DSP que le plugin, donc ce qu'il mesure est bien ce
// que le plugin fait.
//
// Compilation (depuis un invite developpeur x64) :
//     cl /O2 /EHsc /std:c++17 tools\banc_moteur.cpp /Fe:banc_moteur.exe
//
// Sortie : un WAV float 32 bits stereo par programme, dans le dossier donne en
// argument. Le prototype les relit et compare.

#include "../Source/Dsp/Moteur.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace luxrev;

namespace
{
    constexpr double kFs = 48000.0;

    void ecrireWav (const std::string& chemin, const std::vector<float>& g,
                    const std::vector<float>& d, double fs)
    {
        const int n = (int) g.size();
        const int canaux = 2, bits = 32;
        const int bloc = canaux * bits / 8;
        const int taille = n * bloc;

        FILE* f = nullptr;
        if (fopen_s (&f, chemin.c_str(), "wb") != 0 || f == nullptr)
        {
            std::printf ("  !! ecriture impossible : %s\n", chemin.c_str());
            return;
        }
        auto u32 = [f] (unsigned int v) { std::fwrite (&v, 4, 1, f); };
        auto u16 = [f] (unsigned short v) { std::fwrite (&v, 2, 1, f); };

        std::fwrite ("RIFF", 1, 4, f); u32 (36 + taille); std::fwrite ("WAVE", 1, 4, f);
        std::fwrite ("fmt ", 1, 4, f); u32 (16); u16 (3); u16 ((unsigned short) canaux);
        u32 ((unsigned int) fs); u32 ((unsigned int) (fs * bloc));
        u16 ((unsigned short) bloc); u16 ((unsigned short) bits);
        std::fwrite ("data", 1, 4, f); u32 (taille);
        for (int i = 0; i < n; ++i) { std::fwrite (&g[(size_t) i], 4, 1, f);
                                      std::fwrite (&d[(size_t) i], 4, 1, f); }
        std::fclose (f);
    }
}

int main (int argc, char** argv)
{
    const std::string dossier = (argc > 1) ? argv[1] : ".";
    const double duree = (argc > 2) ? std::atof (argv[2]) : 4.0;
    const float color = (argc > 3) ? (float) std::atof (argv[3]) : 0.5f;
    // Multiplicateur de modulation. A 0, les deux moteurs deviennent
    // deterministes et comparables au bit pres : c'est ce qui permet de dire si
    // un ecart de timbre vient du tirage aleatoire ou d'une faute de portage.
    const float mod = (argc > 4) ? (float) std::atof (argv[4]) : 1.0f;
    // 1 = on court-circuite le tank : on ne mesure plus que la chaine de
    // conversion. C'est ce qui permet de dire OU se loge un ecart de timbre.
    const int chaine = (argc > 5) ? std::atoi (argv[5]) : 0;   // 1 = sans tank, 2 = sonde interne
    // 0 = pas de quantification, pour sortir le dither de la comparaison.
    const int bits = (argc > 6) ? std::atoi (argv[6]) : 16;
    // 0 = impulsion, 1 = bruit deterministe.
    //
    // ⚠️ L'impulsion est un MAUVAIS signal de comparaison ici. Le bloqueur
    // d'ordre zero maintient chaque echantillon jusqu'au suivant : la reponse a
    // une impulsion depend donc de la PHASE de cette impulsion par rapport a
    // l'horloge interne de 31,25 kHz. Mesure faite : decaler l'impulsion de 300
    // echantillons deplace le spectre de 10 dB. Un bruit stationnaire moyenne
    // toutes les phases et donne une mesure stable.
    const int signal = (argc > 7) ? std::atoi (argv[7]) : 0;
    // Position de l'impulsion, en echantillons.
    //
    // ⚠️ Elle compte, et c'est contre-intuitif. Le bloqueur d'ordre zero
    // maintient chaque echantillon jusqu'au suivant : la reponse a une
    // impulsion depend donc de la phase de celle-ci par rapport a l'horloge de
    // 31,25 kHz. Comparer deux moteurs sur une seule position d'impulsion, c'est
    // comparer deux phases. Il faut moyenner sur plusieurs.
    //
    // Une position non nulle sert aussi cote prototype : np.convolve(..., "same")
    // retire (N-1)/2 echantillons au debut, donc une impulsion posee a l'indice 0
    // y perd la moitie avant de la reponse de ses deux FIR.
    const int decalage = (argc > 8) ? std::atoi (argv[8]) : 0;

    const int n = (int) (kFs * duree);
    std::vector<float> x ((size_t) n, 0.0f), g ((size_t) n), d ((size_t) n);

    std::printf ("Banc du moteur C++ — %d programmes, %.1f s, COLOR %.0f %%, modulation x%.1f\n",
                 kNombreProgrammes, duree, color * 100.0f, mod);

    for (int i = 0; i < kNombreProgrammes; ++i)
    {
        const Programme& p = kProgrammes[i];

        Moteur m;
        m.preparer (kFs);
        Reglages r = reglagesDe (p);
        r = macroColor (r, color, p);
        r.modProfondeur *= mod;
        r.bits = bits;
        m.contournerTank = (chaine != 0);
        std::vector<float> sonde;
        if (chaine == 2) { sonde.reserve ((size_t) n); m.sondeInterne = &sonde; }
        m.appliquer (r, &p);
        m.vider();

        std::fill (x.begin(), x.end(), 0.0f);
        if (signal == 0)
        {
            x[(size_t) std::min (decalage, n - 1)] = 1.0f;
        }
        else
        {
            // congruentiel lineaire de Numerical Recipes — reproduit a
            // l'identique cote Python, pour que les deux moteurs recoivent
            // exactement le meme signal
            unsigned int e = 12345u;
            for (int k = 0; k < n; ++k)
            {
                e = e * 1664525u + 1013904223u;
                x[(size_t) k] = 0.25f * ((float) ((e >> 8) & 0xFFFFFFu) / 8388608.0f - 1.0f);
            }
        }
        const auto t0 = std::chrono::steady_clock::now();
        // par blocs, comme un hote : cela verifie aussi que l'etat traverse
        // correctement les frontieres de bloc
        constexpr int bloc = 512;
        for (int k = 0; k < n; k += bloc)
        {
            const int m2 = std::min (bloc, n - k);
            m.traiter (&x[(size_t) k], &g[(size_t) k], &d[(size_t) k], m2);
        }

        const double secondes = std::chrono::duration<double> (
                                    std::chrono::steady_clock::now() - t0).count();
        // Part d'un coeur : temps de calcul rapporte a la duree du signal rendu.
        const double charge = secondes / duree * 100.0;

        float crete = 0.0f;
        for (int k = 0; k < n; ++k) crete = std::max (crete, std::max (std::abs (g[(size_t) k]),
                                                                       std::abs (d[(size_t) k])));
        std::string nom (p.nom);
        for (auto& c : nom) if (c == ' ') c = '_';
                if (chaine == 2)
            ecrireWav (dossier + "/INTERNE_" + nom + ".wav", sonde, sonde, kFsInterne);
        else
            ecrireWav (dossier + (chaine ? "/CHAINE_" : (signal ? "/BRUIT_" : "/CPP_"))
                       + nom + (decalage ? ("_d" + std::to_string (decalage)) : "") + ".wav", g, d, kFs);
        std::printf ("  %-10s crete %.4f  latence %3d ech.  CPU %5.2f %% d'un coeur\n",
                     p.nom, crete, m.latenceEchantillons(), charge);
    }
    return 0;
}
