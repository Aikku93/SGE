/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: &Driver
@ r1:  VoxCnt
@ r2:  RateHz
@ r3:  BufCnt
@ sp+00h:  BufLen
@ sp+04h: &MixBuf

ASM_FUNC_GLOBAL(SGE_Driver_Init)
ASM_FUNC_BEG   (SGE_Driver_Init, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_Init:
	PUSH	{r4,lr}
1:	CMP	r1, #0x00                   @ !VoxCnt?
	BEQ	.LExit_Error
#if (defined(__GBA__) && SGE_MIXER_VOLFRACBITS > 0)
	CMP	r1, #(1 << (8-(SGE_MIXER_VOLFRACBITS+1))) @ Too many voices? NOTE: +1 bit because volume can be FFh/80h == 1.99
#else
	CMP	r1, #0xFF
#endif
	BHI	.LExit_Error
	LSR	r4, r2, #0x06               @ RateHz too low (<8000Hz) or too high?
	CMP	r4, #(8000 >> 6)
	BCC	.LExit_Error
	LSR	r4, r2, #0x10
	BNE	.LExit_Error
#if (!defined(SGE_PLATFORM_HAVE_REVERB) && defined(SGE_PLATFORM_HAVE_FAKE_REVERB))
	STR	r4, [r0, #0x14]             @ Clear reverb control
#endif
	LDR	r4, [sp, #0x0C]             @ MixBuf -> r4
	CMP	r3, #0x02                   @ Invalid BufCnt?
	BCC	.LExit_Error
	CMP	r3, #0xFF
	BHI	.LExit_Error
	CMP	r4, #0x00                   @ No MixBuf?
	BEQ	.LExit_Error

.LPrepareWorkArea:
	STR	r4, [r0, #0x04]             @ Store MixBuf
	LDR	r4, [sp, #0x08]             @ BufLen -> r4
	STRB	r3, [r0, #0x09]             @ Store BufCnt
	STRB	r1, [r0, #0x0A]             @ Store VoxCnt
	STRH	r2, [r0, #0x0C]             @ Store RateHz
	STRH	r4, [r0, #0x0E]             @ Store BufLen
#ifdef __GBA__
	MUL	r3, r4
	LSL	r3, #(32-4)                 @ Ensure (BufLen*BufCnt) % 16 == 0
	BNE	.LExit_Error
# if SGE_USE_VOLSUBDIV
	LSL	r3, r4, #(32-SGE_VOLSUBDIV_LOG2MAXSUBDIV)
	BNE	.LExit_Error                @ Ensure BufLen%SUBDIV == 0
# endif
#else
	AND	r3, r4                      @ Ensure (BufLen*BufCnt) % 2 == 0
	LSR	r3, #0x01
	BCS	.LExit_Error
#endif
1:	ADD	r0, #0x10                   @ Clear remaining parts of Driver and voices
	MOV	r3, #SGE_VOX_SIZE
	MUL	r1, r3
	MOV	r2, #SGE_DRIVER_HEADER_SIZE-0x10
	ADD	r2, r1
	MOV	r1, #0x00
	BL	memset
2:	LDR	r1, =SGE_DRIVER_STATE_PAUSED
	SUB	r0, #0x10
	STR	r1, [r0, #0x00]             @ Store State = Paused (we will call Resume() to start playback)
#if SGE_USE_OVERSAMPLING
	MOV	r1, #0x80                   @ Clear TapL,TapR for oversampling
# ifdef __NDS__
	LSL	r1, #0x08
# endif
	STRH	r1, [r0, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS+0x00]
	STRH	r1, [r0, #0x18+SGE_PLATFORM_OVERSAMPLING_OFFS+0x02]
#endif
3:	BL	SGE_Driver_Resume

.LExit:
	POP	{r4}
	POP	{r3}
	BX	r3

.LExit_Error:
	MOV	r0, #0x00                   @ Return 0 on failure
	B	.LExit

ASM_FUNC_END(SGE_Driver_Init)

/************************************************/
//! EOF
/************************************************/
