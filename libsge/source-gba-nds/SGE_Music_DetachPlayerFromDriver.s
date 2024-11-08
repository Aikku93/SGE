/************************************************/
#include "SGE-AsmMacros.h"
/************************************************/

@ r0: &Player

ASM_FUNC_GLOBAL(SGE_Music_DetachPlayerFromDriver)
ASM_FUNC_BEG   (SGE_Music_DetachPlayerFromDriver, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_DetachPlayerFromDriver:
	PUSH	{r0,lr}
	LDR	r0, [r0, #0x14] @ <- Need synchronization
	BL	SGE_CriticalSection_Enter
	MOV	ip, r0
	POP	{r1}
0:	LDR	r0, [r1, #0x14] @ Driver -> r0?
	LDR	r2, [r1, #0x18] @ Prev -> r2
	LDR	r3, [r1, #0x1C] @ Next -> r3
	CMP	r0, #0x00
	BEQ	.LExit
0:	CMP	r2, #0x00       @ Prev != NULL?
	BEQ	2f
1:	STR	r3, [r2, #0x1C] @  Y: Prev.Next = Next
	B	0f
2:	STR	r3, [r0, #0x10] @  N: Driver.PlayerList = Next
0:	CMP	r3, #0x00       @ Next != NULL?
	BEQ	0f
1:	STR	r2, [r3, #0x18] @  Y: Next.Prev = Prev
0:	MOV	r2, #0x00
	STR	r2, [r1, #0x14] @ Driver = NULL

.LExit:
	PUSH	{r0-r1}
	@MO	r0, r0
	MOV	r1, ip
	BL	SGE_CriticalSection_Leave
0:	POP	{r0-r1}         @ Stop all voices
	BL	SGE_Music_KillPlayerVoices
	POP	{r3}
	BX	r3

ASM_FUNC_END(SGE_Music_DetachPlayerFromDriver)

/************************************************/
//! EOF
/************************************************/
