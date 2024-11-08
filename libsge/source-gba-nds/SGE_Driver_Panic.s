/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_Panic)
ASM_FUNC_BEG   (SGE_Driver_Panic, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_Panic:
	LDRB	r1, [r0, #0x0A] @ nVox -> r1
	ADD	r0, #SGE_DRIVER_HEADER_SIZE
	MOV	r2, #0x00
1:	STRB	r2, [r0, #0x00] @ Vox.Stat = 0
	ADD	r0, #SGE_VOX_SIZE
	SUB	r1, #0x01
	BHI	1b
2:	BX	lr

ASM_FUNC_END(SGE_Driver_Panic)

/************************************************/
//! EOF
/************************************************/
