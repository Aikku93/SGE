/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Wav

ASM_FUNC_GLOBAL(SGE_FileDb_UnloadWave)
ASM_FUNC_BEG   (SGE_FileDb_UnloadWave, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_UnloadWave:
	CMP	r0, #0x00
	BEQ	.LExit_Fail
0:	LDR	r1, [r0, #0x10] @ Wav.Db -> r1
	LDRH	r0, [r0, #0x02] @ WaveIdx -> r0
	LSR	r2, r1, #0x01   @ Check dbLink.isRaw
	BCS	.LExit_Fail
	@B	SGE_FileDb_UnloadWaveByIndex

ASM_FUNC_END(SGE_FileDb_UnloadWave)

/************************************************/

@ r0:  Index
@ r1: &Db

ASM_FUNC_GLOBAL(SGE_FileDb_UnloadWaveByIndex)
ASM_FUNC_BEG   (SGE_FileDb_UnloadWaveByIndex, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_UnloadWaveByIndex:
	LDR	r3, [r1, #0x04] @ Return FreeInstance(WaveIdx, nWaves, WaveTab, Callbacks)
	LDR	r2, [r1, #0x08]
	LDRH	r1, [r1, #0x00]
	@MOV	r0, r0
	B	SGE_FileDb_FreeInstance

.LExit_Fail:
	MOV	r0, #0x00       @ Return FALSE
	BX	lr

ASM_FUNC_END(SGE_FileDb_UnloadWaveByIndex)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
