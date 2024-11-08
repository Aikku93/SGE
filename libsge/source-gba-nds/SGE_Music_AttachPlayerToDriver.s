/************************************************/
#include "SGE-AsmMacros.h"
/************************************************/

@ r0: &Driver
@ r1: &Player

ASM_FUNC_GLOBAL(SGE_Music_AttachPlayerToDriver)
ASM_FUNC_BEG   (SGE_Music_AttachPlayerToDriver, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_AttachPlayerToDriver:
	PUSH	{r0-r1,lr}
0:	MOV	r0, r1          @ First, detach player from its old driver if needed (safety)
	BL	SGE_Music_DetachPlayerFromDriver
0:	LDR	r0, [sp, #0x00] @ <- Need synchronization
	BL	SGE_CriticalSection_Enter
	MOV	ip, r0
	POP	{r0-r1}
0:	MOV	r2, #0x00       @ Prev = NULL -> r2
	LDR	r3, [r0, #0x10] @ Next = Driver.PlayerList -> r3
1:	CMP	r3, #0x00       @ Next != NULL?
	BEQ	2f
	MOV	r2, r3          @  Y: Prev = Next
	LDR	r3, [r3, #0x1C] @     Next = Next->Next
	B	1b
2:	STR	r0, [r1, #0x14] @ Player.Driver = Driver
	STR	r2, [r1, #0x18] @ Player.Prev = Prev
	STR	r3, [r1, #0x1C] @ Player.Next = Next
0:	CMP	r2, #0x00       @ Prev != NULL?
	BEQ	2f
1:	STR	r1, [r2, #0x1C] @  Y: Prev.Next = Player
	B	3f
2:	STR	r1, [r0, #0x10] @  N: Driver.PlayerList = Player
3:	@MOV	r0, r0
	MOV	r1, ip
	BL	SGE_CriticalSection_Leave
	POP	{r3}
	BX	r3

ASM_FUNC_END(SGE_Music_AttachPlayerToDriver)

/************************************************/
//! EOF
/************************************************/
