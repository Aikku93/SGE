/************************************************/
#pragma once
/************************************************/

//! Platform options
//! Any of these may be disabled
#define SGE_PLATFORM_HAVE_FILEDB
#define SGE_PLATFORM_HAVE_REVERB
//#define SGE_PLATFORM_HAVE_FANCY_REVERB
//#define SGE_PLATFORM_HAVE_FAKE_REVERB //! <- This only has an effect if SGE_PLATFORM_HAVE_REVERB is disabled

//! Compilation options
//! Notes:
//!  -NOTELUT_BITS defines the resolution between each semitone of the lookup
//!   table used to convert a note value into a frequency value. A value of 0
//!   is only exact to the semitone (up to rounding), with all fine-tuning
//!   being linearly interpolated between the semitones (0.775 cents RMSE).
//!   Increasing this value reduces the error by 75% per step - by introducing
//!   more values in between the semitones -, at the cost of doubling the size
//!   of the lookup table, but execution time remains constant.
//!   Possible values, and their resulting RMSE and lookup table size:
//!    0: 0.775 cents RMSE,  52 bytes
//!    1: 0.194 cents RMSE, 100 bytes
//!    2: 0.048 cents RMSE, 196 bytes
//!    3: 0.012 cents RMSE, 388 bytes
//!    4: 0.003 cents RMSE, 772 bytes
//!  -VARIABLE_SYNC_RATE will perform mixing in chunks of length specified
//!   during the driver initialization. This allows updates rates other than
//!   the native rate of ~60Hz, but adds a non-neglibile amount of CPU load
//!   due to needing to calculate envelope values dynamically, plus overhead
//!   from the update routines.
//!   This mode will make use of a reciprocals table, declared as:
//!    RecpLUT_t SGE_RecpLUT[256]
//!   where RecpLUT_t depends on RECPLUT_BITS:
//!    RECPLUT_BITS == 8:  uint8_t
//!    RECPLUT_BITS == 16: uint16_t
//!    RECPLUT_BITS == 32: uint32_t
//!   This is then paired with RECPLUT_PRECISION to define the lookup table.
//!   Note that for correct functioning, the entry corresponding to 1/x for
//!   x==1 must equal 2^PRECISION-1, or overflow may result.
//!   The library defines this table, but it is declared as weak so that it
//!   can be overriden by a user-defined reciprocals table.
//!  -BPM_FRACBITS controls the accuracy of the BPM counter for music. When
//!   the update rate is low enough, it is strongly recommended to set this
//!   to 0. However, with a high enough update rate, this can be set to a
//!   maximum value of 5. Note that high precision can cause 'skipping' in
//!   music output, which is why it should only be used with high update rates.
//!  -STEREO_WAVEFORMS enables global support for stereo waveforms. This will
//!   add a large amount of code in RAM, but allows direct mixing of stereo
//!   waveforms in optimized mixing loops instead of using separate voices.
//!  -USE_CURVED_ATTACK bends the attack curve to use a reverse-quadratic
//!   curve (that is: 1-(1-x)^2 = 2*x-x^2). This can sound more natural.
//!  -EG1_LOG2THRESHOLD defines the threshold below which a voice will be
//!   killed when in the Decay or Release phase.
//!  -LFO_RAMP_FREQ enables ramping of the LFO frequency over the delay time,
//!   and LFO_RAMP_AMP enables ramping of the LFO amplitude over the delay
//!   time. Note that these options only apply when a waveform articulation
//!   is using the ramping feature.
//!  -VOLBITS controls the precision of the mixing volumes. Note that this is
//!   NOT like the GBA control VOLFRACBITS; whereas VOLFRACBITS refers to the
//!   precision to preserve /after/ multiplying by the 1.7fxp volume, VOLBITS
//!   refers to the volume precision directly (ie. the volume precision during
//!   mixing is 1.VOLBITS fxp, eg. a value of 7 would give results identical
//!   to those of the GBA version for the purposes of volume scaling).
//!  -USE_VOLRAMP applies ramping to a voice's output volume. This adds extra
//!   CPU load, but can reduce crackle from sharp envelopes. Note that unless
//!   VOLBITS is very high (eg. 13 or 14), this option usually has no effect,
//!   as the delta values divided by the buffer length usually collapse to 0.
//!   Note that this option CANNOT be enabled alongside USE_VOLSUBDIV.
//!   NOTE: This option is only available when mixing on ARM9.
//!  -USE_VOLSUBDIV enables subdivision of the mixing chunk (up to a target
//!   volume level, or length) to avoid harsh volume transitions with larger
//!   mixing chunks. It will slightly increase CPU, but may greatly reduce
//!   envelope-related crackling. This option is used in conjunction with the
//!   VOLSUBDIV_MINLENGTH (smallest subdivision length) option, as well as the
//!   VOLSUBDIV_LOG2RATIO (largest allowed volume ratio per update, expressed
//!   as its base-2 logarithm; eg. 2 is 1/4 volume step ratio) option.
//!   Note that there is a balance to strike here, as subdividing too much may
//!   cause the volume delta to collapse to 0, which will obviously have very
//!   detrimental effects, so it is suggested to not drop the subdivision
//!   threshold too low.
//!  -USE_OVERSAMPLING uses linear interpolation to upsample the direct
//!   output signal to compensate for the sometimes imperfect sampling rates
//!   used on the NDS. This adds an extra CPU and RAM cost, but the audio
//!   will generally sound far less aliased, at the cost of some muffling;
//!   this can be compensated by using a high-shelf filter on the waveforms.
//!   It is generally recommended to just run this driver at 32728Hz with
//!   no oversampling, however.
//!  -USE_CLIPPING will clip the audio signal if the amplitude is exceeded.
//!   It is strongly recommended to use this, unless you carefully test the
//!   output of the mixer to ensure overflow does not happen.
//!   Note that any overflows will propagate into the reverb output.
//!  -PCMX_MONO_UNROLL and PCMX_STEREO_UNROLL control loop unrolling for the
//!   inner mixing loops. Increasing the unroll count can potentially lead
//!   to much better performance, but will add a lot of code in RAM.
//!   Any samples left after the unrolled block will be mixed in a
//!   one-sample-per-iteration loop.
#define SGE_SUPPORT_ADPCM       1 //! 0 = Disable ADPCM waveforms, 1 = Enable ADPCM waveforms
#define SGE_SUPPORT_PCM8        1 //! 0 = Disable PCM8 waveforms, 1 = Enable PCM8 waveforms
#define SGE_SUPPORT_PCM16       1 //! 0 = Disable PCM16 waveforms, 1 = Enable PCM16 waveforms
#define SGE_SUPPORT_LERP        1 //! 0 = Never interpolate, 1 = Allow interpolation to happen
#define SGE_ALWAYS_LERP         0 //! 0 = Interpolate according to waveform options, 1 = Always interpolate
#define SGE_NOTELUT_BITS        3 //! 0 = Minimum precision, 4 = Maximum precision
#define SGE_VARIABLE_SYNC_RATE  1 //! 0 = Fixed ~60Hz, 1 = Depend on BufLen
#define SGE_RECPLUT_BITS       16 //! Size of each entry in the table, in bits (8, 16, or 32)
#define SGE_RECPLUT_PRECISION  16 //! Precision of the reciprocals table (ie. 2^PRECISION/x)
#define SGE_BPM_FRACBITS        5 //! Fractional bits of tempo counter
#define SGE_STEREO_WAVEFORMS    1 //! 0 = Mono waveforms only, 1 = Enable stereo-sampled waveforms
#define SGE_USE_CURVED_ATTACK   1 //! 0 = Linear attack, 1 = Reverse-quadratic attack
#define SGE_EG1_LOG2THRESHOLD   8 //! Voice is killed when EG1 drops below 1.0*2^-n
#define SGE_LFO_RAMP_FREQ       1 //! 0 = Constant frequency, 1 = Ramp frequency from 0Hz over ramp time
#define SGE_LFO_RAMP_AMP        1 //! 0 = Constant amplitude, 1 = Ramp amplitude from 0.0 to 1.0 over ramp time
#define SGE_MIXER_VOLBITS      14 //! Volume bits to use in mix (maximum 14)
#define SGE_USE_VOLRAMP         0 //! 0 = No volume ramping, 1 = Apply volume ramping
#define SGE_USE_VOLSUBDIV       1 //! 0 = No chunk subdivision for volume, 1 = Subdivide mix chunk as needed
#define SGE_USE_OVERSAMPLING    0 //! 0 = Direct signal, 1 = Linearly interpolate to twice the sampling rate
#define SGE_USE_CLIPPING        1 //! 0 = Don't clip, 1 = Clip output signal
#define SGE_PCM8_MONO_UNROLL    4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 2)
#define SGE_PCM8_STEREO_UNROLL  4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 2)
#define SGE_PCM16_MONO_UNROLL   4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 2)
#define SGE_PCM16_STEREO_UNROLL 4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 2)
#define SGE_VOLSUBDIV_MINLENGTH 32 //! Suggested value: 32
#define SGE_VOLSUBDIV_LOG2RATIO  2 //! Suggested value: 2

//! Sanity checks
#if (SGE_ALWAYS_LERP && !SGE_SUPPORT_LERP)
# error "SGE_ALWAYS_LERP requires SGE_SUPPORT_LERP."
#endif
#if (SGE_NOTELUT_BITS < 0 || SGE_NOTELUT_BITS > 4)
# error "SGE_NOTELUT_BITS must be 0 ~ 4."
#endif
#if (SGE_RECPLUT_BITS != 8 && SGE_RECPLUT_BITS != 16 && SGE_RECPLUT_BITS != 32)
# error "SGE_RECPLUT_BITS must be 8, 16, or 32."
#endif
#if (SGE_RECPLUT_PRECISION < 0 || SGE_RECPLUT_PRECISION > 32)
# error "SGE_RECPLUT_PRECISION must be 0 ~ 32."
#endif
#if (SGE_BPM_FRACBITS < 0 || SGE_BPM_FRACBITS > 5)
# error "SGE_BPM_FRACBITS must be 0 ~ 5."
#endif
#if (SGE_EG1_LOG2THRESHOLD < 0 || SGE_EG1_LOG2THRESHOLD > SGE_MIXER_VOLBITS)
# error "SGE_EG1_LOG2THRESHOLD must be 0 ~ VOLBITS."
#endif
#if (SGE_MIXER_VOLBITS < 8 || SGE_MIXER_VOLBITS > 14)
# error "SGE_MIXER_VOLBITS must be 8 ~ 14."
#endif
#if SGE_USE_VOLSUBDIV
# if SGE_USE_VOLRAMP
#  error "SGE_USE_VOLSUBDIV cannot be used with SGE_USE_VOLRAMP."
# endif
# if (SGE_VOLSUBDIV_MINLENGTH < 1 || SGE_VOLSUBDIV_MINLENGTH > 256)
#  error "SGE_VOLSUBDIV_MINLENGTH must be 1~256."
# endif
#endif
#if (SGE_PCM8_MONO_UNROLL < 1 || SGE_PCM8_MONO_UNROLL > 16 || (SGE_PCM8_MONO_UNROLL != 1 && (SGE_PCM8_MONO_UNROLL%2) != 0))
# error "SGE_PCM8_MONO_UNROLL must be 1, or 2~16 in steps of 2."
#endif
#if (SGE_PCM8_STEREO_UNROLL < 1 || SGE_PCM8_STEREO_UNROLL > 16 || (SGE_PCM8_STEREO_UNROLL != 1 && (SGE_PCM8_STEREO_UNROLL%2) != 0))
# error "SGE_PCM8_STEREO_UNROLL must be 1, or 2~16 in steps of 2."
#endif
#if (SGE_PCM16_MONO_UNROLL < 1 || SGE_PCM16_MONO_UNROLL > 16 || (SGE_PCM16_MONO_UNROLL != 1 && (SGE_PCM16_MONO_UNROLL%2) != 0))
# error "SGE_PCM16_MONO_UNROLL must be 1, or 4~16 in steps of 2."
#endif
#if (SGE_PCM16_STEREO_UNROLL < 1 || SGE_PCM16_STEREO_UNROLL > 16 || (SGE_PCM16_STEREO_UNROLL != 1 && (SGE_PCM16_STEREO_UNROLL%2) != 0))
# error "SGE_PCM16_STEREO_UNROLL must be 1, or 2~16 in steps of 2."
#endif

/************************************************/
//! EOF
/************************************************/
