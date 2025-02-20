/************************************************/
#pragma once
/************************************************/
#include <stdint.h>
#include <stdio.h>
/************************************************/
#define SGE_DECLSPEC
#define SGE_PLATFORM_IS_COMPILER
#ifdef __LP64__
# define SGE_PLATFORM_IS_64BIT
#endif
#include "SGE.h"
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

//! Maximum waveforms allowed in database
//! This limit is due to SGE_ToneRegion_t::Wave being 11-bit.
#define SGE_DB_MAX_WAVEFORMS 2048

//! Maximum songs allowed in database
//! This limit is due to SGE_DbHeader_t::nSong being 16-bit.
#define SGE_DB_MAX_SONGS 65535

/************************************************/

//! SGE_gOptions_t::WavFormat
//! This can also use the SGE_WAV_FRMT_x definitions in SGE.h
#define SGE_OPT_WAVFORMAT_DEFAULT SGE_WAV_FRMT_CNT

//! SGE_gOptions_t::WavInterpolate
//! This can also use the SGE_WAV_INTERPOLATE_x definitions in SGE.h
#define SGE_OPT_WAVINTERPOLATE_OFF    0
#define SGE_OPT_WAVINTERPOLATE_ON     1
#define SGE_OPT_WAVINTERPOLATE_RATELT 2
#define SGE_OPT_WAVINTERPOLATE_RATEGT 3

//! SGE_gOptions_t::WavFormatOverride
#define SGE_OPT_WAVFORMAT_OVERRIDE_CMDLINE 0
#define SGE_OPT_WAVFORMAT_OVERRIDE_ALL     1

//! SGE_gOptions_t::WavResampleCondition
#define SGE_OPT_WAVRESAMPCOND_NEVER  0
#define SGE_OPT_WAVRESAMPCOND_ALWAYS 1
#define SGE_OPT_WAVRESAMPCOND_GT     2
#define SGE_OPT_WAVRESAMPCOND_LT     3

//! SGE_gOptions_t::WavMonoConvWindowType
#define SGE_OPT_MONOCONV_WINDOW_SINE     0
#define SGE_OPT_MONOCONV_WINDOW_HANN     1
#define SGE_OPT_MONOCONV_WINDOW_HAMMING  2
#define SGE_OPT_MONOCONV_WINDOW_BLACKMAN 3
#define SGE_OPT_MONOCONV_WINDOW_NUTTALL  4

//! SGE_gOptions_t::WavResampleRate
#define SGE_OPT_WAVRESAMPLE_NONE       0
#define SGE_OPT_WAVRESAMPLE_MINRATE    5000
#define SGE_OPT_WAVRESAMPLE_MAXRATE    384000

//! SGE_gOptions_t::SRCAlign
#define SGE_OPT_SRCALIGN_ANY           0
#define SGE_OPT_SRCALIGN_LOOPS         1
#define SGE_OPT_SRCALIGN_LOOPS_NONPERC 2

//! SGE_gOptions_t::SRCRound
#define SGE_OPT_SRCROUND_DOWN          0
#define SGE_OPT_SRCROUND_MIDDLE        1
#define SGE_OPT_SRCROUND_UP            2

//! SGE_gOptions_t::SRCWindow
#define SGE_OPT_SRCWINDOW_NONE     0
#define SGE_OPT_SRCWINDOW_SINE     1
#define SGE_OPT_SRCWINDOW_HANN     2
#define SGE_OPT_SRCWINDOW_HAMMING  3
#define SGE_OPT_SRCWINDOW_BLACKMAN 4
#define SGE_OPT_SRCWINDOW_NUTTALL  5
#define SGE_OPT_SRCWINDOW_LANCZOS  6
#define SGE_OPT_SRCWINDOW_LANCZOS2 7

//! SGE_LocalDb_Export()
#define SGE_LOCALDB_ERROR_NONE               ( 0)
#define SGE_LOCALDB_ERROR_OUT_OF_MEMORY      (-1)
#define SGE_LOCALDB_ERROR_TOO_MANY_WAVEFORMS (-2)
#define SGE_LOCALDB_ERROR_TOO_MANY_SONGS     (-3)
#define SGE_LOCALDB_ERROR_IO                 (-4)
#define SGE_LOCALDB_ERROR_UNKNOWN            (-5)
#define SGE_LOCALDB_ERROR_NEED_RESAMPLING    (-6)
#define SGE_LOCALDB_ERROR_PARAM_RANGE        (-7)
#define SGE_LOCALDB_ERROR_WRITE_WAVEDATA     (-8)
#define SGE_LOCALDB_ERROR_INITMML            (-9)
#define SGE_LOCALDB_ERROR_MML                (-10) //! <- Must display error from MML structure

/************************************************/

//! SGE_Wav_t::Frmt
#define SGE_WAV_FRMT_PCM24   (SGE_WAV_FRMT_CNT+1) //! <- These are only used internally
#define SGE_WAV_FRMT_PCM32   (SGE_WAV_FRMT_CNT+2)
#define SGE_WAV_FRMT_FLOAT32 (SGE_WAV_FRMT_CNT+3)

/************************************************/

//! Global options
struct SGE_gOptions_t {
	uint8_t  WavFormat;
	uint8_t  WavInterpolate;
	uint8_t  WavResampleCondition;
	uint8_t  WavEnableCull;
	uint8_t  WavForceMono;
	uint8_t  SRCAlign;
	uint8_t  SRCRound;
	uint8_t  SRCHalfOrder;
	uint8_t  SRCWindow;
	uint8_t  UseGlobalToneBank;
	uint8_t  ToneEG1ParabolicAttack;
	uint8_t  ToneLFOAmpRamp;
	uint8_t  ToneLFOFreqRamp;
	uint8_t  ToneLFOShape;
	uint16_t WavMonoConvWindowSize;
	uint16_t WavMonoConvWindowType;
	uint16_t WavMonoConvHops;
	uint32_t WavResampleRate;
	 int32_t WavMinLoopSize; //! Negative value = ms, Positive value = Samples
	double   WavOversampleRate;
	double   WavTransposeRate;
	double   WavLowpassCutoff;
	double   WavHighShelfGain;
	double   WavGlobalGain;
	double   WavGainAdjust;
	double   SRCDitherLevel;
	double   SRCNoiseShapeLevel;
};

/************************************************/

//! Local waveform structure
struct SGE_LocalWav_t {
	uint32_t Referenced:1;   //! Sample is referenced by a used tone
	uint32_t IsPercussive:1; //! Sample is used by a drum kit
	uint32_t IsSorted:1;     //! Waveform has been sorted during export
	uint32_t FileOffs;       //! Offset of sample data in original file
	uint32_t FileSize;       //! Size of data in file
	struct SGE_gOptions_t Options;
	struct SGE_Wav_t Header;
};

//! Local tone region structure
struct SGE_LocalTone_Region_t {
	uint8_t  KeyLo;
	uint8_t  KeyHi;
	uint16_t WaveIdx;
	uint8_t  Referenced;
	struct SGE_WavArt_t Art;
};

//! Local tone layer structure
struct SGE_LocalTone_Layer_t {
	uint8_t VelLo;
	uint8_t VelHi;
	uint8_t nReg;
	struct SGE_LocalTone_Region_t *Regions;
};

//! Local tone structure
struct SGE_LocalTone_t {
	uint8_t  Patch;
	uint8_t  CC0;
	uint8_t  CC32;
	uint8_t  DrumKit;
	uint8_t  nLayers;
	uint16_t GlobalPatchIdx; //! FFFFh = Unreferenced in global tone bank
	struct SGE_LocalTone_Layer_t *Layers;
};

//! Local song structure
/*!
    NOTE: Songs create an entirely new copy of an instrument as they
    use it. This is so that we can cull any unused regions from the
    instrument, reducing memory usage when loading them.
!*/
struct SGE_LocalSong_t {
	uint32_t nTracks;
	uint32_t TrackDataSize;
	uint32_t nTones;
	uint32_t dbOffs;
	uint32_t dbSize;
	uint8_t *TrackData;
	struct SGE_LocalTone_t *Tones;
};

/************************************************/

//! Local database structure
struct SGE_LocalDb_t {
	uint32_t nWaves;
	uint32_t nTones;
	uint32_t nSongs;
	uint32_t nGlobalTones;
	struct SGE_LocalWav_t  *Waves;
	struct SGE_LocalTone_t *Tones;
	struct SGE_LocalSong_t *Songs;
	uint16_t *WavRemapTable;
	uint32_t *ToneRemapTable;
};

/************************************************/

//! Read raw 32bit integer
//! Returns 0 on failure, 1 on success.
int SGE_ReadUInt32(uint32_t *Target, const char *Str, const char **StrEnd);

//! Read raw double float
//! Returns 0 on failure, 1 on success.
int SGE_ReadDouble(double *Target, const char *Str, const char **StrEnd);

//! Read gain in linear form, or dB form
//! Returns 0 on failure, 1 on success.
int SGE_ReadGain(double *Target, const char *Str, const char **StrEnd);

//! Read relative key value as a ratio, semitones, or cents
//! Returns 0 on failure, 1 on success.
int SGE_ReadRelativeKey(double *Target, const char *Str, const char **StrEnd);

/************************************************/

//! Parse options from strings/arguments
//! Will return:
//!  < 0: Errors occurred while parsing options.
//!  = 0: No options passed to function (OptArgc == 0 or OptArgv[0] == NULL or no SGE tag found).
//!  > 0: Number of options parsed, all of which succeeded.
//! Note that parsing will continue even after any errors.
//! If passing extra options, terminate the list by setting OptStr = NULL.
typedef void (*SGE_ParseOptions_ErrorLogger_t)(void *Userdata, const char *Format, ...);
struct SGE_ParseOptions_ExtraOpt_t {
	const char *OptStr;
	int (*OptFunc)(
		const char *OptArg,
		void *Userdata,
		struct SGE_gOptions_t *Options,
		SGE_ParseOptions_ErrorLogger_t ErrorLogger,
		void *ErrorLogger_Userdata
	);
};
int SGE_ParseOptions(
	struct SGE_gOptions_t *Options,
	const char **OptArgv,
	uint32_t OptArgc,
	uint8_t  UseBlockTag,
	SGE_ParseOptions_ErrorLogger_t ErrorLogger,
	void *ErrorLogger_Userdata,
	const struct SGE_ParseOptions_ExtraOpt_t *ExtraOpt,
	void *ExtraOpt_Userdata
);

/************************************************/

//! Initialize local database
void SGE_LocalDb_Init(struct SGE_LocalDb_t *Db);

//! Destroy local database
void SGE_LocalDb_Destroy(struct SGE_LocalDb_t *Db);

//! Export local database to final file
int SGE_LocalDb_Export(struct SGE_LocalDb_t *Db, FILE *SGEFile, FILE *WavFile, const struct SGE_gOptions_t *Options);
const char *SGE_LocalDb_Export_ErrorCodeToString(int ErrorCode);

/************************************************/

//! Load MML song into local database
struct MML_t;
int SGE_LocalDb_LoadMML(struct SGE_LocalDb_t *Db, FILE *SongFile, struct MML_t *MML, const struct SGE_gOptions_t *Options);

/************************************************/

//! Generate sound bank for local database from DLS file
//! Returns DLS_ERROR_* codes (eg. DLS_ERROR_NONE on success).
//! On failure, the database data is destroyed before returning.
int SGE_LocalDb_SoundBankFromDLS(struct SGE_LocalDb_t *Db, FILE *DLSFile, const struct SGE_gOptions_t *Options);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
