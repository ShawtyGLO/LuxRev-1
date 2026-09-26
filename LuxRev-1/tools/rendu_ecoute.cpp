// Rend un fichier a travers un programme, pour ECOUTER.
//
// ⚠️ POURQUOI CE BANC MANQUAIT. Tous les autres mesurent : bruit stationnaire,
// impulsion, chiffres. Aucun ne produisait de quoi juger a l'oreille sur de la
// matiere reelle. La note « Accorder une reverbe » (Thomas Guillory, Arturia)
// rappelle que la moitie de l'accordage d'une reverbe se fait sur caisse claire,
// batterie et voix — trois signaux qu'aucun banc du projet n'utilisait.
//
// Il prend donc n'importe quel WAV : ceux de _proto/signaux_reverbe/, ou une
// prise sortie du DAW.
//
// Usage :
//   rendu_ecoute.exe <entree.wav> <sortie.wav> [programme] [mix] [color] [drive]
//     programme : indice 0-11, defaut 6
//     mix       : 0-1, defaut 0,5
//     color     : 0-1, defaut 0,5
//     drive     : 0-1, defaut 0
//
// Compilation (deux fois pour comparer la quantification en boucle) :
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=0 tools\rendu_ecoute.cpp /Fe:tools\rendu_hors.exe
//   cl /O2 /EHsc /std:c++17 /DLUXREV_QUANT_BOUCLE=1 tools\rendu_ecoute.cpp /Fe:tools\rendu_dans.exe

#include "../Source/Dsp/Moteur.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace luxrev;

//------------------------------------------------------------------ lecture WAV
// Lecteur minimal : PCM 16 bits ou float 32, mono ou stereo. Assez pour des
// signaux de test, et pas une bibliotheque de plus a trainer.
static bool lireWav (const char* chemin, std::vector<float>& mono, double& fs)
{
    FILE* f = std::fopen (chemin, "rb");
    if (f == nullptr) { std::printf ("  introuvable : %s\n", chemin); return false; }

    char id[4];
    std::fread (id, 1, 4, f);
    if (std::memcmp (id, "RIFF", 4) != 0) { std::fclose (f); return false; }
    std::fseek (f, 8, SEEK_SET);
    std::fread (id, 1, 4, f);
    if (std::memcmp (id, "WAVE", 4) != 0) { std::fclose (f); return false; }

    int canaux = 0, bits = 0, format = 1;
    unsigned int taux = 0;
    while (std::fread (id, 1, 4, f) == 4)
    {
        unsigned int n = 0;
        if (std::fread (&n, 4, 1, f) != 1) break;
        const long suite = std::ftell (f) + (long) n + (long) (n & 1u);

        if (std::memcmp (id, "fmt ", 4) == 0)
        {
            unsigned short fm = 0, ca = 0, bi = 0;
            std::fread (&fm, 2, 1, f); std::fread (&ca, 2, 1, f);
            std::fread (&taux, 4, 1, f);
            std::fseek (f, 6, SEEK_CUR);                  // debit, alignement
            std::fread (&bi, 2, 1, f);
            format = fm; canaux = ca; bits = bi;
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            if (canaux <= 0 || bits <= 0) { std::fclose (f); return false; }
            const int octets = bits / 8;
            const size_t trames = n / (size_t) (octets * canaux);
            mono.resize (trames);
            std::vector<unsigned char> brut (n);
            std::fread (brut.data(), 1, n, f);
            for (size_t i = 0; i < trames; ++i)
            {
                double s = 0.0;
                for (int c = 0; c < canaux; ++c)
                {
                    const unsigned char* p = &brut[(i * (size_t) canaux + (size_t) c)
                                                   * (size_t) octets];
                    if (format == 3 && bits == 32)      { float v; std::memcpy (&v, p, 4); s += v; }
                    else if (bits == 16) { short v; std::memcpy (&v, p, 2); s += v / 32768.0; }
                    else if (bits == 24)
                    {
                        const int v = (int) ((unsigned) p[0] << 8 | (unsigned) p[1] << 16
                                             | (unsigned) p[2] << 24);
                        s += (double) v / 2147483648.0;
                    }
                    else if (bits == 32) { int v; std::memcpy (&v, p, 4); s += v / 2147483648.0; }
                }
                mono[i] = (float) (s / canaux);           // somme mono : l'entree du LXP-1
            }
            fs = (double) taux;
            std::fclose (f);
            return true;
        }
        std::fseek (f, suite, SEEK_SET);
    }
    std::fclose (f);
    return false;
}

//------------------------------------------------------------------ ecriture WAV
static void ecrireWav (const std::string& chemin, const std::vector<float>& g,
                       const std::vector<float>& d, double fs)
{
    FILE* f = std::fopen (chemin.c_str(), "wb");
    if (f == nullptr) return;
    const unsigned int n = (unsigned int) g.size();
    const unsigned int octets = n * 2u * 4u;
    auto u32 = [f] (unsigned int v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [f] (unsigned short v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36u + octets); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); u32 (16u); u16 (3); u16 (2);
    u32 ((unsigned int) fs); u32 ((unsigned int) fs * 8u); u16 (8); u16 (32);
    std::fwrite ("data", 1, 4, f); u32 (octets);
    for (unsigned int i = 0; i < n; ++i) { std::fwrite (&g[i], 4, 1, f); std::fwrite (&d[i], 4, 1, f); }
    std::fclose (f);
}

//------------------------------------------------------------------------- main
int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("  usage : rendu_ecoute <entree.wav> <sortie.wav> "
                     "[programme 0-11] [mix] [color] [drive]\n");
        return 1;
    }
    const int iProg   = (argc > 3) ? std::atoi (argv[3]) : 6;
    const float mix   = (argc > 4) ? (float) std::atof (argv[4]) : 0.5f;
    const float color = (argc > 5) ? (float) std::atof (argv[5]) : 0.5f;
    const float drive = (argc > 6) ? (float) std::atof (argv[6]) : 0.0f;

    std::vector<float> x;
    double fs = 48000.0;
    if (! lireWav (argv[1], x, fs)) { std::printf ("  lecture impossible\n"); return 1; }

    // ⚠️ On ajoute de la QUEUE : sans ca la reverbe est coupee net a la fin du
    // fichier, et c'est justement la fin qu'on veut entendre quand on juge un
    // plancher de bruit.
    const int queue = (int) (fs * 6.0);
    x.resize (x.size() + (size_t) queue, 0.0f);
    const int n = (int) x.size();

    const Programme& p = kProgrammes[std::clamp (iProg, 0, kNombreProgrammes - 1)];
    Moteur m;
    m.preparer (fs);
    Reglages r = macroColor (reglagesDe (p), color, p);
    r.drive = drive;
    m.appliquer (r, &p);
    m.vider();

    std::vector<float> g ((size_t) n), d ((size_t) n);
    constexpr int bloc = 256;
    for (int i = 0; i < n; i += bloc)
        m.traiter (&x[(size_t) i], &g[(size_t) i], &d[(size_t) i], std::min (bloc, n - i));

    // Fondu a puissance constante, comme le plugin.
    const float aSec = std::cos (mix * 1.57079632679f);
    const float aMou = std::sin (mix * 1.57079632679f);
    const int lat = m.latenceEchantillons();
    for (int i = 0; i < n; ++i)
    {
        const float sec = (i - lat >= 0) ? x[(size_t) (i - lat)] : 0.0f;
        g[(size_t) i] = aSec * sec + aMou * g[(size_t) i];
        d[(size_t) i] = aSec * sec + aMou * d[(size_t) i];
    }

    ecrireWav (argv[2], g, d, fs);
    std::printf ("  %-22s programme %-10s mix %.2f color %.2f  boucle %s\n",
                 argv[2], p.nom, mix, color, LUXREV_QUANT_BOUCLE ? "OUI" : "non");
    return 0;
}
