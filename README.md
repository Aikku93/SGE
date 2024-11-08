# SGE

Perhaps the most ambitious and overpowered GBA/NDS audio driver ever developed.

Definitely not the fastest by a wide margin (there are way too many features implemented), but it is VERY heavily optimized, and fully written in assembly.

## Features

* Mono/stereo waveform support (with support for linear interpolation)
* Waveforms can be PCM8, PCM16 (NDS only), or ADPCM (all features supported for all formats)
* Volume ramping (by slicing a mix chunk into 2^N pieces or less, as needed)
* Reverb processing (using series all-pass filters with feedback and low-pass filtering)
  * Not just a simple delay line; this is REAL, diffuse-sounding reverb (with crosstalk to improve stereo width).
  * Defined using parameters such as feedback, room density, etc.; filter taps are calculated at run-time.
* All mixer loops are extremely optimized for GBA
  * NDS9 is mostly optimized for ADPCM playback, and NDS7 doesn't have much room for optimization.
* Sampling rate and number of voices is controlled at run-time
* Support for 2x oversampling (using linear interpolation) to reduce hardware aliasing artifacts
  * This is really only needed for GBA, where the usual playback rates are not nice multiples of 32768Hz.
* Music is defined using MML, combined with a DLS soundbank (SF2 and MIDI support planned... eventually)
* Extremely feature-rich (read as: overengineered) compiler, with many, many options regarding waveform conversions.

## ... Why? Just why?

I'm developing this driver for a tech demo I'm working on. It's been a very wild, but fun ride, so far.

## Demos

Maybe eventually. [I'm uploading some ports I'm working on to YouTube, though](https://www.youtube.com/@Aikku93). Bear in mind that I've been uploading these as I work on the driver and my soundbank, so earlier videos may sound different than later ones.

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)
