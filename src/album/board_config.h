#ifndef _BOARD_CONFIG_
#define _BOARD_CONFIG_

#define  OV5640             0
#define  OV2640             0
#define  GC0328             1

#define  BOARD_KD233        0
#define  BOARD_LICHEEDAN    1
#define  BOARD_K61          0

#if (OV5640 && OV2640) || (!OV5640 && !OV2640)

#endif

#if (BOARD_LICHEEDAN && BOARD_KD233) || (BOARD_LICHEEDAN && BOARD_K61) || (BOARD_K61 && BOARD_KD233) || (!BOARD_LICHEEDAN && !BOARD_KD233 && !BOARD_K61)
#error board only choose one
#endif

#endif
