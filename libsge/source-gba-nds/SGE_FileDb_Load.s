/************************************************/
#include "SGE-AsmMacros.h"
#include "SGE-GBANDS.h"
/************************************************/
#ifdef SGE_PLATFORM_HAVE_FILEDB
/************************************************/

@ r0: &Db
@ r1: &Callbacks

ASM_FUNC_GLOBAL(SGE_FileDb_Load)
ASM_FUNC_BEG   (SGE_FileDb_Load, ASM_FUNCSECT_TEXT;ASM_MODE_THUMB)

SGE_FileDb_Load:
	MOV	r3, lr
	PUSH	{r3-r6}
	MOV	r4, r0            @ Db -> r4
	STR	r1, [r4, #0x04]   @ Store Callbacks

.LReadHeader:
	SUB	sp, #0x10         @ Allocate space for reading the header
	MOV	r0, sp            @ Read(Dst=Header, Offs=0, Size=sizeof(Header)) -> r0,r1,r2,r3 == Size?
	MOV	r1, #0x00
	MOV	r2, #0x10
	LDR	r3, [r4, #0x04]
	BL	SGE_FileDb_Read
	CMP	r0, #0x10
	POP	{r0-r3}
	LDR	r3, =SGE_DB_MAGIC
	BNE	.LExit_Fail
	CMP	r0, r3            @ Invalid signature?
	BNE	.LExit_Fail
	LSL	r0, r1, #0x10     @ nWaves == 0?
	BEQ	.LExit_Fail
	STR	r1, [r4, #0x00]   @ Store nWaves,nSongs
	LSR	r0, #0x10         @ nItems = nWaves+nSongs+1 -> r5
	LSR	r1, #0x10         @ NOTE: N+1 is needed to use Size=(Offs[n+1]-Offs[n])
	ADD	r0, r1
	ADD	r5, r0, #0x01
	MOV	r6, r2            @ WaveTabOffs -> r6

.LAllocateInstanceTable:
	LSL	r0, r5, #0x03     @ Allocate nItems+1 instances
	LDR	r1, [r4, #0x04]
	BL	SGE_FileDb_Alloc
	CMP	r0, #0x00
	BEQ	.LExit_Fail
	LDRH	r1, [r4, #0x00]   @ nWaves -> r2
	STR	r0, [r4, #0x08]   @ Store WaveTab
	LSL	r1, #0x03
	ADD	r1, r0
	STR	r1, [r4, #0x0C]   @ Store SongTab = WaveTab + nWaves

.LReadInstanceTable:
	LDR	r3, [r4, #0x04]
	LSL	r2, r5, #0x02     @ Read offsets into high half of the instance table memory
	MOV	r1, r6
	ADD	r0, r2
	BL	SGE_FileDb_Read
	LSL	r1, r5, #0x02
	CMP	r0, r1
	BNE	.LExit_Fail_FreeInstanceTable

.LUnpackInstanceTable:
	LDR	r0, [r4, #0x08]   @ Dst = (SGE_FileDbInstance_t*)InstanceTable
	MOV	r3, #0x00
	ADD	r1, r0            @ Src = (uint32_t*)OffsTable
1:	LDMIA	r1!, {r2}         @ Unpack as {Offs=OffsTable[n]/32,Inst=0,Data=NULL}
	LSR	r2, #0x05
	LSR	r6, r2, #0x18     @ If Offs exceeds the maximum range, then fail or our
		                  @ size calculations will become broken :(
	BNE	.LExit_Fail_FreeInstanceTable
0:	STMIA	r0!, {r2-r3}
	SUB	r5, #0x01
	BNE	1b

.LExit_Ok:
	MOV	r0, #0x01         @ All done - return TRUE
	POP	{r3-r6}
	BX	r3

.LExit_Fail_FreeInstanceTable:
	LDR	r0, [r4, #0x08]   @ Deallocate instance table
	LDR	r1, [r4, #0x04]
	BL	SGE_FileDb_Free

.LExit_Fail:
	MOV	r0, #0x00         @ Return FALSE
	STR	r0, [r4, #0x00]   @ nWaves = nSongs = 0
	POP	{r3-r6}
	BX	r3

ASM_FUNC_END(SGE_FileDb_Load)

/************************************************/
#endif
/************************************************/
//! EOF
/************************************************/
