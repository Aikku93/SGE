/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: VoxCnt
@ r1: BufCnt
@ r2: RateHz

ASM_FUNC_GLOBAL(SGE_Driver_GetWorkAreaSizeDefault)
ASM_FUNC_BEG   (SGE_Driver_GetWorkAreaSizeDefault, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_GetWorkAreaSizeDefault:
	MOV	r3, #0x01                @ RateHz too low or too high?
	LSL	r3, #0x08
	CMP	r2, r3
	BLS	.LExit_Error
	LSR	r3, r2, #0x10
#ifdef __GBA__
	LDR	r3, =GBA_FRAME_CYCLES/64 @ (lower 6 bits are 0)
#else
	LDR	r3, =35055               @ FRAME_CYCLES / HW_RATE [.21fxp]
#endif
	BNE	.LExit_Error
0:	MUL	r2, r3                   @ RawBufLen = RateHz * RecpFrameRate -> r2
	MOV	r3, #0x00
#ifdef __GBA__
	LSR	r2, #(24-6+4)            @ BufLen = Round[RawBufLen / 16] * 16
	ADC	r2, r3                   @ NOTE: HW_RATE = 2^24, so this becomes easier
	LSL	r2, #0x04
#else
	LSR	r2, #(21+1)              @ BufLen = Round[RawBufLen / 2] * 2
	ADC	r2, r3
	LSL	r2, #0x01
#endif
	B	SGE_Driver_GetWorkAreaSize

.LExit_Error:
	MOV	r0, #0x00
	BX	lr

ASM_FUNC_END(SGE_Driver_GetWorkAreaSizeDefault)

/************************************************/
//! EOF
/************************************************/
