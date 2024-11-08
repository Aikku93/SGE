/************************************************/
#pragma once
/************************************************/

//! Global helpers
#define ASM_ALIGN(x)                 .balign x;
#define ASM_WEAK(...)                .weak __VA_ARGS__;
#define ASM_REFERENCE(Alias, Target) .set Alias, Target;
#define ASM_WEAKREF(Alias, Target)   ASM_WEAK(Alias); ASM_REFERENCE(Alias, Target);

/************************************************/

//! Function helpers
#define ASM_MODE_ARM   .balign 4; .arm;
#define ASM_MODE_THUMB .balign 2; .thumb_func;
#define ASM_FUNCSECT(x, ...) .section x __VA_OPT__(,) __VA_ARGS__;
#define ASM_FUNCSECT_ENTRY ASM_FUNCSECT(.entry, "ax",  %progbits)
#define ASM_FUNCSECT_TEXT  ASM_FUNCSECT(.text,  "ax",  %progbits)
#ifdef __GBA__
# define ASM_FUNCSECT_FAST ASM_FUNCSECT(.iwram, "awx", %progbits)
#endif
#ifdef __NDS__
# if (__NDS__ == 7)
#  define ASM_FUNCSECT_FAST ASM_FUNCSECT(.text, "ax", %progbits)
# else
#  define ASM_FUNCSECT_FAST ASM_FUNCSECT(.itcm, "awx", %progbits)
# endif
#endif

//! Function global macro
#define ASM_FUNC_GLOBAL(...) \
	.global __VA_ARGS__;

//! Function begin macro
//! Usually pass a section in varg
#define ASM_FUNC_BEG(Name, Args) \
	Args;                    \
	.type Name, %function;

//! Function end macro
#define ASM_FUNC_END(Name) \
	.size Name, . - Name;

/************************************************/

//! Data helpers
#define ASM_DATASECT(x, ...) .section x __VA_OPT__(,) __VA_ARGS__;
#define ASM_DATASECT_DATA   ASM_DATASECT(.data,   "aw",  %progbits)
#define ASM_DATASECT_RODATA ASM_DATASECT(.rodata, "a",   %progbits)
#define ASM_DATASECT_BSS    ASM_DATASECT(.bss,    "aw",  %nobits)
#define ASM_DATASECT_SBSS   ASM_DATASECT(.sbss,   "aw",  %nobits)

//! Data global macro
#define ASM_DATA_GLOBAL(...) \
	.global __VA_ARGS__;

//! Data begin macro
//! Usually pass a section in varg
#define ASM_DATA_BEG(Name, Args) \
	Args;                    \
	.type Name, %object;

//! Data end macro
#define ASM_DATA_END(Name) \
	.size Name, . - Name;

/************************************************/
//! EOF
/************************************************/
