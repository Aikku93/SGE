/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

@ r0: &ReverbParam
@ r1: &Dst (u16[][2])
@ Returns sum of all line lengths

ASM_FUNC_GLOBAL(SGE_Reverb_GetLineLengths)
ASM_FUNC_BEG   (SGE_Reverb_GetLineLengths, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Reverb_GetLineLengths:
	LDRB	r2, [r0, #0x00]  @ nTaps -> r2
	MOV	r3, lr
	PUSH	{r1-r7}
	LDRB	r4, [r0, #0x01]  @ RoomDensity -> r4
	LDRH	r5, [r0, #0x02]  @ StereoWidth -> r5
	LDR	r6, [r0, #0x0C]  @ RateHz -> r6

.LGetLineLengths_GetMaximalLength:
	MOV	r3, #0x01        @ DelayLast = 1.0 -> r3
	LSL	r3, #0x18
	SUB	r1, r2, #0x01    @ nTapsRem = nTaps-1 -> r1 (we just did this one)
	BCC	.LGetLineLengths_NoTaps
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	CMP	r2, #0x0100/(0x0C*2) @ Ensure nTaps*sizeof(Line_t[2]) <= 0xFF
#else
	CMP	r2, #0x0100/(0x08*2)
#endif
	BCS	.LGetLineLengths_TooLong
	MOV	r2, r3           @ DelayInvScale = DelayLast -> r2
	CMP	r1, #0x00
	BEQ	2f
1:	MUL	r3, r4           @ DelayLast *= RoomDensity
	LSR	r3, #0x08
	BEQ	.LGetLineLengths_TooSparse
	ADD	r2, r3           @ DelayInvScale += DelayLast
	SUB	r1, #0x01
	BNE	1b
2:	LDRH	r1, [r0, #0x04]  @ DecayTime -> r1
	LDRH	r3, [r0, #0x06]  @ Feedback -> r3
	LDR	r7, =559241      @ 559241 = 1/60 * 2^25
	MUL	r3, r1           @ DecayTime*Feedback -> r3
	BL	.LGetLineLengths_GetMaximalLength_64x64Mul  @ Numer -> r0,r1
	MOV	r3, #0x00        @ Denom = DelayInvScale
	BL	__aeabi_uldivmod @ LineLen = MaximalLineLength -> r2 [.17fxp] <- 64/64 division... ouch :(
	LSL	r2, r1, #0x20-(17-8) @ Chop precision to .8fxp (with rounding)
	MOV	r3, #0x00
	LSR	r1, #(17-8)
	LSR	r0, #(17-8)
	ADC	r0, r2
	ADC	r1, r3
	@CMP	r1, #0x00        @ LineLen /way/ too long?
	BNE	.LGetLineLengths_TooLong
	LSR	r1, r0, #0x08    @ LineLen too long? (hard limit of 65536 samples)
	SUB	r1, #0x01
	LSR	r1, #0x10
	BNE	.LGetLineLengths_TooLong

.LGetLineLengths_GetLinesFromMaximalLength:
	POP	{r1,r3}          @ Dst -> r1, nTapsRem = nTaps -> r3
	MUL	r6, r5           @ Spread = RateHz * StereoWidth / 1000 -> r5 [.8fxp]
	MOV	r2, #0x00        @ TotalLength = 0 -> r2
	LSR	r5, r6, #0x06    @ (x/1000 ~= x * (1+2^-6)(1+2^-7)(1+2^-11)*2^-10)
	ADD	r6, r5
	LSR	r5, r6, #0x07
	ADD	r6, r5
	LSR	r5, r6, #0x0B
	ADD	r6, r5
	LSR	r5, r6, #0x0A
	CMP	r1, #0x00        @ Seek to last line (if storing output)
	BEQ	1f
	LSL	r6, r3, #0x01+1
	ADD	r1, r6
1:	SUB	r6, r0, r5       @ LengthR = LineLen -/+ Spread
	ASR	r6, #0x08
	SUB	r7, r6, #0x01
	ASR	r7, #0x10
	BNE	.LGetLineLengths_TooWide
	ADD	r2, r6           @ TotalLength += LengthR
	CMP	r1, #0x00
	BEQ	0f
	SUB	r1, #0x02
	STRH	r6, [r1]
0:	ADD	r6, r0, r5       @ LengthL = LineLen +/- Spread
	ASR	r6, #0x08
	SUB	r7, r6, #0x01
	ASR	r7, #0x10
	BNE	.LGetLineLengths_TooWide
	ADD	r2, r6           @ TotalLength += LengthL
	CMP	r1, #0x00
	BEQ	0f
	SUB	r1, #0x02
	STRH	r6, [r1]
0:	MUL	r0, r4           @ LineLen *= RoomDensity (with rounding)
	MUL	r5, r4           @ Spread  *= RoomDensity (with rounding)
	ADD	r0, #0x80
	ADD	r5, #0x80
	LSR	r0, #0x08
	ASR	r5, #0x08
	NEG	r5, r5           @ Spread = -Spread (to flip the spread on the next tap)
	SUB	r3, #0x01        @ --nTaps?
	BNE	1b
0:	MOV	r0, r2           @ Return TotalLength
.LExit_POP_r3_r7:
	POP	{r3-r7}
	BX	r3

.LGetLineLengths_NoTaps:
.LGetLineLengths_TooShort:
	MOV	r0, #-SGE_REVERB_ERROR_TOO_SHORT
.LExit_NEG_r0_r0_POP_r1_r7:
	NEG	r0, r0
	POP	{r1-r7}
	BX	r3

.LGetLineLengths_TooSparse:
	MOV	r0, #-SGE_REVERB_ERROR_TOO_SPARSE
	B	.LExit_NEG_r0_r0_POP_r1_r7

.LGetLineLengths_TooLong:
	MOV	r0, #-SGE_REVERB_ERROR_TOO_LONG
	B	.LExit_NEG_r0_r0_POP_r1_r7

.LGetLineLengths_TooWide:
	MOV	r0, #-SGE_REVERB_ERROR_TOO_WIDE
	NEG	r0, r0
	B	.LExit_POP_r3_r7

ASM_ALIGN(4)
.LGetLineLengths_GetMaximalLength_64x64Mul:
	BX	pc
	NOP
ASM_MODE_ARM
	UMULL	r7, ip, r6, r7    @ RateHz * 559241 * DecayTime * Feedback / DelayInvScale >> 8 [.9fxp output]
	UMULL	r0, r1, r7, r3    @ 32.0[RateHz] + .25[Scale] + .8[DecayTime] + .8[Feedback] - .24[DelayInvScale] = 32.17fxp
	MLA	r1, ip, r3, r1
	BX	lr

ASM_FUNC_END(SGE_Reverb_GetLineLengths)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
