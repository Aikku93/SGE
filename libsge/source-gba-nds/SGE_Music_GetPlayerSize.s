/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: TrackCnt

ASM_FUNC_GLOBAL(SGE_Music_GetPlayerSize)
ASM_FUNC_BEG   (SGE_Music_GetPlayerSize, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_GetPlayerSize:
	MOV	r2, #SGE_TRACK_SIZE
	SUB	r3, r0, #0x01 @ Verify TrackCnt
	CMP	r3, #0xFF-1
	BCS	.LExit_Error
1:	MUL	r0, r2        @ Return PLAYER_HEADER_SIZE + TrackCnt*TRACK_SIZE
	ADD	r0, #SGE_PLAYER_HEADER_SIZE
	BX	lr

.LExit_Error:
	MOV	r0, #0x00     @ Return Size=0 on error
	BX	lr

ASM_FUNC_END(SGE_Music_GetPlayerSize)

/************************************************/
//! EOF
/************************************************/
