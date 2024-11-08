/************************************************/
#include "SGE-AsmMacros.h"
/************************************************/

@ r0: &Player
@ r1:  TrackCnt
@ r2: &ExtCallFunc

ASM_FUNC_GLOBAL(SGE_Music_InitializePlayer)
ASM_FUNC_BEG   (SGE_Music_InitializePlayer, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Music_InitializePlayer:
	STRB	r1, [r0, #0x00] @ Store nTracks
	STR	r2, [r0, #0x04] @ Store ExtCallFunc
	MOV	r3, #0x00
	STR	r3, [r0, #0x10] @ Song   = NULL
	STR	r3, [r0, #0x14] @ Driver = NULL
	STR	r3, [r0, #0x18] @ Prev   = NULL
	STR	r3, [r0, #0x1C] @ Next   = NULL
	BX	lr

ASM_FUNC_END(SGE_Music_InitializePlayer)

/************************************************/
//! EOF
/************************************************/
