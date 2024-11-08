/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDSHw.h"
/************************************************/

@ r0: &Driver

ASM_FUNC_GLOBAL(SGE_CriticalSection_Enter)
ASM_FUNC_BEG   (SGE_CriticalSection_Enter, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_CriticalSection_Enter:
	LDR	r2, =REG_IME
	MOV	r3, #0x00
	LDR	r0, [r2]
	STR	r3, [r2]
	BX	lr

ASM_FUNC_END(SGE_CriticalSection_Enter)
ASM_WEAK    (SGE_CriticalSection_Enter)

/************************************************/

@ r0: &Driver
@ r1:  Key (OldIME)

ASM_FUNC_GLOBAL(SGE_CriticalSection_Leave)
ASM_FUNC_BEG   (SGE_CriticalSection_Leave, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_CriticalSection_Leave:
	LDR	r0, =REG_IME
	STR	r1, [r0]
	BX	lr

ASM_FUNC_END(SGE_CriticalSection_Leave)
ASM_WEAK    (SGE_CriticalSection_Leave)

/************************************************/
//! EOF
/************************************************/
