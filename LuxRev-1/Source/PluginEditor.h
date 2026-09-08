#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/** Palette de la facade. Portage de maquettes/commun.css.

    Direction : un Lexicon qui a vieilli. Sombre, bleu ; la version claire en
    rouge reste en reserve. Le vieillissement se lit dans la COULEUR, jamais dans
    des ornements ajoutes — Lexicon n'a jamais fait de bois ni de vis dorees.
*/
struct Palette
{
    juce::Colour fond        { 0xff0e0e10 };
    juce::Colour panneauA    { 0xff2b2825 };
    juce::Colour panneauB    { 0xff1e1c1a };
    juce::Colour panneauC    { 0xff141312 };
    juce::Colour bande       { 0xff24567f };
    juce::Colour bande2      { 0xff4a8cc4 };
    juce::Colour serigraphie { 0xffd2caba };
    juce::Colour serigraphie2{ 0xff8b8375 };
    juce::Colour boutonA     { 0xff45413b };
    juce::Colour boutonB     { 0xff221f1c };
    juce::Colour repere      { 0xfff0e8d4 };
    juce::Colour verre       { 0xff08111a };
    juce::Colour verre2      { 0xff0d1a26 };
    juce::Colour lueur       { 0xff63c6e8 };
    juce::Colour lueurFaible { 0xff2d6a84 };
};

//==============================================================================
/** Les boutons ronds de Lexicon : un cylindre sombre, un seul repere clair.

    Le repere est le SEUL ornement — c'est la signature. Autour, une piste avec
    l'arc de valeur, et un cran de repos pour les commandes qui en ont un.
*/
class ApparenceBouton : public juce::LookAndFeel_V4
{
public:
    explicit ApparenceBouton (const Palette& p) : pal (p) {}

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float debutAngle, float finAngle,
                           juce::Slider&) override;

    /** Position neutre, en proportion de la course. Negative = pas de cran :
        l'arc part alors du minimum, comme un reglage ordinaire. */
    void poserCentre (juce::Slider& s, float t) { centres[&s] = t; }

private:
    const Palette& pal;
    std::map<juce::Slider*, float> centres;
};

//==============================================================================
/** Les douze cellules de la grille de programmes.

    ⚠️ Sans cette apparence, JUCE dessine ses propres boutons — coins arrondis,
    degrade, bordure claire — et la grille jure avec le reste de la facade. La
    maquette pose des rectangles plats sur du verre, avec une lueur sur celui qui
    est choisi.
*/
class ApparenceCellule : public juce::LookAndFeel_V4
{
public:
    explicit ApparenceCellule (const Palette& p) : pal (p) {}

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour&, bool survol, bool enfonce) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool survol, bool enfonce) override;

private:
    const Palette& pal;
};

//==============================================================================
/** L'ecran : il fait DEUX metiers, comme celui du Model 200.

    1. il montre la decroissance du programme — l'enveloppe REELLEMENT MESUREE,
       transformee par les commandes ;
    2. il affiche la reponse en frequences.

    ⚠️ Trois transformations sont exactes (Decay dilate le temps, Pre-Delay
    decale, Mix descend le niveau) ; la quatrieme ne l'est pas. COLOR n'a pas
    d'effet mesurable sur l'enveloppe, il agit sur la rugosite : le remplissage
    REPRESENTE ce grain, il n'est pas releve.
*/
class Ecran : public juce::Component
{
public:
    Ecran (LuxRevProcessor&, const Palette&);
    void paint (juce::Graphics&) override;

    void poserEtat (int programme, float decay, float predelaiMs,
                    float color, float stereo, float tilt, float mix);
    void poserNiveau (float db) { niveauDb = db; }
    void poserAllume (bool o) { allume = o; }

private:
    void dessinerCourbe (juce::Graphics&, juce::Rectangle<float>);
    void dessinerSpectre (juce::Graphics&, juce::Rectangle<float>);
    void dessinerCercles (juce::Graphics&, juce::Rectangle<float>);
    void dessinerHeadroom (juce::Graphics&, juce::Rectangle<float>);

    LuxRevProcessor& proc;
    const Palette& pal;

    int   iProg = 6;
    float decay = 2.6f, predelaiMs = 33.0f, color = 50.0f;
    float stereo = 100.0f, tilt = 0.0f, mix = 100.0f;
    float niveauDb = -100.0f;
    bool  allume = true;
};

//==============================================================================
class LuxRevEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit LuxRevEditor (LuxRevProcessor&);
    ~LuxRevEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void construireBouton (juce::Slider&, juce::Label&, const juce::String& nom,
                           const juce::String& id, float centre);

    LuxRevProcessor& proc;
    Palette pal;
    ApparenceBouton  apparence { pal };
    ApparenceCellule apparenceCellule { pal };

    Ecran ecran;
    juce::OwnedArray<juce::TextButton> cellules;      // la grille des 12 programmes
    juce::Slider sDecay, sPredelai, sColor, sStereo, sTilt, sMix;
    juce::Label  lDecay, lPredelai, lColor, lStereo, lTilt, lMix;
    juce::Label  lecture;                             // le parametre en cours de reglage

    juce::Rectangle<int> zoneInterrupteur;            // diode + titre : marche/arret

    using Attache = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attache> aDecay, aPredelai, aColor, aStereo, aTilt, aMix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuxRevEditor)
};
