/************************************************/
#pragma once
/************************************************/
#include <stdio.h>
#include <stdint.h>
/************************************************/
#ifdef __cplusplus
extern "C" {
#endif
/************************************************/

static inline uint16_t FileIO_ByteSwap16(uint16_t x) {
	return x<<8 | x>>8;
}

static inline uint32_t FileIO_ByteSwap32(uint32_t x) {
	return x<<24 | (x&0xFF00)<<8 | (x&0xFF0000)>>8 | x>>24;
}

/************************************************/

uint8_t FileIO_Get_u8   (uint8_t  *x, FILE *f);
uint8_t FileIO_Get_u16le(uint16_t *x, FILE *f);
uint8_t FileIO_Get_u16be(uint16_t *x, FILE *f);
uint8_t FileIO_Get_u32le(uint32_t *x, FILE *f);
uint8_t FileIO_Get_u32be(uint32_t *x, FILE *f);
uint8_t FileIO_Get_s8   ( int8_t  *x, FILE *f);
uint8_t FileIO_Get_s16le( int16_t *x, FILE *f);
uint8_t FileIO_Get_s16be( int16_t *x, FILE *f);
uint8_t FileIO_Get_s32le( int32_t *x, FILE *f);
uint8_t FileIO_Get_s32be( int32_t *x, FILE *f);

/************************************************/
#ifdef __cplusplus
}
#endif
/************************************************/
//! EOF
/************************************************/
