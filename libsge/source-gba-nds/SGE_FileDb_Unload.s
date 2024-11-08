/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db

ASM_FUNC_GLOBAL(SGE_FileDb_Unload)
ASM_FUNC_BEG   (SGE_FileDb_Unload, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_Unload:
	MOV	r3, lr
	PUSH	{r3-r6}
	MOV	r4, r0            @ Db -> r4
	LDRH	r2, [r4, #0x00]   @ nWaves -> r2
	LDRH	r3, [r4, #0x02]   @ nSongs -> r3
	LDR	r5, [r4, #0x08]   @ NextItem = InstanceTable -> r5
	ADD	r6, r2, r3        @ nItemsRem = nWaves+nSongs -> r6?
	BEQ	.LExit

.LUnloadItems:
0:	LDR	r0, [r5, #0x04]   @ Deallocate item
	LSR	r1, r0, #0x01     @ NOTE: Check for not-loaded, and Persistent flag
	BEQ	1f
	BCS	1f
	LDR	r1, [r4, #0x04]
	BL	SGE_FileDb_Free
1:	ADD	r5, #0x08         @ Move to next item
	SUB	r6, #0x01         @ --nItemsRem?
	BNE	0b
2:	LDR	r0, [r4, #0x08]   @ Deallocate the instance table
	LDR	r1, [r4, #0x04]
	BL	SGE_FileDb_Free
3:	STR	r6, [r4, #0x00]   @ nWaves = nSongs = 0

.LExit:
	POP	{r3-r6}
	BX	r3

ASM_FUNC_END(SGE_FileDb_Unload)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
