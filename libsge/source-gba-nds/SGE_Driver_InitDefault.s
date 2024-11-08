/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: &Driver
@ r1:  VoxCnt
@ r2:  RateHz
@ r3:  BufCnt
@ sp+00h: &MixBuf

ASM_FUNC_GLOBAL(SGE_Driver_InitDefault)
ASM_FUNC_BEG   (SGE_Driver_InitDefault, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_InitDefault:
	PUSH	{r4-r5,lr}
#ifdef __GBA__
	LDR	r4, =GBA_FRAME_CYCLES/64 @ (lower 6 bits are 0)
#else
	LDR	r4, =35055               @ FRAME_CYCLES / HW_RATE [.21fxp]
#endif
	MOV	r5, r2                   @ RawBufLen = RateHz * RecpFrameRate -> r5
	MUL	r5, r4
	MOV	r4, #0x00
#ifdef __GBA__
	LSR	r5, #(24-6+4)            @ BufLen = Round[RawBufLen / 16] * 16
	ADC	r4, r5                   @ NOTE: HW_RATE = 2^24, so this becomes easier
	LSL	r4, #0x04
#else
	LSR	r5, #(21+1)              @ BufLen = Round[RawBufLen / 2] * 2
	ADC	r4, r5
	LSL	r4, #0x01
#endif
	LDR	r5, [sp, #0x0C]
	PUSH	{r4-r5}
	BL	SGE_Driver_Init
	ADD	sp, #0x08
	POP	{r4-r5}
	POP	{r3}
	BX	r3

ASM_FUNC_END(SGE_Driver_InitDefault)

/************************************************/
//! EOF
/************************************************/
