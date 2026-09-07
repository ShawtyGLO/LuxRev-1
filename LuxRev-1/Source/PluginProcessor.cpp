#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace luxrev;

//==============================================================================
juce::StringArray LuxRevProcessor::nomsProgrammes()
{
    juce::StringArray n;
    for (int i = 0; i < kNombreProgrammes; ++i)
        n.add (juce::String (kProgrammes[i].nom));
    return n;
}

juce::AudioProcessorValueTreeState::ParameterLayout LuxRevProcessor::construireParametres()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "programme", 1 }, "Programme", nomsProgrammes(), 6));   // Hall B

    // Plage Decay commune : de la plus courte a la plus longue des douze plages
    // du manuel. Chaque programme borne ensuite la sienne — un Decay de 9 s n'a
    // pas de sens sur Small 1, dont le manuel s'arrete a 2,4 s.
    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        NormalisableRange<float> (0.08f, 9.0f, 0.0f, 0.35f), 2.6f,
        AudioParameterFloatAttributes().withLabel ("s")));

    // 0 a 246 ms : le maximum de la machine, Program table du manuel.
    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "predelai", 1 }, "Pre-Delay",
        NormalisableRange<float> (0.0f, 246.0f, 1.0f, 0.6f), 33.0f,
        AudioParameterFloatAttributes().withLabel ("ms")));

    // 50 % = le LXP-1 mesure. En dessous plus cremeux, au-dessus plus metallique.
    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "color", 1 }, "Color",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    // 100 % = l'image de la machine. La borne haute est a 160 % : au-dela,
    // l'ecart entre l'ecoute casque et l'ecoute mono devient assez grand pour
    // piéger un mixage.
    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "stereo", 1 }, "Stereo",
        NormalisableRange<float> (0.0f, 160.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tilt", 1 }, "Tilt",
        NormalisableRange<float> (-9.0f, 9.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    l.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    return l;
}

//==============================================================================
LuxRevProcessor::LuxRevProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      etat (*this, nullptr, "LUXREV", construireParametres())
{
    pProgramme = etat.getRawParameterValue ("programme");
    pDecay     = etat.getRawParameterValue ("decay");
    pPredelai  = etat.getRawParameterValue ("predelai");
    pColor     = etat.getRawParameterValue ("color");
    pStereo    = etat.getRawParameterValue ("stereo");
    pTilt      = etat.getRawParameterValue ("tilt");
    pMix       = etat.getRawParameterValue ("mix");
}

bool LuxRevProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    // Une reverbe stereo depuis une entree mono est un cas legitime et courant.
    if (in != out && in != juce::AudioChannelSet::mono())
        return false;
    return true;
}

double LuxRevProcessor::getTailLengthSeconds() const
{
    // La queue la plus longue que la facade permette, plus une marge : l'hote
    // s'en sert pour savoir combien de temps continuer a nous appeler apres la
    // fin du morceau. La sous-declarer coupe la reverbe net au rendu.
    return 12.0;
}

//==============================================================================
void LuxRevProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateCourant = sampleRate;
    moteur.preparer (sampleRate);
    tampon.setSize (3, juce::jmax (samplesPerBlock, 1), false, false, true);

    mixLisse.reset (sampleRate, 0.02);
    mixLisse.setCurrentAndTargetValue (pMix->load() * 0.01f);

    dernierProgramme = -1;          // force une pose complete au premier bloc
    rafraichirReglages();
    moteur.vider();

    setLatencySamples (moteur.latenceEchantillons());
}

/** Traduit l'etat des parametres en reglages du moteur.

    ⚠️ Aucune allocation : `appliquer` a ete ecrit pour ca. On ne repose les
    reglages que lorsqu'une valeur a reellement bouge — sinon on recalculerait
    une centaine de coefficients a chaque bloc pour rien.
*/
void LuxRevProcessor::rafraichirReglages()
{
    const int    ip    = juce::jlimit (0, kNombreProgrammes - 1, (int) pProgramme->load());
    const float  decay = pDecay->load();
    const float  pre   = pPredelai->load();
    const float  color = pColor->load();
    const float  stereo= pStereo->load();
    const float  tiltDb= pTilt->load();

    if (ip == dernierProgramme
        && juce::approximatelyEqual (decay,  dernierDecay)
        && juce::approximatelyEqual (pre,    dernierPredelai)
        && juce::approximatelyEqual (color,  dernierColor)
        && juce::approximatelyEqual (stereo, dernierStereo)
        && juce::approximatelyEqual (tiltDb, dernierTilt))
        return;

    const Programme& p = kProgrammes[ip];

    Reglages r = reglagesDe (p);
    // Le Decay de la facade remplace le T60 accorde, borne a la plage que le
    // manuel donne pour CE programme.
    r.t60S       = juce::jlimit (p.t60Min, p.t60Max, decay);
    r.predelaiMs = pre;

    r = macroColor (r, color * 0.01f, p);
    r.stereo  = stereo * 0.01f;
    r.tiltDb  = tiltDb;

    moteur.appliquer (r, &p);

    dernierProgramme = ip;
    dernierDecay = decay; dernierPredelai = pre; dernierColor = color;
    dernierStereo = stereo; dernierTilt = tiltDb;
}

//==============================================================================
void LuxRevProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nEntrees = getTotalNumInputChannels();
    const int nSorties = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int c = nEntrees; c < nSorties; ++c)
        buffer.clear (c, 0, n);

    if (n == 0) return;
    if (tampon.getNumSamples() < n)
        tampon.setSize (3, n, false, false, true);

    rafraichirReglages();

    // --- entree mono : la machine n'a qu'une entree, c'est un 1/4" asymetrique
    // 50 kOhm (manuel de service). Sommer avant le tank est donc fidele, pas une
    // simplification.
    float* mono = tampon.getWritePointer (0);
    if (nEntrees >= 2)
    {
        const float* g = buffer.getReadPointer (0);
        const float* d = buffer.getReadPointer (1);
        for (int i = 0; i < n; ++i) mono[i] = 0.5f * (g[i] + d[i]);
    }
    else if (nEntrees == 1)
    {
        juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), n);
    }
    else
    {
        juce::FloatVectorOperations::clear (mono, n);
    }

    // Niveau d'entree pour le bandeau de headroom de la facade. La quatrieme
    // barre est le plafond d'entree du manuel, +15 dBV.
    {
        const float crete = juce::FloatVectorOperations::findMaximum (mono, n);
        const float creux = juce::FloatVectorOperations::findMinimum (mono, n);
        const float amp = juce::jmax (std::abs (crete), std::abs (creux));
        const float db = juce::Decibels::gainToDecibels (amp, -100.0f);
        const float ancien = niveauEntreeDb.load();
        // descente lente, montee immediate : un vu-metre se lit mieux ainsi
        niveauEntreeDb.store (db > ancien ? db : ancien + (db - ancien) * 0.25f);
    }

    float* wetL = tampon.getWritePointer (1);
    float* wetR = tampon.getWritePointer (2);
    moteur.traiter (mono, wetL, wetR, n);

    mixLisse.setTargetValue (pMix->load() * 0.01f);

    if (nSorties >= 2)
    {
        float* g = buffer.getWritePointer (0);
        float* d = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixLisse.getNextValue();
            const float sec = 1.0f - m;
            const float dg = g[i], dd = d[i];
            g[i] = dg * sec + wetL[i] * m;
            d[i] = dd * sec + wetR[i] * m;
        }
    }
    else if (nSorties == 1)
    {
        float* g = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixLisse.getNextValue();
            g[i] = g[i] * (1.0f - m) + 0.5f * (wetL[i] + wetR[i]) * m;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* LuxRevProcessor::createEditor()
{
    return new LuxRevEditor (*this);
}

void LuxRevProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto arbre = etat.copyState(); arbre.isValid())
        if (auto xml = arbre.createXml())
            copyXmlToBinary (*xml, dest);
}

void LuxRevProcessor::setStateInformation (const void* data, int taille)
{
    if (auto xml = getXmlFromBinary (data, taille))
        if (xml->hasTagName (etat.state.getType()))
            etat.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuxRevProcessor();
}
