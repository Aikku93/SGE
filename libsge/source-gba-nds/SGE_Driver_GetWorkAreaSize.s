/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/

@ r0: VoxCnt
@ r1: BufCnt
@ r2: BufLen

ASM_FUNC_GLOBAL(SGE_Driver_GetWorkAreaSize)
ASM_FUNC_BEG   (SGE_Driver_GetWorkAreaSize, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_GetWorkAreaSize:
	CMP	r0, #0x00                          @ VoxCnt too low or too high?
	BEQ	.LExit_Error
	CMP	r0, #0xFF
	BHI	.LExit_Error
	CMP	r1, #0x00                          @ Invalid BufCnt?
	BEQ	.LExit_Error
	CMP	r1, #0xFF
	BHI	.LExit_Error
	CMP	r2, #0x00                          @ Invalid BufLen?
	BEQ	.LExit_Error
	LSR	r3, r2, #0x10
	BNE	.LExit_Error
1:	MOV	r3, #SGE_VOX_SIZE                  @ Size = sizeof(Header) + sizeof(Vox_t[VoxCnt])
	MUL	r0, r3
	MUL	r1, r2                             @ BufCnt *= BufLen -> r1
	ADD	r0, #SGE_DRIVER_HEADER_SIZE
#ifdef __GBA__
# if (SGE_SELFMANAGED_HW && !defined(SGE_RESAMPLE_TARGET))
	LSL	r2, r1, #0x20-4                    @ GBA: BufCnt*BufLen must be a multiple of 16 samples
	BNE	.LExit_Error
# endif
	LSL	r1, #0x00+1+SGE_USE_OVERSAMPLING   @      8bit samples
#else
	LSL	r2, r1, #0x20-1                    @ NDS: BufCnt*BufLen must be a multiple of 2 samples
	BNE	.LExit_Error
	LSL	r1, #0x01+1+SGE_USE_OVERSAMPLING   @      16bit samples
#endif
	ADD	r0, r1                             @ Size += BufSize
#if (defined(__GBA__) && defined(SGE_RESAMPLE_TARGET))
	LDR	r1, =0x01*SGE_RESAMPLE_BUFSIZE*SGE_RESAMPLE_NBUFFERS * 2 @ Add resampled buffers
	ADD	r0, r1
#endif
	BX	lr

.LExit_Error:
	MOV	r0, #0x00                          @ Return Size=0 on error
	BX	lr

ASM_FUNC_END(SGE_Driver_GetWorkAreaSize)

/************************************************/
//! EOF
/************************************************/
