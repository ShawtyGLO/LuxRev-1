#include "PluginEditor.h"

using namespace luxrev;

namespace
{
    // ⚠️ Le format vient de la maquette 03-remote : PLUS HAUT que large. C'est
    // tout l'argument du format « remote » — le MRC de Lexicon ne ressemble pas a
    // une face avant de rack. Une premiere version a 700 x 470 ecrasait la facade
    // et coupait la ligne de lecture du bas.
    constexpr int   kLargeur = 700, kHauteur = 612;
    constexpr float kPlancherDb = -60.0f;

    /** Fenetres d'affichage, en secondes. La courbe doit tenir ENTIERE : a
        Decay 9 s, une fenetre fixe de 3 s coupait la queue. */
    constexpr float kEchelles[] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f };

    // Points de trace de la courbe. Au niveau du fichier, pas dans la fonction :
    // MSVC refuse qu'une lambda sans capture par defaut lise un constexpr local.
    constexpr int kPointsCourbe = 220;

    /** Course des boutons.

        ⚠️ Convention JUCE, et c'est la qu'une premiere version s'est trompee :
        les deux angles se comptent DANS LE SENS DES AIGUILLES depuis midi, et
        l'angle de fin doit etre le PLUS GRAND. Un depart a 0,78 pi et une fin a
        1,22 pi ne donnent donc pas une course de +/-140 degres autour de midi :
        ils donnent 80 degres autour du BAS, ce qui met le repere a l'envers et
        rend la commande presque immobile. Une course classique part en bas a
        gauche (220 degres) et arrive en bas a droite (500 = 140 + un tour).
    */
    constexpr float kAngleDebut = juce::MathConstants<float>::pi * 1.22f;
    constexpr float kAngleFin   = juce::MathConstants<float>::pi * 2.78f;

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
    const auto cercle = zone.withSizeKeepingCentre (d, d).reduced (d * 0.17f);
    const auto centre = cercle.getCentre();
    const float rayon = cercle.getWidth() * 0.5f;
    const float angle = debutAngle + pos * (finAngle - debutAngle);

    // --- la piste, et son cran de repos
    const float rp = rayon + d * 0.10f;
    juce::Path piste;
    piste.addCentredArc (centre.x, centre.y, rp, rp, 0.0f, debutAngle, finAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (piste, juce::PathStrokeType (d * 0.05f, juce::PathStrokeType::curved,
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
    if (std::abs (a1 - a0) > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, rp, rp, 0.0f, a0, a1, true);
        g.setColour (pal.bande2);
        g.strokePath (arc, juce::PathStrokeType (d * 0.07f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }
    if (aUnCentre)
    {
        // Sans ce cran, un arc de longueur nulle au centre se lit comme une
        // commande morte plutot qu'au repos.
        const float ac = debutAngle + tc * (finAngle - debutAngle);
        const juce::Point<float> p0 (centre.x + std::sin (ac) * (rp - d * 0.05f),
                                     centre.y - std::cos (ac) * (rp - d * 0.05f));
        const juce::Point<float> p1 (centre.x + std::sin (ac) * (rp + d * 0.055f),
                                     centre.y - std::cos (ac) * (rp + d * 0.055f));
        g.setColour (pal.serigraphie2.withAlpha (0.8f));
        g.drawLine ({ p0, p1 }, d * 0.035f);
    }

    // --- le cylindre
    g.setGradientFill (juce::ColourGradient (pal.boutonA, centre.x, cercle.getY(),
                                             pal.boutonB, centre.x, cercle.getBottom(), false));
    g.fillEllipse (cercle);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cercle, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (cercle.reduced (1.0f), 1.0f);

    // --- le repere, unique ornement : la signature Lexicon
    // Un point a l'angle t est en (sin t, -cos t) : midi vers le haut, sens des
    // aiguilles quand t croit.
    const juce::Point<float> r0 (centre.x + std::sin (angle) * rayon * 0.40f,
                                 centre.y - std::cos (angle) * rayon * 0.40f);
    const juce::Point<float> r1 (centre.x + std::sin (angle) * rayon * 0.92f,
                                 centre.y - std::cos (angle) * rayon * 0.92f);
    g.setColour (pal.repere.withAlpha (0.95f));
    g.drawLine ({ r0, r1 }, juce::jmax (2.0f, d * 0.055f));
}

//==============================================================================
void ApparenceCellule::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                             const juce::Colour&, bool survol, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool choisi = b.getToggleState();

    g.setColour (pal.lueur.withAlpha (choisi ? 0.22f : (survol ? 0.16f : 0.10f)));
    g.fillRoundedRectangle (r, 2.0f);
    if (choisi)
    {
        g.setColour (pal.lueur.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }
}

void ApparenceCellule::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                       bool survol, bool)
{
    const bool choisi = b.getToggleState();
    g.setColour (choisi ? pal.lueur : (survol ? pal.lueur.withAlpha (0.85f) : pal.lueurFaible));
    g.setFont (juce::FontOptions (10.5f).withStyle (choisi ? "bold" : "plain"));
    g.drawText (b.getButtonText(), b.getLocalBounds().reduced (8, 0),
                juce::Justification::centredLeft, false);
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

    // A l'arret, seuls la diode et l'ecran se grisent : le panneau reste, comme
    // une machine eteinte dont la face avant est toujours la.
    if (! allume)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (zone, 4.0f);
        g.setColour (pal.serigraphie2.withAlpha (0.5f));
        g.setFont (juce::FontOptions (13.0f).withStyle ("bold"));
        g.drawText ("ARRET", zone, juce::Justification::centred);
        return;
    }

    auto inner = zone.reduced (12.0f, 11.0f);

    // ⚠️ Le bandeau de headroom passe a gauche sur toute la hauteur, avec son
    // titre a l'horizontale. Range en colonne etroite avec un titre vertical, il
    // etait illisible — et c'est le seul afficheur qui doit se lire d'un coup
    // d'oeil pendant qu'on joue.
    auto colonne = inner.removeFromLeft (62.0f);
    dessinerHeadroom (g, colonne);
    inner.removeFromLeft (12.0f);

    auto haut = inner.removeFromTop (22.0f);
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
    g.drawText ("PROGRAM " + juce::String (iProg + 1).paddedLeft ('0', 2),
                haut, juce::Justification::centredLeft);
    g.setColour (pal.lueur);
    g.setFont (juce::FontOptions (19.0f).withStyle ("bold"));
    g.drawText (juce::String (kProgrammes[iProg].nom).toUpperCase(),
                haut, juce::Justification::centredRight);

    // ⚠️ La reponse en frequences est reduite a une bande etroite. Elle occupait
    // presque toute la largeur, ce qui lui donnait un poids qu'elle n'a pas : la
    // courbe de decroissance est l'information principale de cet ecran.
    auto bas = inner.removeFromBottom (44.0f);
    dessinerCourbe (g, inner.reduced (0.0f, 5.0f));

    g.setColour (pal.lueur.withAlpha (0.18f));
    g.drawLine (bas.getX(), bas.getY(), bas.getRight(), bas.getY(), 1.0f);
    bas.removeFromTop (5.0f);

    // Les deux afficheurs se partagent la largeur a parts egales, au lieu d'etre
    // tasses a gauche avec du vide a droite.
    const float moitie = bas.getWidth() * 0.5f;
    dessinerCercles (g, bas.removeFromLeft (moitie).reduced (2.0f, 0.0f));
    dessinerSpectre (g, bas.reduced (2.0f, 0.0f));
}

void Ecran::dessinerCourbe (juce::Graphics& g, juce::Rectangle<float> z)
{
    const Programme& p = kProgrammes[iProg];
    const float T = fenetreS (decay, predelaiMs);
    const float pasMs = kDureeEnveloppeS * 1000.0f / (kTailleEnveloppe - 1);

    int iPic = 0;
    for (int i = 1; i < kTailleEnveloppe; ++i)
        if (p.enveloppeDb[i] > p.enveloppeDb[iPic]) iPic = i;
    const float tPic = iPic * pasMs;

    const float r = juce::jmax (0.05f, decay / juce::jmax (p.t60MesureS, 0.01f));
    const float gainMix = juce::Decibels::gainToDecibels (juce::jmax (mix * 0.01f, 1.0e-3f));

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

    const float pasSec = T <= 1.5f ? 0.25f : (T <= 4.0f ? 0.5f : (T <= 8.0f ? 1.0f : 2.0f));
    g.setFont (juce::FontOptions (8.5f));
    for (float s = pasSec; s < T - 1.0e-4f; s += pasSec)
    {
        const float x = z.getX() + s / T * z.getWidth();
        g.setColour (pal.lueur.withAlpha (0.12f));
        g.drawLine (x, z.getY(), x, z.getBottom(), 1.0f);
        g.setColour (pal.lueurFaible);
        g.drawText (juce::String (s, s < 1.0f ? 2 : 0).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."),
                    juce::Rectangle<float> (x + 2.0f, z.getBottom() - 12.0f, 26.0f, 11.0f),
                    juce::Justification::centredLeft);
    }

    // ⚠️ Le fond suit le trait. Un remplissage immobile sous une courbe mobile ne
    // se lit pas comme « voici la reference », ca se lit comme un bug d'affichage.
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
    g.strokePath (trace, juce::PathStrokeType (1.5f));

    const float xm = z.getX() + (p.melangeMs * 0.001f) / T * z.getWidth();
    if (xm < z.getRight() - 4.0f)
    {
        g.setColour (pal.lueur.withAlpha (0.35f));
        const float pointille[] = { 2.0f, 3.0f };
        g.drawDashedLine ({ xm, z.getY(), xm, z.getBottom() }, pointille, 2, 1.0f);
        g.setColour (pal.lueurFaible);
        g.drawText ("MELANGE " + juce::String ((int) p.melangeMs) + " MS",
                    juce::Rectangle<float> (xm + 3.0f, z.getY(), 96.0f, 11.0f),
                    juce::Justification::centredLeft);
    }
    g.setColour (pal.lueurFaible);
    g.drawText (juce::String (T, T < 1.0f ? 2 : 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + " S",
                juce::Rectangle<float> (z.getRight() - 32.0f, z.getY(), 32.0f, 11.0f),
                juce::Justification::centredRight);
}

void Ecran::dessinerSpectre (juce::Graphics& g, juce::Rectangle<float> z)
{
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("REPONSE EN FREQUENCES", z.removeFromTop (11.0f),
                juce::Justification::centredLeft);

    const Programme& p = kProgrammes[iProg];
    const float zero = z.getCentreY();
    g.setColour (pal.lueur.withAlpha (0.28f));
    g.drawLine (z.getX(), zero, z.getRight(), zero, 1.0f);

    const float larg = z.getWidth() / kTailleSpectre;
    for (int i = 0; i < kTailleSpectre; ++i)
    {
        const float f = 250.0f * std::pow (16000.0f / 250.0f, (float) i / (kTailleSpectre - 1));
        const float v = p.spectreDb[i] + (tilt * 0.5f)
                          * juce::jlimit (-1.0f, 1.0f, std::log2 (f / 700.0f) / 3.0f);
        const float t = juce::jlimit (-1.0f, 1.0f, v / 12.0f);
        const float h = juce::jmax (1.0f, std::abs (t) * z.getHeight() * 0.5f);
        g.setColour (pal.lueur.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> (z.getX() + i * larg + 1.0f,
                                            t >= 0.0f ? zero - h : zero,
                                            juce::jmax (larg - 2.0f, 1.0f), h));
    }
}

void Ecran::dessinerCercles (juce::Graphics& g, juce::Rectangle<float> z)
{
    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (8.0f));
    g.drawText ("STEREO", z.removeFromTop (11.0f), juce::Justification::centredLeft);

    // ⚠️ L'ecartement EST la decorrelation : un seul disque en mono
    // (correlation +1), deux disques imbriques a la reference (correlation 0),
    // detaches au-dela. La figure dit ce que mesure la correlation, sans chiffre.
    const float R = juce::jmin (13.0f, z.getHeight() * 0.46f);
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
    // Quatre temoins comme sur les unites Lexicon. Le quatrieme est le plafond
    // d'entree du manuel (+15 dBV) : au-dela, l'etage d'entree ecrete.
    //
    // ⚠️ C'est le gain d'ENTREE qui decide ou l'on se situe dans cette plage.
    // Sans lui le quatrieme temoin ne s'allumait qu'au-dessus de 0 dBFS, ce qui
    // n'arrive presque jamais — le bandeau ne servait a rien.
    static constexpr float seuils[4] = { -24.0f, -12.0f, -4.0f, 0.0f };
    static const char* etiquettes[4] = { "-24", "-12", "-4", "0" };

    g.setColour (pal.lueurFaible);
    g.setFont (juce::FontOptions (8.5f).withStyle ("bold"));
    g.drawText ("HEADROOM", z.removeFromTop (12.0f), juce::Justification::centredLeft);
    z.removeFromTop (4.0f);

    const float h = juce::jmin (16.0f, (z.getHeight() - 3 * 5.0f) / 4.0f);
    for (int i = 0; i < 4; ++i)
    {
        auto ligne = juce::Rectangle<float> (z.getX(), z.getY() + i * (h + 5.0f), z.getWidth(), h);
        auto txt = ligne.removeFromRight (20.0f);
        const bool on = niveauDb >= seuils[3 - i];      // le plus fort en haut

        if (on && i == 0) g.setColour (juce::Colour (0xffe5623c));
        else if (on)      g.setColour (pal.lueur.withAlpha (0.8f));
        else              g.setColour (pal.lueur.withAlpha (0.10f));
        g.fillRoundedRectangle (ligne.reduced (0.0f, 1.0f), 1.5f);

        g.setColour (on ? pal.lueur : pal.lueurFaible.withAlpha (0.7f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (etiquettes[3 - i], txt, juce::Justification::centredRight);
    }
}

//==============================================================================
LuxRevEditor::LuxRevEditor (LuxRevProcessor& p)
    : AudioProcessorEditor (&p), proc (p), ecran (p, pal)
{
    setLookAndFeel (&apparence);
    addAndMakeVisible (ecran);

    auto* choix = dynamic_cast<juce::AudioParameterChoice*> (proc.etat.getParameter ("programme"));
    for (int i = 0; i < kNombreProgrammes; ++i)
    {
        auto* b = cellules.add (new juce::TextButton (juce::String (kProgrammes[i].nom).toUpperCase()));
        b->setLookAndFeel (&apparenceCellule);
        b->setClickingTogglesState (false);
        b->onClick = [this, i, choix]
        {
            if (choix == nullptr) return;
            choix->beginChangeGesture();
            *choix = i;
            choix->endChangeGesture();
            // Le programme impose ses reglages d'usine : Decay et Pre-Delay de la
            // Program table du manuel.
            if (auto* d = proc.etat.getParameter ("decay"))
                d->setValueNotifyingHost (d->convertTo0to1 (
                    juce::jlimit (kProgrammes[i].t60Min, kProgrammes[i].t60Max, kProgrammes[i].t60S)));
            if (auto* pd = proc.etat.getParameter ("predelai"))
                pd->setValueNotifyingHost (pd->convertTo0to1 (kProgrammes[i].predelaiUsineMs));
        };
        addAndMakeVisible (b);
    }

    // ⚠️ Les trois commandes qui vont au-dela de la machine ont un CENTRE : leur
    // arc part de la position neutre, qui redonne exactement le LXP-1 mesure.
    // ⚠️ ENTREE est place AVANT Decay parce qu'il est avant dans la chaine :
    // il precede l'etage d'entree, donc c'est lui qui decide ou l'on se situe
    // dans la plage du manuel et ce que le bandeau de headroom affiche.
    construireBouton (sEntree,   lEntree,   "ENTREE",    "entree",    0.5f);
    construireBouton (sDecay,    lDecay,    "DECAY",     "decay",    -1.0f);
    construireBouton (sPredelai, lPredelai, "PRE-DELAY", "predelai", -1.0f);
    construireBouton (sColor,    lColor,    "COLOR",     "color",     0.5f);
    construireBouton (sStereo,   lStereo,   "STEREO",    "stereo",    100.0f / 160.0f);
    construireBouton (sTilt,     lTilt,     "TILT",      "tilt",      0.5f);
    construireBouton (sMix,      lMix,      "MIX",       "mix",      -1.0f);

    lecture.setColour (juce::Label::textColourId, pal.lueur);
    lecture.setJustificationType (juce::Justification::centredLeft);
    lecture.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (lecture);

    setSize (kLargeur, kHauteur);
    startTimerHz (30);
}

LuxRevEditor::~LuxRevEditor()
{
    for (auto* b : cellules) b->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void LuxRevEditor::construireBouton (juce::Slider& s, juce::Label& l,
                                     const juce::String& nom, const juce::String& id,
                                     float centre)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (kAngleDebut, kAngleFin, true);
    s.setDoubleClickReturnValue (true, s.getValue());
    apparence.poserCentre (s, centre);
    addAndMakeVisible (s);

    l.setText (nom, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, pal.serigraphie);
    l.setFont (juce::FontOptions (9.5f).withStyle ("bold"));
    addAndMakeVisible (l);

    auto& a = (id == "entree")   ? aEntree
            : (id == "decay")    ? aDecay
            : (id == "predelai") ? aPredelai
            : (id == "color")    ? aColor
            : (id == "stereo")   ? aStereo
            : (id == "tilt")     ? aTilt : aMix;
    a = std::make_unique<Attache> (proc.etat, id, s);
}

//==============================================================================
void LuxRevEditor::mouseDown (const juce::MouseEvent& e)
{
    // La diode et le titre forment un interrupteur invisible. A l'arret, SEULS
    // la diode et l'ecran se grisent : le panneau garde son aspect, comme une
    // machine eteinte dont la face avant est toujours la.
    if (! zoneInterrupteur.contains (e.getPosition())) return;
    if (auto* a = proc.etat.getParameter ("actif"))
    {
        a->beginChangeGesture();
        a->setValueNotifyingHost (a->getValue() > 0.5f ? 0.0f : 1.0f);
        a->endChangeGesture();
    }
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
    const bool  actif = proc.etat.getRawParameterValue ("actif")->load() > 0.5f;

    sDecay.setDoubleClickReturnValue (true,
        juce::jlimit (kProgrammes[ip].t60Min, kProgrammes[ip].t60Max, kProgrammes[ip].t60S));
    sPredelai.setDoubleClickReturnValue (true, kProgrammes[ip].predelaiUsineMs);

    ecran.poserAllume (actif);
    ecran.poserEtat (ip, decay, pre, color, ster, tilt, mix);
    ecran.poserNiveau (actif ? proc.niveauEntreeDb.load() : -100.0f);
    repaint (zoneInterrupteur);

    // L'ecran affiche le parametre qu'on est en train de bouger : c'est ce qui
    // autorise des boutons nus et des etiquettes courtes.
    juce::String txt = "DECAY";
    juce::String val = juce::String (decay, 2) + " s";
    if      (sEntree.isMouseOverOrDragging())   { txt = "ENTREE";    const float e = proc.etat.getRawParameterValue ("entree")->load();
                                                 val = (e > 0 ? "+" : "") + juce::String (e, 1) + " dB"; }
    else if (sPredelai.isMouseOverOrDragging()) { txt = "PRE-DELAY"; val = juce::String ((int) pre) + " ms"; }
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

    auto panneau = zone.reduced (8.0f);
    juce::ColourGradient grad (pal.panneauA, panneau.getX() + panneau.getWidth() * 0.12f, panneau.getY(),
                               pal.panneauC, panneau.getRight(), panneau.getBottom(), true);
    grad.addColour (0.52, pal.panneauB);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (panneau, 7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (panneau.reduced (0.5f), 7.0f, 1.0f);

    const bool actif = proc.etat.getRawParameterValue ("actif")->load() > 0.5f;

    const float x = panneau.getX() + 18.0f;
    const float y = panneau.getY() + 16.0f;
    g.setColour (actif ? pal.lueur : pal.serigraphie2.withAlpha (0.45f));
    g.fillEllipse (x, y + 9.0f, 7.0f, 7.0f);
    g.setColour (pal.serigraphie);
    g.setFont (juce::FontOptions (23.0f).withStyle ("bold"));
    g.drawText ("LuxRev-1", juce::Rectangle<float> (x + 16.0f, y, 220.0f, 25.0f),
                juce::Justification::centredLeft);
    g.setColour (pal.serigraphie2);
    g.setFont (juce::FontOptions (9.0f));
    g.drawText ("DIGITAL REVERBERATOR",
                juce::Rectangle<float> (panneau.getRight() - 220.0f, y, 202.0f, 25.0f),
                juce::Justification::centredRight);

    // ⚠️ La ligne de lecture est posee sur du VERRE, comme dans la maquette. Du
    // texte pose a meme le panneau ne se lit pas comme un afficheur.
    {
        auto v = zoneLecture.toFloat();
        g.setGradientFill (juce::ColourGradient (pal.verre2, v.getX(), v.getY(),
                                                 pal.verre, v.getRight(), v.getBottom(), false));
        g.fillRoundedRectangle (v, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (v.reduced (0.5f), 4.0f, 1.0f);
    }

    auto bande = juce::Rectangle<float> (panneau.getX() + 18.0f, y + 32.0f,
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

    // la diode et le titre : l'interrupteur invisible
    zoneInterrupteur = juce::Rectangle<int> (zone.getX() - 2, 16, 200, 30);

    zone.removeFromTop (56);
    zone.removeFromBottom (12);

    ecran.setBounds (zone.removeFromTop (226));
    zone.removeFromTop (12);

    auto grille = zone.removeFromTop (96);
    constexpr int cols = 4, rows = 3;
    const int lc = grille.getWidth() / cols, lh = grille.getHeight() / rows;
    for (int i = 0; i < cellules.size(); ++i)
        cellules[i]->setBounds (juce::Rectangle<int> (grille.getX() + (i % cols) * lc,
                                                      grille.getY() + (i / cols) * lh,
                                                      lc - 5, lh - 5));
    zone.removeFromTop (10);

    auto rangee = zone.removeFromTop (128);

    // ENTREE est plus petit que les six autres : c'est un reglage de niveau, pas
    // une commande de caractere, et la facade doit le dire.
    {
        auto c = rangee.removeFromLeft (72);
        lEntree.setBounds (c.removeFromTop (16));
        sEntree.setBounds (c.reduced (10, 10));
    }
    rangee.removeFromLeft (8);

    juce::Slider* sl[] = { &sDecay, &sPredelai, &sColor, &sStereo, &sTilt, &sMix };
    juce::Label*  la[] = { &lDecay, &lPredelai, &lColor, &lStereo, &lTilt, &lMix };
    const int pas = rangee.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto c = juce::Rectangle<int> (rangee.getX() + i * pas, rangee.getY(), pas, rangee.getHeight());
        la[i]->setBounds (c.removeFromTop (16));
        sl[i]->setBounds (c.reduced (4, 2));
    }

    zone.removeFromTop (8);
    zoneLecture = zone.removeFromTop (32);
    lecture.setBounds (zoneLecture.reduced (14, 0));
}
