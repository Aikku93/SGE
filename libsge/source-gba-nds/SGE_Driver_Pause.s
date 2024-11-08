/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_Pause)
ASM_FUNC_BEG   (SGE_Driver_Pause, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_Pause:
	LDR	r2, [r0, #0x00]           @ Magic -> r2
	LDR	r3, =SGE_DRIVER_STATE_PAUSED
	SUB	r2, r3                    @ Already paused?
	BEQ	2f
	SUB	r2, #SGE_DRIVER_STATE_READY - SGE_DRIVER_STATE_PAUSED
	BNE	2f                        @ Invalid state?
1:	STRB	r3, [r0, #0x00]           @ Mark paused
#ifdef __GBA__
	LDR	r2, =REG_SOUNDFIFO_A
	MOV	r3, #0x00                 @ Stop DMA and timer
	STR	r3, [r2, #REG_DMACNT(1) - REG_SOUNDFIFO_A]
	STR	r3, [r2, #REG_DMACNT(2) - REG_SOUNDFIFO_A]
	STR	r3, [r2, #REG_TIMER(SGE_HWTIMER_IDX) - REG_SOUNDFIFO_A]
#else
	LDR	r1, =SGE_Usercall_StopStream
	LDR	r0, [r0, #0x18+SGE_PLATFORM_STREAMTOKEN_OFFS]
	BX	r1
#endif
2:	BX	lr

ASM_FUNC_END(SGE_Driver_Pause)

/************************************************/
//! EOF
/************************************************/
