/************************************************/
#include "SGE-AsmMacros.h"
/************************************************/

@ r0: x (5.27fxp - 0.0 ~ 31.99999...)
@ Outputs 2^x in 32.0fxp (adjust integer part of x as needed)

@ We fit 2^x to a polynomial for x in range [0,1). Because of
@ the way 2^x works, the sum of the coefficients must be equal
@ to 1.0. For a second-order fit, this gives a single variable:
@ f(x) = 1 + a*x + b*x^2
@      = 1 + a*x + (1-a)*x^2 | Expand definition of b = 1-a
@      = 1 + a*x*(1-x) + x^2 | Expand brackets
@      = 1 + x*(a*(1-x) + x) | Factor out x
@ For RMSE minimization, we have a = 0.6563658611
@ For a zero-mean error, we have a = 0.6561702453
@ For optimization, we use a = (1-2^-3)(1-2^-2) = 0.65625

ASM_FUNC_GLOBAL(SGE_Exp2fxp)
ASM_FUNC_BEG   (SGE_Exp2fxp, ASM_FUNCSECT_TEXT;ASM_MODE_ARM)

SGE_Exp2fxp:
	MOV	r1, r0, lsl #0x20-27            @ frac(x) -> r1 [.32fxp]
	RSB	ip, r1, #0x00                   @ Apply polynomial to frac(x)
	SUB	ip, ip, ip, lsr #0x03
	SUB	ip, ip, ip, lsr #0x02
	ADD	ip, ip, r1
	UMULL	r2, r3, r1, ip
	MOV	r0, r0, lsr #0x00+27            @ int(x) -> r0
	RSBS	r0, r0, #0x1F                   @ C=1, so that RRX will shift down 1 bit to .31fxp and add 1.0
	MOV	r1, r3, rrx
	MOV	r0, r1, lsr r0
	BX	lr

ASM_FUNC_END(SGE_Exp2fxp)

/************************************************/
//! EOF
/************************************************/
