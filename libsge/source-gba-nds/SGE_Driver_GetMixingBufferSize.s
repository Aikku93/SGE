/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: RateHz

ASM_FUNC_GLOBAL(SGE_Driver_GetMixingBufferSizeDefault)
ASM_FUNC_BEG   (SGE_Driver_GetMixingBufferSizeDefault, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_GetMixingBufferSizeDefault:
#ifdef __GBA__
	LDR	r1, =GBA_FRAME_CYCLES/64 @ (lower 6 bits are 0)
#else
	LDR	r1, =35055               @ FRAME_CYCLES / HW_RATE [.21fxp]
#endif
	MOV	r2, #0x01                @ RateHz too low?
	LSL	r2, #0x08
	CMP	r0, r2
	BLS	.LExit_Error
0:	MUL	r1, r0                   @ RawBufLen = RateHz * RecpFrameRate -> r1
	LSR	r0, #0x10                @ RateHz too high?
	BNE	.LExit_Error
#ifdef __GBA__
	LSR	r1, #(24-6+4)            @ BufLen = Round[RawBufLen / 16] * 16
	ADC	r0, r1                   @ NOTE: HW_RATE = 2^24, so this becomes easier
	LSL	r0, #0x01+1+4            @ Return sizeof(u16[2]) * BufLen
#else
	LSR	r1, #0x15+1              @ BufLen = Round[RawBufLen / 2] * 2
	ADC	r0, r1
	LSL	r0, #0x02+1+1            @ Return sizeof(s32[2]) * BufLen
#endif
	BX	lr

.LExit_Error:
	MOV	r0, #0x00                @ Return Size=0 on error
	BX	lr

ASM_FUNC_END(SGE_Driver_GetMixingBufferSizeDefault)

/************************************************/
//! EOF
/************************************************/
