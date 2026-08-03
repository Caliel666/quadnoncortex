#include "TunerComponent.h"
#include "Theme.h"
#include <cmath>

TunerComponent::TunerComponent()
{
    ring.setSize (1, kRingSize);
    ring.clear();

    refDown.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    refUp.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    muteBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);

    refDown.onClick = [this] { setReferenceHz (referenceHz - 1.0f); };
    refUp.onClick   = [this] { setReferenceHz (referenceHz + 1.0f); };
    muteBtn.onClick = [this] { setOutputMuted (! muteOutput); };

    refLabel.setJustificationType (juce::Justification::centred);
    refLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (refDown);
    addAndMakeVisible (refUp);
    addAndMakeVisible (refLabel);
    addAndMakeVisible (muteBtn);

    updateRefLabel();
    updateMuteButton();
    startTimerHz (30);
}

void TunerComponent::setReferenceHz (float hz)
{
    referenceHz = juce::jlimit (420.0f, 460.0f, hz);
    updateRefLabel();
}

void TunerComponent::setOutputMuted (bool muted)
{
    if (muteOutput == muted)
        return;
    muteOutput = muted;
    updateMuteButton();
    if (onMuteChanged)
        onMuteChanged (muteOutput);
}

void TunerComponent::updateRefLabel()
{
    refLabel.setText (juce::String (referenceHz, 0) + " Hz", juce::dontSendNotification);
    // Large readable font for bottom bar
    const float fs = juce::jlimit (22.0f, 36.0f, (float) getHeight() * 0.055f);
    refLabel.setFont (juce::FontOptions (fs > 1.0f ? fs : 28.0f, juce::Font::bold));
}

void TunerComponent::updateMuteButton()
{
    muteBtn.setButtonText (muteOutput ? "MUTED" : "UNMUTED");
    auto& th = Theme::get();
    if (muteOutput)
    {
        muteBtn.setColour (juce::TextButton::buttonColourId, th.danger.withAlpha (0.85f));
        muteBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        muteBtn.setColour (juce::TextButton::buttonColourId, th.accent.withAlpha (0.75f));
        muteBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
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

    if (rms < 0.0030f)
    {
        hasNote = false;
        detectedFreq = 0.0f;
        centsOffset = 0.0f;
        smoothCents *= 0.88f;
        smoothFreq  *= 0.88f;
        // Decay lock slowly so a brief gap doesn't throw octave away
        if (lockFrames > 0)
            --lockFrames;
        if (lockFrames <= 0)
            lockedFreq = 0.0f;
        return;
    }

    // YIN-style cumulative mean normalized difference (more stable than raw ACF)
    const int minPeriod = juce::jmax (2, (int) (sr / (double) kMaxFreq));
    const int maxPeriod = juce::jmin (kRingSize / 3, (int) (sr / (double) kMinFreq));

    std::vector<float> d ((size_t) maxPeriod + 1, 0.0f);
    for (int tau = 1; tau <= maxPeriod; ++tau)
    {
        double sum = 0.0;
        const int n = kRingSize - tau;
        // stride for speed on large buffers
        const int step = (n > 6000) ? 2 : 1;
        for (int i = 0; i < n; i += step)
        {
            const double delta = (double) buf[(size_t) i] - (double) buf[(size_t) (i + tau)];
            sum += delta * delta;
        }
        d[(size_t) tau] = (float) sum;
    }

    // Cumulative mean normalize
    double running = 0.0;
    std::vector<float> cmnd ((size_t) maxPeriod + 1, 1.0f);
    cmnd[0] = 1.0f;
    for (int tau = 1; tau <= maxPeriod; ++tau)
    {
        running += (double) d[(size_t) tau];
        cmnd[(size_t) tau] = (float) (d[(size_t) tau] * (double) tau / (running + 1.0e-12));
    }

    // Absolute threshold: find first dip below threshold, then local minimum
    const float yinThresh = 0.18f;
    int bestTau = -1;
    float bestVal = 1.0f;

    for (int tau = minPeriod; tau <= maxPeriod; ++tau)
    {
        if (cmnd[(size_t) tau] < yinThresh)
        {
            // Walk to local minimum
            while (tau + 1 <= maxPeriod && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;
            bestTau = tau;
            bestVal = cmnd[(size_t) tau];
            break;
        }
    }

    // Fallback: global minimum in range if no threshold cross
    if (bestTau < 0)
    {
        for (int tau = minPeriod; tau <= maxPeriod; ++tau)
        {
            if (cmnd[(size_t) tau] < bestVal)
            {
                bestVal = cmnd[(size_t) tau];
                bestTau = tau;
            }
        }
        // Require a reasonably deep dip
        if (bestVal > 0.35f)
        {
            hasNote = false;
            return;
        }
    }

    // Parabolic interpolation around bestTau
    float period = (float) bestTau;
    if (bestTau > minPeriod && bestTau < maxPeriod)
    {
        const float s0 = cmnd[(size_t) (bestTau - 1)];
        const float s1 = cmnd[(size_t) bestTau];
        const float s2 = cmnd[(size_t) (bestTau + 1)];
        const float denom = 2.0f * (2.0f * s1 - s2 - s0);
        if (std::abs (denom) > 1.0e-8f)
            period = (float) bestTau + (s0 - s2) / denom;
    }

    float freq = (float) (sr / (double) period);
    if (freq < kMinFreq || freq > kMaxFreq)
    {
        hasNote = false;
        return;
    }

    // ---- Octave disambiguation ----
    // Candidates: f, 2f, f/2 (if in range). Pick using YIN value + hysteresis.
    struct Cand { float freq; float yin; };
    Cand cands[3];
    int nCands = 0;

    auto yinAtFreq = [&] (float f) -> float
    {
        const int tau = juce::jlimit (minPeriod, maxPeriod, (int) std::lround (sr / (double) f));
        return cmnd[(size_t) tau];
    };

    cands[nCands++] = { freq, bestVal };

    if (freq * 2.0f <= kMaxFreq)
        cands[nCands++] = { freq * 2.0f, yinAtFreq (freq * 2.0f) };
    if (freq * 0.5f >= kMinFreq)
        cands[nCands++] = { freq * 0.5f, yinAtFreq (freq * 0.5f) };

    // Prefer candidate with lowest YIN (best period match)
    int bestC = 0;
    for (int i = 1; i < nCands; ++i)
        if (cands[i].yin < cands[bestC].yin * 0.97f) // need clear win to override
            bestC = i;

    // Hysteresis: if we already have a stable lock, prefer staying in that octave
    if (lockedFreq > 1.0f && lockFrames >= 4)
    {
        int closest = 0;
        float bestRatio = 99.0f;
        for (int i = 0; i < nCands; ++i)
        {
            const float r = std::max (cands[i].freq / lockedFreq, lockedFreq / cands[i].freq);
            // Prefer same octave (ratio ~1) or exact octave (ratio ~2) only if YIN is competitive
            if (r < bestRatio)
            {
                bestRatio = r;
                closest = i;
            }
        }
        // Stay with locked octave if within ~3% or exact octave and not much worse YIN
        if (bestRatio < 1.04f
            || (bestRatio < 2.08f && bestRatio > 1.92f && cands[closest].yin < cands[bestC].yin * 1.15f))
        {
            // If closest is an octave off, snap to locked octave frequency scaling
            if (bestRatio > 1.5f)
            {
                // Pick the cand nearest lockedFreq
                bestC = closest;
            }
            else
            {
                bestC = closest;
            }
        }
    }

    freq = cands[bestC].freq;

    // Median of recent estimates for display stability
    recentFreqs[recentIdx] = freq;
    recentIdx = (recentIdx + 1) % 5;
    if (recentCount < 5) ++recentCount;
    {
        float tmp[5];
        for (int i = 0; i < recentCount; ++i)
            tmp[i] = recentFreqs[i];
        // simple insertion sort
        for (int i = 1; i < recentCount; ++i)
        {
            float v = tmp[i];
            int j = i;
            while (j > 0 && tmp[j - 1] > v) { tmp[j] = tmp[j - 1]; --j; }
            tmp[j] = v;
        }
        freq = tmp[recentCount / 2];
    }

    // Update lock
    if (lockedFreq < 1.0f)
    {
        lockedFreq = freq;
        lockFrames = 1;
    }
    else
    {
        const float ratio = std::max (freq / lockedFreq, lockedFreq / freq);
        if (ratio < 1.06f)
        {
            lockedFreq = 0.85f * lockedFreq + 0.15f * freq;
            lockFrames = juce::jmin (60, lockFrames + 1);
        }
        else if (ratio > 1.9f && ratio < 2.1f)
        {
            // Octave jump candidate — only accept after sustained disagreement
            lockFrames = juce::jmax (0, lockFrames - 2);
            if (lockFrames < 3)
            {
                lockedFreq = freq;
                lockFrames = 1;
                recentCount = 0; // reset median buffer on accepted octave change
            }
            else
            {
                // Hold previous octave
                freq = lockedFreq;
            }
        }
        else
        {
            // Unrelated note — retune lock
            lockedFreq = freq;
            lockFrames = 1;
            recentCount = 1;
            recentIdx = 0;
            recentFreqs[0] = freq;
        }
    }

    detectedFreq = freq;

    const float ref = referenceHz > 1.0f ? referenceHz : 440.0f;
    const float midi = 69.0f + 12.0f * std::log2 (detectedFreq / ref);
    const int nearest = (int) std::round (midi);
    centsOffset = (midi - (float) nearest) * 100.0f;

    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const int nn = ((nearest % 12) + 12) % 12;
    noteName = juce::String (names[nn]);
    noteOctave = (nearest / 12) - 1;
    hasNote = true;

    const float a = 0.32f;
    smoothCents = smoothCents + a * (centsOffset - smoothCents);
    smoothFreq  = smoothFreq  + a * (detectedFreq - smoothFreq);
}

void TunerComponent::timerCallback() { analyse(); repaint(); }

void TunerComponent::paint (juce::Graphics& g)
{
    auto& th = Theme::get();
    g.fillAll (th.background);
    const bool light = th.currentName == "Light";
    // Keep controls readable; do not alter arc geometry
    const auto inkCtrl = light ? juce::Colour (0xff1f2329) : juce::Colours::white;
    refLabel.setColour (juce::Label::textColourId, inkCtrl);
    refDown.setColour (juce::TextButton::textColourOffId, inkCtrl);
    refUp.setColour (juce::TextButton::textColourOffId, inkCtrl);
    const auto ink = light ? juce::Colour (0xff1f2329) : juce::Colours::white;
    const auto inkDim = light ? juce::Colour (0xff6b7280) : juce::Colours::white.withAlpha (0.35f);

    const float W = (float) getWidth();
    const float H = (float) getHeight();
    if (W < 10.0f || H < 10.0f) return;

    const float marginX   = W * 0.04f;
    const float chordHalf = (W * 0.5f) - marginX;
    const float arcDepth  = juce::jlimit (28.0f, H * 0.16f, W * 0.08f);
    const float peakY     = H * 0.10f;
    const float radius = (chordHalf * chordHalf + arcDepth * arcDepth) / (2.0f * arcDepth);
    const float cx = W * 0.5f;
    const float cy = peakY + radius;
    const float halfAngle = std::asin (juce::jlimit (0.0f, 1.0f, chordHalf / radius));
    const float thickness = juce::jlimit (14.0f, 32.0f, H * 0.045f);

    auto centsToAngle = [&] (float cents) -> float
    {
        const float tt = juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
        return (juce::MathConstants<float>::halfPi + halfAngle)
               - (tt + 1.0f) * 0.5f * (2.0f * halfAngle);
    };

    auto centsToPos = [&] (float cents) -> juce::Point<float>
    {
        const float a = centsToAngle (cents);
        return { cx + radius * std::cos (a), cy - radius * std::sin (a) };
    };

    auto strokeArc = [&] (float c0, float c1, juce::Colour col, float thick)
    {
        const float a0 = centsToAngle (c0);
        const float a1 = centsToAngle (c1);
        juce::Path p;
        const int steps = juce::jmax (8, (int) (std::abs (c1 - c0) * 1.2f));
        for (int i = 0; i <= steps; ++i)
        {
            const float a = a0 + (a1 - a0) * ((float) i / (float) steps);
            const float x = cx + radius * std::cos (a);
            const float y = cy - radius * std::sin (a);
            if (i == 0) p.startNewSubPath (x, y);
            else        p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };

    // Neutral base arc
    strokeArc (-50.0f, 50.0f, inkDim.withAlpha (0.4f), thickness);

    // Ticks
    g.setColour (inkDim);
    for (int c = -50; c <= 50; c += 10)
    {
        const float a = centsToAngle ((float) c);
        const float cosA = std::cos (a), sinA = std::sin (a);
        const float r0 = radius - thickness * 0.55f;
        const float r1 = radius + thickness * 0.55f;
        g.drawLine (cx + cosA * r0, cy - sinA * r0,
                    cx + cosA * r1, cy - sinA * r1,
                    c == 0 ? 3.0f : 1.4f);
    }

    // Centre marker
    {
        auto peak = centsToPos (0.0f);
        juce::Path tri;
        tri.addTriangle (peak.x, peak.y - thickness * 0.7f - 2.0f,
                         peak.x - 10.0f, peak.y - thickness * 0.7f - 18.0f,
                         peak.x + 10.0f, peak.y - thickness * 0.7f - 18.0f);
        g.setColour (ink.withAlpha (0.85f));
        g.fillPath (tri);
    }

    // Indicator: arc segment ON the curve (small/green centre → long/red at edges)
    if (hasNote)
    {
        const float absC = std::abs (smoothCents);
        const float tAmt = juce::jlimit (0.0f, 1.0f, absC / 50.0f);
        // Half-width in cents grows toward edges
        const float halfW = juce::jmap (tAmt, 2.5f, 14.0f);
        const float thick = thickness * juce::jmap (tAmt, 1.0f, 1.15f);
        const juce::Colour col = juce::Colour (0xff2ecc71).interpolatedWith (
                                     juce::Colour (0xffe74c3c), tAmt);

        float c0 = smoothCents - halfW;
        float c1 = smoothCents + halfW;
        // Clamp to arc range but keep segment length when at extreme
        if (c0 < -50.0f) { c1 = juce::jmin (50.0f, c1 + (-50.0f - c0)); c0 = -50.0f; }
        if (c1 >  50.0f) { c0 = juce::jmax (-50.0f, c0 - (c1 - 50.0f)); c1 = 50.0f; }

        // Soft glow under
        strokeArc (c0, c1, col.withAlpha (0.25f), thick * 1.6f);
        strokeArc (c0, c1, col, thick);

        // Small highlight bead at exact position
        auto pos = centsToPos (smoothCents);
        const float bead = thick * juce::jmap (tAmt, 0.35f, 0.22f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillEllipse (pos.x - bead, pos.y - bead, bead * 2.0f, bead * 2.0f);
    }

    // Note / cents / Hz
    {
        const float noteTop = peakY + arcDepth + thickness + H * 0.04f;
        const float noteH   = H * 0.48f;
        auto noteArea = juce::Rectangle<float> (0.0f, noteTop, W, noteH);

        if (hasNote)
        {
            const float absC = std::abs (smoothCents);
            const float tAmt = juce::jlimit (0.0f, 1.0f, absC / 50.0f);
            const juce::Colour accent = juce::Colour (0xff2ecc71).interpolatedWith (
                                            juce::Colour (0xffe74c3c), tAmt);

            g.setColour (accent.withAlpha (0.08f));
            const float glow = juce::jmin (W, noteH) * 0.5f;
            g.fillEllipse (noteArea.withSizeKeepingCentre (glow, glow));

            const float fontSize = juce::jlimit (90.0f, 240.0f, noteH * 0.75f);
            g.setFont (juce::FontOptions (fontSize, juce::Font::bold));
            g.setColour (accent);
            g.drawText (noteName, noteArea.toNearestInt(), juce::Justification::centred);

            g.setFont (juce::FontOptions (fontSize * 0.3f, juce::Font::bold));
            g.setColour (inkDim);
            g.drawText (juce::String (noteOctave),
                        noteArea.reduced (W * 0.18f, noteH * 0.12f).toNearestInt(),
                        juce::Justification::topRight);

            g.setFont (juce::FontOptions (juce::jlimit (16.0f, 26.0f, H * 0.04f)));
            g.setColour (accent);
            const juce::String centsStr = (smoothCents >= 0 ? "+" : "")
                                          + juce::String ((int) std::round (smoothCents)) + " cent";
            g.drawText (centsStr,
                        juce::Rectangle<float> (W * 0.55f, 4.0f, W * 0.4f, H * 0.06f),
                        juce::Justification::centredRight);

            g.setFont (juce::FontOptions (juce::jlimit (18.0f, 32.0f, H * 0.05f), juce::Font::bold));
            g.setColour (ink.withAlpha (0.7f));
            g.drawText (juce::String (smoothFreq, 1) + " Hz",
                        juce::Rectangle<float> (W * 0.04f, H * 0.90f, W * 0.4f, H * 0.08f),
                        juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (ink.withAlpha (0.2f));
            g.setFont (juce::FontOptions (juce::jlimit (22.0f, 36.0f, H * 0.055f)));
            g.drawText ("Play a note", noteArea.toNearestInt(), juce::Justification::centred);
        }
    }
}

void TunerComponent::resized()
{
    const int W = getWidth();
    const int H = getHeight();
    if (W < 40 || H < 40) return;

    const int ctrlH = juce::jlimit (40, 56, H / 11);
    const int margin = juce::jmax (10, W / 36);
    const int btnW = juce::jlimit (48, 72, W / 12);
    const int gap = 6;

    // Bottom-left: [ - ]  440 Hz  [ + ]
    auto bottom = juce::Rectangle<int> (margin, H - margin - ctrlH, W - 2 * margin, ctrlH);
    auto left = bottom.removeFromLeft (btnW * 2 + gap * 2 + juce::jlimit (90, 140, W / 6));
    refDown.setBounds (left.removeFromLeft (btnW));
    left.removeFromLeft (gap);
    const int labelW = left.getWidth() - btnW - gap;
    refLabel.setBounds (left.removeFromLeft (labelW));
    left.removeFromLeft (gap);
    refUp.setBounds (left.removeFromLeft (btnW));

    // Bottom-right: MUTED / UNMUTED
    const int muteW = juce::jlimit (100, 150, W / 5);
    muteBtn.setBounds (bottom.removeFromRight (muteW));

    updateRefLabel();
}
