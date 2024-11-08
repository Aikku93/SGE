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

//! SRC_Config_t::Dst/SrcFormat
#define SRC_FORMAT_PCM8    0
#define SRC_FORMAT_PCM16   1
#define SRC_FORMAT_PCM24   2
#define SRC_FORMAT_PCM32   3
#define SRC_FORMAT_FLOAT32 4
#define SRC_FORMAT_CUSTOM  5

//! SRC_Config_t::MonoConvWindow
#define SRC_MONOCONV_WINDOW_SINE     0
#define SRC_MONOCONV_WINDOW_HANN     1
#define SRC_MONOCONV_WINDOW_HAMMING  2
#define SRC_MONOCONV_WINDOW_BLACKMAN 3
#define SRC_MONOCONV_WINDOW_NUTTALL  4

//! SRC_Config_t::FilterWindow
#define SRC_WINDOW_NONE     0
#define SRC_WINDOW_SINE     1
#define SRC_WINDOW_HANN     2
#define SRC_WINDOW_HAMMING  3
#define SRC_WINDOW_BLACKMAN 4
#define SRC_WINDOW_NUTTALL  5
#define SRC_WINDOW_LANCZOS  6
#define SRC_WINDOW_LANCZOS2 7

/************************************************/

//! Conversion configuration
struct SRC_Config_t {
	uint8_t  DstFormat;
	uint8_t  SrcFormat;
	uint8_t  DstChans;
	uint8_t  SrcChans;
	uint16_t MonoConvWindowSize;
	uint8_t  MonoConvWindow;
	uint8_t  MonoConvHops;
	uint8_t  FilterHalfOrder;
	uint8_t  FilterWindow;
	double   DstRate;
	double   SrcRate;
	double   Cutoff; //! = 2*Fc (ie. ranges 0.0 .. 1.0)
	float    GlobalGain;
	float    HighShelfGain;
	float    DitherLevel;

	//! CustomWrite() must return 0 on failure, and 1 on success
	void *CustomWrite_Userdata;
	int (*CustomWrite)(FILE *DstFile, const float *Src, uint32_t N, void *Userdata);
};

/************************************************/

//! Convert data, with input from one stream, and output to another stream
//! Returns 0 on failure, 1 on success.
//! Assumes that the data starts at the current SrcFile offset.
//! NOTE: Multi-channel data is assumed to be interleaved.
//! NOTE: Setting SrcLoopSize == 0 will disable looping, and
//! treat any samples "past the end" as silence.
int SRC_ConvertStreamedData(
	FILE *DstFile,
	FILE *SrcFile,
	uint32_t nSrcSamples,
	uint32_t nDstSamples,
	uint32_t SrcLoopSize,
	const struct SRC_Config_t *Config
);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
