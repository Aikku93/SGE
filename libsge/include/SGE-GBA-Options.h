/************************************************/
#pragma once
/************************************************/

//! Platform options
//! Any of these may be disabled
//#define SGE_PLATFORM_HAVE_FILEDB //! <- This can be enabled, but there's not much point without a DLDI-like interface
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
//!  -EG1_LOG2THRESHOLD defines the threshold below which a voice will be
//!   killed when in the Decay or Release phase.
//!  -VOLFRACBITS refers to extra precision bits to preserve during mixing.
//!   These will be chopped during the final mix, but can alter the output.
//!   For example, a value of 4 will mix in 12bit precision before being
//!   chopped to 8bit during mixdown.
//!  -USE_VOLSUBDIV enables subdivision of the mixing chunk (up to a target
//!   volume level, or length) to avoid harsh volume transitions with larger
//!   mixing chunks. It will slightly increase CPU, but may greatly reduce
//!   envelope-related crackling. This option is used in conjunction with the
//!   VOLSUBDIV_MINLENGTH (smallest subdivision length) option, as well as the
//!   VOLSUBDIV_LOG2RATIO (largest allowed volume ratio per update, expressed
//!   as its base-2 logarithm; eg. 2 is 1/4 volume step ratio) option.
//!   Note: As a limitation of GBA mixing unsigned samples, subdivision is
//!   limited to a certain number of steps N, where N is specified via the
//!   VOLSUBDIV_LOG2MAXSUBDIV option (eg. 0 = No subdivision, 1 = 1/2, etc.),
//!   and the buffer sizes must be divisible by the maximum subdivision.
//!  -FAST_INTERPOLATE uses a faster interpolation method where available,
//!   at the cost of reduced quality output. This may be either from a
//!   reduction in the sample bit-depth, interpolation accuracy, or both.
//!   Note that not all mixing cores support this option.
//!  -USE_OVERSAMPLING uses linear interpolation to upsample the direct
//!   output signal to compensate for the typically imperfect sampling rates
//!   used on the GBA. This adds an extra CPU and RAM cost, but the audio
//!   will generally sound far less aliased, at the cost of some muffling;
//!   this can be compensated by using a high-shelf filter on the waveforms.
//!  -USE_CLIPPING will clip the audio signal if the amplitude is exceeded.
//!   It is strongly recommended to use this, unless you carefully test the
//!   output of the mixer to ensure overflow does not happen.
//!   Note that any overflows will propagate into the reverb output.
//!  -PCM8_MONO_UNROLL and PCM8_STEREO_UNROLL control loop unrolling for the
//!   inner mixing loops. Increasing the unroll count can potentially lead
//!   to much better performance, but will add a lot of code in RAM.
//!   Any samples left after the unrolled block will be mixed in a
//!   one-sample-per-iteration loop.
#define SGE_SELFMANAGED_HW      1 //! 0 = User manages sound hardware, 1 = Driver manages hardware
#define SGE_SUPPORT_ADPCM       1 //! 0 = Disable ADPCM waveforms, 1 = Enable ADPCM waveforms
#define SGE_SUPPORT_PCM8        1 //! 0 = Disable PCM8 waveforms, 1 = Enable PCM8 waveforms
#define SGE_SUPPORT_PCM16       0 //! 0 = Disable PCM16 waveforms. PCM16 is NOT available on GBA.
#define SGE_SUPPORT_LERP        1 //! 0 = Never interpolate, 1 = Allow interpolation to happen
#define SGE_ALWAYS_LERP         0 //! 0 = Interpolate according to waveform options, 1 = Always interpolate
#define SGE_HWTIMER_IDX         0 //! Hardware timer to use for audio output (must be 0 or 1)
#define SGE_NOTELUT_BITS        3 //! 0 = Minimum precision, 4 = Maximum precision
#define SGE_VARIABLE_SYNC_RATE  0 //! 0 = Fixed ~60Hz, 1 = Depend on BufLen
#define SGE_RECPLUT_BITS       16 //! Size of each entry in the table, in bits (8, 16, or 32)
#define SGE_RECPLUT_PRECISION  15 //! Precision of the reciprocals table (ie. 2^PRECISION/x)
#define SGE_BPM_FRACBITS        5 //! Fractional bits of tempo counter
#define SGE_PRECISE_KEYON       1 //! 0 = Snap key-on to mix chunks, 1 = Align key-on to samples
#define SGE_STEREO_WAVEFORMS    1 //! 0 = Mono waveforms only, 1 = Enable stereo-sampled waveforms
#define SGE_EG1_LOG2THRESHOLD   6 //! Voice is killed when EG1 drops below 1.0*2^-n (maximum 16)
#define SGE_MIXER_VOLBITS       7 //! Volume bits to use in mix (fixed 7 in GBA driver)
#define SGE_MIXER_VOLFRACBITS   3 //! Volume bits to preserve in mix (maximum 7) - NOTE: Max voices = 2^(7-x)
#define SGE_USE_VOLSUBDIV       1 //! 0 = No chunk subdivision for volume, 1 = Subdivide mix chunk as needed
#define SGE_FAST_INTERPOLATE    1 //! 0 = Full-precision interpolation, 1 = Reduced precision interpolation
#define SGE_USE_OVERSAMPLING    1 //! 0 = Direct signal, 1 = Linearly interpolate to twice the sampling rate
#define SGE_USE_CLIPPING        1 //! 0 = Don't clip, 1 = Clip output signal
#define SGE_PCM8_MONO_UNROLL    4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 4)
#define SGE_PCM8_STEREO_UNROLL  4 //! 1 = No unroll, M = Unroll M iterations (must be a multiple of 2)
#define SGE_VOLSUBDIV_MINLENGTH    32 //! Suggested value: 32
#define SGE_VOLSUBDIV_LOG2RATIO     2 //! Suggested value: 2
#define SGE_VOLSUBDIV_LOG2MAXSUBDIV 3 //! Suggested value: 2 or 3

//! Reverb-specific compilation options
//! Notes:
//!  -ALLPASS_BITS controls the accuracy of the all-pass filter buffers.
//!   When this is > 8, the all-pass filter buffers will use 16-bit data
//!   buffers, which will double the memory consumption, but will result
//!   in much cleaner audio, with reduced hiss/noise in the reverb tail.
//!   Using "fancy" reverb with any value greater than 8 adds a 4c/sample
//!   penalty during final mixdown to apply scaling and rounding. Using a
//!   value of 7 will add a lot of hiss/noise to the output, but should
//!   minimize the need to apply clipping to the all-pass output and avoid
//!   most 'crackly' artifacts resulting from clipping.
//!  -CLIP_ALLPASS will clip the intermediate output of the reverb all-pass
//!   filters to ensure no overflow. The intermediate is formed as:
//!    u[t] = x[t] +/- u[t-N]
//!   which may cause overflows if left unchecked. However, at intermediate
//!   output levels, this is rarely necessary, and can easily be left out.
//!   This option only has any effect when ALLPASS_BITS is <= 8; when using
//!   high-precision buffers, clipping becomes impossible, and so the number
//!   of bits in ALLPASS_BITS is limited to 14 at most to avoid the need to
//!   apply any clipping at all.
#define SGE_ALLPASS_BITS 14 //! 7 = Minimum precision, 14 = Maximum precision (see notes)
#define SGE_CLIP_ALLPASS  0 //! 0 = Don't clip reverb all-pass output, 1 = Clip output

//! Sanity checks
#if SGE_SUPPORT_PCM16
# error "SGE_SUPPORT_PCM16 must be 0 (PCM16 is unsupported)."
#endif
#if (SGE_ALWAYS_LERP && !SGE_SUPPORT_LERP)
# error "SGE_ALWAYS_LERP requires SGE_SUPPORT_LERP."
#endif
#if (SGE_HWTIMER_IDX < 0 || SGE_HWTIMER_IDX > 1)
# error "SGE_HWTIMER_IDX must be 0 ~ 1."
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
#if (SGE_EG1_LOG2THRESHOLD < 0 || SGE_EG1_LOG2THRESHOLD > (7+SGE_MIXER_VOLFRACBITS))
# error "SGE_EG1_LOG2THRESHOLD must be 0 ~ 7+VOLFRACBITS."
#endif
#if (SGE_MIXER_VOLBITS != 7)
# error "SGE_MIXER_VOLBITS must be equal to 7."
#endif
#if (SGE_MIXER_VOLFRACBITS < 0 || SGE_MIXER_VOLFRACBITS > 7)
# error "SGE_MIXER_VOLFRACBITS must be 0 ~ 7."
#endif
#if SGE_USE_VOLSUBDIV
# if (SGE_VOLSUBDIV_MINLENGTH < 1 || SGE_VOLSUBDIV_MINLENGTH > 256)
#  error "SGE_VOLSUBDIV_MINLENGTH must be 1~256."
# endif
# if (SGE_VOLSUBDIV_LOG2MAXSUBDIV < 1)
#  error "SGE_VOLSUBDIV_LOG2MAXSUBDIV must be >= 1; disable SGE_USE_VOLSUBDIV if this is intended."
# endif
# if (SGE_VOLSUBDIV_LOG2MAXSUBDIV > 4)
#  error "SGE_VOLSUBDIV_LOG2MAXSUBDIV must be <= 4."
# endif
#endif
#if (SGE_PCM8_MONO_UNROLL < 1 || SGE_PCM8_MONO_UNROLL > 16 || (SGE_PCM8_MONO_UNROLL != 1 && (SGE_PCM8_MONO_UNROLL%4) != 0))
# error "SGE_PCM8_MONO_UNROLL must be 1, or 4~16 in steps of 4."
#endif
#if (SGE_PCM8_STEREO_UNROLL < 1 || SGE_PCM8_STEREO_UNROLL > 16 || (SGE_PCM8_STEREO_UNROLL != 1 && (SGE_PCM8_STEREO_UNROLL%2) != 0))
# error "SGE_PCM8_STEREO_UNROLL must be 1, or 2~16 in steps of 2."
#endif
#if (SGE_ALLPASS_BITS < 7 || SGE_ALLPASS_BITS > 14)
# error "SGE_ALLPASS_BITS must be 7 ~ 14."
#endif

/************************************************/
//! EOF
/************************************************/
