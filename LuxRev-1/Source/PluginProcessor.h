#pragma once

#include <JuceHeader.h>
#include "Dsp/Moteur.h"

//==============================================================================
/** LuxRev-1 — reverbe algorithmique de lignee Lexicon.

    Facade v1, arretee le 2026-09-07 :
        Programme · Decay · Pre-Delay · COLOR · Stereo · Tilt · Mix

    Decay et Pre-Delay sont les deux vrais boutons de la machine, avec les plages
    de la Program table du manuel. COLOR, Stereo et Tilt vont au-dela d'elle et
    sont construits pour que leur position neutre redonne EXACTEMENT le LXP-1
    mesure — c'est verifie, pas suppose.
*/
class LuxRevProcessor : public juce::AudioProcessor
{
public:
    LuxRevProcessor();
    ~LuxRevProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState etat;

    /** Niveau d'entree lisse, pour le bandeau de headroom de la facade.
        Ecrit par le fil audio, lu par l'editeur — d'ou l'atomique. */
    std::atomic<float> niveauEntreeDb { -100.0f };

    static juce::StringArray nomsProgrammes();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout construireParametres();
    void rafraichirReglages();

    luxrev::Moteur moteur;
    juce::AudioBuffer<float> tampon;     // mono d'entree + deux sorties humides

    // Cache des valeurs de parametres : on ne repose les reglages que lorsqu'ils
    // ont reellement bouge. appliquer() ne fait aucune allocation, mais il
    // recalcule une centaine de coefficients — inutile a chaque bloc.
    int   dernierProgramme = -1;
    float dernierDecay = -1.0f, dernierPredelai = -1.0f, dernierColor = -1.0f;
    float dernierStereo = -1.0f, dernierTilt = -1000.0f;

    std::atomic<float>* pProgramme = nullptr;
    std::atomic<float>* pDecay     = nullptr;
    std::atomic<float>* pPredelai  = nullptr;
    std::atomic<float>* pColor     = nullptr;
    std::atomic<float>* pStereo    = nullptr;
    std::atomic<float>* pTilt      = nullptr;
    std::atomic<float>* pMix       = nullptr;

    juce::SmoothedValue<float> mixLisse;
    double sampleRateCourant = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuxRevProcessor)
};
