/************************************************/
#include "GlobalHelpers.h"
/************************************************/
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/************************************************/

//! Implement count-leading-zeros for a 32bit value
#ifndef BUILTINCLZ
uint8_t clz32(uint32_t x) {
# define ITER(Shift) if((x >> Shift) == 0) n += Shift, x << Shift
	uint8_t n = 0;
	ITER(16);
	ITER(8);
	ITER(4);
	ITER(2);
	ITER(1);
	return n;
# undef ITER
}
#endif

/************************************************/

//! Write byte to dynamic buffer
int DynamicBuffer_WriteByte(uint8_t Byte, uint8_t **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr) {
	//! Do we need more space?
	if(*DataBufferOffsPtr >= *DataBufferSizePtr) {
		if(!DynamicBuffer_IncreaseSize((void**)DataBufferPtr, DataBufferSizePtr, 1)) {
			return 0;
		}
	}

	//! Actually write the data
	(*DataBufferPtr)[(*DataBufferOffsPtr)++] = Byte;
	return 1;
}

/************************************************/

//! Write bytes to dynamic buffer
int DynamicBuffer_WriteBytes(const uint8_t *Bytes, uint32_t Length, void **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr) {
	//! Do we need more space?
	if(*DataBufferOffsPtr+Length > *DataBufferSizePtr) {
		uint32_t DeltaSize = (*DataBufferOffsPtr+Length) - *DataBufferSizePtr;
		if(!DynamicBuffer_IncreaseSize((void**)DataBufferPtr, DataBufferSizePtr, DeltaSize)) {
			return 0;
		}
	}

	//! Actually write the data
	memcpy(*DataBufferPtr + *DataBufferOffsPtr, Bytes, Length);
	*DataBufferOffsPtr += Length;
	return 1;
}

/************************************************/

//! Resize dynamic buffer
int DynamicBuffer_IncreaseSize(void **DataBufferPtr, uint32_t *DataBufferSizePtr, uint32_t MinIncrementSize) {
	uint32_t NewSize = *DataBufferSizePtr; {
		uint32_t DeltaSize = NewSize >> 1;
		if(NewSize < 32) DeltaSize = 32;
		if(DeltaSize < MinIncrementSize) DeltaSize = MinIncrementSize;
		uint32_t t = NewSize + DeltaSize;
		if(t > NewSize) NewSize = t;
		else {
			//! BufSize overflow
			return 0;
		}
	}
	uint8_t *NewMemory = (uint8_t*)realloc(*DataBufferPtr, NewSize);
	if(NewMemory) {
		*DataBufferPtr     = NewMemory;
		*DataBufferSizePtr = NewSize;
		return 1;
	} else return 0;
}

/************************************************/

//! Write formatted string to dynamic buffer
int DynamicBuffer_WriteFormatted(const char *Format, char **DataBufferPtr, uint32_t *DataBufferOffsPtr, uint32_t *DataBufferSizePtr, ...) {
	va_list vl;
	va_start(vl, DataBufferSizePtr);

	//! First, get size of formatted string and resize buffer
	//! NOTE: We must allocate space for the NUL terminator
	//! because vsnprintf() doesn't allow unterminated output.
	int Length = vsnprintf(NULL, 0, Format, vl);
	if(Length < 0) return 0;
	if(Length == 0) {
		//! Empty format string
		va_end(vl);
		return 1;
	}
	uint32_t NewEndOffs = *DataBufferOffsPtr + (uint32_t)Length + 1;
	if(NewEndOffs < *DataBufferOffsPtr) {
		//! BufOffs overflow
		va_end(vl);
		return 0;
	}
	if(NewEndOffs > *DataBufferSizePtr) {
		uint32_t DeltaSize = NewEndOffs - *DataBufferSizePtr;
		if(!DynamicBuffer_IncreaseSize((void**)DataBufferPtr, DataBufferSizePtr, DeltaSize)) {
			va_end(vl);
			return 0;
		}
	}

	//! Finally, write the string to the buffer
	//! NOTE: The NUL terminator is written, but BufOffs does not account for it.
	vsnprintf(*DataBufferPtr + *DataBufferOffsPtr, Length + 1, Format, vl);
	*DataBufferOffsPtr += Length;
	va_end(vl);
	return 1;
}

/************************************************/
//! EOF
/************************************************/
