#include "TunerComponent.h"
#include <cmath>

TunerComponent::TunerComponent()
{
    ring.setSize (1, kRingSize);
    ring.clear();
    startTimerHz (30);
}

void TunerComponent::pushSamples (const float* data, int numSamples)
{
    if (data == nullptr || numSamples <= 0) return;
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < numSamples; ++i)
    {
        ring.setSample (0, writePos, data[i]);
        writePos = (writePos + 1) % kRingSize;
    }
}

void TunerComponent::analyse()
{
    const double sr = sampleRate;
    std::vector<float> buf ((size_t) kRingSize);
    {
        const juce::ScopedLock sl (lock);
        for (int i = 0; i < kRingSize; ++i)
            buf[(size_t) i] = ring.getSample (0, (writePos + i) % kRingSize);
    }

    double mean = 0.0;
    for (float s : buf) mean += (double) s;
    mean /= (double) kRingSize;
    for (auto& s : buf) s = (float) ((double) s - mean);

    float rms = 0.0f;
    for (float s : buf) rms += s * s;
    rms = std::sqrt (rms / (float) kRingSize);

    if (rms < 0.008f)
    {
        hasNote = false;
        detectedFreq = 0.0f;
        centsOffset = 0.0f;
        smoothCents *= 0.88f;
        smoothFreq  *= 0.88f;
        return;
    }

    const int minPeriod = juce::jmax (2, (int) (sr / 400.0));
    const int maxPeriod = juce::jmin (kRingSize / 2 - 2, (int) (sr / 70.0));

    float bestCorr = -1.0f;
    int   bestLag  = minPeriod;

    for (int lag = minPeriod; lag <= maxPeriod; ++lag)
    {
        double corr = 0.0, e1 = 0.0, e2 = 0.0;
        const int n = kRingSize - lag;
        for (int i = 0; i < n; ++i)
        {
            const double a = (double) buf[(size_t) i];
            const double b = (double) buf[(size_t) (i + lag)];
            corr += a * b; e1 += a * a; e2 += b * b;
        }
        const float ncorr = (float) (corr / (std::sqrt (e1 * e2) + 1.0e-12));
        if (ncorr > bestCorr) { bestCorr = ncorr; bestLag = lag; }
    }

    if (bestCorr < 0.55f) { hasNote = false; return; }

    float period = (float) bestLag;
    if (bestLag > minPeriod && bestLag < maxPeriod)
    {
        auto corrAt = [&] (int lag) -> float
        {
            double corr = 0.0, e1 = 0.0, e2 = 0.0;
            const int n = kRingSize - lag;
            for (int i = 0; i < n; i += 2)
            {
                const double a = (double) buf[(size_t) i];
                const double b = (double) buf[(size_t) (i + lag)];
                corr += a * b; e1 += a * a; e2 += b * b;
            }
            return (float) (corr / (std::sqrt (e1 * e2) + 1.0e-12));
        };
        const float ym1 = corrAt (bestLag - 1);
        const float y0  = bestCorr;
        const float yp1 = corrAt (bestLag + 1);
        const float denom = 2.0f * (2.0f * y0 - yp1 - ym1);
        if (std::abs (denom) > 1.0e-6f)
            period = (float) bestLag + (ym1 - yp1) / denom;
    }

    detectedFreq = (float) (sr / (double) period);
    if (detectedFreq < 70.0f || detectedFreq > 400.0f) { hasNote = false; return; }

    const float midi = 69.0f + 12.0f * std::log2 (detectedFreq / 440.0f);
    const int nearest = (int) std::round (midi);
    centsOffset = (midi - (float) nearest) * 100.0f;

    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const int n = ((nearest % 12) + 12) % 12;
    noteName = juce::String (names[n]);
    noteOctave = (nearest / 12) - 1;
    hasNote = true;

    const float a = 0.22f;
    smoothCents = smoothCents + a * (centsOffset - smoothCents);
    smoothFreq  = smoothFreq  + a * (detectedFreq - smoothFreq);
}

void TunerComponent::timerCallback() { analyse(); repaint(); }

//==============================================================================
void TunerComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0a));

    const float W = (float) getWidth();
    const float H = (float) getHeight();
    if (W < 10.0f || H < 10.0f) return;

    const bool inTune = hasNote && std::abs (smoothCents) < 5.0f;
    const juce::Colour accent = inTune ? juce::Colour (0xff2ecc71) : juce::Colours::white;

    // ---- Geometry of the wide shallow arc ----
    // Chord spans nearly full width; arc bows upward a modest amount.
    const float marginX   = W * 0.04f;
    const float chordHalf = (W * 0.5f) - marginX;          // half-width of arc
    const float arcDepth  = juce::jlimit (28.0f, H * 0.16f, W * 0.08f); // how tall the bow is
    const float peakY     = H * 0.10f;                      // top of the arc
    // Circle that passes through the three points (left end, peak, right end):
    // radius = (chordHalf^2 + arcDepth^2) / (2 * arcDepth)
    const float radius = (chordHalf * chordHalf + arcDepth * arcDepth) / (2.0f * arcDepth);
    const float cx = W * 0.5f;
    const float cy = peakY + radius;                        // centre sits below the peak

    // Angle from vertical (negative x is flat/left). Math angles CCW from +x:
    // left end:  angle = pi - alpha, right end: angle = alpha, peak: angle = pi/2
    // where cos(alpha) style: half-angle from top
    const float halfAngle = std::asin (juce::jlimit (0.0f, 1.0f, chordHalf / radius));
    // In standard math (CCW from +x, y up): left = pi/2 + halfAngle, right = pi/2 - halfAngle
    // Screen y grows down, so we flip: use x = cx + r*cos(a), y = cy - r*sin(a) with math angles.

    auto centsToPos = [&] (float cents) -> juce::Point<float>
    {
        const float t = juce::jlimit (-1.0f, 1.0f, cents / 50.0f); // -1..+1
        // Interpolate angle from left to right
        const float a = (juce::MathConstants<float>::halfPi + halfAngle)
                        - (t + 1.0f) * 0.5f * (2.0f * halfAngle);
        // a goes left→right: (pi/2+half) → (pi/2-half)
        return { cx + radius * std::cos (a),
                 cy - radius * std::sin (a) };
    };

    auto centsToAngle = [&] (float cents) -> float
    {
        const float t = juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
        return (juce::MathConstants<float>::halfPi + halfAngle)
               - (t + 1.0f) * 0.5f * (2.0f * halfAngle);
    };

    const float thickness = juce::jlimit (14.0f, 36.0f, H * 0.045f);

    // ---- Draw coloured arc segments ----
    auto strokeSeg = [&] (float c0, float c1, juce::Colour col)
    {
        const float a0 = centsToAngle (c0);
        const float a1 = centsToAngle (c1);
        // Build polyline along the arc (math angles, y flipped)
        juce::Path p;
        const int steps = 24;
        for (int i = 0; i <= steps; ++i)
        {
            const float a = a0 + (a1 - a0) * ((float) i / (float) steps);
            const float x = cx + radius * std::cos (a);
            const float y = cy - radius * std::sin (a);
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };

    strokeSeg (-50.0f, -20.0f, juce::Colour (0xffe74c3c).withAlpha (0.7f));
    strokeSeg (-20.0f,  -5.0f, juce::Colour (0xfff1c40f).withAlpha (0.7f));
    strokeSeg ( -5.0f,   5.0f, juce::Colour (0xff2ecc71).withAlpha (0.9f));
    strokeSeg (  5.0f,  20.0f, juce::Colour (0xfff1c40f).withAlpha (0.7f));
    strokeSeg ( 20.0f,  50.0f, juce::Colour (0xffe74c3c).withAlpha (0.7f));

    // ---- Tick marks ----
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    for (int c = -50; c <= 50; c += 10)
    {
        const float a = centsToAngle ((float) c);
        const float cosA = std::cos (a);
        const float sinA = std::sin (a);
        const float r0 = radius - thickness * 0.55f;
        const float r1 = radius + thickness * 0.55f;
        g.drawLine (cx + cosA * r0, cy - sinA * r0,
                    cx + cosA * r1, cy - sinA * r1,
                    c == 0 ? 3.0f : 1.5f);
    }

    // ---- Centre marker (triangle above peak) ----
    {
        auto peak = centsToPos (0.0f);
        juce::Path tri;
        tri.addTriangle (peak.x, peak.y - thickness * 0.7f - 2.0f,
                         peak.x - 10.0f, peak.y - thickness * 0.7f - 18.0f,
                         peak.x + 10.0f, peak.y - thickness * 0.7f - 18.0f);
        g.setColour (juce::Colours::white);
        g.fillPath (tri);
    }

    // ---- Needle ----
    if (hasNote)
    {
        auto pos = centsToPos (smoothCents);
        const float rad = juce::jlimit (10.0f, 16.0f, thickness * 0.55f);
        g.setColour (inTune ? juce::Colour (0xff2ecc71) : juce::Colour (0xffff5252));
        g.fillEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (pos.x - rad, pos.y - rad, rad * 2.0f, rad * 2.0f, 2.0f);
    }

    // ---- Cents text (top right) ----
    if (hasNote)
    {
        g.setFont (juce::FontOptions (juce::jlimit (16.0f, 26.0f, H * 0.04f)));
        g.setColour (inTune ? juce::Colour (0xff2ecc71) : juce::Colours::white.withAlpha (0.85f));
        const juce::String centsStr = (smoothCents >= 0 ? "+" : "")
                                      + juce::String ((int) std::round (smoothCents)) + " cent";
        g.drawText (centsStr,
                    juce::Rectangle<float> (W * 0.55f, 4.0f, W * 0.4f, H * 0.06f),
                    juce::Justification::centredRight);
    }

    // ---- Giant note ----
    {
        const float noteTop = peakY + arcDepth + thickness + H * 0.04f;
        const float noteH   = H * 0.48f;
        auto noteArea = juce::Rectangle<float> (0.0f, noteTop, W, noteH);

        if (hasNote)
        {
            g.setColour (accent.withAlpha (0.07f));
            const float glow = juce::jmin (W, noteH) * 0.5f;
            g.fillEllipse (noteArea.withSizeKeepingCentre (glow, glow));

            const float fontSize = juce::jlimit (90.0f, 240.0f, noteH * 0.75f);
            g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
            g.setColour (accent);
            g.drawText (noteName, noteArea.toNearestInt(), juce::Justification::centred);

            // Octave
            g.setFont (juce::FontOptions (fontSize * 0.3f, juce::Font::bold));
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText (juce::String (noteOctave),
                        noteArea.reduced (W * 0.18f, noteH * 0.12f).toNearestInt(),
                        juce::Justification::topRight);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.setFont (juce::FontOptions (juce::jlimit (22.0f, 36.0f, H * 0.055f)));
            g.drawText ("Play a note", noteArea.toNearestInt(), juce::Justification::centred);
        }
    }

    // ---- Bottom: Hz ----
    if (hasNote)
    {
        g.setFont (juce::FontOptions (juce::jlimit (18.0f, 32.0f, H * 0.05f), juce::Font::bold));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText (juce::String (smoothFreq, 1) + " Hz",
                    juce::Rectangle<float> (W * 0.04f, H * 0.90f, W * 0.4f, H * 0.08f),
                    juce::Justification::centredLeft);
    }
}

void TunerComponent::resized() {}
