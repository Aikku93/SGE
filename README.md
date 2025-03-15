# SGE

Perhaps the most ambitious and overpowered GBA/NDS audio driver ever developed.

Definitely not the fastest by a wide margin (there are way too many features implemented), but it is VERY heavily optimized, and fully written in assembly.

## Features

* Mono/stereo waveform support (with support for linear interpolation)
* Waveforms can be PCM8, PCM16 (NDS only), or ADPCM (all features supported for all formats)
  * ADPCM is supported on GBA as well, without limitations - resampling and interpolation are implemented
* Volume ramping (by slicing a mix chunk into 2^N pieces or less, as needed)
* Reverb processing (using series all-pass filters with feedback and low-pass filtering)
  * Not just a simple delay line; this is REAL, diffuse-sounding reverb (with crosstalk to improve stereo width).
  * Defined using parameters such as feedback, room density, etc.; filter taps are calculated at run-time.
  * Choice between "simple" processing (one lowpass filter per sample), and "fancy" processing (one low-pass filter per all-pass tap)
* All mixer loops are extremely optimized for GBA
  * NDS9 is mostly optimized for ADPCM playback, and NDS7 doesn't have much room for optimization.
  * GBA dynamically loads the mixer loops from ROM into IWRAM to reduce the memory footprint
* Sampling rate and number of voices is controlled at run-time
* Support for 2x oversampling (using linear interpolation) to reduce hardware aliasing artifacts
  * This is really only needed for GBA, where the usual playback rates are not nice multiples of 32768Hz.
* Support for resampling the final signal to 32768Hz (or any 2^n sampling rate) using linear (or 4-tap sinc) interpolation
  * This is only available on GBA, with the intent to replace hardware aliasing with interpolation aliasing instead.
  * The 4-tap sinc kernel is fairly useless for rates close to the target, and should only be used when resampling from <50% of the target.
* Music and waveforms can be loaded from disk/card using a simple IO interface (read/allocate/free)
* Music is defined using MML, combined with a DLS soundbank (SF2 and MIDI support planned... eventually)
* Extremely feature-rich (read as: overengineered) compiler, with many, many options regarding waveform conversions.

Additionally, the library can be very thoroughly configured with compile-time switches for different purposes/trade-offs:
* Enabling/disabling specific waveform formats
* Enabling/disabling resampling interpolation
  * Including forcibly enabling/disabling it, regardless of waveform options
* Enabling/disabling reverb
  * Including switches for "fancy" reverb processing and "fake" reverb (via a second release phase)
* Enabling/disabling a fixed ~60Hz update rate mode
  * The fixed ~60Hz rate allows more optimizations because more assumptions can be made
* Resolution of the key->frequency table, mixer fractional bits, reverb buffer bits
* Resolution of volume ramping subdivisions
* Loop unrolling

## MML Reference

The MML reference specs can be found [here](docs/MML.md). It may need to be expanded upon at a later stage.

## ... Why? Just why?

I'm developing this driver for a tech demo I'm working on. It's been a very wild, but fun ride, so far.

## Demos

Maybe eventually. [I'm uploading some ports I'm working on to YouTube, though](https://www.youtube.com/@Aikku93). Bear in mind that I've been uploading these as I work on the driver and my soundbank, so earlier videos may sound different than later ones.

## Authors
* **Ruben Nunez** - *Initial work* - [Aikku93](https://github.com/Aikku93)
