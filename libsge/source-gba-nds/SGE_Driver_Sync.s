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
	LDRH	r6, [r4, #0x0C]           @ Rate = RateHz * 2^16/ResampleRateHz -> r6
	LDR	r3, =SGE_RESAMPLE_BUFSIZE @ N = ResampleBufSize -> r3
# if (SGE_RESAMPLE_LOG2TARGET < 16)
	LSL	r6, #(16 - SGE_RESAMPLE_LOG2TARGET)
# endif
	LDRB	r1, [r4, #0x0A]           @ VoxCnt -> r1
	MOV	r2, #SGE_VOX_SIZE         @ Seek past voices to SrcBuffer -> r1
	MUL	r1, r2
	ADD	r1, r4
	ADD	r1, #SGE_DRIVER_HEADER_SIZE
	LDRB	r4, [r4, #0x08]           @ Seek OutBuffer -> r4
	SUB	r4, #0x01                 @ Write to the buffer that was last played
# if (SGE_RESAMPLE_NBUFFERS == 2)
	AND	r4, r3
# else
	BCS	0f
	ADD	r4, #SGE_RESAMPLE_NBUFFERS
0:	MUL	r4, r3
# endif
	ADD	r4, r1
	ADD	r4, r7
	ADD	r4, r7
	MOV	r2, r3                    @ InputSamples = N*Rate + Phase -> r2
	MUL	r2, r6
	ADD	r2, r0
	LSR	r2, #0x10
	LSL	r0, #0x10                 @ Rate | Phase<<16 -> r6
	ORR	r6, r0
	ADD	r2, r5                    @ EndOffs = Offs + InputSamples -> r2
	PUSH	{r1,r5}                   @ Stash SrcBuffer and Offs for reading old samples
	ADD	r5, r1                    @ SrcL = SrcBuffer + Offs -> r5
	CMP	r2, r7                    @ EndOffs <= BufSize?
	BLS	2f                        @  Y: N = ResampleBufSize
1:	SUB	r2, r5, r1                @  N: N = Ceiling[(((BufSize-Offs) << BITS) - Phase) / Rate]
	SUB	r2, r7, r2
	LSL	r2, #0x10
	LSR	r3, r6, #0x10
	SUB	r2, r3
	SUB	r0, r2, #0x01
	LSL	r1, r6, #0x10
	LSR	r1, #0x10
	BL	__aeabi_uidiv
	ADD	r3, r0, #0x01
2:	LDR	r0, =SGE_RESAMPLE_BUFSIZE
	SUB	r0, r3                    @ NextN(=ResampleBufSize-ThisN) | ThisN<<16
	LSL	r3, #0x10
	ORR	r3, r0
# if SINC_TABLE_BITS
	LDR	r1, =SGE_Driver_ResampleLUT
	MOV	r2, #0xFF
# else
	MOV	r1, #0x80                 @ SignMask -> r2
	LSL	r2, r1, #0x10             @ We use unsigned lerp due to potential overflow issues
	ORR	r2, r1
# endif
ASM_ALIGN(4)
	BX	pc
#endif

.LEarlyExit:
	BX	lr

#ifdef SGE_RESAMPLE_TARGET
ASM_MODE_ARM
	LDMFD	sp!, {ip,lr}              @ SrcBuffer -> ip, Offs -> lr
	SUBS	lr, lr, #0x01             @ Pre-seek first old sample
	ADDCC	lr, lr, r7
# if SINC_TABLE_BITS
	STMFD	sp!, {r8-fp}
	ADD	r0, ip, r7                @ &SrcR -> r0
	LDRSB	fp, [ip, lr]              @ Load older samples -> r9,sl,fp
	LDRSB	r8, [r0, lr]
	ADD	fp, fp, r8, lsl #0x10
	SUBS	lr, lr, #0x01
	ADDCC	lr, lr, r7
	LDRSB	sl, [ip, lr]
	LDRSB	r8, [r0, lr]
	ADD	sl, sl, r8, lsl #0x10
	SUBS	lr, lr, #0x01
	ADDCC	lr, lr, r7
	LDRSB	r9, [ip, lr]
	LDRSB	r8, [r0, lr]
	ADD	r9, r9, r8, lsl #0x10
# else
	LDRB	r0, [ip, lr]!             @ Prepare OldL,OldR for interpolation loop
	LDRB	r1, [ip, r7]
	ORR	r0, r0, r1, lsl #0x10
	EOR	r0, r0, r2
	MOV	r0, r0, lsl #0x08
	SUB	r0, r0, r1, lsl #0x08
# endif
	LDR	pc, =SGE_Driver_ResampleCore
#endif

ASM_FUNC_END(SGE_Driver_Sync)

/************************************************/
#ifdef SGE_RESAMPLE_TARGET
/************************************************/

ASM_FUNC_BEG(SGE_Driver_ResampleCore, ASM_FUNCSECT_FAST;ASM_MODE_ARM)

@ r3:  NextN | ThisN<<16
@ r4: &OutBuffer
@ r5: &SrcL
@ r6:  Rate | Phase<<16
@ r7:  BufSize(=BfCnt*BfLen)
@ ip: [Temp]
@ Without sinc interpolation:
@  r0: (OldL  | OldR) << 8
@  r1: (StepL | StepR)
@  r2:  SignMask
@ With sinc interpolation:
@  r0: [Temp]
@  r1: &SincTable
@  r2:  FFh
@  r8:  x[-3] (L|R)
@  r9:  x[-2] (L|R)
@  sl:  x[-1] (L|R)
@  fp:  x[ 0] (L|R)
@  lr: [Temp]

SGE_Driver_ResampleCore:
0:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCC	2f
#if SINC_TABLE_BITS
	MOV	r8, r9
	MOV	r9, sl
	MOV	sl, fp
	LDRSB	ip, [r5, r7]              @ NewL,NewR -> fp,ip
	LDRSB	fp, [r5], #0x01
	ADD	fp, fp, ip, lsl #0x10     @ NewL|NewR -> fp
1:	MOV	r0, r6, lsr #0x20-SINC_TABLE_BITS
	LDR	r0, [r1, r0, lsl #0x02]   @ Convolve sample (order of signs: -++-)
	ANDS	ip, r2, r0, lsr #0x00
	MULNE	ip, r8, ip
	ANDS	lr, r2, r0, lsr #0x18
	MLANE	ip, fp, lr, ip
	AND	lr, r2, r0, lsr #0x08
	MUL	lr, r9, lr
	RSB	ip, ip, lr
	AND	lr, r2, r0, lsr #0x10
	MLA	ip, sl, lr, ip
	MOV	ip, ip, ror #0x18         @ Rotate to right sample bits
#else
	ADD	r0, r0, r1, lsl #0x08     @ Step to next sample
	LDRB	ip, [r5, r7]              @ NewL,NewR -> r1,ip
	LDRB	r1, [r5], #0x01
	ORR	r1, r1, ip, lsl #0x10     @ NewL|NewR -> r1
	EOR	r1, r1, r2                @ Convert to unsigned
	SUB	r1, r1, r0, lsr #0x08     @ StepL|StepR -> r1
1:	MOV	ip, r6, lsr #0x20-8       @ Out = Old + Step*Phase -> ip
	MLA	ip, r1, ip, r0
	EOR	ip, r2, ip, ror #0x18     @ Back to signed and rotate to correct sample bits
#endif
	STRB	ip, [r4, #SGE_RESAMPLE_BUFSIZE*SGE_RESAMPLE_NBUFFERS]
	MOV	ip, ip, ror #0x10
	STRB	ip, [r4], #0x01
10:	ADDS	r6, r6, r6, lsl #0x10     @ Phase += Rate?
	BCS	0b
11:	SUBS	r3, r3, #0x01<<16         @ --N?
	BCS	1b
2:	MOVS	r3, r3, lsl #0x10         @ More samples remaining?
	SUBNE	r5, r5, r7                @  Rewind SrcBuffer (we only restart when wrapping around, and only wrap once)
	BNE	0b
#if SINC_TABLE_BITS
	LDMFD	sp!, {r8-fp}
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
