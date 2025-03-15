/************************************************/
#ifdef __GBA__
/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
#include "SGE-GBANDSHw.h"
/************************************************/

//! Enable/disable sinc interpolation
//! Setting this value to 0 will default to linear interpolation.
#define SINC_TABLE_BITS 0

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
	ADD	r1, #0x01                 @ ++BfIdx >= BfCnt?
	CMP	r1, #SGE_RESAMPLE_NBUFFERS
	BCC	2f
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
	LDR	r2, =SGE_RESAMPLE_BUFSIZE * SGE_RESAMPLE_PERIOD
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
#endif

.LEarlyExit:
	BX	lr

#ifdef SGE_RESAMPLE_TARGET
ASM_MODE_ARM
.LResampleStart_ARM:
#if SINC_TABLE_BITS
	STMFD	sp!, {r8-fp}
#else
	STMFD	sp!, {r8-r9}
#endif
	LDRB	ip, [r4, #0x08]           @ Seek OutBuffer as needed
	ORR	r0, r6, r0, lsl #0x10     @ Rate | Phase<<16 -> r0
	MOV	r3, #SGE_RESAMPLE_BUFSIZE
#if (SGE_RESAMPLE_NBUFFERS == 2)
	CMP	ip, #0x00
	ADDEQ	r1, r1, #0x01*SGE_RESAMPLE_BUFSIZE
#else
	SUBS	ip, ip, #0x01             @ Write to the buffer that was last played
	ADDCC	ip, ip, #SGE_RESAMPLE_NBUFFERS
	MLA	r1, r3, ip, r1
#endif
	SUBS	ip, r5, #0x01
	ADDCC	ip, ip, r7
#if SINC_TABLE_BITS
	ADD	lr, r2, r7                @ &SrcR -> lr
	LDRSB	fp, [r2, ip]              @ Load older samples -> r9,sl,fp
	LDRSB	r8, [lr, ip]
	ADD	fp, fp, r8, lsl #0x10
	SUBS	ip, ip, #0x01
	ADDCC	ip, ip, r7
	LDRSB	sl, [r2, ip]
	LDRSB	r8, [lr, ip]
	ADD	sl, sl, r8, lsl #0x10
	SUBS	ip, ip, #0x01
	ADDCC	ip, ip, r7
	LDRSB	r9, [r2, ip]
	LDRSB	r8, [lr, ip]
	ADD	r9, r9, r8, lsl #0x10
#else
	LDRB	r8, [ip, r2]!             @ Prepare OldL,OldR for interpolation loop
	LDRB	ip, [ip, r7]
	EOR	r8, r8, #0x80
	EOR	ip, ip, #0x80
	ORR	r8, r8, ip, lsl #0x10
	MOV	r8, r8, lsl #0x08
	SUB	r8, r8, r9, lsl #0x08
#endif

@ r0:  Rate | Phase<<16
@ r1: &OutBuffer
@ r2: &SrcBuffer
@ r3:  OutSampleRem | N<<16
@ r4: &Driver
@ r5:  SrcSampleOffs
@ r6: &SrcL
@ r7:  BufSize(=BfCnt*BfLen)
@ ip: [Temp]
@ lr:  SignMask
@ Without sinc interpolation:
@  r8: (OldL  | OldR) << 8
@  r9: (StepL | StepR)
@ With sinc interpolation:
@  r2: &SincTable
@  r4:  FFh
@  r5: [Temp]
@  r8:  x[-3] (L|R)
@  r9:  x[-2] (L|R)
@  sl:  x[-1] (L|R)
@  fp:  x[ 0] (L|R)
@  lr: [Temp]

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
#if SINC_TABLE_BITS
	STMFD	sp!, {r2,r4-r5}
	LDR	r2, =SGE_Driver_ResampleLUT
	MOV	r4, #0xFF
#else
	MOV	lr, #0x80                 @ Build SignMask -> lr
	ORR	lr, lr, lr, lsl #0x10     @ We use unsigned lerp due to potential overflow issues
#endif
	LDR	pc, =SGE_Driver_ResampleCore
ASM_MODE_THUMB
#endif

ASM_FUNC_END(SGE_Driver_Sync)

/************************************************/
#ifdef SGE_RESAMPLE_TARGET
/************************************************/

ASM_FUNC_BEG(SGE_Driver_ResampleCore, ASM_FUNCSECT_FAST;ASM_MODE_ARM)

SGE_Driver_ResampleCore:
0:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCC	2f
#if SINC_TABLE_BITS
	MOV	r8, r9
	MOV	r9, sl
	MOV	sl, fp
	LDRSB	ip, [r6, r7]              @ NewL,NewR -> fp,ip
	LDRSB	fp, [r6], #0x01
	ADD	fp, fp, ip, lsl #0x10     @ NewL|NewR -> fp
1:	MOV	r5, r0, lsr #0x20-SINC_TABLE_BITS
	LDR	r5, [r2, r5, lsl #0x02]   @ Convolve sample (order of signs: -++-)
	ANDS	ip, r4, r5, lsr #0x00
	MULNE	ip, r8, ip
	AND	lr, r4, r5, lsr #0x08
	MUL	lr, r9, lr
	RSB	ip, ip, lr
	AND	lr, r4, r5, lsr #0x10
	MLA	ip, sl, lr, ip
	ANDS	lr, r4, r5, lsr #0x18
	MULNE	lr, fp, lr
	SUB	ip, ip, lr
	MOV	ip, ip, ror #0x18         @ Rotate to right sample bits
#else
	ADD	r8, r8, r9, lsl #0x08     @ Step to next sample
	LDRB	ip, [r6, r7]              @ NewL,NewR -> r9,ip
	LDRB	r9, [r6], #0x01
	ORR	r9, r9, ip, lsl #0x10     @ NewL|NewR -> r9
	EOR	r9, r9, lr                @ Convert to unsigned
	SUB	r9, r9, r8, lsr #0x08     @ StepL|StepR -> r9
1:	MOV	ip, r0, lsr #0x20-8       @ Out = Old + Step*Phase -> ip
	MLA	ip, r9, ip, r8
	EOR	ip, lr, ip, ror #0x18     @ Back to signed and rotate to correct sample bits
#endif
	STRB	ip, [r1, #SGE_RESAMPLE_BUFSIZE*SGE_RESAMPLE_NBUFFERS]
	MOV	ip, ip, ror #0x10
	STRB	ip, [r1], #0x01
10:	ADDS	r0, r0, r0, lsl #0x10     @ Phase += Rate?
	BCS	0b
11:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCS	1b
2:	ADDS	r3, r3, #0x01<<16         @ Restore OutSampleRem. More samples remaining?
#if SINC_TABLE_BITS
	LDMFD	sp!, {r2,r4-r5}
#endif
	LDRNE	pc, =.LResampleLoop
#if SINC_TABLE_BITS
	LDMFD	sp!, {r8-fp}
#else
	LDMFD	sp!, {r8-r9}
#endif
	LDMFD	sp!, {r4-r7,lr}
	BX	lr

ASM_FUNC_END(SGE_Driver_ResampleCore)

/************************************************/
#if SINC_TABLE_BITS
/************************************************/

ASM_DATA_BEG(SGE_Driver_ResampleLUT, ASM_DATASECT_RODATA;ASM_ALIGN(4))

SGE_Driver_ResampleLUT:
#if (SINC_TABLE_BITS == 6)
	.byte 0x00,0xC1,0x00,0x00, 0x02,0xC1,0x02,0x00, 0x04,0xC1,0x04,0x00, 0x06,0xC0,0x07,0x00
	.byte 0x08,0xBF,0x0A,0x00, 0x09,0xBE,0x0C,0x00, 0x0B,0xBE,0x0F,0x01, 0x0C,0xBC,0x12,0x01
	.byte 0x0D,0xBB,0x14,0x01, 0x0E,0xB9,0x18,0x02, 0x0F,0xB7,0x1B,0x02, 0x10,0xB5,0x1E,0x02
	.byte 0x11,0xB3,0x22,0x03, 0x12,0xB1,0x25,0x03, 0x12,0xAE,0x29,0x04, 0x13,0xAC,0x2C,0x04
	.byte 0x13,0xA9,0x30,0x05, 0x13,0xA6,0x33,0x05, 0x15,0xA3,0x37,0x06, 0x14,0xA0,0x3B,0x08
	.byte 0x14,0x9D,0x3F,0x07, 0x14,0x9A,0x43,0x08, 0x14,0x96,0x47,0x0A, 0x12,0x93,0x4B,0x09
	.byte 0x13,0x8F,0x4F,0x0A, 0x13,0x8C,0x53,0x0B, 0x13,0x88,0x57,0x0B, 0x12,0x84,0x5B,0x0C
	.byte 0x12,0x80,0x60,0x0D, 0x11,0x7C,0x64,0x0E, 0x11,0x78,0x68,0x0E, 0x10,0x74,0x6C,0x0F
	.byte 0x0F,0x70,0x70,0x0E, 0x0F,0x6C,0x74,0x10, 0x0E,0x68,0x78,0x11, 0x0E,0x64,0x7C,0x11
	.byte 0x0D,0x60,0x80,0x12, 0x0C,0x5B,0x84,0x12, 0x0B,0x57,0x88,0x13, 0x0B,0x53,0x8C,0x13
	.byte 0x0A,0x4F,0x8F,0x13, 0x09,0x4B,0x93,0x12, 0x0A,0x47,0x96,0x14, 0x08,0x43,0x9A,0x14
	.byte 0x07,0x3F,0x9D,0x14, 0x08,0x3B,0xA0,0x14, 0x06,0x37,0xA3,0x15, 0x05,0x33,0xA6,0x13
	.byte 0x05,0x30,0xA9,0x13, 0x04,0x2C,0xAC,0x13, 0x04,0x29,0xAE,0x12, 0x03,0x25,0xB1,0x12
	.byte 0x03,0x22,0xB3,0x11, 0x02,0x1E,0xB5,0x10, 0x02,0x1B,0xB7,0x0F, 0x02,0x18,0xB9,0x0E
	.byte 0x01,0x14,0xBB,0x0D, 0x01,0x12,0xBC,0x0C, 0x01,0x0F,0xBE,0x0B, 0x00,0x0C,0xBE,0x09
	.byte 0x00,0x0A,0xBF,0x08, 0x00,0x07,0xC0,0x06, 0x00,0x04,0xC1,0x04, 0x00,0x02,0xC1,0x02
#else
# error "FIXME: Re-generate the sinc LUT."
#endif

ASM_DATA_END(SGE_Driver_ResampleLUT)

/************************************************/
#endif
/************************************************/
#endif
/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
