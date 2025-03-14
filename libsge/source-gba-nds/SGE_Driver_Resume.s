/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: Driver

ASM_FUNC_GLOBAL(SGE_Driver_Resume)
ASM_FUNC_BEG   (SGE_Driver_Resume, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Driver_Resume:
	MOV	r3, lr
	PUSH	{r3-r5}
	MOV	r4, r0                        @ Driver -> r4
1:	LDR	r2, [r0, #0x00]               @ State -> r2
	LDR	r3, =SGE_DRIVER_STATE_PAUSED
	CMP	r2, r3                        @ Not already paused?
	BNE	.LExit

.LClearOutBufs:
	LDRB	r2, [r4, #0x0A]               @ VoxCnt -> r2
	MOV	r3, #SGE_VOX_SIZE
	ADD	r0, #SGE_DRIVER_HEADER_SIZE   @ Seek to voices
	MUL	r2, r3
	LDRB	r3, [r4, #0x09]               @ BufCnt -> r3
	ADD	r0, r2                        @ Seek to OutBuf - Done!
	LDRH	r2, [r4, #0x0E]               @ BufLen -> r2
	SUB	r1, r3, #0x01                 @ BufIdxR = BufCnt-1
#ifdef __GBA__
	STRB	r1, [r4, #0x08]               @  This forces a reset on the next Sync().
#endif
	STRB	r1, [r4, #0x0B]               @ BufIdxW = BufCnt-1
0:	MUL	r2, r3                        @ memset(OutBuf, 0, sizeof(Sample_t[2])*BufCnt*BufLen)
	MOV	r1, #0x00
	@MOV	r0, r0
#ifdef __GBA__
	LSL	r2, #0x00+1+SGE_USE_OVERSAMPLING
#else
	LSL	r2, #0x01+1+SGE_USE_OVERSAMPLING
#endif
	LSR	r5, r2, #0x01                 @ RightBufOffs -> r5
	BL	memset
#if (defined(__NDS__) && __NDS__ == 9)
	PUSH	{r0}
	LSL	r1, r5, #0x01
	BL	DC_FlushRange
	POP	{r0}
#endif

.LSetupHardware:
#ifdef __GBA__
# if SGE_SELFMANAGED_HW
	LDR	r1, =REG_SOUNDCNT           @ Enable all required audio hardware
	LDRH	r2, [r1, #REG_SOUNDCNT_X - REG_SOUNDCNT]
	MOV	r3, #REG_SOUNDCNT_X_MASTER_ENABLE
	ORR	r2, r3
	STRH	r2, [r1, #REG_SOUNDCNT_X - REG_SOUNDCNT]
	LDR	r2, [r1, #REG_SOUNDCNT   - REG_SOUNDCNT]
	LDR	r3, =REG_SOUNDCNT_LH_VALUE(0, \
		      REG_SOUNDCNT_H_FIFO_A_VOL_MASK   | \
		      REG_SOUNDCNT_H_FIFO_B_VOL_MASK   | \
		      REG_SOUNDCNT_H_FIFO_A_ENABLE_R   | \
		      REG_SOUNDCNT_H_FIFO_A_ENABLE_L   | \
		      REG_SOUNDCNT_H_FIFO_A_TIMER_MASK | \
		      REG_SOUNDCNT_H_FIFO_B_ENABLE_R   | \
		      REG_SOUNDCNT_H_FIFO_B_ENABLE_L   | \
		      REG_SOUNDCNT_H_FIFO_B_TIMER_MASK   \
		     )
	BIC	r2, r3
	LDR	r3, =REG_SOUNDCNT_LH_VALUE(0, \
		      REG_SOUNDCNT_H_FIFO_A_VOL_100                | \
		      REG_SOUNDCNT_H_FIFO_B_VOL_100                | \
		      REG_SOUNDCNT_H_FIFO_A_ENABLE_L               | \
		      REG_SOUNDCNT_H_FIFO_A_TIMER(SGE_HWTIMER_IDX) | \
		      REG_SOUNDCNT_H_FIFO_B_ENABLE_R               | \
		      REG_SOUNDCNT_H_FIFO_B_TIMER(SGE_HWTIMER_IDX)   \
		     )
	ORR	r2, r3
	STR	r2, [r1, #REG_SOUNDCNT   - REG_SOUNDCNT]
1:	LDR	r2, =REG_SOUNDCNT_H
	LDR	r1, =REG_SOUNDCNT_H_FIFO_A_FLUSH | REG_SOUNDCNT_H_FIFO_B_FLUSH
	LDRH	r3, [r2]                                   @ Flush FIFO
	ORR	r3, r1
	STRH	r3, [r2]
	ADD	r2, #REG_SOUNDFIFO_A - REG_SOUNDCNT_H
	MOV	r1, #0x00
	STR	r1, [r2, #REG_SOUNDFIFO_A - REG_SOUNDFIFO_A]
	STR	r1, [r2, #REG_SOUNDFIFO_B - REG_SOUNDFIFO_A]
1:	LDRH	r3, [r2, #REG_DMACNT_H(1) - REG_SOUNDFIFO_A] @ Wait DMA1 (safety)
	LSR	r3, #0x10
	BCS	1b
1:	LDRH	r3, [r2, #REG_DMACNT_H(2) - REG_SOUNDFIFO_A] @ Wait DMA2 (safety)
	LSR	r3, #0x10
	BCS	1b
1:	STR	r3, [r2, #REG_TIMER(SGE_HWTIMER_IDX) - REG_SOUNDFIFO_A] @ Stop timer
1:	MOV	r1, r2
	ADD	r1, #REG_DMASAD(1) - REG_SOUNDFIFO_A
	MOV	r3, #0xB6                                  @ Cnt = DST_INC | SRC_INC | REPEAT | DATA32 | MODE_SOUNDFIFO | ENABLE
	LSL	r3, #0x18
	STMIA	r1!, {r0,r2,r3}                            @ DMA1SAD = LeftBuf,  DMA1DAD = FIFO_A, DMA1CNT = Cnt
	ADD	r0, r5
	ADD	r2, #REG_SOUNDFIFO_B - REG_SOUNDFIFO_A
	STMIA	r1!, {r0,r2,r3}                            @ DMA2SAD = RightBuf, DMA1DAD = FIFO_B, DMA1CNT = Cnt
	MOV	r5, r1                                     @ &DMA3SAD -> r5
# endif
2:	LDR	r0, =GBA_HW_FREQ_HZ*2                      @ Get Period = Round[HW_FREQ / RateHz]
	LDRH	r1, [r4, #0x0C]
	BL	__aeabi_uidiv
# if SGE_USE_OVERSAMPLING
	ADD	r0, #0x02
	LSR	r0, #0x02
# else
	ADD	r0, #0x01
	LSR	r0, #0x01
# endif
# if SGE_SELFMANAGED_HW
	MOV	r1, #REG_TIMER_H_ENABLE+1                  @ Start timer and return Period
	LSL	r1, #0x10
	SUB	r1, r0
	STR	r1, [r5, #REG_TIMER(SGE_HWTIMER_IDX) - REG_DMASAD(3)]
# endif
#else
	LDRH	r3, [r4, #0x0C]                            @ BeginStream(LeftBuf, RightBuf, BufLen*BufCnt, RateHz)?
	LSR	r2, r5, #0x01+1-1
	ADD	r1, r0, r5
	@MOV	r0, r0
# if SGE_USE_OVERSAMPLING
	LSL	r3, #0x01
# endif
	BL	SGE_Usercall_BeginStream
	CMP	r0, #0x00
	BEQ	.LExit
	STR	r0, [r4, #0x18+SGE_PLATFORM_STREAMTOKEN_OFFS]
#endif
3:	MOV	r1, #(SGE_DRIVER_STATE_READY) & 0xFF       @ Mark State as Ready
	STRB	r1, [r4, #0x00]

.LExit:
	POP	{r3-r5}
	BX	r3

ASM_FUNC_END(SGE_Driver_Resume)

/************************************************/
//! EOF
/************************************************/
