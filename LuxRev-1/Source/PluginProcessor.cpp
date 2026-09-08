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

    // ⚠️ La diode et le titre de la facade sont un interrupteur INVISIBLE, mais
    // ce qu'ils commandent doit rester un vrai parametre : sinon l'hote ne peut
    // ni l'automatiser ni le rappeler avec le projet.
    l.add (std::make_unique<AudioParameterBool> (
        ParameterID { "actif", 1 }, "Actif", true));

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
    pActif     = etat.getRawParameterValue ("actif");
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

    // A l'arret, on descend le humide a zero au lieu de couper net : un
    // interrupteur qui claque est un interrupteur rate, et l'hote peut
    // l'automatiser.
    const bool actif = pActif->load() > 0.5f;
    mixLisse.setTargetValue (actif ? pMix->load() * 0.01f : 0.0f);

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
/** Presets d'usine.

    ⚠️ Ce ne sont PAS les douze programmes — ceux-la sont un parametre, et les
    doubler en presets n'apprendrait rien. Un preset est un POINT DE DEPART :
    un programme, une durée, et un dosage. D'ou les valeurs de Mix bien en
    dessous de 100 % sur les presets d'insertion, la ou le plugin seul demarre
    en tout-humide, ce qui convient a un depart d'auxiliaire.

    Les valeurs de Decay restent dans la plage que le manuel donne pour chaque
    programme ; le processeur les borne de toute facon.
*/
const std::vector<LuxRevProcessor::Preset>& LuxRevProcessor::presets()
{
    // programme, decay, pre-delai, color, stereo, tilt, mix
    static const std::vector<Preset> p = {
        { "LXP-1 Hall B",        6, 2.60f,  33.0f,  50.0f, 100.0f,  0.0f, 100.0f },
        { "Plaque courte",       8, 1.20f,   0.0f,  62.0f, 105.0f, +2.0f, 100.0f },
        { "Grande salle",        5, 3.50f,  40.0f,  50.0f, 115.0f,  0.0f, 100.0f },
        { "Porte 80s",          10, 0.60f,   0.0f,  58.0f, 125.0f, +1.0f,  45.0f },
        { "Ambiance discrete",   1, 0.90f,  16.0f,  45.0f,  95.0f, -1.0f,  18.0f },
        { "Metallique",          9, 1.80f,  20.0f,  85.0f, 120.0f, +3.0f, 100.0f },
        { "Cremeux large",       7, 3.00f,  33.0f,  15.0f, 130.0f, -2.0f, 100.0f },
        { "Voix, court",         2, 1.10f,  25.0f,  50.0f, 100.0f, +1.5f,  25.0f },
        { "Batterie, plaque",    8, 1.60f,  12.0f,  55.0f, 110.0f,  0.0f,  30.0f },
        { "Inverse",            11, 0.90f,   0.0f,  50.0f, 100.0f,  0.0f,  60.0f },
    };
    return p;
}

int LuxRevProcessor::getNumPrograms()
{
    return (int) presets().size();
}

const juce::String LuxRevProcessor::getProgramName (int i)
{
    if (juce::isPositiveAndBelow (i, (int) presets().size()))
        return juce::String (presets()[(size_t) i].nom);
    return {};
}

void LuxRevProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, (int) presets().size())) return;
    presetCourant = i;
    const auto& p = presets()[(size_t) i];

    auto poser = [this] (const char* id, float valeur)
    {
        if (auto* param = etat.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (valeur));
    };
    poser ("programme", (float) p.programme);
    poser ("decay",     p.decay);
    poser ("predelai",  p.predelai);
    poser ("color",     p.color);
    poser ("stereo",    p.stereo);
    poser ("tilt",      p.tilt);
    poser ("mix",       p.mix);
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
