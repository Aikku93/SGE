/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_UnderrunRecover)
ASM_FUNC_BEG   (SGE_Driver_UnderrunRecover, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_UnderrunRecover:
	MOV	r3, lr
	PUSH	{r3-r5}
0:	LDRB	r2, [r0, #0x0B]             @ Driver.BfIdxW -> r2
	LDRB	r4, [r0, #0x09]             @ Driver.BfCnt  -> r4
	LDRH	r5, [r0, #0x0E]             @ Driver.BufLen -> r5
	ADD	r1, r2, #0x01               @ Store BfIdxW = WRAP(BfIdxW+1)
	SUB	r1, r4
	BCS	0f
	ADD	r1, r4
0:	STRB	r1, [r0, #0x0B]
	MUL	r2, r5                      @ Offs = BfIdx*BufLen -> r2
	MUL	r4, r5                      @ OffsR = BufLen*BfCnt -> r4
	LDRB	r1, [r0, #0x0A]             @ VoxCnt -> r1
	MOV	r3, #SGE_VOX_SIZE
	ADD	r0, #SGE_DRIVER_HEADER_SIZE @ &Driver.Vox[] -> r0
	MUL	r1, r3
	ADD	r0, r1                      @ DstL = &Driver.OutBuf[] -> r0
#ifdef __GBA__
# if SGE_USE_OVERSAMPLING
	LSL	r4, #0x01
	LSL	r2, #0x01
# endif
#else
# if SGE_USE_OVERSAMPLING
	LSL	r4, #0x01+1
	LSL	r2, #0x01+1
# else
	LSL	r4, #0x01
	LSL	r2, #0x01
# endif
#endif
	ADD	r0, r2                      @ Seek to current target buffer
0:	@ADD	r0, #0x00                   @ Clear left buffer
	MOV	r1, #0x00
#ifdef __GBA__
# if SGE_USE_OVERSAMPLING
	LSL	r2, r5, #0x01
# else
	MOV	r2, r5
# endif
#else
# if SGE_USE_OVERSAMPLING
	LSL	r2, r5, #0x01+1
# else
	LSL	r2, r5, #0x01
# endif
#endif
	BL	memset
0:	ADD	r0, r4                      @ Clear right buffer
	MOV	r1, #0x00
#ifdef __GBA__
# if SGE_USE_OVERSAMPLING
	LSL	r2, r5, #0x01
# else
	MOV	r2, r5
# endif
#else
# if SGE_USE_OVERSAMPLING
	LSL	r2, r5, #0x01+1
# else
	LSL	r2, r5, #0x01
# endif
#endif
	BL	memset
0:	POP	{r3-r5}
	BX	r3

ASM_FUNC_END(SGE_Driver_UnderrunRecover)

/************************************************/
//! EOF
/************************************************/
