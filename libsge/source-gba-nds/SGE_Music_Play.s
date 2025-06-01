/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: &Player
@ r1: &Song

ASM_FUNC_GLOBAL(SGE_Music_Play)
ASM_FUNC_BEG   (SGE_Music_Play, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_Play:
	MOV	r2, #0x80     @ MasterVolume = 100%
	LSL	r3, r2, #0x01 @ TempoStrech  = 100%
	@B	SGE_Music_PlayEx

ASM_FUNC_END(SGE_Music_Play)

/************************************************/

@ r0: &Player
@ r1: &Song
@ r2:  MasterVolume
@ r3:  TempoStretch

ASM_FUNC_GLOBAL(SGE_Music_PlayEx)
ASM_FUNC_BEG   (SGE_Music_PlayEx, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_PlayEx:
	PUSH	{r0-r3,r4-r7,lr}
	BL	SGE_Music_Stop  @ Stop target player first (safety)
	POP	{r0-r3}

.LInitPlayer:
	LDRB	r4, [r1, #0x00] @ nTracks = Song->nTrack -> r4?
	MOV	r5, #0x10       @ TrkOffs = &Song->TrkOffs[] -> r5
	ADD	r5, r1
	LDRB	r6, [r0, #0x00] @ Player->nTracks -> r6
	CMP	r4, #0x00       @ <- Exit when nTracks == 0
	BEQ	.LExit_Error
	SUB	r6, r4          @ nTracksToDisable -> r6?
	BCS	0f
	ADD	r4, r6          @  Y: nTracks = Player->nTracks?
	BEQ	.LExit_Error    @     <- Exit when Player->nTracks == 0
	MOV	r6, #0x00       @     nTracksToDisable = 0
0:	MOV	ip, r4
0:	CMP	r2, #0x80       @ MasterVolume = MIN(0x80, MasterVolume)
	BCC	0f
	MOV	r2, #0x80
0:	LSR	r4, r3, #0x10   @ TempoStretch = MIN(0xFFFF, TempoStretch)
	BEQ	0f
	MVN	r3, r4
	LSR	r3, #0x10
0:	MOV	lr, r3          @ If TempoStretch==0, then do NOT add Song to the pointers
	CMP	r3, #0x00
	BEQ	0f
	MOV	lr, r1
0:	MOV	r4, ip
	STRB	r2, [r0, #0x01] @ Store MasterVolume
	STRH	r3, [r0, #0x02] @ Store TempoStretch
	MOV	r3, #120
	LSL	r2, r3, #0x10
	STR	r2, [r0, #0x08] @ Store Tempo = 120, TimePhase = 0
	STR	r3, [r0, #0x0C]
	MOV	ip, r0          @ Player -> ip
	ADD	r0, #0x20       @ Track = &Player->Tracks[] -> r0

.LInitTracks:
1:	MOV	r2, #0x08       @ {NybbleSrc = 0, PortamentoEnable = 0, StackDepth = 0, Program = 0, Priority = 8}
	LSL	r2, #0x18
	MOV	r3, #0x05*12    @ {Octave = 5, Transpose = 0, Rest = 0}
	MOV	r7, #0x20       @ {LastTimeCode = 0, NoteLengthMul = 32/32, NoteLengthAdd = 0}
	LSL	r7, #0x10
	STMIA	r0!, {r2-r3,r7}
	LDR	r2, =0<<0 | 0<<8 | 100<<16 | 100<<24
	LDR	r7, =0<<0 | 0<<8 | 128<<16 | 128<<24
	MOV	r3, r2
	STMIA	r0!, {r2-r3,r7} @ Vel = 100, Vol = 100, Exp = 128
	MOV	r3, #0<<0 | 0<<8 | 0<<16
	LDR	r2, =0<<0 | 0<<8 | 64<<16 | 64<<24
	LSR	r7, r3, #0x10
	STMIA	r0!, {r2-r3,r7} @ Pan = 64, Bnd = 0, RepeatType = 0, NybbleStack = 0, PortamentoVoice = 0
	ADD	r0, #0x04       @ Skip portamento data
	LDMIA	r5!, {r3}
	ADD	r3, lr          @ Src = Song + TrkOffs[i]
	LSL	r7, r3, #(32-2) @ Pack nybble offset into high bits
	LSR	r3, #0x02
	LSL	r3, #0x02
	ORR	r3, r7
	STR	r3, [r0]
	ADD	r0, #SGE_TRACK_SIZE-0x28
	SUB	r4, #0x01       @ --nTracks?
	BHI	1b
2:	CMP	r6, #0x00       @ nTracksToDisable?
	BEQ	3f
20:	STR	r4, [r0, #0x28] @ Track.Src = NULL
	ADD	r0, #SGE_TRACK_SIZE
	SUB	r6, #0x01
	BNE	20b
3:

.LFinish:
	MOV	r0, ip
	STR	r1, [r0, #0x10] @ Player->Song = Song

.LExit:
	POP	{r4-r7}
	POP	{r3}
	BX	r3

.LExit_Error:
	MOV	r0, #0x00       @ Return NULL
	B	.LExit

ASM_FUNC_END(SGE_Music_PlayEx)

/************************************************/
//! EOF
/************************************************/
