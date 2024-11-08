/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_REVERB
/************************************************/

@ r0: &ReverbParam
@ r1: &ReverbData
@ Assumes that ReverbData can hold everything

ASM_FUNC_GLOBAL(SGE_Reverb_InitReverbData)
ASM_FUNC_BEG   (SGE_Reverb_InitReverbData, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Reverb_InitReverbData:
	MOV	r3, lr
#if (defined(SGE_PLATFORM_HAVE_FANCY_REVERB) || defined(__NDS__) || (SGE_ALLPASS_BITS > 8))
	PUSH	{r3-r7}
#else
	PUSH	{r3-r6}
#endif
	MOV	r4, r0                 @ ReverbParam -> r4
	MOV	r5, r1                 @ ReverbData -> r5
1:	@MOV	r0, r4                 @ Store line lengths in work area temporarily
	@MOV	r1, r5
	BL	SGE_Reverb_GetLineLengths
	CMP	r0, #0x00              @ <- On failure, return error code
	BLE	.LExit
	LDRB	r1, [r4, #0x00]        @ nLines*2 -> r1 (*2 to account for L+R)
	MOV	r0, r5                 @ LineLengths[] -> r0
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	MOV	r2, #0x0C              @ CurLine = &Data.Lines[nLines] -> r2 (past the end)
	LSL	r1, #0x01              @ ^ We have to write backwards in case LineLengths[] overwrites the line structs
	MUL	r2, r1
	ADD	r2, #0x04
#else
	LSL	r1, #0x01
	LSL	r2, r1, #0x03
	ADD	r2, #0x0C
#endif
	ADD	r2, r5
	LSL	r3, r1, #0x01          @ Seek LineLengths[] to the last line
	ADD	r0, r3
	MOV	ip, r2                 @ LineBufferData = &Data.Lines[nLines] -> ip (past the end of the line structs)
	MOV	r3, ip                 @ CurLineBuffer = LineBufferData -> r3
#if (defined(SGE_PLATFORM_HAVE_FANCY_REVERB) && defined(__GBA__) && (SGE_ALLPASS_BITS <= 8))
	MOV	r7, #0x00              @ SGE_ALLPASS_BITS > 8 sets r7=0 in the loop, so without it, we need to set it manually
#endif
1:	SUB	r0, #0x02              @ Seek one line back
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	SUB	r2, #0x0C
#else
	SUB	r2, #0x08
#endif
	LDRH	r6, [r0]               @ LineLen -> r6
#if (defined(__NDS__) || (SGE_ALLPASS_BITS > 8))
	LSR	r7, r6, #0x10          @ If Len >= 32768, then we need to rewind Line.Buf by one byte.
	BCC	0f                     @ This is because we access the buffer via:
	SUB	r3, #0x01              @  Addr = Line.Buf + (Line.Len|Line.Idx<<16) >> 15
0:		                       @ So Line.Len.bit15 will add to Addr.bit0 and we must compensate
#endif
	STR	r3, [r2, #0x00]        @ Line.Buf = CurLineBuffer
#if (defined(__NDS__) || (SGE_ALLPASS_BITS > 8))
	ADC	r3, r6                 @ CurLineBuffer += LineLen (and undo Line.Len.bit15 correction)
	ADD	r3, r6
#else
	ADD	r3, r6
#endif
	STRH	r6, [r2, #0x04]        @ Line.Len = LineLen
	STRH	r6, [r2, #0x06]        @ Line.Idx = LineLen
#ifdef SGE_PLATFORM_HAVE_FANCY_REVERB
	STR	r7, [r2, #0x08]        @ Line.zLp = 0
#endif
	SUB	r1, #0x01
	BNE	1b
1:	MOV	r0, ip                 @ Clear all lines to silence
	@MOV	r1, #0x00
	SUB	r2, r3, r0
#ifndef SGE_PLATFORM_HAVE_FANCY_REVERB
	STR	r1, [r5, #0x04]        @ zLpL = 0
	STR	r1, [r5, #0x08]        @ zLpR = 0
#endif
	BL	memset
1:	MOV	r0, r5                 @ Set feedback coefficient
	LDRH	r1, [r4, #0x06]
	BL	SGE_Reverb_SetFeedbackLevel
	MOV	r0, r5                 @ Set lowpass coefficient
	LDR	r1, [r4, #0x08]
	LDR	r2, [r4, #0x0C]
	BL	SGE_Reverb_SetLowpassCutoff
	LDRB	r0, [r4, #0x00]        @ Store number of lines and return that
	STRB	r0, [r5, #0x02]

.LExit:
#if (defined(SGE_PLATFORM_HAVE_FANCY_REVERB) || defined(__NDS__) || (SGE_ALLPASS_BITS > 8))
	POP	{r3-r7}
#else
	POP	{r3-r6}
#endif
	BX	r3

ASM_FUNC_END(SGE_Reverb_InitReverbData)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
