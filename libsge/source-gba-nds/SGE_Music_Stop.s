/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_Stop)
ASM_FUNC_BEG   (SGE_Music_Stop, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_Stop:
	MOV	r1, #0x00
	STR	r1, [r0, #0x10] @ Player.Song = NULL
	@B	SGE_Music_StopPlayerVoices

ASM_FUNC_END(SGE_Music_Stop)

/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_StopPlayerVoices)
ASM_FUNC_BEG   (SGE_Music_StopPlayerVoices, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_StopPlayerVoices:
	CMP	r0, #0x00         @ Player == NULL?
	BEQ	.Lbxlr
0:	MOV	r3, lr
	PUSH	{r3-r7}
	MOV	r4, r0            @ Player -> r4
	LDR	r0, [r4, #0x14]   @ <- Need synchronization
	BL	SGE_CriticalSection_Enter
	MOV	ip, r0
0:	LDR	r0, [r4, #0x14]   @ Driver -> r0?
	CMP	r0, #0x00
	BEQ	3f
0:	LDRB	r2, [r0, #0x0A]   @ nVox -> r2
	ADD	r0, #SGE_DRIVER_HEADER_SIZE
1:	LDR	r3, [r0, #0x2C]   @ Vox.Ply == Player?
	CMP	r3, r4
	BNE	2f
	LDRB	r3, [r0, #0x00]   @ Check for KEYOFF or NOPLAYER, and then set flags
	LSL	r5, r3, #0x20-5
	BCS	2f
	BMI	2f
	ADD	r3, #SGE_VOX_STAT_KEYOFF|SGE_VOX_STAT_NOPLAYER
	STRB	r3, [r0, #0x00]
	LDR	r3, [r0, #0x28]   @ Trk -> r3
	LDRB	r5, [r3, #0x10+2] @ Trk.Vol -> r5
	LDRB	r6, [r3, #0x14+2] @ Trk.Exp -> r6
	LDRB	r7, [r4, #0x01]   @ Ply.Vol -> r7
	MUL	r5, r6
	LDRB	r6, [r3, #0x18+2] @ Trk.Pan -> r6
	MUL	r5, r7
	LDRH	r7, [r3, #0x1C+2] @ Trk.Bnd -> r7
	LSR	r5, #(21-7)       @ Vol = Ply.Vol * Trk.Vol * Trk.Exp -> 1.7fxp
	MOV	r3, #0x7F         @ Unbias the bend value
	LSL	r3, #0x07
	SUB	r7, r3
	ADD	r7, r7            @ Convert Bnd to 8.8fxp
	LSL	r7, #0x08         @ Vol | Pan<<8 | Bnd<<16 -> r7
	ORR	r7, r6
	LSL	r7, #0x08
	ORR	r7, r5
	STR	r7, [r0, #0x2C]
2:	ADD	r0, #SGE_VOX_SIZE
	SUB	r2, #0x01         @ --nVox?
	BNE	1b
3:	LDR	r0, [r4, #0x14]
	MOV	r1, ip
	BL	SGE_CriticalSection_Leave
	POP	{r3-r7}
	BX	r3
.Lbxlr:	BX	lr

ASM_FUNC_END(SGE_Music_StopPlayerVoices)

/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_Pause)
ASM_FUNC_BEG   (SGE_Music_Pause, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_Pause:
	LDRH	r1, [r0, #0x02] @ Exit when already paused
	CMP	r1, #0x00
	BEQ	.Lbxlr
0:	MOV	ip, r0
	MOV	r1, #0x00
	STRH	r1, [r0, #0x02] @ TempoStretch = PAUSE

.LConvertToOffsets:
	LDR	r1, [r0, #0x10] @ Song -> r1
	LDRB	r2, [r0, #0x00] @ nTracks -> r2
	ADD	r0, #SGE_PLAYER_HEADER_SIZE
1:	LDR	r3, [r0, #0x28] @ Check Src != NULL
	CMP	r3, #0x00
	BEQ	2f
	SUB	r3, r1
	STR	r3, [r0, #0x28] @ Adjust Src
.macro AdjStackAddr i=0
.if (\i < SGE_MAX_STACK_DEPTH)
	LDR	r3, [r0, #0x2C + 0x04*(\i)]
	SUB	r3, r1
	STR	r3, [r0, #0x2C + 0x04*(\i)]
	AdjStackAddr (\i+1)
.endif
.endm
	AdjStackAddr @ Adjust stack pointers
2:	ADD	r0, #SGE_TRACK_SIZE
	SUB	r2, #0x01       @ --nTracks?
	BNE	1b

.LStopVoices:
	MOV	r0, ip
	B	SGE_Music_StopPlayerVoices

ASM_FUNC_END(SGE_Music_Pause)

/************************************************/
//! EOF
/************************************************/
