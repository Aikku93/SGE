/************************************************/
#pragma once
/************************************************/

//! Platform specs
#define SGE_DECLSPEC
#define SGE_PLATFORM_DRIVERDATA_SIZE 0x00
#define SGE_PLATFORM_HAVE_ALLOCATING_DATABASE
#define SGE_PLATFORM_HAVE_REVERB
#if (defined(__LP64__) || defined(_WIN64))
# define SGE_PLATFORM_IS_64BIT
#endif

/************************************************/
#ifndef __ASSEMBLER__
/************************************************/
#include <stdint.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

//! Platform specs
typedef float   SGE_MixSmp_t;
typedef int16_t SGE_OutSmp_t;
struct SGE_Driver_Platform_t {
	int x[0]; //! <- This is unused, and only to make compilers happy
};

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
#endif
/************************************************/

//! Include the core definitions
#include "SGE.h"

/************************************************/
//! EOF
/************************************************/
