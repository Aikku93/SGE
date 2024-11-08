/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db
@ r1:  Idx

ASM_FUNC_GLOBAL(SGE_FileDb_GetWave)
ASM_FUNC_BEG   (SGE_FileDb_GetWave, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_GetWave:
	LDRH	r3, [r0, #0x00] @ GetInstance(Idx, nWaves, WaveTab)
	LDR	r2, [r0, #0x08]
	MOV	r0, r1
	MOV	r1, r3
	B	SGE_FileDb_GetInstance

ASM_FUNC_END(SGE_FileDb_GetWave)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
