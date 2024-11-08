/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: &Db
@ r1:  Idx

ASM_FUNC_GLOBAL(SGE_Db_GetSong)
ASM_FUNC_BEG   (SGE_Db_GetSong, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Db_GetSong:
	LDR	r2, [r0, #0x00]
	LDR	r3, =SGE_DB_MAGIC
	CMP	r2, r3            @ Invalid signature?
	BNE	1f
0:	LDRH	r3, [r0, #0x06]   @ nSong -> r3
	LDR	r2, [r0, #0x0C]   @ Return Db + SongTab[Idx].Offs
	CMP	r1, r3            @ Out of range?
	BCS	1f
	LSL	r1, #0x02
	ADD	r1, r0
	LDR	r3, [r2, r1]
	ADD	r0, r3
	BX	lr
1:	MOV	r0, #0x00         @ Return NULL on failure
	BX	lr

ASM_FUNC_END(SGE_Db_GetSong)

/************************************************/
//! EOF
/************************************************/
