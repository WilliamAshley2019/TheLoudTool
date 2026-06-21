THIS IS THE OLD VERSION README I STILL NEED TO UPDAE IT BUT BASICALLY THE SAME WITH SOME NEW VISUAL MODES ETC...  MUST TACKLE CPU LOAD NEXT IT IS A BEAST.
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![JUCE](https://img.shields.io/badge/Built%20with-JUCE%208-blue)](https://juce.com)
[![Platform](https://img.shields.io/badge/Platform-Windows)]()
[![Format](https://img.shields.io/badge/Format-VST3%20%7C%20-orange)]()

## Author: **William Ashley** — AlphaAudio
## Project: *The Loud Tool* (TLT)

**Purpose:** Advanced loudness maximization, multiband density control, harmonic texture, phase enhancement, and professional broadcast-style metering.

# The Loud Tool

A comprehensive final-stage loudness and dynamics processor designed to bring mixes to competitive streaming, broadcast, and mastering levels while maintaining punch, clarity, and transparency.

Built with **JUCE 8** as a VST3 (AU/AAX compatible) plugin. It features perceptual loudness targeting, oversampled saturation, true-peak limiting, multiband compression for density, phase rotation, and rich real-time metering.

## What It Does

The Its Loud Tool combines several pro-level tools into one cohesive rack-style unit:

| Section | Function |
|---------|----------|
| **TARGET LUFS** | Perceptual auto-makeup gain targeting user-selected integrated LUFS standards (-30 to -8). |
| **TEXTURE (Exciter)** | Oversampled asymmetric harmonic saturation (1–5x drive) for adding density and presence. |
| **CEILING** | Lookahead true-peak brickwall limiter with adjustable ceiling. |
| **MULTIBAND DENSITY** | 3-band compression (Low/Mid/High) with adjustable thresholds for controlled density without squashing transients. |
| **PHASE** | Subtle all-pass phase rotation for improved mono compatibility and low-end tightness. |
| **Advanced Metering** | Full BS.1770-style LUFS (Integrated/Short-Term/Momentary), True Peak, multiple measurement modes (K-weighted, A-weighted, RMS, dBu), and analog-style VU/PPM with selectable ballistics. |

Additional tools: **RESET INT**, **MODE** cycling (measurement type), **INT** standard cycling (EBU R128, Streaming, ATSC A/85, Apple Music, Custom), **VU** ballistic modes, **VIEW** (AID spectral bars / A/B comparison), and **EXPORT CSV**.

## Signal Flow

Input → A/B Pre-Process Level Capture  
→ Multiband Density Compression (optional)  
→ Phase Rotation (optional)  
→ Perceptual Makeup Gain (to target LUFS)  
→ Harmonic Exciter / Saturation (oversampled)  
→ True-Peak Lookahead Limiter  
→ Post-Process Metering & Output

- **Oversampling**: 2x with high-quality filters.
- **Latency**: Automatically reported (oversampling + lookahead).
- **Mono handling**: Graceful fallback.

## UI Design

- **Professional rack-mount aesthetic** with dark chassis, side ears, screws, and teal/orange accents.
- Custom **BroadcastKnob** rotary controls with glowing indicators and detailed graphics.
- Large **analog-style VU/PPM meter** with needle, peak LED, and realistic face (including "TLT" branding).
- **LCD-style digital display** for real-time loudness readings, standards, and modes.
- **Resizable editor** (default **850 × 420 px**, min 750×380).

## Controls Summary

- **TARGET** (-30 to -8 LUFS): Loudness target + standard cycling.
- **TEXTURE** (1–5x): Saturation drive.
- **CEILING** (-10 to 0 dBTP): Limiter ceiling.
- **LOW / MID / HIGH**: Multiband compression thresholds.
- **PHASE** (0–1): Phase rotator blend.
- Buttons for all metering/view/export functions.

## Technical Notes

- **Thread-safe atomics** for all parameters and meter values.
- Multiple measurement engines: K-weighted (BS.1770), A-weighted, unweighted RMS, dBu (EBU/SMPTE).
- Custom A-weighting IIR filters and VU/PPM ballistics (Classic VU & BBC PPM).
- Multiband IIR crossovers + envelope followers with fast attack/slow release.
- Ring buffers for short-term and integrated loudness.
- True-peak detection with hold.
- Full state save/load via JUCE serialization.
- **AID (Auditory Information Density)** spectral analysis for visual feedback.

## Default Settings

- Target: **-14 LUFS** (Streaming)
- Texture: **2.0x**
- Ceiling: **-1 dBTP**
- Multiband enabled with sensible thresholds.

## Building

Requires **JUCE 8** and C++20. Standard JUCE audio plugin project structure.  
Compiled `.vst3` included in the repo for quick testing.

## Quick Install (Windows)

Copy the `.vst3` folder to:  
`C:\Program Files\Common Files\VST3\`  
Rescan in your DAW. Plugin name: **The Its Loud Tool** (VIAU-MAX internally).

## Website & Contact

- Website: [williamashley.music](http://williamashley.music)
- Email: contact@williamashley.music

---

Copyright (c) 2026 William Ashley d/b/a William Ashley Music ( http://WilliamAshley.music )

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License (v3).

This program is distributed in the hope that it will be useful... (standard GPL notice)

Attribution and notice of use appreciated.

---

## Third-Party Licenses

Built using the **JUCE framework** (© Raw Material Software Limited).  
VST3 is a trademark of Steinberg Media Technologies GmbH.

 
 NOTE: This plugin was inspired by the Dolby LM100 Broadcast Loudness meter but no dolby sourcecode was used in its development. It expands on the idea
 of a loudness monitor and aims to apply specific loudness monitoring methods to become loudness transforms using a variety of loudness theories that will be touched on as they are developed more into this plugins next version.

 FUTURE GOALS - incorporating "stem intelligence"  like Dolby's dialog intelligence but for specific stem types not just dialog so it can fill a greater 
 music production role rather than just broadcast monitoring role.
 There are some more loudness technologies from the past I would like and add a few more loudness type dials. The VU meter is likely to be changed a little
 making it more traditional so the next version vu style display for the needle etc.. will likely be cleaned up a little.
