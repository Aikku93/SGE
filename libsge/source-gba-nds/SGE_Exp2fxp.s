/************************************************/
#include "SGE-AsmMacros.h"
/************************************************/

/*!
 Because of the way the LUT is formatted, the LUT size
 and/or precision depend on each other:
  ((27-LUT_SIZE_LOG2) + (LUT_PRECISION_LOG2+1 - LUT_SIZE_LOG2) <= 31)
 =(28-2*LUT_SIZE_LOG2 + LUT_PRECISION_LOG2 <= 31)
 Refactor for LUT_SIZE_LOG2:
  LUT_SIZE_LOG2 >= (LUT_PRECISION_LOG2-3) / 2
 Refactor for LUT_PRECISION_LOG2:
  LUT_PRECISION_LOG2 <= 3 + 2*LUT_SIZE_LOG2
!*/
#define LUT_SIZE_LOG2       6
#define LUT_PRECISION_LOG2 15
#if ((27-LUT_SIZE_LOG2) + (LUT_PRECISION_LOG2+1 - LUT_SIZE_LOG2) > 31)
# error "Impossible LUT parameters (will cause overflows)."
#endif

/************************************************/

@ r0: x (5.27fxp - 0.0 ~ 31.99999...)
@ Outputs 2^x in 32.0fxp (adjust integer part of x as needed)

ASM_FUNC_GLOBAL(SGE_Exp2fxp)
ASM_FUNC_BEG   (SGE_Exp2fxp, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_Exp2fxp:
	LDR	r3, =SGE_Exp2fxp_LUT
	LSR	r1, r0, #0x00+27                @ int(x) -> r1
	LSL	r0, #0x20-27                    @ frac(x) -> r0 (.32fxp)
	LSR	r2, r0, #0x20-LUT_SIZE_LOG2     @ {a,b-a} = LUT[frac(x)*LUT_SIZE] -> r2,r3
	LSL	r2, #0x02
	LDR	r2, [r3, r2]
	LSL	r0, #LUT_SIZE_LOG2              @ We now have LUT_SIZE_LOG2 accuracy, so interpolate the remaining fraction
	LSR	r0, #0x1F-LUT_PRECISION_LOG2    @ Rescale the fraction so that x*(b-a) becomes scaled to 31.0fxp
	LSR	r3, r2, #(LUT_PRECISION_LOG2+1) @ <- The table contains 2^(0.0~0.999), so we need an extra bit because
	MUL	r0, r3                          @    the values are between 1.0~2.0.
	LSL	r2, #0x1F-LUT_PRECISION_LOG2
	SUB	r1, #0x1F                       @ Then finally adjust for the integer part
	NEG	r1, r1
	ADD	r0, r2
	LSR	r0, r1
	BX	lr

ASM_FUNC_END(SGE_Exp2fxp)

/************************************************/

ASM_DATA_BEG(SGE_Exp2fxp_LUT, ASM_DATASECT_RODATA;ASM_ALIGN(4))

@ f[x_] := Floor[LUT_PRECISION_LOG2 * 2^(x/LUT_SIZE)];
@ (f[#] + (f[#+1]-f[#])*2^(LUT_PRECISION_LOG2+1)) &@ Range[0,LUT_SIZE-1]
SGE_Exp2fxp_LUT:
	.word 0x01648000,0x01698164,0x016D82CD,0x0170843A
	.word 0x017585AA,0x0179871F,0x017C8898,0x01818A14
	.word 0x01858B95,0x018A8D1A,0x018D8EA4,0x01929031
	.word 0x019791C3,0x019A935A,0x01A094F4,0x01A39694
	.word 0x01A99837,0x01AD99E0,0x01B19B8D,0x01B79D3E
	.word 0x01BB9EF5,0x01C0A0B0,0x01C5A270,0x01C9A435
	.word 0x01CFA5FE,0x01D4A7CD,0x01D9A9A1,0x01DEAB7A
	.word 0x01E3AD58,0x01E8AF3B,0x01EEB123,0x01F3B311
	.word 0x01F9B504,0x01FEB6FD,0x0204B8FB,0x0209BAFF
	.word 0x020FBD08,0x0215BF17,0x021AC12C,0x0221C346
	.word 0x0226C567,0x022CC78D,0x0233C9B9,0x0238CBEC
	.word 0x023FCE24,0x0245D063,0x024BD2A8,0x0251D4F3
	.word 0x0259D744,0x025ED99D,0x0265DBFB,0x026CDE60
	.word 0x0273E0CC,0x027AE33F,0x0280E5B9,0x0287E839
	.word 0x028FEAC0,0x0295ED4F,0x029DEFE4,0x02A4F281
	.word 0x02ABF525,0x02B3F7D0,0x02BBFA83,0x02C2FD3E

ASM_DATA_END(SGE_Exp2fxp_LUT)

/************************************************/
//! EOF
/************************************************/
