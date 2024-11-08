/************************************************/
#pragma once
/************************************************/

//! Definitions for GBA/NDS hardware architecture
//! This is just to avoid relying on one specific GBA/NDS library or another

/************************************************/

#ifdef __GBA__
# define GBA_HW_FREQ_HZ 16777216
# define GBA_FRAME_CYCLES 280896
#else
# define NDS_HW_FREQ_HZ 33513982
# define NDS_FRAME_CYCLES 560190
#endif

/************************************************/

#define REG_BASE_ADR 0x04000000
#define REG_IME (REG_BASE_ADR + 0x0208)

/************************************************/
#if defined(__GBA__)
/************************************************/

//! REG_SOUNDCNT[L/H/X]
#define REG_SOUNDCNT    (REG_BASE_ADR + 0x0080)
#define REG_SOUNDCNT_L  (REG_BASE_ADR + 0x0080)
#define REG_SOUNDCNT_H  (REG_BASE_ADR + 0x0082)
#define REG_SOUNDCNT_X  (REG_BASE_ADR + 0x0084)
#define REG_SOUNDFIFO_A (REG_BASE_ADR + 0x00A0)
#define REG_SOUNDFIFO_B (REG_BASE_ADR + 0x00A4)
#define REG_SOUNDCNT_LH_VALUE(Lo,Hi)     ((Lo) | (Hi)<<16)
#define REG_SOUNDCNT_L_PSG_VOL_R(x)      ((x) << 0) //! 0~7
#define REG_SOUNDCNT_L_PSG_VOL_R_MASK    (REG_SOUNDCNT_L_PSG_VOL_R(7))
#define REG_SOUNDCNT_L_PSG_VOL_L(x)      ((x) << 4) //! 0~7
#define REG_SOUNDCNT_L_PSG_VOL_L_MASK    (REG_SOUNDCNT_L_PSG_VOL_L(7))
#define REG_SOUNDCNT_L_PSG_ENABLE_R(x)   (1 << ( 8 + (x)-1)) //! 1~4
#define REG_SOUNDCNT_L_PSG_ENABLE_L(x)   (1 << (12 + (x)-1)) //! 1~4
#define REG_SOUNDCNT_H_PSG_VOL(x)        ((x) << 0)
#define REG_SOUNDCNT_H_PSG_VOL_25        (REG_SOUNDCNT_H_PSG_VOL(0))
#define REG_SOUNDCNT_H_PSG_VOL_50        (REG_SOUNDCNT_H_PSG_VOL(1))
#define REG_SOUNDCNT_H_PSG_VOL_100       (REG_SOUNDCNT_H_PSG_VOL(2))
#define REG_SOUNDCNT_H_PSG_VOL_MASK      (REG_SOUNDCNT_H_PSG_VOL(3))
#define REG_SOUNDCNT_H_FIFO_A_VOL(x)     ((x) << 2)
#define REG_SOUNDCNT_H_FIFO_A_VOL_50     (REG_SOUNDCNT_H_FIFO_A_VOL(0))
#define REG_SOUNDCNT_H_FIFO_A_VOL_100    (REG_SOUNDCNT_H_FIFO_A_VOL(1))
#define REG_SOUNDCNT_H_FIFO_A_VOL_MASK   (REG_SOUNDCNT_H_FIFO_A_VOL(1))
#define REG_SOUNDCNT_H_FIFO_B_VOL(x)     ((x) << 3)
#define REG_SOUNDCNT_H_FIFO_B_VOL_50     (REG_SOUNDCNT_H_FIFO_B_VOL(0))
#define REG_SOUNDCNT_H_FIFO_B_VOL_100    (REG_SOUNDCNT_H_FIFO_B_VOL(1))
#define REG_SOUNDCNT_H_FIFO_B_VOL_MASK   (REG_SOUNDCNT_H_FIFO_B_VOL(1))
#define REG_SOUNDCNT_H_FIFO_A_ENABLE_R   (0x0100)
#define REG_SOUNDCNT_H_FIFO_A_ENABLE_L   (0x0200)
#define REG_SOUNDCNT_H_FIFO_A_TIMER(x)   ((x) << 10)
#define REG_SOUNDCNT_H_FIFO_A_TIMER_0    (REG_SOUNDCNT_H_FIFO_A_TIMER(0))
#define REG_SOUNDCNT_H_FIFO_A_TIMER_1    (REG_SOUNDCNT_H_FIFO_A_TIMER(1))
#define REG_SOUNDCNT_H_FIFO_A_TIMER_MASK (REG_SOUNDCNT_H_FIFO_A_TIMER(1))
#define REG_SOUNDCNT_H_FIFO_A_FLUSH      (0x0800)
#define REG_SOUNDCNT_H_FIFO_B_ENABLE_R   (0x1000)
#define REG_SOUNDCNT_H_FIFO_B_ENABLE_L   (0x2000)
#define REG_SOUNDCNT_H_FIFO_B_TIMER(x)   ((x) << 14)
#define REG_SOUNDCNT_H_FIFO_B_TIMER_0    (REG_SOUNDCNT_H_FIFO_B_TIMER(0))
#define REG_SOUNDCNT_H_FIFO_B_TIMER_1    (REG_SOUNDCNT_H_FIFO_B_TIMER(1))
#define REG_SOUNDCNT_H_FIFO_B_TIMER_MASK (REG_SOUNDCNT_H_FIFO_B_TIMER(1))
#define REG_SOUNDCNT_H_FIFO_B_FLUSH      (0x8000)
#define REG_SOUNDCNT_X_PSG_ON(x)         (1 << (0 + (x)-1)) //! 1~4
#define REG_SOUNDCNT_X_MASTER_ENABLE     (0x0080)

//! REG_DMACNT[L/H]
#define REG_DMASAD(x)   (REG_BASE_ADR + 0x00B0 + (x)*0x0C)
#define REG_DMADAD(x)   (REG_BASE_ADR + 0x00B4 + (x)*0x0C)
#define REG_DMACNT(x)   (REG_BASE_ADR + 0x00B8 + (x)*0x0C)
#define REG_DMACNT_L(x) (REG_BASE_ADR + 0x00B8 + (x)*0x0C)
#define REG_DMACNT_H(x) (REG_BASE_ADR + 0x00BA + (x)*0x0C)
#define REG_DMACNT_LH_VALUE(Lo,Hi)       ((Lo) | (Hi)<<16)
#define REG_DMACNT_L_COUNT(x)            ((x) << 0)
#define REG_DMACNT_L_COUNT_MASK          (REG_DMACNT_L_COUNT(0xFFFF))
#define REG_DMACNT_H_DST_MODE(x)         ((x) << 5)
#define REG_DMACNT_H_DST_MODE_INC        (REG_DMACNT_H_DST_MODE(0))
#define REG_DMACNT_H_DST_MODE_DEC        (REG_DMACNT_H_DST_MODE(1))
#define REG_DMACNT_H_DST_MODE_FIXED      (REG_DMACNT_H_DST_MODE(2))
#define REG_DMACNT_H_DST_MODE_INC_REPEAT (REG_DMACNT_H_DST_MODE(3))
#define REG_DMACNT_H_DST_MODE_MASK       (REG_DMACNT_H_DST_MODE(3))
#define REG_DMACNT_H_SRC_MODE(x)         ((x) << 7)
#define REG_DMACNT_H_SRC_MODE_INC        (REG_DMACNT_H_SRC_MODE(0))
#define REG_DMACNT_H_SRC_MODE_DEC        (REG_DMACNT_H_SRC_MODE(1))
#define REG_DMACNT_H_SRC_MODE_FIXED      (REG_DMACNT_H_SRC_MODE(2))
#define REG_DMACNT_H_SRC_MODE_MASK       (REG_DMACNT_H_SRC_MODE(3))
#define REG_DMACNT_H_REPEAT              (0x0200)
#define REG_DMACNT_H_32BIT               (0x0400)
#define REG_DMACNT3_H_GAMEPAK_DRQ        (0x0800)
#define REG_DMACNT_H_MODE(x)             ((x) << 12)
#define REG_DMACNT_H_MODE_IMMEDIATE      (REG_DMACNT_H_MODE(0))
#define REG_DMACNT_H_MODE_VBLANK         (REG_DMACNT_H_MODE(1))
#define REG_DMACNT_H_MODE_HBLANK         (REG_DMACNT_H_MODE(2))
#define REG_DMACNT1_H_MODE_SOUNDFIFO     (REG_DMACNT_H_MODE(3))
#define REG_DMACNT2_H_MODE_SOUNDFIFO     (REG_DMACNT_H_MODE(3))
#define REG_DMACNT3_H_MODE_VIDEOCAPTURE  (REG_DMACNT_H_MODE(3))
#define REG_DMACNT_H_MODE_MASK           (REG_DMACNT_H_MODE(3))
#define REG_DMACNT_H_IRQ_ON_FINISH       (0x4000)
#define REG_DMACNT_H_ENABLE              (0x8000)

//! REG_TIMER[L/H]
#define REG_TIMER(x)   (REG_BASE_ADR + 0x0100 + (x)*0x04)
#define REG_TIMER_L(x) (REG_BASE_ADR + 0x0100 + (x)*0x04)
#define REG_TIMER_H(x) (REG_BASE_ADR + 0x0102 + (x)*0x04)
#define REG_TIMER_PERIOD_FROM_FREQ(x)    (AGB_HW_FREQ_HZ / (x))
#define REG_TIMER_LH_VALUE(Lo,Hi)        ((Lo) | (Hi)<<16)
#define REG_TIMER_L_VALUE(x)             ((x) << 0)
#define REG_TIMER_L_VALUE_FROM_PERIOD(x) REG_TIMER_L_VALUE((1<<16) - (x))
#define REG_TIMER_L_VALUE_FROM_FREQ(x)   REG_TIMER_L_VALUE_FROM_PERIOD(REG_TIMER_PERIOD_FROM_FREQ(x))
#define REG_TIMER_H_DIVIDER(x)           ((x) << 0)
#define REG_TIMER_H_DIVIDER_1X           (REG_TIMER_H_DIVIDER(0))
#define REG_TIMER_H_DIVIDER_64X          (REG_TIMER_H_DIVIDER(1))
#define REG_TIMER_H_DIVIDER_256X         (REG_TIMER_H_DIVIDER(2))
#define REG_TIMER_H_DIVIDER_1024X        (REG_TIMER_H_DIVIDER(3))
#define REG_TIMER_H_DIVIDER_MASK         (REG_TIMER_H_DIVIDER(3))
#define REG_TIMER_H_SLAVE                (0x0004)
#define REG_TIMER_H_IRQ_ON_BURST         (0x0040)
#define REG_TIMER_H_ENABLE               (0x0080)

/************************************************/
#elif (__NDS__ == 7)
/************************************************/

/************************************************/
#elif (__NDS__ == 9)
/************************************************/

/************************************************/
#endif
/************************************************/

//! REG_IME
#define REG_IME_ON (0x01)

/************************************************/
//! EOF
/************************************************/
