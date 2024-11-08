/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_Kill)
ASM_FUNC_BEG   (SGE_Music_Kill, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_Kill:
	MOV	r1, #0x00
	STR	r1, [r0, #0x10] @ Player.Song = NULL
	@B	SGE_Music_KillPlayerVoices

ASM_FUNC_END(SGE_Music_Kill)

/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_KillPlayerVoices)
ASM_FUNC_BEG   (SGE_Music_KillPlayerVoices, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_KillPlayerVoices:
	CMP	r0, #0x00       @ Player == NULL?
	BEQ	.Lbxlr
0:	PUSH	{r0,lr}
	LDR	r0, [r0, #0x14] @ <- Need synchronization
	BL	SGE_CriticalSection_Enter
	MOV	ip, r0
	POP	{r0}
0:	LDR	r1, [r0, #0x14] @ Driver -> r1?
	CMP	r1, #0x00
	BEQ	3f
	LDRB	r2, [r1, #0x0A] @ nVox -> r2
	ADD	r1, #SGE_DRIVER_HEADER_SIZE
1:	LDR	r3, [r1, #0x2C] @ Vox.Ply == Player?
	CMP	r3, r0
	BNE	2f
	LDRB	r3, [r1, #0x00] @ Check for NOPLAYER
	LSL	r3, #0x20-4
	BCS	2f
	STRB	r3, [r1, #0x00] @  Y: Vox.Stat = 0
2:	ADD	r1, #SGE_VOX_SIZE
	SUB	r2, #0x01       @ --nVox?
	BNE	1b
3:	LDR	r0, [r0, #0x14]
	MOV	r1, ip
	BL	SGE_CriticalSection_Leave
	POP	{r3}
	BX	r3
.Lbxlr:	BX	lr

ASM_FUNC_END(SGE_Music_KillPlayerVoices)

/************************************************/
//! EOF
/************************************************/
