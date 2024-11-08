/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db
@ r1:  Idx
@ r2:  Persistent

ASM_FUNC_GLOBAL(SGE_FileDb_LoadWave)
ASM_FUNC_BEG   (SGE_FileDb_LoadWave, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_LoadWave:
	PUSH	{r0,r2,lr}
	LDRH	r3, [r0, #0x00] @ LoadInstance(Idx, nWaves, WaveTab, Callbacks)?
	LDR	r2, [r0, #0x08]
	MOV	r0, r1
	MOV	r1, r3
	LDR	r3, [sp, #0x00]
	LDR	r3, [r3, #0x04]
	BL	SGE_FileDb_LoadInstance
	POP	{r1,r2,r3}
	CMP	r0, #0x00
	BEQ	1f
	STR	r1, [r0, #0x10] @ Store Wave.dbLink.Db = Db
0:	CMP	r2, #0x00       @ Need to mark as Persistent?
	BEQ	1f
	LDRH	r2, [r0, #0x02] @ WaveIdx -> r2
	LDR	r1, [r1, #0x08] @ WaveTab -> r1
	LSL	r2, #0x03
	ADD	r1, r2          @ Seek to WaveTab[WaveIdx] -> r1
	MOV	r2, #0x01       @ Set Persistent bit in Data pointer
	ORR	r2, r0
	STR	r2, [r1, #0x04]
1:	BX	r3

ASM_FUNC_END(SGE_FileDb_LoadWave)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
