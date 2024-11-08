/************************************************/
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
/************************************************/
#include "DLS.h"
#include "SGE-Compiler.h"
#include "GlobalHelpers.h"
/************************************************/

static void WaveformOptionsErrorLogger(void *Userdata, const char *Format, ...) {
	const struct DLS_Waveform_t *SrcWav = (const struct DLS_Waveform_t*)Userdata;
	va_list vl; va_start(vl, Format);
	printf("\nWhile parsing waveform `%s`:\n ", SrcWav->Name);
	vprintf(Format, vl);
	va_end(vl);
}

/************************************************/

//! Adjust root key so that tuning is always a positive value,
//! and convert the tuning units from cents into .8fxp semitones.
//! Additionally ensure that Rate is valid.
static void Fix_Rate_RootKey_Tune(uint32_t *Rate_, int *Root_, int *Tune_) {
	uint32_t Rate = *Rate_;
	int      Root = *Root_;
	int      Tune = *Tune_;

	//! First, drop octaves while Rate is too high
	while(Rate >= (1u<<24)) Rate = Rate/2 + (Rate%2), Root -= 12;

	//! Next, fix up tuning, and clamp everything
	while(Tune <    0) Root += 1, Tune += 100;
	while(Tune >= 100) Root -= 1, Tune -= 100;
	Tune = (Tune * 256 + 50) / 100u;
	Root = CLAMP(Root, 0, 0xFF);
	Tune = CLAMP(Tune, 0, 0xFF);

	//! Write out final values
	*Rate_ = Rate;
	*Root_ = Root;
	*Tune_ = Tune;
}

//! Convert raw gain value into a waveform gain value
static uint8_t FixWaveformGain(double Gain) {
	Gain = lrint(Gain * (double)(1 << 7)); //! Gain is 1.7fxp
	return (uint8_t)fmin(fmax(Gain, 1.0), 255.0);
}

//! Convert seconds into companded format
static uint8_t ConvertSecondsToTimeFormat(double x, double MaxSeconds) {
	return lrint(sqrt(fmin(fmax(x, 0.0), MaxSeconds) * 1626));
}

/************************************************/

//! Translate articulation and wave control into local articulation
static void TranslateArticulation(
	struct SGE_WavArt_t *NewArt,
	const struct DLS_Articulation_t *Art,
	const struct DLS_WaveformCtrl_t *wCtrl_Region,
	const struct DLS_WaveformCtrl_t *wCtrl_Waveform
) {
	//! Volume must be made relative to waveform gain
	{
		double VolAdj = wCtrl_Region->Gain / wCtrl_Waveform->Gain;
		       VolAdj = lrint(VolAdj * 256.0) - 1.0;
		NewArt->Vol = (uint8_t)fmin(fmax(round(VolAdj), 0.0), 255.0);
	}

	//! Tuning must be made relative to waveform root key
	{
		double WavRoot = wCtrl_Waveform->Root - wCtrl_Waveform->Tune/100.0;
		double RegRoot = wCtrl_Region  ->Root - wCtrl_Region  ->Tune/100.0;
		double Tune = lrint((Art->MasterTune + WavRoot - RegRoot) * 256.0);
		NewArt->Tune = (int16_t)fmin(fmax(round(Tune), (double)-0x7FFF), (double)+0x7FFF);
	}

	//! Because we only have one LFO, we must combine the two
	{
		//! These weights are fairly arbitrary
		double w1 = fabs(Art->LFO1ToPitch) + fabs(Art->LFO1ToGain);
		double w2 = fabs(Art->LFO2ToPitch);
		double InvWeight = w1 + w2; if(InvWeight != 0.0) InvWeight = 1.0 / InvWeight;
		NewArt->LFOToKey = (int16_t)fmin(fmax(lrint((Art->LFO1ToPitch*w1 + Art->LFO2ToPitch*w2)*InvWeight * 256.0), (double)-0x7FFF), (double)+0x7FFF);
		NewArt->LFOToVol = (uint8_t)fmin(fmax(lrint(Art->LFO1ToGain * 255.0), 0.0), 255.0);
		NewArt->LFOToPan = 0;
		NewArt->LFORate  = (uint8_t)fmin(fmax(lrint((Art->LFO1Freq*w1 + Art->LFO2Freq*w2)*InvWeight * 16.0), 0.0), 255.0);
		NewArt->LFORamp  = 1;
		NewArt->LFODelay = ConvertSecondsToTimeFormat((Art->LFO1Delay*w1 + Art->LFO2Delay*w2)*InvWeight, 9.9);
	}

	//! Set up everything else
	//! FIXME: PanToVol+PanWidth with stereo linkage
	NewArt->Pan      = (int8_t)lrint(Art->MasterPan * 126.0);
	NewArt->PanWidth = (int8_t)fmin(fmax(lrint(fabs(Art->PanWidth) * 128.0), 0), 128.0);
	NewArt->PanToVol = 0;
	NewArt->EG2ToKey = (int8_t)fmin(fmax(lrint(Art->EG2ToPitch * 16.0), -128.0), +127.0);
	NewArt->EG2ToPan = 0;
	NewArt->EG1.Attack  = ConvertSecondsToTimeFormat(Art->EG1.A, 40.0);
	NewArt->EG1.Hold    = ConvertSecondsToTimeFormat(Art->EG1.H, 40.0);
	NewArt->EG1.Decay   = ConvertSecondsToTimeFormat(Art->EG1.D, 40.0);
	NewArt->EG1.Sustain = (uint8_t)fmin(fmax(lrint(Art->EG1.S * 255.0), 0.0), 255.0);
	NewArt->EG1.Release = ConvertSecondsToTimeFormat(Art->EG1.R, 40.0);
	NewArt->EG2.Attack  = ConvertSecondsToTimeFormat(Art->EG2.A, 40.0);
	NewArt->EG2.Hold    = ConvertSecondsToTimeFormat(Art->EG2.H, 40.0);
	NewArt->EG2.Decay   = ConvertSecondsToTimeFormat(Art->EG2.D, 40.0);
	NewArt->EG2.Sustain = (uint8_t)fmin(fmax(lrint(Art->EG2.S * 255.0), 0.0), 255.0);
	NewArt->EG2.Release = ConvertSecondsToTimeFormat(Art->EG2.R, 40.0);
}

/************************************************/

//! Append waveform to database
static int AppendWaveform(struct SGE_LocalDb_t *Db, const struct SGE_LocalWav_t *WavEntry) {
	//! First, enlarge the array
	uint32_t nWaves = Db->nWaves;
	struct SGE_LocalWav_t *NewWaveTbl = realloc(Db->Waves, (nWaves+1) * sizeof(struct SGE_LocalWav_t));
	if(!NewWaveTbl) return 0;
	Db->Waves  = NewWaveTbl;
	Db->nWaves = nWaves+1;

	//! Write new waveform
	memcpy(&NewWaveTbl[nWaves], WavEntry, sizeof(struct SGE_LocalWav_t));
	return 1;
}

//! Append instrument to database
static int AppendInstrument(struct SGE_LocalDb_t *Db, const struct DLS_Instrument_t *Instrument) {
	//! First, enlarge the array
	uint32_t nTones = Db->nTones;
	struct SGE_LocalTone_t *NewTonesTbl = realloc(Db->Tones, (nTones+1)*sizeof(struct SGE_LocalTone_t));
	if(!NewTonesTbl) return 0;
	Db->Tones  = NewTonesTbl;
	Db->nTones = nTones+1;

	//! Fill in instrument information
	struct SGE_LocalTone_t *NewTone = &NewTonesTbl[nTones];
	NewTone->Patch   = Instrument->Patch;
	NewTone->CC0     = Instrument->CC0;
	NewTone->CC32    = Instrument->CC32;
	NewTone->DrumKit = Instrument->DrumKit;
	NewTone->nLayers = Instrument->nLayers;
	NewTone->Layers  = (struct SGE_LocalTone_Layer_t*)malloc(Instrument->nLayers * sizeof(struct SGE_LocalTone_Layer_t));
	if(!NewTone->Layers) {
		NewTone->nLayers = 0; //! <- Compatibility with LocalDb_Destroy()
		return 0;
	}

	//! Create layers
	uint32_t LayerIdx;
	for(LayerIdx=0;LayerIdx<Instrument->nLayers;LayerIdx++) {
		struct SGE_LocalTone_Layer_t *NewLayer = &NewTone->Layers[LayerIdx];
		const struct DLS_Layer_t *Layer = &Instrument->Layers[LayerIdx];
		NewLayer->VelLo = Layer->VelLo + 1; //! MIDI is 0..127, SGE is 1..128
		NewLayer->VelHi = Layer->VelHi + 1;
		NewLayer->nReg  = Layer->nRegions;
		NewLayer->Regions = (struct SGE_LocalTone_Region_t*)malloc(Layer->nRegions * sizeof(struct SGE_LocalTone_Region_t));
		if(!NewLayer->Regions) {
			NewLayer->nReg   = 0;        //! <- Compatibility with LocalDb_Destroy()
			NewTone->nLayers = LayerIdx; //! <- Compatibility with LocalDb_Destroy()
			return 0;
		}

		//! Create regions
		uint32_t RegionIdx;
		for(RegionIdx=0;RegionIdx<Layer->nRegions;RegionIdx++) {
			struct SGE_LocalTone_Region_t *NewRegion = &NewLayer->Regions[RegionIdx];
			const struct DLS_Region_t *Region = &Layer->Regions[RegionIdx];
			NewRegion->KeyLo   = Region->KeyLo;
			NewRegion->KeyHi   = Region->KeyHi;
			NewRegion->WaveIdx = Region->WaveformIdx;
			NewRegion->Referenced = 0;
			TranslateArticulation(&NewRegion->Art, &Region->Art, &Region->WavCtrl, &Region->Waveform->wCtrl);

			//! If the referenced waveform was part of a drum kit, mark as percussive
			if(Instrument->DrumKit) Db->Waves[Region->WaveformIdx].IsPercussive = 1;
		}
	}

	//! All done
	return 1;
}

/************************************************/

//! Generate sound bank for local database from DLS file
int SGE_LocalDb_SoundBankFromDLS(struct SGE_LocalDb_t *Db, FILE *DLSFile, const struct SGE_gOptions_t *Options) {
	int ReturnValue;
	struct DLS_t DLS;

	//! Try to parse file
	ReturnValue = DLS_Read(&DLS, DLSFile);
	if(ReturnValue != DLS_ERROR_NONE) goto SoundBankFromDLS_Exit;

	//! Check waveform count
	if(DLS.nWaveforms > 65535) return DLS_ERROR_UNSUPPORTED;

	//! Add all waveforms
	uint32_t WaveIdx;
	for(WaveIdx=0;WaveIdx<DLS.nWaveforms;WaveIdx++) {
		const struct DLS_Waveform_t *SrcWav = &DLS.Waveforms[WaveIdx];

		//! Determine waveform length and format
		int Format = -1;
		uint32_t Size = SrcWav->Size / SrcWav->nChan;
		switch(SrcWav->Format) {
			case 0x0001: { //! PCM
				if(SrcWav->BitDepth == 8) {
					Format = SGE_WAV_FRMT_PCM8;
					Size /= sizeof(uint8_t);
				}
				if(SrcWav->BitDepth == 16) {
					Format = SGE_WAV_FRMT_PCM16;
					Size /= sizeof(int16_t);
				}
				if(SrcWav->BitDepth == 24) {
					Format = SGE_WAV_FRMT_PCM24;
					Size /= sizeof(int8_t)*3;
				}
				if(SrcWav->BitDepth == 32) {
					Format = SGE_WAV_FRMT_PCM32;
					Size /= sizeof(int32_t);
				}
			} break;
			case 0x0003: { //! IEEE Float
				if(SrcWav->BitDepth == 32) {
					Format = SGE_WAV_FRMT_FLOAT32;
					Size /= sizeof(float);
				}
			} break;
		}
		if(Format < 0) {
			ReturnValue = DLS_ERROR_UNSUPPORTED;
			goto SoundBankFromDLS_Exit;
		}

		//! For looped samples, chop everything after the loop point
		uint32_t LoopLen = SrcWav->wCtrl.LoopLen;
		if(LoopLen) Size = SrcWav->wCtrl.LoopBeg + LoopLen;

		//! Get sample rate, root key, and fine-tuning, and fix them up
		uint32_t Rate = SrcWav->Rate;
		int RootKey = SrcWav->wCtrl.Root, FineTune = SrcWav->wCtrl.Tune;
		Fix_Rate_RootKey_Tune(&Rate, &RootKey, &FineTune);

		//! Fill in basic information and parse options
		struct SGE_LocalWav_t WavEntry;
		struct SGE_Wav_t *Header = &WavEntry.Header;
		WavEntry.FileOffs     = SrcWav->FileDataOffs;
		WavEntry.Referenced   = 0;
		WavEntry.IsPercussive = 0;
		WavEntry.IsSorted     = 0;
		memcpy(&WavEntry.Options, Options, sizeof(struct SGE_gOptions_t));
		SGE_ParseOptions(&WavEntry.Options, (const char **)(&SrcWav->Comment), 1, 1, WaveformOptionsErrorLogger, (void*)SrcWav, NULL, NULL);

		//! Now fill out the waveform header
		Header->Frmt = (uint8_t)Format;
		Header->Chan = (uint8_t)SrcWav->nChan;
		Header->Size = Size;
		Header->Root = (uint8_t)RootKey;
		Header->Loop = LoopLen;
		Header->Fine = (uint8_t)FineTune;
		Header->Freq = Rate;
		Header->Gain = FixWaveformGain(SrcWav->wCtrl.Gain * WavEntry.Options.WavGainAdjust);

		//! Add to list of waveforms
		if(!AppendWaveform(Db, &WavEntry)) {
			ReturnValue = DLS_ERROR_OUT_OF_MEMORY;
			goto SoundBankFromDLS_Exit;
		}
	}

	//! Add all instruments
	uint32_t InstrumentIdx;
	for(InstrumentIdx=0;InstrumentIdx<DLS.nInstruments;InstrumentIdx++) {
		const struct DLS_Instrument_t *SrcInstrument = &DLS.Instruments[InstrumentIdx];
		if(!AppendInstrument(Db, SrcInstrument)) {
			ReturnValue = DLS_ERROR_OUT_OF_MEMORY;
			goto SoundBankFromDLS_Exit;
		}
	}

	//! Clean up and exit
SoundBankFromDLS_Exit:
	DLS_Destroy(&DLS);
	if(ReturnValue != DLS_ERROR_NONE) SGE_LocalDb_Destroy(Db);
	return ReturnValue;
}

/************************************************/
//! EOF
/************************************************/
