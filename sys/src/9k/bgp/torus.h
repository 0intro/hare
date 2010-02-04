/*
 * 
 * Defines imported from the Linux Torus Driver
 * Copyright (C) 2007-2008, Volkmar Uhlig, IBM Corporation
 *                
 * Description:   Torus definitions
 *                
 * All rights reserved
 *                
 */

#define BGP_TORUS_COUNTERS	64
#define BGP_TORUS_DMA_REGIONS	8

#define BGP_PRIORITY_FIFOMAP 0x11
#define BGP_NORMAL_FIFOMAP 0xEE
#define BGP_KERNEL_FIFOMAP BGP_NORMAL_FIFOMAP
#define BGP_DEFAULT_FIFOMAP BGP_NORMAL_FIFOMAP

/* Watermark Bit Bashing */ /* TODO: Cleanup */
#define  _iDMA_TS_FIFO_WM_N0(x) 	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N1(x)         _B6(15,(x))	// bit {10..15} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N2(x)         _B6(23,(x))	// bit {18..23} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_P0(x)         _B6(31,(x))	// bit {26..31} of _BGP_DCR_iDMA_TORUS_FIFO_WM(0), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N3(x)		_B6(7,(x))	// bit {2..7}   of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N4(x)		_B6(15,(x))	// bit {10..15} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_N5(x)		_B6(23,(x))	// bit {18..23} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM_P1(x)         _B6(31,(x))	// bit {26..31} of _BGP_DCR_iDMA_TORUS_FIFO_WM(1), should be set to decimal 20
#define  _iDMA_TS_FIFO_WM0_INIT   	(_iDMA_TS_FIFO_WM_N0(20) | \
					 _iDMA_TS_FIFO_WM_N1(20) | \
					 _iDMA_TS_FIFO_WM_N2(20) | \
					 _iDMA_TS_FIFO_WM_P0(20))
#define  _iDMA_TS_FIFO_WM1_INIT   	(_iDMA_TS_FIFO_WM_N3(20) | \
					 _iDMA_TS_FIFO_WM_N4(20) | \
					 _iDMA_TS_FIFO_WM_N5(20) | \
					 _iDMA_TS_FIFO_WM_P1(20))

#define  _rDMA_TS_FIFO_WM_G0N0(x)	_B6(7,(x))	// bit {2..7}   of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N1(x)	_B6(15,(x))	// bit {10..15} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N2(x)	_B6(23,(x))	// bit {18..23} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM_G0N3(x)	_B6(31,(x))	// bit {26..31} of _BGP_DCR_rDMA_TORUS_FIFO_WM(0), must be 0
#define  _rDMA_TS_FIFO_WM0_INIT		(_rDMA_TS_FIFO_WM_G0N0(0) | \
					 _rDMA_TS_FIFO_WM_G0N1(0) | \
					 _rDMA_TS_FIFO_WM_G0N2(0) | \
					 _rDMA_TS_FIFO_WM_G0N3(0))

#define  _iDMA_LOCAL_FIFO_WM(x)		_B7(7,(x))	// bit {1..7}   of _BGP_DCR_iDMA_LOCAL_FIFO_WM_RPT_CNT, set to decimal 55, 0x37
#define  _iDMA_HP_INJ_FIFO_RPT_CNT(x)   _B4(11,(x))	// bit {8..11}  dma repeat count for using torus high priority injection fifo
#define  _iDMA_NP_INJ_FIFO_RPT_CNT(x)   _B4(15,(x))	// bit {12..15} dma repeat count for using torus normal priority injection fifo
#define  _iDMA_INJ_DELAY(x)		_B4(23,(x))	// bit {20..23} dma delay this amount of clock_x2 cycles before injecting next packet
#define  _iDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT	(_iDMA_LOCAL_FIFO_WM(55) | \
						 _iDMA_HP_INJ_FIFO_RPT_CNT(0) | \
						 _iDMA_NP_INJ_FIFO_RPT_CNT(0) | \
						 _iDMA_INJ_DELAY(0))


#define  _iDMA_SERVICE_QUANTA_HP(x)	_B16(15,(x))
#define  _iDMA_SERVICE_QUANTA_NP(x)	_B16(31,(x))
#define  _iDMA_SERVICE_QUANTA_INIT	(_iDMA_SERVICE_QUANTA_HP(0) | _iDMA_SERVICE_QUANTA_NP(0))

#define   _DMA_BASE_CONTROL_USE_DMA     _BN( 0)            // Use DMA and *not* the Torus if 1, reset state is 0.
#define   _DMA_BASE_CONTROL_STORE_HDR   _BN( 1)            // Store DMA Headers in Reception Header Fifo (debugging)
#define   _DMA_BASE_CONTROL_PF_DIS      _BN( 2)            // Disable Torus Prefetch Unit (should be 0)
#define   _DMA_BASE_CONTROL_L3BURST_EN  _BN( 3)            // Enable L3 Burst when 1 (should be enabled, except for debugging)
#define   _DMA_BASE_CONTROL_ITIU_EN     _BN( 4)            // Enable Torus Injection Data Transfer Unit (never make this zero)
#define   _DMA_BASE_CONTROL_RTIU_EN     _BN( 5)            // Enable Torus Reception Data Transfer Unit
#define   _DMA_BASE_CONTROL_IMFU_EN     _BN( 6)            // Enable DMA Injection Fifo Unit Arbiter
#define   _DMA_BASE_CONTROL_RMFU_EN     _BN( 7)            // Enable DMA Reception fifo Unit Arbiter
#define   _DMA_BASE_CONTROL_L3PF_DIS    _BN( 8)            // Disable L3 Read Prefetch (should be 0)
                                                           //  9..27 reserved.
#define   _DMA_BASE_CONTROL_REC_FIFO_FULL_STOP_RDMA   _BN( 28) // DD2 Only, ECO 777, RDMA stops when fifo is full
#define   _DMA_BASE_CONTROL_REC_FIFO_CROSSTHRESH_NOTSTICKY  _BN( 29) // DD2 Only, ECO 777, Rec. Fifo Threshold crossed is not sticky
#define   _DMA_BASE_CONTROL_INJ_FIFO_CROSSTHRESH_NOTSTICKY  _BN( 30) // DD2 Only, ECO 777, Inj. Fifo Threshold crossed is not sticky
                                                           // 31 - ECO 653, leave at 0
#define _BGP_DCR_DMA_BASE_CONTROL_INIT  ( _DMA_BASE_CONTROL_USE_DMA    | \
                                          _DMA_BASE_CONTROL_L3BURST_EN | \
                                          _DMA_BASE_CONTROL_ITIU_EN    | \
                                          _DMA_BASE_CONTROL_RTIU_EN    | \
                                          _DMA_BASE_CONTROL_IMFU_EN    | \
                                          _DMA_BASE_CONTROL_RMFU_EN)


#define  _rDMA_TS_REC_FIFO_MAP_XP(x)		_B8(7,(x))
#define  _rDMA_TS_REC_FIFO_MAP_XM(x)		_B8(15,(x))
#define  _rDMA_TS_REC_FIFO_MAP_YP(x)		_B8(23,(x))
#define  _rDMA_TS_REC_FIFO_MAP_YM(x)		_B8(31,(x))
#define  _rDMA_TS_REC_FIFO_MAP_ZP(x)		_B8(7,(x))
#define  _rDMA_TS_REC_FIFO_MAP_ZM(x)		_B8(15,(x))
#define  _rDMA_TS_REC_FIFO_MAP_HIGH(x)		_B8(23,(x))
#define  _rDMA_TS_REC_FIFO_MAP_LOCAL(x)		_B8(31,(x))

#define  _rDMA_LOCAL_FIFO_WM(x)		_B7(7,(x))	// bit {1..7}, local fifo watermark, must be 0
#define  _rDMA_HP_REC_FIFO_RPT_CNT(x)	_B4(11,(x))	// bit {8..11}, dma repeat count for torus high priority reception fifos
#define  _rDMA_NP_REC_FIFO_RPT_CNT(x)	_B4(15,(x))	// bit {12..15}, dma repeat count for torus normal priority reception fifos
#define  _rDMA_DELAY(x)			_B4(23,(x))	// bit {20..23}, dma delay this amount of clock_x2 cycles between packets

#define  _rDMA_LOCAL_FIFO_WM_RPT_CNT_DELAY_INIT	(_rDMA_LOCAL_FIFO_WM(0) | \
						 _rDMA_HP_REC_FIFO_RPT_CNT(0) | \
						 _rDMA_NP_REC_FIFO_RPT_CNT(0) | \
						 _rDMA_DELAY(0))

#define  _rDMA_FIFO_CLEAR_MASK0_INIT		0xFF000000
#define  _rDMA_FIFO_CLEAR_MASK1_INIT		0x00FF0000
#define  _rDMA_FIFO_CLEAR_MASK2_INIT		0x0000FF00
#define  _rDMA_FIFO_CLEAR_MASK3_INIT		0x000000FF
#define  _rDMA_FIFO_HEADER_CLEAR_MASK_INIT	0x08040201

#define _BGP_DCR_DMA_CLEAR0                             (_BGP_DCR_DMA+0xb1)
#define  _DMA_CLEAR0_IMFU_ARB_WERR		_BN(0)
#define  _DMA_CLEAR0_IMFU_COUNTER_UNDERFLOW	_BN(1)
#define  _DMA_CLEAR0_IMFU_COUNTER_OVERFLOW	_BN(2)
#define  _DMA_CLEAR0_RMFU_COUNTER_UNDERFLOW	_BN(3)
#define  _DMA_CLEAR0_RMFU_COUNTER_OVERFLOW	_BN(4)
#define  _DMA_CLEAR0_RMFU_ARB_WERR		_BN(5)
#define  _DMA_CLEAR0_PQUE_WR0_BEN_WERR		_BN(6)
#define  _DMA_CLEAR0_PQUE_WR0_ADDR_CHK_WERR	_BN(7)
#define  _DMA_CLEAR0_PQUE_RD0_ADDR_CHK_WERR	_BN(8)
#define  _DMA_CLEAR0_PQUE_WR1_BEN_WERR		_BN(9)
#define  _DMA_CLEAR0_PQUE_WR1_ADDR_CHK_WERR	_BN(10)
#define  _DMA_CLEAR0_PQUE_RD1_ADDR_CHK_WERR	_BN(11)
#define  _DMA_CLEAR0_PQUE_WR0_HOLD_BAD_ADDR	_BN(12)
#define  _DMA_CLEAR0_PQUE_RD0_HOLD_BAD_ADDR	_BN(13)
#define  _DMA_CLEAR0_PQUE_WR1_HOLD_BAD_ADDR	_BN(14)
#define  _DMA_CLEAR0_PQUE_RD1_HOLD_BAD_ADDR	_BN(15)
#define  _DMA_CLEAR0_IFIFO_ARRAY_UE0		_BN(16)
#define  _DMA_CLEAR0_IFIFO_ARRAY_UE1		_BN(17)
#define  _DMA_CLEAR0_ICOUNTER_ARRAY_UE		_BN(18)
#define  _DMA_CLEAR0_IMFU_DESC_UE		_BN(19)
#define  _DMA_CLEAR0_RFIFO_ARRAY_UE0		_BN(20)
#define  _DMA_CLEAR0_RFIFO_ARRAY_UE1		_BN(21)
#define  _DMA_CLEAR0_RCOUNTER_ARRAY_UE		_BN(22)
#define  _DMA_CLEAR0_LOCAL_FIFO_UE0		_BN(23)
#define  _DMA_CLEAR0_LOCAL_FIFO_UE1		_BN(24)

#define _BGP_DCR_DMA_CLEAR1                             (_BGP_DCR_DMA+0xb2)
#define  _DMA_CLEAR1_TS_LINK_CHK		_BN(0)
