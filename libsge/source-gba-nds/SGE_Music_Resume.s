/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_Resume)
ASM_FUNC_BEG   (SGE_Music_Resume, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_Resume:
	LDR	r1, [r0, #0x10] @ Song = Player.Song? (ie. check if Resume was called when nothing was playing)
	MOV	r2, #0x01       @ TempoStretch = 100%
	CMP	r1, #0x00
	BEQ	.Lbxlr
	LSL	r2, #0x08
	@B	SGE_Music_ResumeEx

ASM_FUNC_END(SGE_Music_Resume)

/************************************************/

@ r0: &Player
@ r1: &Song
@ r2:  TempoStretch

ASM_FUNC_GLOBAL(SGE_Music_ResumeEx)
ASM_FUNC_BEG   (SGE_Music_ResumeEx, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_ResumeEx:
	LDRH	r3, [r0, #0x02] @ Ensure track is paused
	CMP	r3, #0x00
	BNE	.Lbxlr
	CMP	r2, #0x00       @ If TempoStretch == 0, we update the song pointer but do nothing else
	BEQ	.LExit_SetSongPtr
0:	MOV	r3, lr
	PUSH	{r3-r5}
	MOV	r3, r0
	ADD	r3, #SGE_PLAYER_HEADER_SIZE
	LDRB	r4, [r0, #0x00] @ nTracks -> r4
1:	LDR	r5, [r3, #0x28] @ Fix Src
	BL	.LFixAddr
	STR	r5, [r3, #0x28]
.macro FixStackAddr i=0
.if (\i < SGE_MAX_STACK_DEPTH)
	LDR	r5, [r3, #0x2C + 0x04*(\i)]
	BL	.LFixAddr
	STR	r5, [r3, #0x2C + 0x04*(\i)]
	FixStackAddr (\i+1)
.endif
.endm
	FixStackAddr @ Fix stack pointers
2:	ADD	r3, #SGE_TRACK_SIZE
	SUB	r4, #0x01       @ --nTracks?
	BNE	1b
3:	STRH	r2, [r0, #0x02] @ Store TempoStretch, and playback resumes
	POP	{r3-r5}

.LExit_SetSongPtr:
	STR	r1, [r0, #0x10] @ Player.Song = Song
	BX	r3

.LFixAddr:
	CMP	r5, #0x00       @ Addr != NULL: Apply correction
	BEQ	.Lbxlr
	ADD	r5, r1
.Lbxlr:	BX	lr

ASM_FUNC_END(SGE_Music_ResumeEx)

/************************************************/
//! EOF
/************************************************/
