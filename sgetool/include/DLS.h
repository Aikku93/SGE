/************************************************/
#pragma once
/************************************************/
#include <stdint.h>
#include <stdio.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

//! Error codes returned by DLS_Read()
#define DLS_ERROR_NONE          ( 0)
#define DLS_ERROR_INVALID       (-1)
#define DLS_ERROR_OUT_OF_MEMORY (-2)
#define DLS_ERROR_IO            (-3)
#define DLS_ERROR_UNSUPPORTED   (-4)

/************************************************/

//! Waveform control structure
//! Derived from wsmp chunk
struct DLS_WaveformCtrl_t {
	uint16_t Root;    //! Root key (60 = Middle-C)
	 int16_t Tune;    //! Fine tuning (in cents)
	uint32_t LoopBeg; //! Loop start point (in samples)
	uint32_t LoopLen; //! Loop length (in samples)
	double   Gain;    //! Waveform gain
};

//! Waveform structure
//! NOTE: Waveform data is NOT loaded into memory upon opening a DLS file.
//! Instead, use DLS_Waveform_LoadData() and DLS_Waveform_UnloadData(), which
//! will load the data, and free it, respectively, updating the Data member.
struct DLS_Waveform_t {
	//! Format specification
	uint16_t Format;     //! wFormatTag from Mmreg.h (eg. WAVE_FORMAT_PCM)
	uint16_t nChan;      //! Number of channels
	uint32_t Rate;       //! Sampling rate
	uint32_t Size;       //! Size in BYTES of the raw data
	uint32_t BitDepth;   //! Bits per sample
	uint32_t wvplOffset; //! Offset of waveform in the wvpl chunk
	uint32_t FileDataOffs; //! Offset of the data in the file
	char *Name;
	char *Comment;
	struct DLS_WaveformCtrl_t wCtrl;
};

//! Articulation structure
//! NOTE: This doesn't represent all available connections, but only what we use.
//! NOTE: LFO1 = ModLFO, LFO2 = VibLFO
struct DLS_Articulation_t {
	double LFO1Freq;    //! Hz
	double LFO1Delay;   //! Seconds
	double LFO1ToGain;  //! Normalized units (bipolar)
	double LFO1ToPitch; //! Semitones (bipolar)
	double LFO2Freq;    //! Hz
	double LFO2Delay;   //! Seconds
	double LFO2ToPitch; //! Semitones (bipolar)
	double EG2ToPitch;  //! Semitones (bipolar)
	double MasterTune;  //! Semitones (bipolar)
	double MasterPan;   //! Normalized units (bipolar)
	double PanWidth;    //! Normalized units (bipolar)
	struct {
		double A, H, D, S, R; //! Seconds (except for Sustain: normalized units (Gain))
	} EG1, EG2;
};

//! Instrument region structure
struct DLS_Region_t {
	uint16_t KeyLo, KeyHi;
	uint16_t VelLo, VelHi;
	uint32_t CueTblIdx;
	uint32_t WaveformIdx;
	struct DLS_Waveform_t *Waveform;

	//! Local articulation
	//! ArtLv == 0: Art was copied from a higher level. Otherwise, corresponds to lart/lar2 type.
	uint32_t ArtLv;
	struct DLS_Articulation_t Art;

	//! Local waveform control
	//! UseLocalWavCtrl == 0: WavCtrl was copied from Waveform->wCtrl.
	uint32_t UseLocalWavCtrl;
	struct DLS_WaveformCtrl_t WavCtrl;
};

//! Instrument layer structure
//! NOTE: These are automatically generated from regions, and are just a local construct;
//! there is no such thing as "layers" in the DLS specification, only "hints".
struct DLS_Layer_t {
	uint16_t VelLo, VelHi;
	uint32_t nRegions;
	struct DLS_Region_t *Regions;

	//! Local articulation
	//! ArtLv == 0: Parent articulation
	uint32_t ArtLv;
	struct DLS_Articulation_t Art;
};

//! Instrument structure
struct DLS_Instrument_t {
	uint8_t  Patch;
	uint8_t  CC0;
	uint8_t  CC32;
	uint8_t  DrumKit;
	uint32_t nLayers;
	struct DLS_Layer_t *Layers;

	//! Global articulation
	//! ArtLv == 0: Default articulation
	uint32_t ArtLv;
	struct DLS_Articulation_t Art;
};

//! DLS soundbank structure
struct DLS_t {
	//! Waveforms and instruments
	uint32_t nWaveforms;
	uint32_t nInstruments;
	struct DLS_Waveform_t   *Waveforms;
	struct DLS_Instrument_t *Instruments;

	//! Internal data
	uint32_t *ptbl;
	uint32_t wvplOffs; //! File offset of the wvpl chunk
};

/************************************************/

//! Read DLS file
//! On failure, the structure is destroyed before returning.
int DLS_Read(struct DLS_t *DLS, FILE *DLSFile);

//! Destroy soundbank
void DLS_Destroy(struct DLS_t *DLS);

//! Convert error code to error string
const char *DLS_ErrorCodeToString(int Code);

/************************************************/

//! Get instrument from MIDI program
struct DLS_Instrument_t *DLS_InstrumentFromMIDIProgram(uint8_t Patch, uint8_t CC0, uint8_t CC32, uint8_t DrumKit);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
