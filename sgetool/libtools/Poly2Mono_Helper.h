/**************************************/
#pragma once
/**************************************/
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define SQR(x) ((x)*(x))
/**************************************/

#define POLY2MONO_FORCED_INLINE static inline __attribute__((always_inline))
#define POLY2MONO_ASSUME(Cond) (Cond) ? ((void)0) : __builtin_unreachable()
#define POLY2MONO_ASSUME_ALIGNED(x,Align) x = __builtin_assume_aligned(x,Align)

/**************************************/

#define POLY2MONO_BUFFER_ALIGNMENT 64u //! Always align memory to 64-byte boundaries (preparation for AVX-512)
#define POLY2MONO_IS_POWEROF_2(x) (((x) & (-(x))) == (x))

/**************************************/
//! EOF
/**************************************/
