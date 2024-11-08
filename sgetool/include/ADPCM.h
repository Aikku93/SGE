/************************************************/
#pragma once
/************************************************/
#include <stddef.h>
#include <stdint.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

#define ADPCM_FRAME_SIZE   7 //! Number of samples per frame
#define ADPCM_FILTER_BITS  6 //! Precision of the prediction filter
#define ADPCM_FILTER_ORDER 2 //! Number of taps in prediction filter

/************************************************/

//! ADPCM state structure
struct ADPCM_t {
	int8_t  cM1, cM2; //! Prediction coefficients
	int16_t zM1, zM2; //! Prediction taps
};

/************************************************/

//! Initialize ADPCM state
void ADPCM_Init(struct ADPCM_t *State, const float *Data, size_t nSamples, size_t DataStride);

//! Encode ADPCM frame
uint32_t ADPCM_EncodeFrame(struct ADPCM_t *State, const float *Data, size_t DataStride);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
