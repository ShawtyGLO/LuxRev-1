#include "PluginEditor.h"

using namespace luxrev;

namespace
{
    constexpr int   kLargeur = 700, kHauteur = 470;
    constexpr float kPlancherDb = -60.0f;

    /** Fenetres d'affichage, en secondes. La courbe doit tenir ENTIERE : a
        Decay 9 s, une fenetre fixe de 3 s coupait la queue. */
    constexpr float kEchelles[] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f };

    // Points de trace de la courbe. Au niveau du fichier, pas dans la fonction :
    // MSVC refuse qu'une lambda sans capture par defaut lise un constexpr local.
    constexpr int kPointsCourbe = 220;

    float fenetreS (float decay, float predelaiMs)
    {
        const float besoin = predelaiMs * 0.001f + decay * 1.25f;
        for (float e : kEchelles) if (e >= besoin) return e;
        return 12.0f;
    }
}

//==============================================================================
void ApparenceBouton::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float debutAngle, float finAngle,
                                        juce::Slider& s)
{
    const auto zone = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float d = juce::jmin (zone.getWidth(), zone.getHeight());
    const auto cercle = zone.withSizeKeepingCentre (d, d).reduced (d * 0.16f);
    const auto centre = cercle.getCentre();
    const float rayon = cercle.getWidth() * 0.5f;
    const float angle = debutAngle + pos * (finAngle - debutAngle);

    // --- la piste, et son cran de repos
    const float rp = rayon + d * 0.11f;
    juce::Path piste;
    piste.addCentredArc (centre.x, centre.y, rp, rp, 0.0f, debutAngle, finAngle, true);
    g.setColour (juce::Colour (0xff000000).withAlpha (0.6f));
    g.strokePath (piste, juce::PathStrokeType (d * 0.045f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    float tc = 0.0f;
    const auto it = centres.find (&s);
    const bool aUnCentre = (it != centres.end() && it->second >= 0.0f);
    if (aUnCentre) tc = it->second;

    // ⚠️ L'arc part du CENTRE quand la commande en a un — Color, Stereo, Tilt.
    // C'est la traduction visuelle de « au centre, c'est le LXP-1 mesure » : la
    // position neutre se lit comme un point de repos, pas comme un reglage parmi
    // d'autres.
    const float a0 = debutAngle + juce::jmin (pos, tc) * (finAngle - debutAngle);
    const float a1 = debutAngle + juce::jmax (pos, tc) * (finAngle - debutAngle);
    if (std::abs (a1 - a0) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, rp, rp, 0.0f, a0, a1, true);
        g.setColour (pal.bande2);
        g.strokePath (arc, juce::PathStrokeType (d * 0.062f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
    if (aUnCentre)
    {
        // Sans ce cran, un arc de longueur nulle au centre se lit comme une
        // commande morte plutot qu'au repos.
        const float ac = debutAngle + tc * (finAngle - debutAngle) - juce::MathConstants<float>::halfPi;
        const juce::Point<float> p0 (centre.x + std::cos (ac) * (rp - d * 0.045f),
                                     centre.y + std::sin (ac) * (rp - d * 0.045f));
        const juce::Point<float> p1 (centre.x + std::cos (ac) * (rp + d * 0.05f),
                                     centre.y + std::sin (ac) * (rp + d * 0.05f));
        g.setColour (pal.serigraphie2.withAlpha (0.75f));
        g.drawLine ({ p0, p1 }, d * 0.035f);
    }

    // --- le cylindre
    g.setGradientFill (juce::ColourGradient (pal.boutonA, centre.x, cercle.getY(),
                                             pal.boutonB, centre.x, cercle.getBottom(), false));
    g.fillEllipse (cercle);
    g.setColour (juce::Colour (0xff000000).withAlpha (0.5f));
    g.drawEllipse (cercle, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawEllipse (cercle.reduced (0.8f).withTrimmedBottom (cercle.getHeight() * 0.5f), 1.0f);

    // --- le repere, unique ornement : la signature Lexicon
    const float ar = angle - juce::MathConstants<float>::halfPi;
    const juce::Point<float> r0 (centre.x + std::cos (ar) * rayon * 0.30f,
                                 centre.y + std::sin (ar) * rayon * 0.30f);
    const juce::Point<float> r1 (centre.x + std::cos (ar) * rayon * 0.90f,
                                 centre.y + std::sin (ar) * rayon * 0.90f);
    g.setColour (pal.repere.withAlpha (0.92f));
    g.drawLine ({ r0, r1 }, juce::jmax (1.6f, d * 0.045f));
}

//==============================================================================
Ecran::Ecran (LuxRevProcessor& p, const Palette& pa) : proc (p), pal (pa) {}

void Ecran::poserEtat (int programme, float d, float pre, float c,
                       float s, float t, float m)
{
    iProg = juce::jlimit (0, kNombreProgrammes - 1, programme);
    decay = d; predelaiMs = pre; color = c; stereo = s; tilt = t; mix = m;
    repaint();
}

void Ecran::paint (juce::Graphics& g)
{
    auto zone = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (pal.verre2, zone.getX(), zone.getY(),
                                             pal.verre, zone.getRight(), zone.getBottom(), false));
    g.fillRoundedRectangle (zone, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (zone.reduced (0.5f), 4.0f, 1.0f);

    auto inner = zone.reduced (11.0f, 10.0f);

    // colonne de gauche : le bandeau de headroom, titre colle au bandeau
    auto colonne = inner.removeFromLeft (34.0f);
    dessinerHeadroom (g, colonne);
    inner.removeFromLeft (10.0f);

    // en-tete : rang et nom du programme
    auto haut = inner.removeFromTop (22.0f);
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
    g.drawText ("PROGRAM " + juce::String (iProg + 1).paddedLeft ('0', 2),
                haut, juce::Justification::centredLeft);
    g.setColour (pal.lueur);
    g.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
    g.drawText (juce::String (kProgrammes[iProg].nom).toUpperCase(),
                haut, juce::Justification::centredRight);

    auto bas = inner.removeFromBottom (54.0f);
    dessinerCourbe (g, inner.reduced (0.0f, 4.0f));

    g.setColour (pal.lueur.withAlpha (0.18f));
    g.drawLine (bas.getX(), bas.getY(), bas.getRight(), bas.getY(), 1.0f);
    bas.removeFromTop (5.0f);

    auto zCercles = bas.removeFromLeft (86.0f);
    dessinerCercles (g, zCercles);
    bas.removeFromLeft (12.0f);
    dessinerSpectre (g, bas);
}

void Ecran::dessinerCourbe (juce::Graphics& g, juce::Rectangle<float> z)
{
    const Programme& p = kProgrammes[iProg];
    const float T = fenetreS (decay, predelaiMs);
    const float pasMs = kDureeEnveloppeS * 1000.0f / (kTailleEnveloppe - 1);

    // instant du pic mesure : la dilatation du temps ne s'applique qu'apres lui,
    // sinon on etirerait aussi l'attaque
    int iPic = 0;
    for (int i = 1; i < kTailleEnveloppe; ++i)
        if (p.enveloppeDb[i] > p.enveloppeDb[iPic]) iPic = i;
    const float tPic = iPic * pasMs;

    const float r = juce::jmax (0.05f, decay / juce::jmax (p.t60MesureS, 0.01f));
    const float gainMix = juce::Decibels::gainToDecibels (juce::jmax (mix * 0.01f, 1.0e-3f));

    // lecture du releve a une date quelconque, pente prolongee au-dela des 3 s
    auto releve = [&p, pasMs] (float ms)
    {
        if (ms <= 0.0f) return p.enveloppeDb[0];
        const float x = ms / pasMs;
        const int n = kTailleEnveloppe;
        if (x <= n - 1)
        {
            const int k = (int) x;
            return p.enveloppeDb[k] + (p.enveloppeDb[juce::jmin (k + 1, n - 1)] - p.enveloppeDb[k]) * (x - k);
        }
        const float pente = (p.enveloppeDb[n - 1] - p.enveloppeDb[n - 11]) / (10.0f * pasMs);
        return p.enveloppeDb[n - 1] + pente * (ms - kDureeEnveloppeS * 1000.0f);
    };

    std::array<float, kPointsCourbe> val {};
    for (int i = 0; i < kPointsCourbe; ++i)
    {
        const float t = (float) i / (kPointsCourbe - 1) * T * 1000.0f;
        const float ts = t - predelaiMs;
        if (ts < 0.0f) { val[(size_t) i] = kPlancherDb; continue; }
        const float src = ts <= tPic ? ts : tPic + (ts - tPic) / r;
        val[(size_t) i] = juce::jmax (kPlancherDb, releve (src) + gainMix);
    }

    auto yDe = [z] (float db)
    {
        const float u = juce::jlimit (0.0f, 1.0f, (db - kPlancherDb) / (0.0f - kPlancherDb));
        return z.getBottom() - u * z.getHeight();
    };
    auto xDe = [z] (int i) { return z.getX() + (float) i / (kPointsCourbe - 1) * z.getWidth(); };

    // --- graduations en secondes : elles suivent l'echelle
    const float pasSec = T <= 1.5f ? 0.25f : (T <= 4.0f ? 0.5f : (T <= 8.0f ? 1.0f : 2.0f));
    g.setFont (juce::FontOptions (8.0f));
    for (float s = pasSec; s < T - 1.0e-4f; s += pasSec)
    {
        const float x = z.getX() + s / T * z.getWidth();
        g.setColour (pal.lueur.withAlpha (0.12f));
        g.drawLine (x, z.getY(), x, z.getBottom(), 1.0f);
        g.setColour (pal.lueurFaible);
        g.drawText (juce::String (s, s < 1.0f ? 2 : 0).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."),
                    juce::Rectangle<float> (x + 2.0f, z.getBottom() - 11.0f, 26.0f, 10.0f),
                    juce::Justification::centredLeft);
    }

    // --- le trace, et son remplissage : ILS BOUGENT ENSEMBLE.
    // ⚠️ Un remplissage immobile sous une courbe mobile ne se lit pas comme
    // « voici la reference », ca se lit comme un bug d'affichage.
    juce::Path trace, aire;
    trace.startNewSubPath (xDe (0), yDe (val[0]));
    for (int i = 1; i < kPointsCourbe; ++i) trace.lineTo (xDe (i), yDe (val[(size_t) i]));
    aire = trace;
    aire.lineTo (z.getRight(), z.getBottom());
    aire.lineTo (z.getX(), z.getBottom());
    aire.closeSubPath();

    // COLOR se lit dans le remplissage : plein et dense cote cremeux, plus aere
    // au centre, strie de raies verticales cote metallique.
    const float grain = (color - 50.0f) / 50.0f;
    g.setColour (pal.lueur.withAlpha (juce::jlimit (0.04f, 0.45f, 0.30f - 0.13f * grain)));
    g.fillPath (aire);

    if (grain > 0.02f)
    {
        const int n = (int) std::round (14.0f + grain * 30.0f);
        g.saveState();
        g.reduceClipRegion (aire, {});
        for (int i = 1; i < n; ++i)
        {
            const float u = (i + 0.35f * std::sin (i * 2.4f)) / (float) n;
            const float x = z.getX() + u * z.getWidth();
            const float a = (0.25f + 0.55f * std::abs (std::sin (i * 1.7f))) * grain;
            g.setColour (pal.lueur.withAlpha (juce::jlimit (0.0f, 0.9f, a)));
            g.drawLine (x, z.getY(), x, z.getBottom(), 1.1f);
        }
        g.restoreState();
    }

    g.setColour (pal.lueur);
    g.strokePath (trace, juce::PathStrokeType (1.4f));

    // le temps de melange mesure : l'instant ou la queue devient un bruit —
    // c'est lui qui separe une salle d'une plaque a l'oreille
    const float xm = z.getX() + (p.melangeMs * 0.001f) / T * z.getWidth();
    if (xm < z.getRight() - 4.0f)
    {
        g.setColour (pal.lueur.withAlpha (0.35f));
        const float pointille[] = { 2.0f, 3.0f };
        g.drawDashedLine ({ xm, z.getY(), xm, z.getBottom() }, pointille, 2, 1.0f);
        g.setColour (pal.lueurFaible);
        g.drawText ("MELANGE " + juce::String ((int) p.melangeMs) + " MS",
                    juce::Rectangle<float> (xm + 3.0f, z.getY(), 90.0f, 10.0f),
                    juce::Justification::centredLeft);
    }
    g.setColour (pal.lueurFaible);
    g.drawText (juce::String (T, T < 1.0f ? 2 : 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " S",
                juce::Rectangle<float> (z.getRight() - 30.0f, z.getY(), 30.0f, 10.0f),
                juce::Justification::centredRight);
}

void Ecran::dessinerSpectre (juce::Graphics& g, juce::Rectangle<float> z)
{
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("REPONSE - BOUGE AVEC TILT, PAS AVEC COLOR",
                z.removeFromTop (10.0f), juce::Justification::centredLeft);

    const Programme& p = kProgrammes[iProg];
    const float zero = z.getCentreY();
    g.setColour (pal.lueur.withAlpha (0.28f));
    g.drawLine (z.getX(), zero, z.getRight(), zero, 1.0f);

    // ⚠️ C'est ici que TILT s'affiche, et c'est ici que COLOR PROUVE qu'il ne
    // touche pas au timbre : les deux courbes de compensation existent
    // exactement pour que ces barres ne bougent pas quand on tourne COLOR.
    const float larg = z.getWidth() / kTailleSpectre;
    for (int i = 0; i < kTailleSpectre; ++i)
    {
        const float f = 250.0f * std::pow (16000.0f / 250.0f, (float) i / (kTailleSpectre - 1));
        float v = p.spectreDb[i] + (tilt * 0.5f)
                    * juce::jlimit (-1.0f, 1.0f, std::log2 (f / 700.0f) / 3.0f);
        const float t = juce::jlimit (-1.0f, 1.0f, v / 12.0f);
        const float h = juce::jmax (1.0f, std::abs (t) * z.getHeight() * 0.5f);
        g.setColour (pal.lueur.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (z.getX() + i * larg + 1.5f,
                                            t >= 0.0f ? zero - h : zero,
                                            juce::jmax (larg - 3.0f, 1.0f), h));
    }
}

void Ecran::dessinerCercles (juce::Graphics& g, juce::Rectangle<float> z)
{
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("STEREO", z.removeFromTop (10.0f), juce::Justification::centredLeft);

    // ⚠️ L'ecartement EST la decorrelation : un seul disque en mono
    // (correlation +1), deux disques imbriques a la reference (correlation 0),
    // detaches au-dela. La figure dit ce que mesure la correlation, sans chiffre.
    const float R = juce::jmin (9.5f, z.getHeight() * 0.42f);
    const float e = R * 0.65f * (stereo * 0.01f);
    const auto c = z.getCentre();

    for (int s : { -1, 1 })
    {
        const juce::Rectangle<float> d (c.x + s * e - R, c.y - R, R * 2.0f, R * 2.0f);
        g.setColour (pal.lueur.withAlpha (0.16f));
        g.fillEllipse (d);
        g.setColour (pal.lueur);
        g.drawEllipse (d, 1.2f);
    }
}

void Ecran::dessinerHeadroom (juce::Graphics& g, juce::Rectangle<float> z)
{
    // Quatre temoins comme sur les unites Lexicon. La quatrieme est le plafond
    // d'entree du manuel (+15 dBV) : au-dela, l'etage d'entree ecrete.
    static constexpr float seuils[4] = { -24.0f, -12.0f, -4.0f, 0.0f };
    constexpr float hb = 11.0f, gap = 3.0f;
    const float total = 4 * hb + 3 * gap;

    auto bandeau = z.removeFromRight (15.0f).withHeight (total).withY (z.getBottom() - total);
    for (int i = 0; i < 4; ++i)
    {
        const auto r = juce::Rectangle<float> (bandeau.getX(),
                                               bandeau.getBottom() - (i + 1) * hb - i * gap,
                                               bandeau.getWidth(), hb);
        const bool on = niveauDb >= seuils[i];
        if (on && i == 3) g.setColour (juce::Colour (0xffe5623c));
        else if (on)      g.setColour (pal.lueur.withAlpha (0.78f));
        else              g.setColour (pal.lueur.withAlpha (0.11f));
        g.fillRect (r);
    }

    // titre colle au bandeau, pas flottant le long de la colonne
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (7.5f));
    juce::Graphics::ScopedSaveState s (g);
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                     z.getRight() - 4.0f, bandeau.getBottom()));
    g.drawText ("HEADROOM",
                juce::Rectangle<float> (z.getRight() - 4.0f, bandeau.getBottom() - 5.0f, total, 10.0f),
                juce::Justification::centredLeft);
}

//==============================================================================
LuxRevEditor::LuxRevEditor (LuxRevProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ecran (p, pal)
{
    setLookAndFeel (&apparence);
    addAndMakeVisible (ecran);

    // La grille de douze : douze programmes se choisissent d'un clic, pas en
    // tournant un bouton. Un selecteur rotatif fonctionne sur une machine, ou
    // tourner est le seul geste disponible ; dans un plugin, c'est douze crans a
    // traverser pour atteindre le douzieme.
    auto* choix = dynamic_cast<juce::AudioParameterChoice*> (proc.etat.getParameter ("programme"));
    for (int i = 0; i < kNombreProgrammes; ++i)
    {
        auto* b = cellules.add (new juce::TextButton (juce::String (kProgrammes[i].nom).toUpperCase()));
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonColourId, pal.lueur.withAlpha (0.14f));
        b->setColour (juce::TextButton::buttonOnColourId, pal.lueur.withAlpha (0.22f));
        b->setColour (juce::TextButton::textColourOffId, pal.lueurFaible);
        b->setColour (juce::TextButton::textColourOnId, pal.lueur);
        b->onClick = [this, i, choix]
        {
            if (choix == nullptr) return;
            choix->beginChangeGesture();
            *choix = i;
            choix->endChangeGesture();
            // Le programme impose ses reglages d'usine : Decay et Pre-Delay de
            // la Program table du manuel.
            proc.etat.getParameter ("decay")->setValueNotifyingHost (
                proc.etat.getParameter ("decay")->convertTo0to1 (
                    juce::jlimit (kProgrammes[i].t60Min, kProgrammes[i].t60Max, kProgrammes[i].t60S)));
            proc.etat.getParameter ("predelai")->setValueNotifyingHost (
                proc.etat.getParameter ("predelai")->convertTo0to1 (kProgrammes[i].predelaiUsineMs));
        };
        addAndMakeVisible (b);
    }

    // ⚠️ Les trois commandes qui vont au-dela de la machine ont un CENTRE : leur
    // arc part de la position neutre, qui redonne exactement le LXP-1 mesure.
    construireBouton (sDecay,    lDecay,    "DECAY",     "decay",    -1.0f);
    construireBouton (sPredelai, lPredelai, "PRE-DELAY", "predelai", -1.0f);
    construireBouton (sColor,    lColor,    "COLOR",     "color",     0.5f);
    construireBouton (sStereo,   lStereo,   "STEREO",    "stereo",    100.0f / 160.0f);
    construireBouton (sTilt,     lTilt,     "TILT",      "tilt",      0.5f);
    construireBouton (sMix,      lMix,      "MIX",       "mix",      -1.0f);

    lecture.setColour (juce::Label::textColourId, pal.lueur);
    lecture.setJustificationType (juce::Justification::centredLeft);
    lecture.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (lecture);

    setSize (kLargeur, kHauteur);
    startTimerHz (30);
}

LuxRevEditor::~LuxRevEditor()
{
    setLookAndFeel (nullptr);
}

void LuxRevEditor::construireBouton (juce::Slider& s, juce::Label& l,
                                     const juce::String& nom, const juce::String& id,
                                     float centre)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 0.78f,
                           juce::MathConstants<float>::pi * (2.0f - 0.78f), true);
    // ⚠️ Le double-clic ramene au reglage D'USINE DU PROGRAMME, pas a un defaut
    // unique pour tout le plugin : « Decay par defaut » ne veut rien dire dans
    // l'absolu — 0,6 s sur Gate, 2,6 s sur Hall B. Un defaut global ferait sortir
    // du programme des qu'on annule un geste. C'est l'attache qui pose le sien,
    // et on le corrige a chaque changement de programme dans timerCallback().
    s.setDoubleClickReturnValue (true, s.getValue());
    apparence.poserCentre (s, centre);
    addAndMakeVisible (s);

    l.setText (nom, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, pal.serigraphie);
    l.setFont (juce::FontOptions (9.0f).withStyle ("bold"));
    addAndMakeVisible (l);

    auto& a = (id == "decay")    ? aDecay
            : (id == "predelai") ? aPredelai
            : (id == "color")    ? aColor
            : (id == "stereo")   ? aStereo
            : (id == "tilt")     ? aTilt : aMix;
    a = std::make_unique<Attache> (proc.etat, id, s);
}

void LuxRevEditor::timerCallback()
{
    const int ip = juce::jlimit (0, kNombreProgrammes - 1,
                                 (int) proc.etat.getRawParameterValue ("programme")->load());

    for (int i = 0; i < cellules.size(); ++i)
        cellules[i]->setToggleState (i == ip, juce::dontSendNotification);

    const float decay = proc.etat.getRawParameterValue ("decay")->load();
    const float pre   = proc.etat.getRawParameterValue ("predelai")->load();
    const float color = proc.etat.getRawParameterValue ("color")->load();
    const float ster  = proc.etat.getRawParameterValue ("stereo")->load();
    const float tilt  = proc.etat.getRawParameterValue ("tilt")->load();
    const float mix   = proc.etat.getRawParameterValue ("mix")->load();

    // le double-clic suit le programme courant
    sDecay.setDoubleClickReturnValue (true,
        juce::jlimit (kProgrammes[ip].t60Min, kProgrammes[ip].t60Max, kProgrammes[ip].t60S));
    sPredelai.setDoubleClickReturnValue (true, kProgrammes[ip].predelaiUsineMs);

    ecran.poserEtat (ip, decay, pre, color, ster, tilt, mix);
    ecran.poserNiveau (proc.niveauEntreeDb.load());

    // L'ecran affiche le parametre qu'on est en train de bouger : c'est ce qui
    // autorise des boutons nus et des etiquettes courtes.
    juce::String txt = "DECAY";
    juce::String val = juce::String (decay, 2) + " s";
    if      (sPredelai.isMouseOverOrDragging()) { txt = "PRE-DELAY"; val = juce::String ((int) pre) + " ms"; }
    else if (sColor.isMouseOverOrDragging())    { txt = "COLOR";     val = juce::String ((int) color) + " %"; }
    else if (sStereo.isMouseOverOrDragging())   { txt = "STEREO";    val = juce::String ((int) ster) + " %"; }
    else if (sTilt.isMouseOverOrDragging())     { txt = "TILT";      val = (tilt > 0 ? "+" : "") + juce::String (tilt, 1) + " dB"; }
    else if (sMix.isMouseOverOrDragging())      { txt = "MIX";       val = juce::String ((int) mix) + " %"; }
    lecture.setText (txt + "     " + val, juce::dontSendNotification);
}

//==============================================================================
void LuxRevEditor::paint (juce::Graphics& g)
{
    auto zone = getLocalBounds().toFloat();
    g.fillAll (pal.fond);

    // le panneau : un noir fume, rechauffe. Le vieillissement se lit dans la
    // couleur, jamais dans des ornements ajoutes.
    auto panneau = zone.reduced (8.0f);
    juce::ColourGradient grad (pal.panneauA, panneau.getX() + panneau.getWidth() * 0.12f, panneau.getY(),
                               pal.panneauC, panneau.getRight(), panneau.getBottom(), true);
    grad.addColour (0.52, pal.panneauB);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (panneau, 7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (panneau.reduced (0.5f), 7.0f, 1.0f);

    // la marque, et la diode
    const float x = panneau.getX() + 18.0f;
    const float y = panneau.getY() + 16.0f;
    g.setColour (pal.lueur);
    g.fillEllipse (x, y + 9.0f, 6.0f, 6.0f);
    g.setColour (pal.serigraphie);
    g.setFont (juce::FontOptions (22.0f).withStyle ("bold"));
    g.drawText ("LuxRev-1", juce::Rectangle<float> (x + 14.0f, y, 220.0f, 24.0f),
                juce::Justification::centredLeft);
    g.setColour (pal.serigraphie2);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("DIGITAL REVERBERATOR",
                juce::Rectangle<float> (panneau.getRight() - 220.0f, y, 202.0f, 24.0f),
                juce::Justification::centredRight);

    // la bande, delavee par endroits comme une serigraphie qui a pris le jour
    auto bande = juce::Rectangle<float> (panneau.getX() + 18.0f, y + 30.0f,
                                         panneau.getWidth() - 36.0f, 4.0f);
    juce::ColourGradient gb (pal.bande, bande.getX(), 0.0f, pal.bande, bande.getRight(), 0.0f, false);
    gb.addColour (0.34, pal.bande2.brighter (0.25f));
    gb.addColour (0.52, pal.bande2);
    g.setGradientFill (gb);
    g.fillRoundedRectangle (bande, 2.0f);
}

void LuxRevEditor::resized()
{
    auto zone = getLocalBounds().reduced (8).reduced (18, 0);
    zone.removeFromTop (54);

    ecran.setBounds (zone.removeFromTop (176));
    zone.removeFromTop (10);

    // la grille de douze, quatre colonnes
    auto grille = zone.removeFromTop (86);
    constexpr int cols = 4, rows = 3;
    const int lc = grille.getWidth() / cols, lh = grille.getHeight() / rows;
    for (int i = 0; i < cellules.size(); ++i)
        cellules[i]->setBounds (juce::Rectangle<int> (grille.getX() + (i % cols) * lc,
                                                      grille.getY() + (i / cols) * lh,
                                                      lc - 4, lh - 4));
    zone.removeFromTop (8);

    auto rangee = zone.removeFromTop (96);
    juce::Slider* sl[] = { &sDecay, &sPredelai, &sColor, &sStereo, &sTilt, &sMix };
    juce::Label*  la[] = { &lDecay, &lPredelai, &lColor, &lStereo, &lTilt, &lMix };
    const int pas = rangee.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto c = juce::Rectangle<int> (rangee.getX() + i * pas, rangee.getY(), pas, rangee.getHeight());
        la[i]->setBounds (c.removeFromTop (14));
        sl[i]->setBounds (c.reduced (6, 2));
    }

    lecture.setBounds (zone.removeFromTop (24).reduced (6, 0));
}
