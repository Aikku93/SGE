/************************************************/
#include <stdio.h>
#include <stdint.h>
/************************************************/
#include "FileIO_Int.h"
/************************************************/

uint8_t FileIO_Get_u8(uint8_t *x, FILE *f) {
	uint8_t v;
	if(!fread(&v, sizeof(uint8_t), 1, f)) return 0;
	*x = v;
	return 1;
}

uint8_t FileIO_Get_u16le(uint16_t *x, FILE *f) {
	uint16_t v;
	if(!fread(&v, sizeof(uint16_t), 1, f)) return 0;
#if IS_BIG_ENDIAN
	v = FileIO_ByteSwap16(v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_u16be(uint16_t *x, FILE *f) {
	uint16_t v;
	if(!fread(&v, sizeof(uint16_t), 1, f)) return 0;
#if !IS_BIG_ENDIAN
	v = FileIO_ByteSwap16(v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_u32le(uint32_t *x, FILE *f) {
	uint32_t v;
	if(!fread(&v, sizeof(uint32_t), 1, f)) return 0;
#if IS_BIG_ENDIAN
	v = FileIO_ByteSwap32(v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_u32be(uint32_t *x, FILE *f) {
	uint32_t v;
	if(!fread(&v, sizeof(uint32_t), 1, f)) return 0;
#if !IS_BIG_ENDIAN
	v = FileIO_ByteSwap32(v);
#endif
	*x = v;
	return 1;
}

/************************************************/

uint8_t FileIO_Get_s8(int8_t *x, FILE *f) {
	int8_t v;
	if(!fread(&v, sizeof(int8_t), 1, f)) return 0;
	*x = v;
	return 1;
}

uint8_t FileIO_Get_s16le(int16_t *x, FILE *f) {
	int16_t v;
	if(!fread(&v, sizeof(int16_t), 1, f)) return 0;
#if IS_BIG_ENDIAN
	v = (int16_t)FileIO_ByteSwap32((int16_t)v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_s16be(int16_t *x, FILE *f) {
	int16_t v;
	if(!fread(&v, sizeof(int16_t), 1, f)) return 0;
#if !IS_BIG_ENDIAN
	v = (int16_t)FileIO_ByteSwap32((int16_t)v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_s32le(int32_t *x, FILE *f) {
	int32_t v;
	if(!fread(&v, sizeof(int32_t), 1, f)) return 0;
#if IS_BIG_ENDIAN
	v = (int32_t)FileIO_ByteSwap32((uint32_t)v);
#endif
	*x = v;
	return 1;
}

uint8_t FileIO_Get_s32be(int32_t *x, FILE *f) {
	int32_t v;
	if(!fread(&v, sizeof(int32_t), 1, f)) return 0;
#if !IS_BIG_ENDIAN
	v = (int32_t)FileIO_ByteSwap32((uint32_t)v);
#endif
	*x = v;
	return 1;
}

/************************************************/
//! EOF
/************************************************/
