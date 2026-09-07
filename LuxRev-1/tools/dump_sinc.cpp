// Vidange des coefficients du FIR de correction sinc, pour les comparer a ceux
// que scipy.firwin2 produit dans le prototype.
#include "../Source/Dsp/Conception.h"
#include <cstdio>
int main()
{
    const auto h = luxrev::correctionSinc (48000.0);
    for (size_t i = 0; i < h.size(); ++i) std::printf ("%.10g\n", h[i]);
    return 0;
}
