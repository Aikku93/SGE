/**************************************/
#pragma once
/**************************************/

//! Available window types
#define POLY2MONO_WINDOW_TYPE_SINE     0
#define POLY2MONO_WINDOW_TYPE_HANN     1
#define POLY2MONO_WINDOW_TYPE_HAMMING  2
#define POLY2MONO_WINDOW_TYPE_BLACKMAN 3
#define POLY2MONO_WINDOW_TYPE_NUTTALL  4

/**************************************/

//! Global state structure
struct Poly2Mono_t {
	//! Global state (do not change after initialization)
	int nChan;     //! Channels in encoding scheme
	int BlockSize; //! Transform block size
	int nHops;     //! Number of STFT hops per block

	//! Internal state
	//! Buffer memory layout:
	//!   char  _Padding[];
	//!   float Window         [BlockSize/2];
	//!   float BfTemp         [BlockSize*3];
	//!   float BfOutput       [BlockSize];
	//!   float BfInvLap       [BlockSize];
	//!   float BfFwdLap[nChan][BlockSize];
	//!   float BfWeight       [BlockSize/2];
	//! BufferData contains the original pointer returned by malloc().
	int    BlockIdx;
	void  *BufferData;
	float *Window;
	float *BfTemp;
	float *BfOutput;
	float *BfInvLap;
	float *BfFwdLap;
	float *BfWeight;
};

/**************************************/

int  Poly2Mono_Init   (struct Poly2Mono_t *State, int WindowType);
void Poly2Mono_Destroy(struct Poly2Mono_t *State);
void Poly2Mono_Process(struct Poly2Mono_t *State, float *Output, const float *Input);

/**************************************/
//! EOF
/**************************************/
