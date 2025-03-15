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
	BNE	.LEarlyExit
0:	LDRB	r1, [r0, #0x08]           @ BfIdx -> r1
#ifdef SGE_RESAMPLE_TARGET
	MOV	r2, #0x01                 @ BfIdx ^= 1?
	EOR	r1, r2
	BNE	2f
#else
	LDRB	r2, [r0, #0x09]           @ BfCnt -> r2
	ADD	r1, #0x01                 @ ++BfIdx >= BfCnt?
	CMP	r1, r2
	BCC	2f
#endif
1:	LDR	r2, =REG_DMACNT(1)        @ Reset DMA
	MOV	r1, #0xB6                 @ DST_INC | SRC_INC | REPEAT | DATA32 | MODE_SOUNDFIFO | ENABLE
	LSL	r1, #0x18                 @ <- This also sets the lower 8 bits to 0, to get BfIdx=0
	STRH	r1, [r2, #REG_DMACNT_H(1) - REG_DMACNT(1)]
	STRH	r1, [r2, #REG_DMACNT_H(2) - REG_DMACNT(1)]
	STR	r1, [r2, #REG_DMACNT  (1) - REG_DMACNT(1)]
	STR	r1, [r2, #REG_DMACNT  (2) - REG_DMACNT(1)]
2:	STRB	r1, [r0, #0x08]           @ Store BfIdx

#ifdef SGE_RESAMPLE_TARGET
.LResampleStart:
	PUSH	{r4-r7,lr}
	MOV	r4, r0                    @ Driver -> r4
	LDR	r0, =GBA_HW_FREQ_HZ*2     @ MixPeriod = Round[HW_FREQ / RateHz] -> r1
	LDRH	r1, [r4, #0x0C]
	BL	__aeabi_uidiv
	ADD	r1, r0, #0x01
	LSR	r1, #0x01
	LDR	r0, [r4, #0x18+SGE_PLATFORM_RESAMPLE_SAMP_OFFS]
	LDRB	r7, [r4, #0x09]           @ BfCnt -> r7
	LDRH	r2, [r4, #0x0E]           @ BfLen -> r2
	MUL	r7, r2                    @ BufSize = BfCnt*BfLen -> r7
	MOV	r3, r7
	LDR	r2, =(SGE_RESAMPLE_BUFSIZE)/2 * SGE_RESAMPLE_PERIOD
	MUL	r3, r1                    @ BufSize*MixPeriod -> r3
	ADD	r2, r0                    @ SampOffs += N*ResamplePeriod
1:	SUB	r2, r3                    @ Wrap SampOffs about BufSize*MixPeriod
	BCS	1b
	ADD	r2, r3
	STR	r2, [r4, #0x18+SGE_PLATFORM_RESAMPLE_SAMP_OFFS]
	MOV	r6, r1
	BL	__aeabi_uidiv             @ SrcSampleOffs = SampOffs / MixPeriod -> r5
	MOV	r5, r0
	LSL	r0, r1, #0x10             @ Phase = ((SampOffs % MixPeriod) << BITS) / MixPeriod -> r0
	MOV	r1, r6
	BL	__aeabi_uidiv
	LDRB	r2, [r4, #0x0A]           @ VoxCnt -> r2
	MOV	r3, #SGE_VOX_SIZE         @ Seek past voices to SrcBuffer -> r2
	MUL	r2, r3
	ADD	r2, r4
	ADD	r2, #SGE_DRIVER_HEADER_SIZE
	LSL	r1, r7, #0x00+1           @ Seek to ResampleBuffer -> r1
	ADD	r1, r2
	LDRH	r6, [r4, #0x0C]           @ Rate = RateHz * 2^16/ResampleRateHz -> r6
# if (SGE_RESAMPLE_LOG2TARGET < 16)
	LSL	r6, #(16 - SGE_RESAMPLE_LOG2TARGET)
# endif
ASM_ALIGN(4)
	BX	pc
	NOP
ASM_MODE_ARM
.LResampleStart_ARM:
	STMFD	sp!, {r8-r9}
	LDRB	ip, [r4, #0x08]           @ Seek OutBuffer as needed
	ORR	r0, r6, r0, lsl #0x10     @ Rate | Phase<<16 -> r0
	MOV	r3, #(SGE_RESAMPLE_BUFSIZE/2)
	CMP	ip, #0x00
	ADDEQ	r1, r1, #0x01*(SGE_RESAMPLE_BUFSIZE/2)
	SUBS	ip, r5, #0x01             @ Load OldL,OldR -> r8,ip
	ADDCC	ip, ip, r7
	LDRB	r8, [ip, r2]!             @ Prepare OldL,OldR for interpolation loop
	LDRB	ip, [ip, r7]
	EOR	r8, r8, #0x80
	EOR	ip, ip, #0x80
	ORR	r8, r8, ip, lsl #0x10
	MOV	r8, r8, lsl #0x08
	SUB	r8, r8, r9, lsl #0x08

@ r0:  Rate | Phase<<16
@ r1: &OutBuffer
@ r2: &SrcBuffer
@ r3:  OutSampleRem | N<<16
@ r4: &Driver
@ r5:  SrcSampleOffs
@ r6: &SrcL
@ r7:  BufSize(=BfCnt*BfLen)
@ r8: (OldL  | OldR) << 8
@ r9: (StepL | StepR)
@ ip: [Temp]
@ lr: [Temp]

.LResampleLoop:
	MOV	ip, r0, lsr #0x10         @ Phase -> ip
	BIC	lr, r0, ip, lsl #0x10     @ Rate -> lr
	MLA	r6, lr, r3, ip            @ InputSamples = N*Rate + Phase -> r6
	ADD	r6, r5, r6, lsr #0x10     @ EndOffs = Offs+InputSamples -> r6
	CMP	r7, r6                    @ EndOffs >= BufSize (via BufSize <= EndOffs)?
	ADD	r6, r2, r5                @ SrcL = SrcBuffer + Offs -> r6
	ADDCS	r3, r3, r3, lsl #0x10     @  N: N = OutSampleRem
	BCS	2f
1:	STMFD	sp!, {r0-r3}              @  Y: N = Ceiling[(((BufSize-Offs) << BITS) - Phase) / Rate]
	SUB	r0, r7, r5
	RSC	r0, ip, r0, lsl #0x10
	MOV	r1, lr
	BL	__aeabi_uidiv
	ADD	ip, r0, #0x01
	MOV	r5, #0x00                 @ Wrap Offs = 0 for next iteration. Note that we never actually advance
	LDMFD	sp!, {r0-r3}              @ it, based on the assumption that we only have two iterations at most
	ADD	r3, r3, ip, lsl #0x10
2:	SUB	r3, r3, r3, lsr #0x10     @ OutSampleRem -= N
	MOV	lr, #0x80                 @ Build SignMask -> lr
	ORR	lr, lr, lr, lsl #0x10     @ We use unsigned lerp due to potential overflow issues
	LDR	pc, =SGE_Driver_ResampleCore
ASM_MODE_THUMB
#endif

.LEarlyExit:
	BX	lr

ASM_FUNC_END(SGE_Driver_Sync)

/************************************************/
#ifdef SGE_RESAMPLE_TARGET
/************************************************/

ASM_FUNC_BEG(SGE_Driver_ResampleCore, ASM_FUNCSECT_FAST;ASM_MODE_ARM)

SGE_Driver_ResampleCore:
0:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCC	2f
	ADD	r8, r8, r9, lsl #0x08     @ Step to next sample
	LDRB	ip, [r6, r7]              @ NewL,NewR -> r9,ip
	LDRB	r9, [r6], #0x01
	ORR	r9, r9, ip, lsl #0x10     @ NewL|NewR -> r9
	EOR	r9, r9, lr                @ Convert to unsigned
	SUB	r9, r9, r8, lsr #0x08     @ StepL|StepR -> r9
1:	MOV	ip, r0, lsr #0x20-8       @ Out = Old + Step*Phase -> ip
	MLA	ip, r9, ip, r8
	EOR	ip, lr, ip, ror #0x18     @ Back to signed and rotate to correct sample bits
	STRB	ip, [r1, #SGE_RESAMPLE_BUFSIZE]
	MOV	ip, ip, ror #0x10
	STRB	ip, [r1], #0x01
10:	ADDS	r0, r0, r0, lsl #0x10     @ Phase += Rate?
	BCS	0b
11:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCS	1b
2:	ADDS	r3, r3, #0x01<<16         @ Restore OutSampleRem. More samples remaining?
	LDRNE	pc, =.LResampleLoop
3:	LDMFD	sp!, {r8-r9}
	LDMFD	sp!, {r4-r7,lr}
	BX	lr

ASM_FUNC_END(SGE_Driver_ResampleCore)

/************************************************/
#endif
/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
