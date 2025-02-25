/************************************************/
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
/************************************************/
#include "DLS.h"
#include "FileIO_Int.h"
#include "MiniRIFF.h"
#include "GlobalHelpers.h"
/************************************************/

//! Helper functions
static struct DLS_Instrument_t *GetLastInstrument(struct DLS_t *DLS) {
	return &DLS->Instruments[DLS->nInstruments-1];
}
static struct DLS_Layer_t *GetLastLayer(struct DLS_t *DLS) {
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);
	return &Instrument->Layers[Instrument->nLayers-1];
}
static struct DLS_Region_t *GetLastRegion(struct DLS_t *DLS) {
	struct DLS_Layer_t *Layer = GetLastLayer(DLS);
	return &Layer->Regions[Layer->nRegions-1];
}
static struct DLS_Waveform_t *GetLastWaveform(struct DLS_t *DLS) {
	return &DLS->Waveforms[DLS->nWaveforms-1];
}

/************************************************/

//! Write default waveform control
static void WriteDefaultWaveControl(struct DLS_WaveformCtrl_t *wCtrl) {
	wCtrl->Root    = 60;
	wCtrl->Tune    = 0;
	wCtrl->LoopBeg = 0;
	wCtrl->LoopLen = 0;
	wCtrl->Gain    = 1.0;
}

//! Write default articulation
static void WriteDefaultArticulation(struct DLS_Articulation_t *Art) {
	Art->LFO1Freq     = 5.0;
	Art->LFO1Delay    = 0.01;
	Art->LFO1ToGain   = 0.0;
	Art->LFO1ToPitch  = 0.0;
	Art->LFO2Freq     = 5.0;
	Art->LFO2Delay    = 0.01;
	Art->LFO2ToPitch  = 0.0;
	Art->EG2ToPitch   = 0.0;
	Art->MasterTune   = 0.0;
	Art->MasterPan    = 0.0;
	Art->PanWidth     = 1.0;
	Art->EG1.A = Art->EG1.H = Art->EG1.D = Art->EG1.R = 0.0, Art->EG1.S = 1.0;
	Art->EG2.A = Art->EG2.H = Art->EG2.D = Art->EG2.R = 0.0, Art->EG2.S = 1.0;
}

/************************************************/

//! Append waveform to soundbank
static struct DLS_Waveform_t *AppendWaveformToDLS(struct DLS_t *DLS) {
	//! Enlarge array and get pointer to last element
	uint32_t nWaveforms = DLS->nWaveforms;
	struct DLS_Waveform_t *NewWaveforms = (struct DLS_Waveform_t*)realloc(DLS->Waveforms, (nWaveforms+1)*sizeof(*NewWaveforms));
	if(!NewWaveforms) return NULL;
	DLS->Waveforms  = NewWaveforms;
	DLS->nWaveforms = nWaveforms+1;
	struct DLS_Waveform_t *Waveform = &NewWaveforms[nWaveforms];

	//! Setup waveform
	Waveform->Format   = 0xFFFF; //! <- Used to check for culling
	Waveform->nChan    = 0;
	Waveform->Rate     = 0;
	Waveform->Size     = 0;
	Waveform->BitDepth = 0;
	Waveform->wvplOffset   = 0;
	Waveform->FileDataOffs = 0;
	Waveform->Name         = NULL;
	Waveform->Comment      = NULL;
	WriteDefaultWaveControl(&Waveform->wCtrl);
	return Waveform;
}

//! Append instrument to soundbank
static struct DLS_Instrument_t *AppendInstrumentToDLS(struct DLS_t *DLS) {
	//! Enlarge array and get pointer to last element
	uint32_t nInstruments = DLS->nInstruments;
	struct DLS_Instrument_t *NewInstruments = (struct DLS_Instrument_t*)realloc(DLS->Instruments, (nInstruments+1)*sizeof(*NewInstruments));
	if(!NewInstruments) return NULL;
	DLS->Instruments  = NewInstruments;
	DLS->nInstruments = nInstruments+1;
	struct DLS_Instrument_t *Instrument = &NewInstruments[nInstruments];

	//! Setup instrument
	Instrument->Patch   = 0xFF; //! <- Used to check for culling
	Instrument->CC0     = 0;
	Instrument->CC32    = 0;
	Instrument->DrumKit = 0;
	Instrument->nLayers = 0;
	Instrument->Layers  = NULL;
	Instrument->Name    = NULL;
	Instrument->Comment = NULL;
	Instrument->ArtLv   = 0;
	WriteDefaultArticulation(&Instrument->Art);
	return Instrument;
}

//! Append layer to instrument
static struct DLS_Layer_t *AppendLayerToInstrument(struct DLS_Instrument_t *Instrument) {
	//! Enlarge array and get pointer to last element
	uint32_t nLayers = Instrument->nLayers;
	struct DLS_Layer_t *NewLayers = (struct DLS_Layer_t*)realloc(Instrument->Layers, (nLayers+1)*sizeof(*NewLayers));
	if(!NewLayers) return NULL;
	Instrument->Layers  = NewLayers;
	Instrument->nLayers = nLayers+1;
	struct DLS_Layer_t *Layer = &NewLayers[nLayers];

	//! Setup layer
	Layer->VelLo    = 0;
	Layer->VelHi    = 0;
	Layer->nRegions = 0;
	Layer->Regions  = NULL;
	Layer->ArtLv    = 0;
	WriteDefaultArticulation(&Layer->Art);
	return Layer;
}

//! Append region to layer
static struct DLS_Region_t *AppendRegionToLayer(struct DLS_Layer_t *Layer) {
	//! Enlarge array and get pointer to last element
	uint32_t nRegions = Layer->nRegions;
	struct DLS_Region_t *NewRegions = (struct DLS_Region_t*)realloc(Layer->Regions, (nRegions+1)*sizeof(*NewRegions));
	if(!NewRegions) return NULL;
	Layer->Regions  = NewRegions;
	Layer->nRegions = nRegions+1;
	struct DLS_Region_t *Region = &NewRegions[nRegions];

	//! Setup region
	Region->KeyLo = 0xFFFF; //! <- Used to check for culling
	Region->KeyHi = 0;
	Region->VelLo = 0;
	Region->VelHi = 0;
	Region->Waveform = NULL;
	Region->ArtLv    = 0;
	Region->UseLocalWavCtrl = 0;
	WriteDefaultArticulation(&Region->Art);
	WriteDefaultWaveControl(&Region->WavCtrl);
	return Region;
}

/************************************************/

//! RIFF(DLS) -> ptbl
struct ptbl_t {
	uint32_t Size;  //! Structure size (unused)
	uint32_t nCues; //! Number of cues in list
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> fmt
enum {
	FMT_PCM   = 0x0001,
	FMT_FLOAT = 0x0003,
};
struct fmt_t {
	uint16_t Format;
	uint16_t Channels;
	uint32_t SampsPerSec;
	uint32_t BytesPerSec;
	uint16_t BlkAlign;
	uint16_t BitsPerSample;
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> wsmp
enum {
	WSMP_LOOP_FORWARD = 0x0000,
	WSMP_LOOP_RELEASE = 0x0001,
	WSMP_OPT_NOTRUNC_BIT    = 0x0001,
	WSMP_OPT_NOCOMPRESS_BIT = 0x0002,
};
struct wsmp_t {
	uint32_t Size; //! Structure size (unused)
	uint16_t Root;
	 int16_t Tune;
	 int32_t Gain;
	uint32_t Options;
	uint32_t nLoops;
	struct {
		uint32_t Size; //! Structure size (unused)
		uint32_t Type;
		uint32_t Beg;
		uint32_t Len;
	} Loop; //! Only available when nLoop > 0
} wsmp;

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> insh
struct insh_t {
	uint32_t nRegions;
	uint32_t Bank; //! CC32 | CC0<<8 | Drumkit<<31
	uint32_t Patch;
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn/rgn2) -> rgnh
struct rgnh_t {
	uint16_t KeyLo, KeyHi;
	uint16_t VelLo, VelHi;
	uint16_t Option;
	uint16_t KeyGroup;
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn/rgn2) -> wlnk
struct wlnk_t {
	uint16_t Option;
	uint16_t PhaseGroup;
	uint32_t Channel;
	uint32_t CueTblIdx;
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn/rgn2) -> LIST(lart/lar2) -> art1/art2
enum {
	//! ::Src/::Ctrl
	ART_SRC_NONE = 0x0000,
	ART_SRC_LFO1 = 0x0001,
	ART_SRC_EG1  = 0x0004,
	ART_SRC_EG2  = 0x0005,
	ART_SRC_LFO2 = 0x0009,
	ART_SRC_CC10 = 0x008A, //! Pan

	//! ::Dst
	ART_DST_GAIN        = 0x0001,
	ART_DST_PITCH       = 0x0003,
	ART_DST_PAN         = 0x0004,
	ART_DST_KEY         = 0x0005,
	ART_DST_LFO1_FREQ   = 0x0104,
	ART_DST_LFO1_DELAY  = 0x0105,
	ART_DST_LFO2_FREQ   = 0x0114,
	ART_DST_LFO2_DELAY  = 0x0115,
	ART_DST_EG1_ATTACK  = 0x0206,
	ART_DST_EG1_DECAY   = 0x0207,
	ART_DST_EG1_RELEASE = 0x0209,
	ART_DST_EG1_HOLD    = 0x020C,
	ART_DST_EG1_SUSTAIN = 0x020A,
	ART_DST_EG2_ATTACK  = 0x030A,
	ART_DST_EG2_DECAY   = 0x030B,
	ART_DST_EG2_RELEASE = 0x030D,
	ART_DST_EG2_SUSTAIN = 0x030E,
	ART_DST_EG2_HOLD    = 0x0310,

	//! ::Xfm
	ART_XFM_SRC_INVERT      = 0x8000,
	ART_XFM_SRC_BIPOLAR     = 0x4000,
	ART_XFM_SRC_TYPE_SHIFT  = 10,
	ART_XFM_CTRL_INVERT     = 0x0200,
	ART_XFM_CTRL_BIPOLAR    = 0x0100,
	ART_XFM_CTRL_TYPE_SHIFT = 4,
	ART_XFM_TYPE_SHIFT      = 0,
	ART_XFM_TYPE_MASK       = 0xF,
};
struct rgnart_t {
	uint16_t Src;
	uint16_t Ctrl;
	uint16_t Dst;
	uint16_t Xfm;
	 int32_t Scale;
};

/************************************************/

//! AbsolutePitch = (1200*Log2[Frequency/440] + 6900) * 65536
//! Frequency = 440*2^((AbsolutePitch/65536 - 6900)/1200)
static double AbsolutePitch(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = 440.0 * pow(2.0, (double)(v/65536.0 - 6900.0) / 1200.0);
	v = CLAMP(v, MinValue, MaxValue);
	return v;
}

//! AbsoluteTime = 1200*log2(Time) * 65536
//! Time = 2^(AbsoluteTime / 65536 / 1200)
static double AbsoluteTime(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale; if(v == -(double)0x80000000ul) return 0.0;
	v = pow(2.0, (double)v / (65536.0 * 1200.0));
	v = CLAMP(v, MinValue, MaxValue);
	return v;
}

//! AbsoluteCents = Value*100 * 65536
//! Value = AbsoluteCents / 100 / 65536
static double AbsoluteCents(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = v / (65536.0 * 100.0);
	v = CLAMP(v, MinValue, MaxValue);
	return v;
}

//! AbsolutePercent = Value*10 * 65536
//! Value = AbsolutePercent / 10 / 65536
//! NOTE: Final output is converted to normalized units.
static double AbsolutePercent(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = v / (65536.0 * 10.0);
	v = CLAMP(v, MinValue, MaxValue);
	return v / 100.0;
}

//! AbsoluteDeciPercent = Value*1000 * 65536
//! Value = AbsoluteDeciPercent / 1000 / 65536
//! NOTE: Final output is converted to normalized units.
//! NOTE2: This is ridiculous. Who the hell wrote this spec?
static double AbsoluteDeciPercent(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = v / (65536.0 * 1000.0);
	v = CLAMP(v, MinValue, MaxValue);
	return v;
}

//! AbsoluteDecibels = (20Log10[Value]/96 + 1.0) * (65536.0 * 10.0 * 100.0)
//! Value = 10^((AbsoluteDecibels / 100 / 10 / 65536 - 1) * 96 / 20)
//! NOTE: Final output is converted to linear units.
//! NOTE2: This is by far the most convoluted mess I've seen in this spec...
static double AbsoluteDecibels(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = (v / (65536.0 * 10.0 * 100.0) - 1.0) * 96.0;
	v = CLAMP(v, MinValue, MaxValue);
	return pow(10.0, v / 20.0);
}

//! RelativeDecibels = 20Log10[Value]*10*65536
//! Value = 10^(RelativeDecibels / 10 / 65536 / 20)
//! NOTE: These values are generally intended to be read inverted, so sign-flip.
static double RelativeDecibels(const struct rgnart_t *c, double MinValue, double MaxValue) {
	double v = (double)c->Scale;
	v = -v / (65536.0 * 10.0);
	v = CLAMP(v, MinValue, MaxValue);
	return pow(10.0, v / 20.0);
}

/************************************************/

static int ParseWaveCtrl(FILE *DLSFile, struct DLS_WaveformCtrl_t *wCtrl, uint32_t CkSize) {
	//! wsmp can be smaller than sizeof(wsmp_t) when nLoop == 0, but only read up to sizeof(wsmp_t)
	struct wsmp_t wsmp;
	uint32_t ReadSize = CkSize; if(ReadSize > sizeof(wsmp)) ReadSize = sizeof(wsmp);
#if !IS_BIG_ENDIAN
	if(!fread(&wsmp, ReadSize, 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u32le(&wsmp.Size,    DLSFile) ||
		!FileIO_Get_u16le(&wsmp.Root,    DLSFile) ||
		!FileIO_Get_s16le(&wsmp.Tune,    DLSFile) ||
		!FileIO_Get_s32le(&wsmp.Gain,    DLSFile) ||
		!FileIO_Get_u32le(&wsmp.Options, DLSFile) ||
		!FileIO_Get_u32le(&wsmp.nLoops,  DLSFile)
	) return DLS_ERROR_IO;
	if(wsmp.nLoops > 0) {
		if(
			!FileIO_Get_u32le(&wsmp.Loop.Size, DLSFile) ||
			!FileIO_Get_u32le(&wsmp.Loop.Type, DLSFile) ||
			!FileIO_Get_u32le(&wsmp.Loop.Beg,  DLSFile) ||
			!FileIO_Get_u32le(&wsmp.Loop.Len,  DLSFile)
		) return DLS_ERROR_IO;
	}
#endif
	wCtrl->Root    = wsmp.Root;
	wCtrl->Tune    = wsmp.Tune;
	wCtrl->Gain    = pow(10.0, (wsmp.Gain / 655360.0) / 20.0);
	wCtrl->LoopBeg = wsmp.nLoops ? wsmp.Loop.Beg : 0;
	wCtrl->LoopLen = wsmp.nLoops ? wsmp.Loop.Len : 0;
	return DLS_ERROR_NONE;
}

//! Read articulations to target
static int ReadArticulations(struct DLS_t *DLS, struct DLS_Articulation_t *Art, FILE *DLSFile) {
	(void)DLS;

	//! Read header
	struct {
		uint32_t Size;   //! Structure size (unused)
		uint32_t nBlock; //! Number of connection blocks in list
	} art;
#if !IS_BIG_ENDIAN
	if(!fread(&art, sizeof(art), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u32le(&art.Size,   DLSFile) ||
		!FileIO_Get_u32le(&art.nBlock, DLSFile)
	) return DLS_ERROR_IO;
#endif
	//! Begin parsing blocks
	uint32_t i;
	double v;
	for(i=0;i<art.nBlock;i++) {
		struct rgnart_t c;
#if !IS_BIG_ENDIAN
		if(!fread(&c, sizeof(struct rgnart_t), 1, DLSFile)) return DLS_ERROR_IO;
#else
		if(
			!FileIO_Get_u16le(&c.Src,   DLSFile) ||
			!FileIO_Get_u16le(&c.Ctrl,  DLSFile) ||
			!FileIO_Get_u16le(&c.Dst,   DLSFile) ||
			!FileIO_Get_u16le(&c.Xfm,   DLSFile) ||
			!FileIO_Get_s32le(&c.Scale, DLSFile)
		) {
			return DLS_ERROR_IO;
		}
#endif
#define MAKE_VALUE(Src, Ctrl, Dst) ((uint64_t)Src | (uint64_t)Ctrl<<16 | (uint64_t)Dst<<32)
		switch(MAKE_VALUE(c.Src, c.Ctrl, c.Dst)) {
			//! DLS2.2 connections
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_LFO1_FREQ):
				Art->LFO1Freq = AbsolutePitch(&c, 0.1, 20.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_LFO1_DELAY):
				Art->LFO1Delay = AbsoluteTime(&c, 0.01, 10.0);
				break;
			case MAKE_VALUE(ART_SRC_LFO1, ART_SRC_NONE, ART_DST_GAIN):
				//! I'm not sure if this is supposed to be scaled by 2 or not.
				//! If I'm not misunderstanding, the DLS spec says that the
				//! volume should oscillate by +/-x decibels, which means
				//! that the overall effect should be multiplied by two if
				//! we're not scaling up, and only scale down, as this will
				//! give the same range for the effect.
				v = RelativeDecibels(&c, -96.0, 0.0);
				Art->LFO1ToGain = 1.0 - v*v; //! 2*x_dB == x_Linear^2
				break;
			case MAKE_VALUE(ART_SRC_LFO1, ART_SRC_NONE, ART_DST_PITCH):
				Art->LFO1ToPitch = AbsoluteCents(&c, -12.0, +12.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_LFO2_FREQ):
				Art->LFO2Freq = AbsolutePitch(&c, 0.1, 20.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_LFO2_DELAY):
				Art->LFO2Delay = AbsoluteTime(&c, 0.01, 10.0);
				break;
			case MAKE_VALUE(ART_SRC_LFO2, ART_SRC_NONE, ART_DST_PITCH):
				Art->LFO2ToPitch = AbsoluteCents(&c, -12.0, +12.0);
				break;
			case MAKE_VALUE(ART_SRC_EG2, ART_SRC_NONE, ART_DST_PITCH):
				Art->EG2ToPitch = AbsoluteCents(&c, -12.0, +12.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_PITCH):
				Art->MasterTune = AbsoluteCents(&c, -12.0, +12.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_PAN):
				Art->MasterPan = AbsoluteDeciPercent(&c, -0.5, +0.5) * 2.0;
				break;
			case MAKE_VALUE(ART_SRC_CC10, ART_SRC_NONE, ART_DST_PAN):
				Art->PanWidth = AbsoluteDeciPercent(&c, -0.5, +0.5) * 2.0;
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG1_ATTACK):
				Art->EG1.A = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG1_HOLD):
				Art->EG1.H = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG1_DECAY):
				Art->EG1.D = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG1_SUSTAIN):
				Art->EG1.S = AbsoluteDecibels(&c, -96.0, 0.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG1_RELEASE):
				Art->EG1.R = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG2_ATTACK):
				Art->EG2.A = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG2_HOLD):
				Art->EG2.H = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG2_DECAY):
				Art->EG2.D = AbsoluteTime(&c, 0.0, 40.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG2_SUSTAIN):
				Art->EG2.S = AbsolutePercent(&c, 0.0, 100.0);
				break;
			case MAKE_VALUE(ART_SRC_NONE, ART_SRC_NONE, ART_DST_EG2_RELEASE):
				Art->EG2.R = AbsoluteTime(&c, 0.0, 40.0);
				break;
		}
#undef MAKE_VALUE
	}
	return DLS_ERROR_NONE;
}

/************************************************/
//! Level 1 regions
/************************************************/

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) (begin)
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbBeg(FILE *DLSFile, void *User) {
	(void)DLSFile;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Layer_t  *Layer  = GetLastLayer(DLS);
	struct DLS_Region_t *Region = AppendRegionToLayer(Layer);
	return Region ? DLS_ERROR_NONE : DLS_ERROR_OUT_OF_MEMORY;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> rgnh
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_rgnh(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 1) return DLS_ERROR_NONE;

	struct rgnh_t rgnh;
#if !IS_BIG_ENDIAN
	if(!fread(&rgnh, sizeof(rgnh), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u16le(&rgnh.KeyLo,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.KeyHi,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.VelLo,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.VelHi,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.Option,   DLSFile) ||
		!FileIO_Get_u16le(&rgnh.KeyGroup, DLSFile)
	) return DLS_ERROR_IO;
#endif
	Region->KeyLo = rgnh.KeyLo, Region->KeyHi = rgnh.KeyHi;
	Region->VelLo = rgnh.VelLo, Region->VelHi = rgnh.VelHi;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> wsmp
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_wsmp(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 1) return DLS_ERROR_NONE;

	Region->UseLocalWavCtrl = 1;
	return ParseWaveCtrl(DLSFile, &Region->WavCtrl, Ck->Size);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> wlnk
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_wlnk(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 1) return DLS_ERROR_NONE;

	struct wlnk_t wlnk;
#if !IS_BIG_ENDIAN
	if(!fread(&wlnk, sizeof(wlnk), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u16le(&wlnk.Option,     DLSFile) ||
		!FileIO_Get_u16le(&wlnk.PhaseGroup, DLSFile) ||
		!FileIO_Get_u32le(&wlnk.Channel,    DLSFile) ||
		!FileIO_Get_u32le(&wlnk.CueTblIdx,  DLSFile)
	) return DLS_ERROR_IO;
#endif
	Region->CueTblIdx = wlnk.CueTblIdx;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> LIST(lart) -> art1
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lart_art1(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 1) return DLS_ERROR_NONE;

	Region->ArtLv = 1;
	return ReadArticulations(DLS, &Region->Art, DLSFile);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> LIST(lar2) -> art2
//! This is a weird case, with level-2 articulations in a level-1 region.
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lar2_art2(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 2) return DLS_ERROR_NONE;

	Region->ArtLv = 2;
	return ReadArticulations(DLS, &Region->Art, DLSFile);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) (end)
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbEnd(FILE *DLSFile, void *User) {
	(void)DLSFile;
	struct DLS_t *DLS = (struct DLS_t*)User;

	//! Check that region ended up being defined
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->KeyLo == 0xFFFF) {
		//! Delete region
		struct DLS_Layer_t *Layer = GetLastLayer(DLS);
		Layer->nRegions--;
	}
	return DLS_ERROR_NONE;
}

/************************************************/
//! Level 2 regions
/************************************************/

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) (begin)
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_CbBeg(FILE *DLSFile, void *User) {
	return RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbBeg(DLSFile, User);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> rgnh
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_rgnh(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 2) return DLS_ERROR_NONE;

	struct rgnh_t rgnh;
#if !IS_BIG_ENDIAN
	if(!fread(&rgnh, sizeof(rgnh), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u16le(&rgnh.KeyLo,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.KeyHi,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.VelLo,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.VelHi,    DLSFile) ||
		!FileIO_Get_u16le(&rgnh.Option,   DLSFile) ||
		!FileIO_Get_u16le(&rgnh.KeyGroup, DLSFile)
	) return DLS_ERROR_IO;
#endif
	Region->KeyLo = rgnh.KeyLo, Region->KeyHi = rgnh.KeyHi;
	Region->VelLo = rgnh.VelLo, Region->VelHi = rgnh.VelHi;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> wsmp
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_wsmp(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 2) return DLS_ERROR_NONE;

	Region->UseLocalWavCtrl = 1;
	return ParseWaveCtrl(DLSFile, &Region->WavCtrl, Ck->Size);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> wlnk
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_wlnk(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 2) return DLS_ERROR_NONE;

	struct wlnk_t wlnk;
#if !IS_BIG_ENDIAN
	if(!fread(&wlnk, sizeof(wlnk), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u16le(&wlnk.Option,     DLSFile) ||
		!FileIO_Get_u16le(&wlnk.PhaseGroup, DLSFile) ||
		!FileIO_Get_u32le(&wlnk.Channel,    DLSFile) ||
		!FileIO_Get_u32le(&wlnk.CueTblIdx,  DLSFile)
	) return DLS_ERROR_IO;
#endif
	Region->CueTblIdx = wlnk.CueTblIdx;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> LIST(lar2) -> art2
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST_lar2_art2(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Region_t *Region = GetLastRegion(DLS);
	if(Region->ArtLv > 2) return DLS_ERROR_NONE;

	Region->ArtLv = 2;
	return ReadArticulations(DLS, &Region->Art, DLSFile);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) (end)
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_CbEnd(FILE *DLSFile, void *User) {
	return RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbEnd(DLSFile, User);
}

/************************************************/
//! Instruments
/************************************************/

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) (begin)
static int RIFF_DLS_LIST_lins_LIST_ins_CbBeg(FILE *DLSFile, void *User) {
	(void)DLSFile;
	struct DLS_t *DLS = (struct DLS_t*)User;

	//! Create instrument
	struct DLS_Instrument_t *Instrument = AppendInstrumentToDLS(DLS);
	if(!Instrument) return DLS_ERROR_OUT_OF_MEMORY;

	//! Create global layer
	struct DLS_Layer_t *Layer = AppendLayerToInstrument(Instrument);
	if(!Layer) return DLS_ERROR_OUT_OF_MEMORY;

	//! Done
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lart) -> art1
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lart_art1(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);
	if(Instrument->ArtLv > 1) return DLS_ERROR_NONE;

	Instrument->ArtLv = 1;
	return ReadArticulations(DLS, &Instrument->Art, DLSFile);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lar2) -> art2
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_lar2_art2(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);
	if(Instrument->ArtLv > 2) return DLS_ERROR_NONE;

	Instrument->ArtLv = 2;
	return ReadArticulations(DLS, &Instrument->Art, DLSFile);
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> insh
static int RIFF_DLS_LIST_lins_LIST_ins_insh(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);

	struct insh_t insh;
#if !IS_BIG_ENDIAN
	if(!fread(&insh, sizeof(insh), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u32le(&insh.nRegions, DLSFile) ||
		!FileIO_Get_u32le(&insh.Bank,     DLSFile) ||
		!FileIO_Get_u32le(&insh.Patch,    DLSFile)
	) return DLS_ERROR_IO
#endif
	Instrument->Patch   = insh.Patch;
	Instrument->CC0     = (uint8_t)(insh.Bank >> 8);
	Instrument->CC32    = (uint8_t)(insh.Bank >> 0);
	Instrument->DrumKit = (uint8_t)(insh.Bank >> 31);
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(INFO) -> INAM
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_INAM(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);
	char *Name = (char*)malloc(Ck->Size * sizeof(char));
	if(!Name) return DLS_ERROR_OUT_OF_MEMORY;
	Instrument->Name = Name;
	if(!fread(Name, Ck->Size, 1, DLSFile)) return DLS_ERROR_IO;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(INFO) -> ICMT
static int RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_ICMT(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Instrument_t *Instrument = GetLastInstrument(DLS);
	char *Comment = (char*)malloc(Ck->Size * sizeof(char));
	if(!Comment) return DLS_ERROR_OUT_OF_MEMORY;
	Instrument->Comment = Comment;
	if(!fread(Comment, Ck->Size, 1, DLSFile)) return DLS_ERROR_IO;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) (end)
static int RIFF_DLS_LIST_lins_LIST_ins_CbEnd(FILE *DLSFile, void *User) {
	(void)DLSFile;
	struct DLS_t *DLS = (struct DLS_t*)User;

	//! Check that instrument ended up being defined
	struct DLS_Layer_t *Layer = GetLastLayer(DLS);
	if(!Layer->nRegions) {
		//! Delete instrument and its layer
		free(Layer);
		DLS->nInstruments--;
	}
	return DLS_ERROR_NONE;
}

/************************************************/
//! Waveforms
/************************************************/

//! RIFF(DLS) -> LIST(wvpl) (begin)
static int RIFF_DLS_LIST_wvpl_CbBeg(FILE *DLSFile, void *User) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	DLS->wvplOffs = ftell(DLSFile);
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) (begin)
//! 12 = sizeof(LISTheader)
static int RIFF_DLS_LIST_wvpl_LIST_wave_CbBeg(FILE *DLSFile, void *User) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = AppendWaveformToDLS(DLS);
	Waveform->wvplOffset = ftell(DLSFile) - 12u - DLS->wvplOffs;
	return Waveform ? DLS_ERROR_NONE : DLS_ERROR_OUT_OF_MEMORY;
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> fmt
static int RIFF_DLS_LIST_wvpl_LIST_wave_fmt(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = GetLastWaveform(DLS);

	struct fmt_t fmt;
#if !IS_BIG_ENDIAN
	if(!fread(&fmt, sizeof(fmt), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u16le(&fmt.Format,        DLSFile) ||
		!FileIO_Get_u16le(&fmt.Channels,      DLSFile) ||
		!FileIO_Get_u32le(&fmt.SampsPerSec,   DLSFile) ||
		!FileIO_Get_u32le(&fmt.BytesPerSec,   DLSFile) ||
		!FileIO_Get_u16le(&fmt.BlkAlign,      DLSFile) ||
		!FileIO_Get_u16le(&fmt.BitsPerSample, DLSFile)
	) return DLS_ERROR_IO;
#endif
	Waveform->Format   = fmt.Format;
	Waveform->nChan    = fmt.Channels;
	Waveform->Rate     = fmt.SampsPerSec;
	Waveform->BitDepth = fmt.BitsPerSample;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> data
static int RIFF_DLS_LIST_wvpl_LIST_wave_data(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = GetLastWaveform(DLS);
	Waveform->Size = Ck->Size;
	Waveform->FileDataOffs = ftell(DLSFile);
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> wsmp
static int RIFF_DLS_LIST_wvpl_LIST_wave_wsmp(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = GetLastWaveform(DLS);

	return ParseWaveCtrl(DLSFile, &Waveform->wCtrl, Ck->Size);
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> LIST(INFO) -> INAM
static int RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_INAM(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = GetLastWaveform(DLS);
	char *Name = (char*)malloc(Ck->Size * sizeof(char));
	if(!Name) return DLS_ERROR_OUT_OF_MEMORY;
	Waveform->Name = Name;
	if(!fread(Name, Ck->Size, 1, DLSFile)) return DLS_ERROR_IO;
	return DLS_ERROR_NONE;
}

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> LIST(INFO) -> ICMT
static int RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_ICMT(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	struct DLS_t *DLS = (struct DLS_t*)User;
	struct DLS_Waveform_t *Waveform = GetLastWaveform(DLS);
	char *Comment = (char*)malloc(Ck->Size * sizeof(char));
	if(!Comment) return DLS_ERROR_OUT_OF_MEMORY;
	Waveform->Comment = Comment;
	if(!fread(Comment, Ck->Size, 1, DLSFile)) return DLS_ERROR_IO;
	return DLS_ERROR_NONE;
}

/************************************************/
//! Pool table
/************************************************/

//! RIFF(DLS) -> ptbl
static int RIFF_DLS_ptbl(FILE *DLSFile, void *User, const struct RIFF_CkHeader_t *Ck) {
	(void)Ck;
	struct DLS_t *DLS = (struct DLS_t*)User;

	struct ptbl_t ptbl;
#if !IS_BIG_ENDIAN
	if(!fread(&ptbl, sizeof(ptbl), 1, DLSFile)) return DLS_ERROR_IO;
#else
	if(
		!FileIO_Get_u32le(&ptbl.Size,  DLSFile) ||
		!FileIO_Get_u32le(&ptbl.nCues, DLSFile)
	) return DLS_ERROR_IO;
#endif
	uint32_t *ptblData = (uint32_t*)malloc(ptbl.nCues * sizeof(uint32_t));
	if(!ptblData) return DLS_ERROR_OUT_OF_MEMORY;
	DLS->ptbl = ptblData;
#if !IS_BIG_ENDIAN
	if(fread(ptblData, sizeof(uint32_t), ptbl.nCues, DLSFile) != ptbl.nCues) return DLS_ERROR_IO;
#else
	uint32_t n;
	for(n=0;n<ptbl.nCues;n++) {
		if(!FileIO_Get_u32le(&ptblData[n], DLSFile)) return DLS_ERROR_IO;
	}
#endif
	return DLS_ERROR_NONE;
}

/************************************************/
//! DLS chunk hierarchy
/************************************************/

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> LIST(lar2) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST_lar2_Ck[] = {
	{
		RIFF_FOURCC("art2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST_lar2_art2
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST[] = {
	{
		RIFF_FOURCC("lar2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST_lar2_Ck,
		NULL,
		NULL,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn2) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_Ck[] = {
	{
		RIFF_FOURCC("rgnh"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_rgnh
	},
	{
		RIFF_FOURCC("wsmp"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_wsmp
	},
	{
		RIFF_FOURCC("wlnk"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_wlnk
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> LIST(lar2) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lar2_Ck[] = {
	{
		RIFF_FOURCC("art2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lar2_art2
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> LIST(lart) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lart_Ck[] = {
	{
		RIFF_FOURCC("art1"),RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lart_art1
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> LIST
//! Note that we support lar2 here, even though that should be encapsulated
//! within a rgn2 chunk instead. This is for compatibility with some editors.
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST[] = {
	{
		RIFF_FOURCC("lart"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lart_Ck,
		NULL,
		NULL,
		NULL
	},
	{
		RIFF_FOURCC("lar2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST_lar2_Ck,
		NULL,
		NULL,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST(rgn) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_Ck[] = {
	{
		RIFF_FOURCC("rgnh"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_rgnh
	},
	{
		RIFF_FOURCC("wsmp"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_wsmp
	},
	{
		RIFF_FOURCC("wlnk"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_wlnk
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(lrgn) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST[] = {
	{
		RIFF_FOURCC("rgn "),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_Ck,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_LIST,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbBeg,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn_CbEnd
	},
	{
		RIFF_FOURCC("rgn2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_Ck,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_LIST,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_CbBeg,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST_rgn2_CbEnd
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST("lar2") -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lar2_Ck[] = {
	{
		RIFF_FOURCC("art2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lar2_art2
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST("lart") -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_lart_Ck[] = {
	{
		RIFF_FOURCC("art1"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lart_art1
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST(INFO) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_LIST[] = {
	{
		RIFF_FOURCC("INAM"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_INAM,
	},
	{
		RIFF_FOURCC("ICMT"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_ICMT,
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_lins_LIST_ins_LIST[] = {
	{
		RIFF_FOURCC("INFO"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_INFO_LIST,
		NULL,
		NULL,
		NULL
	},
	{
		RIFF_FOURCC("lrgn"),
		NULL,
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lrgn_LIST,
		NULL,
		NULL
	},
	{
		RIFF_FOURCC("lart"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lart_Ck,
		NULL,
		NULL,
		NULL
	},
	{
		RIFF_FOURCC("lar2"),
		RIFF_DLS_LIST_lins_LIST_ins_LIST_lar2_Ck,
		NULL,
		NULL,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST(ins) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_lins_LIST_ins_Ck[] = {
	{
		RIFF_FOURCC("insh"),
		RIFF_DLS_LIST_lins_LIST_ins_insh
	},
	{0},
};

//! RIFF(DLS) -> LIST(lins) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_lins_LIST[] = {
	{
		RIFF_FOURCC("ins "),
		RIFF_DLS_LIST_lins_LIST_ins_Ck,
		RIFF_DLS_LIST_lins_LIST_ins_LIST,
		RIFF_DLS_LIST_lins_LIST_ins_CbBeg,
		RIFF_DLS_LIST_lins_LIST_ins_CbEnd
	},
	{0},
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> LIST(INFO) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_LIST[] = {
	{
		RIFF_FOURCC("INAM"),
		RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_INAM,
	},
	{
		RIFF_FOURCC("ICMT"),
		RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_ICMT,
	},
	{0},
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_wvpl_LIST_wave_LIST[] = {
	{
		RIFF_FOURCC("INFO"),
		RIFF_DLS_LIST_wvpl_LIST_wave_LIST_INFO_LIST,
		NULL,
		NULL,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST(wave) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_LIST_wvpl_LIST_wave_Ck[] = {
	{
		RIFF_FOURCC("fmt "),RIFF_DLS_LIST_wvpl_LIST_wave_fmt
	},
	{
		RIFF_FOURCC("data"),RIFF_DLS_LIST_wvpl_LIST_wave_data
	},
	{
		RIFF_FOURCC("wsmp"),RIFF_DLS_LIST_wvpl_LIST_wave_wsmp
	},
	{0},
};

//! RIFF(DLS) -> LIST(wvpl) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST_wvpl_LIST[] = {
	{
		RIFF_FOURCC("wave"),
		RIFF_DLS_LIST_wvpl_LIST_wave_Ck,
		RIFF_DLS_LIST_wvpl_LIST_wave_LIST,
		RIFF_DLS_LIST_wvpl_LIST_wave_CbBeg,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> LIST
static const struct RIFF_CkListHdl_t RIFF_DLS_LIST[] = {
	{
		RIFF_FOURCC("wvpl"),
		NULL,
		RIFF_DLS_LIST_wvpl_LIST,
		RIFF_DLS_LIST_wvpl_CbBeg,
		NULL
	},
	{
		RIFF_FOURCC("lins"),
		NULL,
		RIFF_DLS_LIST_lins_LIST,
		NULL,
		NULL
	},
	{0},
};

//! RIFF(DLS) -> Chunks
static const struct RIFF_CkHdl_t RIFF_DLS_Ck[] = {
	{
		RIFF_FOURCC("ptbl"),
		RIFF_DLS_ptbl
	},
	{0},
};

//! RIFF(DLS)
//! This has global linkage
const struct RIFF_CkListHdl_t RIFF_DLS[] = {
	{
		RIFF_FOURCC("DLS "),
		RIFF_DLS_Ck,
		RIFF_DLS_LIST,
		NULL,
		NULL
	},
	{0},
};

/************************************************/
//! EOF
/************************************************/
