/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db
@ r1:  Idx

ASM_FUNC_GLOBAL(SGE_FileDb_GetSong)
ASM_FUNC_BEG   (SGE_FileDb_GetSong, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_GetSong:
	LDRH	r3, [r0, #0x02] @ GetInstance(Idx, nSongs, SongTab)
	LDR	r2, [r0, #0x0C]
	MOV	r0, r1
	MOV	r1, r3
	B	SGE_FileDb_GetInstance

ASM_FUNC_END(SGE_FileDb_GetSong)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
