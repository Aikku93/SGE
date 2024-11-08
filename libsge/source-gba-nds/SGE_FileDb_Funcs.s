/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

//! All these functions are called from THUMB mode,
//! but if we need ARM code, then on ARM9 we can
//! patch with BLX and just start with ARM directly.
#if (!defined(__NDS__) || __NDS__ != 9)
# define ARM_ENTRY_MODE ASM_MODE_THUMB;ASM_ALIGN(4)
#else
# define ARM_ENTRY_MODE ASM_MODE_ARM
#endif

/************************************************/

@ r0: &Dst
@ r1:  Offs
@ r2:  Size
@ r3: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_Read)
ASM_FUNC_BEG   (SGE_FileDb_Read, ASM_FUNCSECT_TEXT;ARM_ENTRY_MODE)

SGE_FileDb_Read:
#if (!defined(__NDS__) || __NDS__ != 9)
	BX	pc
	NOP
ASM_MODE_ARM
	LDMIA	r3, {r3,ip} @ Replace Callbacks with ReadArg, and call ReadFnc
	BX	ip
#else
	LDMIA	r3, {r3,ip}
	STMFD	sp!, {r0,r2,lr}
	BLX	ip
	LDMFD	sp!, {r2,r3,lr}
1:	ADD	r3, r3, r2
	BIC	r2, r2, #0x1F
10:	MCR	p15,0,r2,c7,c10,1 @ DCCleanLine
	ADD	r2, r2, #0x20
	CMP	r2, r3
	BCC	10b
2:	MOV	r2, #0x00         @ WrBufDrain
	MCR	p15,0,r2,c7,c10,4
0:	BX	lr
#endif

ASM_FUNC_END(SGE_FileDb_Read)

/************************************************/

@ r0:  Size
@ r1: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_Alloc)
ASM_FUNC_BEG   (SGE_FileDb_Alloc, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_Alloc:
	ADD	r1, #0x08
	LDMIA	r1, {r1,r2} @ Replace Callbacks with AllocArg, and call AllocFnc
	BX	r2

ASM_FUNC_END(SGE_FileDb_Alloc)

/************************************************/

@ r0: &Mem
@ r1: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_Free)
ASM_FUNC_BEG   (SGE_FileDb_Free, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_Free:
	ADD	r1, #0x10
	LDMIA	r1, {r1,r2} @ Replace Callbacks with FreeArg, and call FreeFnc
	BX	r2

ASM_FUNC_END(SGE_FileDb_Free)

/************************************************/

@ r0:  Idx
@ r1:  nEntry
@ r2: &InstanceTable

ASM_FUNC_GLOBAL(SGE_FileDb_GetInstance)
ASM_FUNC_BEG   (SGE_FileDb_GetInstance, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_GetInstance:
	CMP	r0, r1            @ Out of range?
	BCS	.LGetInstance_Exit_Fail
	LSL	r0, #0x03         @ Seek to InstanceTable[Idx] -> r2
	ADD	r2, r0
	LDR	r0, [r2, #0x04]   @ Clear Persistent flag and return Data
	MOV	r1, #0x01
	BIC	r0, r1
	BX	lr

.LGetInstance_Exit_Fail:
	MOV	r0, #0x00
	BX	lr

ASM_FUNC_END(SGE_FileDb_GetInstance)

/************************************************/

@ r0:  Idx
@ r1:  nEntry
@ r2: &InstanceTable
@ r3: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_LoadInstance)
ASM_FUNC_BEG   (SGE_FileDb_LoadInstance, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_LoadInstance:
	CMP	r0, r1            @ Out of range?
	BCS	.LLoadInstance_EarlyExit_Fail
0:	LSL	r0, #0x03         @ Seek to Instance=InstanceTable[Idx] -> r2
	ADD	r2, r0
	LDR	r0, [r2, #0x04]   @ Data -> r0
	MOV	r1, #0x01
	AND	r1, r0            @ Persistent -> r1, and clear Persistent flag -> r0. Data == NULL?
	BIC	r0, r1
	BEQ	.LLoadInstance_Read

.LLoadInstance_AlreadyExists:
	CMP	r1, #0x00         @ If this is a persistent entry, just return Data and exit
	BNE	.LLoadInstance_EarlyExit
0:	LDRB	r1, [r2, #0x03]   @ Attempt to increase the instance count
	ADD	r1, #0x01
	CMP	r1, #0xFF
	BHI	.LLoadInstance_EarlyExit_Fail
	STRB	r1, [r2, #0x03]   @ Instance count increased ok - store new value and return Data

.LLoadInstance_EarlyExit:
	BX	lr

.LLoadInstance_EarlyExit_Fail:
	MOV	r0, #0x00
	BX	lr

.LLoadInstance_Read:
	PUSH	{r4-r7,lr}
	LDR	r0, [r2, #0x00]   @ Offs0|Inst0<<24 -> r0
	LDR	r1, [r2, #0x08]   @ Offs1|Inst1<<24 -> r1 (for next entry)
	MOV	r4, r2            @ Instance -> r4
	MOV	r5, r3            @ Callbacks -> r5
	@LSL	r0, #0x08         @ Clear Inst so we can read Offs
	@LSR	r0, #0x08         @  NOTE: The first entry already has Inst=0
	LSL	r1, #0x08
	LSR	r1, #0x08
	SUB	r6, r1, r0        @ Size = (Offs1 - Offs0)*32 -> r6
	LSL	r6, #0x05
0:	MOV	r0, r6            @ Mem = Alloc(Size) -> r7?
	MOV	r1, r5
	BL	SGE_FileDb_Alloc
	MOV	r7, r0
	BEQ	.LLoadInstance_Read_Exit
0:	@MOV	r0, r7            @ Read(Dst=Mem, Offs=Offs0*32, Size=Size) == Size?
	LDR	r1, [r4, #0x00]
	MOV	r2, r6
	MOV	r3, r5
	@LSL	r1, #0x08
	@LSR	r1, #0x08
	LSL	r1, #0x05
	BL	SGE_FileDb_Read
	CMP	r0, r6
	BNE	.LLoadInstance_Read_Fail_FreeData
0:	STR	r7, [r4, #0x04]   @ Store Instance.Data = Mem and return Mem
	MOV	r0, r7

.LLoadInstance_Read_Exit:
#if (defined(__NDS__) && __NDS__ == 9)
	POP	{r4-r7,pc}
#else
	POP	{r4-r7}
	POP	{r3}
	BX	r3
#endif

.LLoadInstance_Read_Fail_FreeData:
	MOV	r0, r7            @ Failed to read data - deallocate memory and return NULL
	LDR	r1, [r4, #0x04]
	BL	SGE_FileDb_Free
	MOV	r0, #0x00
#if (defined(__NDS__) && __NDS__ == 9)
	POP	{r4-r7,pc}
#else
	B	.LLoadInstance_Read_Exit
#endif

ASM_FUNC_END(SGE_FileDb_LoadInstance)

/************************************************/

@ r0:  Idx
@ r1:  nEntry
@ r2: &InstanceTable
@ r3: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_FreeInstance)
ASM_FUNC_BEG   (SGE_FileDb_FreeInstance, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_FreeInstance:
	CMP	r0, r1            @ Out of range?
	BCS	.LFreeInstance_EarlyExit_Fail
0:	LSL	r0, #0x03         @ Seek to Instance=InstanceTable[Idx] -> r2
	ADD	r2, r0
	LDR	r0, [r2, #0x04]   @ Data -> r0?
	LSR	r1, r0, #0x01     @  Persistent? Skip unloading
	BEQ	.LFreeInstance_EarlyExit_Fail
	BCS	.LFreeInstance_EarlyExit_Fail
1:	LDRB	r1, [r2, #0x03]   @ Attempt to decrease the instance count
	SUB	r1, #0x01
	BCC	.LFreeInstance_Unload
	STRB	r1, [r2, #0x03]   @ Instance count decreased ok - store new value
	MOV	r0, #0x01         @ Return TRUE
	BX	lr

.LFreeInstance_Unload:
	MOV	r1, #0x00
	STR	r1, [r2, #0x04]   @ Data = NULL
0:	PUSH	{lr}
	@MOV	r0, r0            @ Deallocate memory
	MOV	r1, r3
	BL	SGE_FileDb_Free
	POP	{r3}
	MOV	r0, #0x01         @ Return TRUE
	BX	r3

.LFreeInstance_EarlyExit_Fail:
	MOV	r0, #0x00         @ Return FALSE
	BX	lr

ASM_FUNC_END(SGE_FileDb_FreeInstance)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
