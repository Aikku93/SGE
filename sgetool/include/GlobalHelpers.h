/************************************************/
#pragma once
/************************************************/
#include <math.h>
#include <stdint.h>
/************************************************/
#ifdef __GNUC__
# define BUILTINCLZ(x) __builtin_clz(x)
# define BUILTINCTZ(x) __builtin_ctz(x)
#endif
/************************************************/

#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define SQR(x) ((x) * (x))
#define CLAMP(x, Min, Max) MIN(Max, MAX(Min, x))

#define IS_POWER_OF_2(x) (((x) & (-(x))) == (x))

#define FORCE_INLINE static inline __attribute__ ((always_inline))

/************************************************/

//! Convert decibel value to linear
FORCE_INLINE
double ConvertDecibelToLinear(double dB) {
	return pow(10.0, dB / 20.0);
}

//! Implement count-leading-zeros for a 32bit value
//! Result is undefined for x == 0
#ifdef BUILTINCLZ
FORCE_INLINE
uint8_t clz32(uint32_t x)
{
	return (uint8_t)BUILTINCLZ(x);
}
#else
uint8_t clz32(uint32_t x);
#endif

/************************************************/

//! Write byte to dynamic buffer
//! This may increase the buffer capacity.
//! Returns 0 on failure.
int DynamicBuffer_WriteByte(uint8_t Byte, uint8_t **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr);

//! Write bytes to dynamic buffer
//! This may increase the buffer capacity.
//! Returns 0 on failure.
int DynamicBuffer_WriteBytes(const uint8_t *Bytes, uint32_t Length, void **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr);

//! Resize dynamic buffer
//! The growth policy is to start at 32 bytes, grow to 64 bytes,
//! and then further growth is by a factor of 1.5X.
//! Returns 0 on failure.
int DynamicBuffer_IncreaseSize(void **DataBufferPtr, uint32_t *DataBufferSizePtr, uint32_t MinIncrementSize);

//! Write formatted string to dynamic buffer
//! This may increase the buffer capacity.
//! Returns 0 on failure.
//! NOTE: The output is NUL-terminated, but this terminator
//! is NOT accounted for in BufOffs.
int DynamicBuffer_WriteFormatted(const char *Format, char **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr, ...);

/************************************************/
//! EOF
/************************************************/
