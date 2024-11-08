/************************************************/
#pragma once
/************************************************/
#define SGE_DECLSPEC
/************************************************/

#ifdef __GBA__
# include "SGE-GBA-Options.h"
#else
# include "SGE-NDS-Options.h"
#endif

/************************************************/

//! Offsets for the platform-specific structure
#ifdef __GBA__
# if !SGE_USE_OVERSAMPLING
#  define SGE_PLATFORM_DRIVERDATA_SIZE   0x00
# else
#  define SGE_PLATFORM_OVERSAMPLING_OFFS 0x00
#  define SGE_PLATFORM_DRIVERDATA_SIZE   0x04
# endif
#else
# define SGE_PLATFORM_STREAMTOKEN_OFFS   0x00
# if !SGE_USE_OVERSAMPLING
#  define SGE_PLATFORM_DRIVERDATA_SIZE   0x04
# else
#  define SGE_PLATFORM_OVERSAMPLING_OFFS 0x04
#  define SGE_PLATFORM_DRIVERDATA_SIZE   0x08
# endif
#endif

/************************************************/
#ifndef __ASSEMBLER__
/************************************************/
#include <stdint.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

//! Platform specs
#ifdef __GBA__
typedef uint16_t SGE_MixSmp_t;
typedef  int8_t  SGE_OutSmp_t;
#else
typedef int32_t SGE_MixSmp_t;
typedef int16_t SGE_OutSmp_t;
#endif
struct __attribute__((aligned(__SIZEOF_POINTER__))) SGE_Driver_Platform_t {
#ifdef __NDS__
	void *StreamToken;
#endif
#if SGE_USE_OVERSAMPLING
	uint16_t TapL, TapR;
#endif
	int x[0];
};

/************************************************/

//! SGE_Driver_GetMixingBufferSizeDefault(RateHz)
//! Description: Get size of the mixing buffer for given parameters.
//! Arguments:
//!   RateHz: Sampling rate (in Hz).
//! Returns: Size in bytes of a mixing buffer to suit RateHz.
//! Notes:
//!  -Returns 0 if RateHz is an unsupported sampling rate.
//!  -This assumes that the driver is using the native update rate.
SGE_DECLSPEC uint32_t SGE_Driver_GetMixingBufferSizeDefault(uint16_t RateHz);

//! SGE_Driver_GetWorkAreaSizeDefault(VoxCnt, BufCnt, RateHz)
//! Description: Get size of SGE_Driver_t structure for given parameters.
//! Arguments:
//!   VoxCnt: Number of voices.
//!   BufCnt: Number of output buffers.
//!   RateHz: Sampling rate (in Hz).
//! Returns: Size in bytes of a SGE_Driver_t structure to suit the parameters.
//! Notes:
//!  -Returns 0 if the requested parameters cannot form a valid driver.
//!  -This assumes that the driver is using the native update rate.
SGE_DECLSPEC uint32_t SGE_Driver_GetWorkAreaSizeDefault(uint8_t VoxCnt, uint8_t BufCnt, uint16_t RateHz);

//! SGE_Driver_InitDefault(Driver, VoxCnt, RateHz, BufCnt, MixBuf)
//! Description: Initialize SGE driver at the default update rate.
//! Arguments:
//!   Driver: Driver work area.
//!   VoxCnt: Number of voices.
//!   RateHz: Sampling rate (in Hz).
//!   BufCnt: Number of output buffers.
//!   MixBuf: Assigned mixing buffer (see SGE_Driver_GetMixingBufferSizeDefault()).
//! Returns: On success, returns a non-zero value. On failure, returns 0.
//! Notes:
//!  -BufCnt must be between 2 and 255.
//!  -MixBuf must be aligned to 4 bytes.
//!  -This routine automatically enables the audio hardware.
//!  -When using reverb, pre-delay is equal to the total buffer size.
//! GBA-specific notes:
//!  -The default update rate is 59.7275Hz, corresponding to VSync timings.
//!  -It is only possible to have one driver open at any time. Multiple drivers
//!   can be initialized, but only one can be active at any given time.
//!  -The driver takes over DMA channels 1+2 and a timer (see SGE_HWTIMER_IDX).
//!  -The buffer sizes are set for a frame's worth of FIFO samples:
//!     BufLen = Round[RateHz * AGB_FRAME_CYCLES/AGB_HW_FREQ_HZ]
//!   Additionally, note that BufLen*BufCnt must be a multiple of 16, or this
//!   function will return an error (the audio hardware fetches in 16-sample
//!   units). Some RateHz,BufCnt configurations that will work:
//!    BufCnt=2,3,4... @ 13379Hz
//!    BufCnt=2,4,6... @ 15768Hz <- BufCnt multiples of 2!
//!    BufCnt=2,3,4... @ 18157Hz
//!    BufCnt=2,3,4... @ 20068Hz
//!    BufCnt=2,3,4... @ 21024Hz
//!    BufCnt=2,3,4... @ 26758Hz <- Can't use SGE_USE_OVERSAMPLING!
//!    BufCnt=2,4,6... @ 27236Hz <- BufCnt multiples of 2!
//!    BufCnt=2,3,4... @ 31536Hz
//!    BufCnt=2,4,6... @ 36792Hz <- BufCnt multiples of 2!
//!    BufCnt=2,3,4... @ 40137Hz
//!    BufCnt=2,3,4... @ 42048Hz
//!   26758Hz cannot be used with oversampling because the timer period is
//!   narrowed from 313.5 cycles to 313 cycles, causing desynchronization.
//!  -The return value of this function is actually the timer period:
//!     Period = Round[AGB_HW_FREQ_HZ / RateHz]
//!   Note that with oversampling, the period is that of RateHz*2.
//! NDS-specific notes:
//!  -The default update rate is 59.8246Hz, approximately equal to VSync timing,
//!   with some rounding error (this should be 59.8261Hz). Although this is
//!   intended for relatively consistent timing, it is not perfectly aligned to
//!   VSync, and it is necessary to keep track of the update timing.
//!  -It is possible to have multiple drivers open at any time, provided that
//!   they are all continuously updated and allocated to hardware voices.
//!  -BufLen*BufCnt must be a multiple of 2. As BufLen is dependent on RateHz
//!   and the update rate, it may be necessary to experiment. Having said that,
//!   some RateHz that will work are: 16364Hz, 24546Hz, 32728Hz (recommended).
//!  -This function, as well as SGE_Driver_Init() and SGE_Driver_Resume(), will
//!   invoke SGE_Usercall_BeginStream() to begin playback; this function is
//!   user-defined for the purposes of portability.
struct SGE_Driver_t;
SGE_DECLSPEC uint32_t SGE_Driver_InitDefault(
	struct SGE_Driver_t *Driver,
	uint8_t  VoxCnt,
	uint16_t RateHz,
	uint8_t  BufCnt,
	SGE_MixSmp_t *MixBuf
);

/************************************************/

#ifdef __GBA__

//! SGE_Driver_Sync(Driver)
//! Description: Synchronize hardware playback.
//! Arguments:
//!   Driver: Driver work area.
//! Returns: Nothing; playback synchronized.
//! Notes:
//!  -Because the GBA doesn't loop sound buffers automatically, this function
//!   is needed to reset the DMA stream upon reaching the end of buffer.
//!  -Timing of this function is EXTREMELY critical (leeway of 1596 cycles
//!   for 10512Hz, down to 399 cycles for 42048Hz). It is not so much /when/
//!   this function is called that matters, but rather the consistency of
//!   the timing. For best results, it should work best to call this function
//!   as soon as the interrupt handler passes control to the user function,
//!   as this should have guaranteed consistent timing.
//!  -This function returns SGE_DRIVER_STATE_READY in r3 on success.
struct SGE_Driver_t;
SGE_DECLSPEC void SGE_Driver_Sync(struct SGE_Driver_t *Driver);

#else

//! SGE_Usercall_BeginStream(BufL, BufR, Length, RateHz)
//! Description: Begin playback of streaming buffers.
//! Arguments:
//!  BufL:   Left buffer memory.
//!  BufR:   Right buffer memory.
//!  Length: Length of each buffer (in samples).
//!  RateHz: Intended playback rate of the stream.
//! Returns: This function must return NULL on failure, or a "stream token" to
//! associate the stream with on success. This token can be any value that will
//! fit in a void* type, and will be passed to SGE_Usercall_StopStream().
//! Notes:
//!  -This is a USER-DEFINED FUNCTION. The SGE driver itself does NOT handle
//!   any portion of updating or streaming, and only synthesizes the output
//!   audio to buffers.
void *SGE_Usercall_BeginStream(SGE_OutSmp_t *BufL, SGE_OutSmp_t *BufR, uint32_t Length, uint32_t RateHz);

//! SGE_Usercall_StopStream(Token)
//! Description: Stop playback of streaming buffers.
//! Arguments:
//!  Token: Token associated with the stream to stop.
//! Returns: Nothing; stream playback must stop.
//! Notes:
//!  -This is a USER-DEFINED FUNCTION.
void SGE_Usercall_StopStream(void *Token);

#endif

/************************************************/
#ifdef SGE_INTERNALS
/************************************************/

//! SGE_Exp2fxp(x)
//! Description: Base-2 exponentiation 2^x.
//! Arguments:
//!   x: Exponent (in 5.27fxp).
//! Returns: 2^x.
//! Notes:
//!  -This routine is only approximate (~103dB PSNR).
//!  -This function is exported because it may be useful elsewhere.
SGE_DECLSPEC uint32_t SGE_Exp2fxp(uint32_t x);

/************************************************/
#endif
/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
#endif
/************************************************/

//! Include the core definitions
#include "SGE.h"

/************************************************/
//! EOF
/************************************************/
