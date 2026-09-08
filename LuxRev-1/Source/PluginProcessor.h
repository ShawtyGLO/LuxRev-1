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

    // ⚠️ Deux notions de « programme » se croisent ici, et il faut les tenir
    // separees. Le PROGRAMME LXP-1 (Hall B, Gate…) est un parametre du plugin,
    // choisi dans la grille de la facade. Ce que JUCE appelle « program » est
    // autre chose : le preset que l'hote affiche dans son propre menu. Un preset
    // pose un programme ET les six commandes.
    int getNumPrograms() override;
    int getCurrentProgram() override { return presetCourant; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState etat;

    /** Niveau d'entree lisse, pour le bandeau de headroom de la facade.
        Ecrit par le fil audio, lu par l'editeur — d'ou l'atomique. */
    std::atomic<float> niveauEntreeDb { -100.0f };

    /** Niveau vu par le declencheur de porte, et ouverture de celle-ci. */
    std::atomic<float> niveauPorteDb { -100.0f };
    std::atomic<float> gainPorte { 0.0f };

    static juce::StringArray nomsProgrammes();

    /** Un preset d'usine : un programme LXP-1 et les six commandes. */
    struct Preset
    {
        const char* nom;
        int   programme;
        float decay, predelai, color, stereo, tilt, mix;
    };
    static const std::vector<Preset>& presets();

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
    float dernierDrive = -1.0f, dernierSeuil = -1000.0f;

    std::atomic<float>* pActif     = nullptr;
    std::atomic<float>* pProgramme = nullptr;
    std::atomic<float>* pDecay     = nullptr;
    std::atomic<float>* pPredelai  = nullptr;
    std::atomic<float>* pColor     = nullptr;
    std::atomic<float>* pStereo    = nullptr;
    std::atomic<float>* pTilt      = nullptr;
    std::atomic<float>* pMix       = nullptr;
    std::atomic<float>* pDrive     = nullptr;
    std::atomic<float>* pSeuil     = nullptr;

    int presetCourant = 0;

    juce::SmoothedValue<float> mixLisse;
    double sampleRateCourant = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuxRevProcessor)
};
