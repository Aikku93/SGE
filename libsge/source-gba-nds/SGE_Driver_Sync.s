/************************************************/
#ifdef __GBA__
/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_Sync)
ASM_FUNC_BEG   (SGE_Driver_Sync, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_Sync:
	LDR	r3, [r0, #0x00]           @ State -> r3
	LDR	r2, =SGE_DRIVER_STATE_READY
	CMP	r3, r2                    @ Invalid state?
	BNE	3f
0:	LDRB	r1, [r0, #0x08]           @ BfIdx -> r1
	LDRB	r2, [r0, #0x09]           @ BfCnt -> r2
	ADD	r1, #0x01                 @ ++BfIdx >= BfCnt?
	CMP	r1, r2
	BCC	2f
1:	LDR	r2, =REG_DMACNT(1)        @ Reset DMA
	MOV	r1, #0xB6                 @ DST_INC | SRC_INC | REPEAT | DATA32 | MODE_SOUNDFIFO | ENABLE
	LSL	r1, #0x18                 @ <- This also sets the lower 8 bits to 0, to get BfIdx=0
	STRH	r1, [r2, #REG_DMACNT_H(1) - REG_DMACNT(1)]
	STRH	r1, [r2, #REG_DMACNT_H(2) - REG_DMACNT(1)]
	STR	r1, [r2, #REG_DMACNT  (1) - REG_DMACNT(1)]
	STR	r1, [r2, #REG_DMACNT  (2) - REG_DMACNT(1)]
2:	STRB	r1, [r0, #0x08]           @ Store BfIdx
3:	BX	lr

ASM_FUNC_END(SGE_Driver_Sync)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
