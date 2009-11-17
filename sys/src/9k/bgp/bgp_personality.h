/*
 * Andrew Tauferner
 *
 * Copyright 2006, 2007 International Business Machines
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef	BGP_PERSONALITY_H_ // Prevent multiple inclusion
#define	BGP_PERSONALITY_H_




//#include <linux/types.h>
typedef u8int uint8_t;
typedef u16int uint16_t;
typedef u32int uint32_t;
typedef u64int uint64_t;
typedef uchar int8_t;
typedef short int16_t;
typedef int int32_t;
typedef vlong int64_t;

typedef struct TBGP_Personality_t BGP_Personality_t;
extern BGP_Personality_t* personality;

// These defines allows use of IBM's bit numberings (MSb=0, LSb=31)for multi-bit fields
//  b = IBM bit number of the least significant bit (highest number)
//  x = value to set in field
//  s = size
#define _BS(b,x,s)( ( ( x) & ( 0x7FFFFFFF>> ( 31- ( s)))) << ( 31- ( b)))
#define _BG(b,x,s)( ( _BS(b,0x7FFFFFFF,s) & x ) >> (31-b) )
#define _BS64(b,x,s)( ( ( x) & ( 0x7FFFFFFFFFFFFFFFLL>> ( 63- ( s)))) << ( 63- ( b)))
#define _BG64(b,x,s)( ( _BS64(b, 0x7FFFFFFFFFFFFFFFLL,s) & x ) >> (63-b) )
#define _BN(b)    ((1<<(31-(b))))
#define _B1(b,x)  (((x)&0x1)<<(31-(b)))
#define _B2(b,x)  (((x)&0x3)<<(31-(b)))
#define _B3(b,x)  (((x)&0x7)<<(31-(b)))
#define _B4(b,x)  (((x)&0xF)<<(31-(b)))
#define _B5(b,x)  (((x)&0x1F)<<(31-(b)))
#define _B6(b,x)  (((x)&0x3F)<<(31-(b)))
#define _B7(b,x)  (((x)&0x7F)<<(31-(b)))
#define _B8(b,x)  (((x)&0xFF)<<(31-(b)))
#define _B9(b,x)  (((x)&0x1FF)<<(31-(b)))
#define _B10(b,x) (((x)&0x3FF)<<(31-(b)))
#define _B11(b,x) (((x)&0x7FF)<<(31-(b)))
#define _B12(b,x) (((x)&0xFFF)<<(31-(b)))
#define _B13(b,x) (((x)&0x1FFF)<<(31-(b)))
#define _B14(b,x) (((x)&0x3FFF)<<(31-(b)))
#define _B15(b,x) (((x)&0x7FFF)<<(31-(b)))
#define _B16(b,x) (((x)&0xFFFF)<<(31-(b)))
#define _B17(b,x) (((x)&0x1FFFF)<<(31-(b)))
#define _B18(b,x) (((x)&0x3FFFF)<<(31-(b)))
#define _B19(b,x) (((x)&0x7FFFF)<<(31-(b)))
#define _B20(b,x) (((x)&0xFFFFF)<<(31-(b)))
#define _B21(b,x) (((x)&0x1FFFFF)<<(31-(b)))
#define _B22(b,x) (((x)&0x3FFFFF)<<(31-(b)))
#define _B23(b,x) (((x)&0x7FFFFF)<<(31-(b)))
#define _B24(b,x) (((x)&0xFFFFFF)<<(31-(b)))
#define _B25(b,x) (((x)&0x1FFFFFF)<<(31-(b)))
#define _B26(b,x) (((x)&0x3FFFFFF)<<(31-(b)))
#define _B27(b,x) (((x)&0x7FFFFFF)<<(31-(b)))
#define _B28(b,x) (((x)&0xFFFFFFF)<<(31-(b)))
#define _B29(b,x) (((x)&0x1FFFFFFF)<<(31-(b)))
#define _B30(b,x) (((x)&0x3FFFFFFF)<<(31-(b)))
#define _B31(b,x) (((x)&0x7FFFFFFF)<<(31-(b)))

#define BGP_UCI_Component_Rack              ( 0)
#define BGP_UCI_Component_Midplane          ( 1)
#define BGP_UCI_Component_BulkPowerSupply   ( 2)
#define BGP_UCI_Component_PowerCable        ( 3)
#define BGP_UCI_Component_PowerModule       ( 4)
#define BGP_UCI_Component_ClockCard         ( 5)
#define BGP_UCI_Component_FanAssembly       ( 6)
#define BGP_UCI_Component_Fan               ( 7)
#define BGP_UCI_Component_ServiceCard       ( 8)
#define BGP_UCI_Component_LinkCard          ( 9)
#define BGP_UCI_Component_LinkChip          (10)
#define BGP_UCI_Component_LinkPort          (11)  // Identifies 1 end of a LinkCable
#define BGP_UCI_Component_NodeCard          (12)
#define BGP_UCI_Component_ComputeCard       (13)
#define BGP_UCI_Component_IOCard            (14)
#define BGP_UCI_Component_DDRChip           (15)
#define BGP_UCI_Component_ENetConnector     (16)

typedef struct BGP_UCI_Rack_t
                {                           // "Rxy": R<RackRow><RackColumn>
                unsigned Component   :  5;  // when BGP_UCI_Component_Rack
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned _zero       : 19;  // zero's
                }
                BGP_UCI_Rack_t;

#define BGP_UCI_RACK_COMPONENT(x)              _B5( 4,x)  // when BGP_UCI_Component_Rack
#define BGP_UCI_RACK_RACKROW(x)                _B4( 8,x)  // 0..F
#define BGP_UCI_RACK_RACKCOLUMN(x)             _B4(12,x)  // 0..F



typedef struct BGP_UCI_Midplane_t
                {                           // "Rxy-Mm": R<RackRow><RackColumn>-M<Midplane>
                unsigned Component   :  5;  // when BGP_UCI_Component_Midplane
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned _zero       : 18;  // zero's
                }
                BGP_UCI_Midplane_t;

#define BGP_UCI_MIDPLANE_COMPONENT(x)          _B5( 4,x)  // when BGP_UCI_Component_Midplane
#define BGP_UCI_MIDPLANE_RACKROW(x)            _B4( 8,x)  // 0..F
#define BGP_UCI_MIDPLANE_RACKCOLUMN(x)         _B4(12,x)  // 0..F
#define BGP_UCI_MIDPLANE_MIDPLANE(x)           _B1(13,x)  // 0=Bottom, 1=Top


typedef struct BGP_UCI_BulkPowerSupply_t
                {                           // "Rxy-B": R<RackRow><RackColumn>-B
                unsigned Component   :  5;  // when BGP_UCI_Component_BulkPowerSupply
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned _zero       : 19;  // zero's
                }
                BGP_UCI_BulkPowerSupply_t;

#define BGP_UCI_BULKPOWERSUPPLY_COMPONENT(x)   _B5( 4,x)  // when BGP_UCI_Component_BulkPowerSupply
#define BGP_UCI_BULKPOWERSUPPLY_RACKROW(x)     _B4( 8,x)  // 0..F
#define BGP_UCI_BULKPOWERSUPPLY_RACKCOLUMN(x)  _B4(12,x)  // 0..F



typedef struct BGP_UCI_PowerCable_t
                {                           // "Rxy-B-C": R<RackRow><RackColumn>-B-C
                unsigned Component   :  5;  // when BGP_UCI_Component_PowerCable
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned _zero       : 19;  // zero's
                }
                BGP_UCI_PowerCable_t;

#define BGP_UCI_POWERCABLE_COMPONENT(x)        _B5( 4,x)  // when BGP_UCI_Component_PowerCable
#define BGP_UCI_POWERCABLE_RACKROW(x)          _B4( 8,x)  // 0..F
#define BGP_UCI_POWERCABLE_RACKCOLUMN(x)       _B4(12,x)  // 0..F



typedef struct BGP_UCI_PowerModule_t
                {                           // "Rxy-B-Pp": R<RackRow><RackColumn>-B-P<PowerModule>
                unsigned Component   :  5;  // when BGP_UCI_Component_PowerModule
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned PowerModule :  3;  // 0..7 (0..3 left to right facing front, 4-7 left to right facing rear)
                unsigned _zero       : 16;  // zero's
                }
                BGP_UCI_PowerModule_t;

#define BGP_UCI_POWERMODULE_COMPONENT(x)       _B5( 4,x)  // when BGP_UCI_Component_PowerModule
#define BGP_UCI_POWERMODULE_RACKROW(x)         _B4( 8,x)  // 0..F
#define BGP_UCI_POWERMODULE_RACKCOLUMN(x)      _B4(12,x)  // 0..F
#define BGP_UCI_POWERMODULE_POWERMODULE(x)     _B3(15,x)  // 0..7 (0..3 left to right facing front, 4-7 left to right facing rear)


typedef struct BGP_UCI_ClockCard_t
                {                           // "Rxy-K": R<RackRow><RackColumn>-K
                unsigned Component   :  5;  // when BGP_UCI_Component_ClockCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned _zero       : 19;  // zero's
                }
                BGP_UCI_ClockCard_t;

#define BGP_UCI_CLOCKCARD_COMPONENT(x)         _B5( 4,x)  // when BGP_UCI_Component_PowerModule
#define BGP_UCI_CLOCKCARD_RACKROW(x)           _B4( 8,x)  // 0..F
#define BGP_UCI_CLOCKCARD_RACKCOLUMN(x)        _B4(12,x)  // 0..F



typedef struct BGP_UCI_FanAssembly_t
                {                           // "Rxy-Mm-Aa": R<RackRow><RackColumn>-M<Midplane>-A<FanAssembly>
                unsigned Component   :  5;  // when BGP_UCI_Component_FanAssembly
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned FanAssembly :  4;  // 0..9 (0=Bot Front, 4=Top Front, 5=Bot Rear, 9=Top Rear)
                unsigned _zero       : 14;  // zero's
                }
                BGP_UCI_FanAssembly_t;

#define BGP_UCI_FANASSEMBLY_COMPONENT(x)       _B5( 4,x)  // when BGP_UCI_Component_FanAssembly
#define BGP_UCI_FANASSEMBLY_RACKROW(x)         _B4( 8,x)  // 0..F
#define BGP_UCI_FANASSEMBLY_RACKCOLUMN(x)      _B4(12,x)  // 0..F
#define BGP_UCI_FANASSEMBLY_MIDPLANE(x)        _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_FANASSEMBLY_FANASSEMBLY(x)     _B4(17,x)  // 0..9 (0=Bot Front, 4=Top Front, 5=Bot Rear, 9=Top Rear)



typedef struct BGP_UCI_Fan_t
                {                           // "Rxy-Mm-Aa-Ff": R<RackRow><RackColumn>-M<Midplane>-A<FanAssembly>-F<Fan>
                unsigned Component   :  5;  // when BGP_UCI_Component_Fan
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned FanAssembly :  4;  // 0..9 (0=Bot Front, 4=Top Front, 5=Bot Rear, 9=Top Rear)
                unsigned Fan         :  2;  // 0..2 (0=Tailstock, 2=Midplane)
                unsigned _zero       : 12;  // zero's
                }
                BGP_UCI_Fan_t;

#define BGP_UCI_FAN_COMPONENT(x)               _B5( 4,x)  // when BGP_UCI_Component_Fan
#define BGP_UCI_FAN_RACKROW(x)                 _B4( 8,x)  // 0..F
#define BGP_UCI_FAN_RACKCOLUMN(x)              _B4(12,x)  // 0..F
#define BGP_UCI_FAN_MIDPLANE(x)                _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_FAN_FANASSEMBLY(x)             _B4(17,x)  // 0..9 (0=Bot Front, 4=Top Front, 5=Bot Rear, 9=Top Rear)
#define BGP_UCI_FAN_FAN(x)                     _B2(19,x)  // 0..2 (0=Tailstock, 2=Midplane)

typedef struct BGP_UCI_ServiceCard_t
                {                           // "Rxy-Mm-S": R<RackRow><RackColumn>-M<Midplane>-S
                unsigned Component   :  5;  // when BGP_UCI_Component_ServiceCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top (Master ServiceCard in M0)
                unsigned _zero       : 18;  // zero's
                }
                BGP_UCI_ServiceCard_t;

#define BGP_UCI_SERVICECARD_COMPONENT(x)       _B5( 4,x)  // when BGP_UCI_Component_ServiceCard
#define BGP_UCI_SERVICECARD_RACKROW(x)         _B4( 8,x)  // 0..F
#define BGP_UCI_SERVICECARD_RACKCOLUMN(x)      _B4(12,x)  // 0..F
#define BGP_UCI_SERVICECARD_MIDPLANE(x)        _B1(13,x)  // 0=Bottom, 1=Top (Master ServiceCard in M0)



typedef struct BGP_UCI_LinkCard_t
                {                           // "Rxy-Mm-Ll": R<RackRow><RackColumn>-M<Midplane>-L<LinkCard>
                unsigned Component   :  5;  // when BGP_UCI_Component_LinkCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned LinkCard    :  2;  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)
                unsigned _zero       : 16;  // zero's
                }
                BGP_UCI_LinkCard_t;

#define BGP_UCI_LINKCARD_COMPONENT(x)          _B5( 4,x)  // when BGP_UCI_Component_LinkCard
#define BGP_UCI_LINKCARD_RACKROW(x)            _B4( 8,x)  // 0..F
#define BGP_UCI_LINKCARD_RACKCOLUMN(x)         _B4(12,x)  // 0..F
#define BGP_UCI_LINKCARD_MIDPLANE(x)           _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_LINKCARD_LINKCARD(x)           _B2(15,x)  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)



typedef struct BGP_UCI_LinkChip_t
                {                           // "Rxy-Mm-Ll-Uu": R<RackRow><RackColumn>-M<Midplane>-L<LinkCard>-U<LinkChip>
                unsigned Component   :  5;  // when BGP_UCI_Component_LinkChip
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned LinkCard    :  2;  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)
                unsigned LinkChip    :  3;  // 00..05: left to right from Front
                unsigned _zero       : 13;  // zero's
                }
                BGP_UCI_LinkChip_t;

#define BGP_UCI_LINKCHIP_COMPONENT(x)          _B5( 4,x)  // when BGP_UCI_Component_LinkChip
#define BGP_UCI_LINKCHIP_RACKROW(x)            _B4( 8,x)  // 0..F
#define BGP_UCI_LINKCHIP_RACKCOLUMN(x)         _B4(12,x)  // 0..F
#define BGP_UCI_LINKCHIP_MIDPLANE(x)           _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_LINKCHIP_LINKCARD(x)           _B2(15,x)  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)
#define BGP_UCI_LINKCHIP_LINKCHIP(x)           _B3(18,x)  // 00..05: left to right from Front

typedef struct BGP_UCI_LinkPort_t
                {                           // "Rxy-Mm-Ll-Jjj": R<RackRow><RackColumn>-M<Midplane>-L<LinkCard>-J<LinkPort>
                unsigned Component   :  5;  // when BGP_UCI_Component_LinkPort
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned LinkCard    :  2;  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)
                unsigned LinkPort    :  4;  // 00..15: left to right from Front
                unsigned _zero       : 12;  // zero's
                }
                BGP_UCI_LinkPort_t;

#define BGP_UCI_LINKPORT_COMPONENT(x)          _B5( 4,x)  // when BGP_UCI_Component_LinkPort
#define BGP_UCI_LINKPORT_RACKROW(x)            _B4( 8,x)  // 0..F
#define BGP_UCI_LINKPORT_RACKCOLUMN(x)         _B4(12,x)  // 0..F
#define BGP_UCI_LINKPORT_MIDPLANE(x)           _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_LINKPORT_LINKCARD(x)           _B2(15,x)  // 0..3: 0=BF, 1=TF, 2=BR, 3=TR)
#define BGP_UCI_LINKPORT_LINKPORT(x)           _B4(19,x)  // 00..15: left to right from Front


typedef struct BGP_UCI_NodeCard_t
                {                           // "Rxy-Mm-Nnn": R<RackRow><RackColumn>-M<Midplane>-N<NodeCard>
                unsigned Component   :  5;  // when BGP_UCI_Component_NodeCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned NodeCard    :  4;  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
                unsigned _zero       : 14;  // zero's
                }
                BGP_UCI_NodeCard_t;

#define BGP_UCI_NODECARD_COMPONENT(x)          _B5( 4,x)  // when BGP_UCI_Component_NodeCard
#define BGP_UCI_NODECARD_RACKROW(x)            _B4( 8,x)  // 0..F
#define BGP_UCI_NODECARD_RACKCOLUMN(x)         _B4(12,x)  // 0..F
#define BGP_UCI_NODECARD_MIDPLANE(x)           _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_NODECARD_NODECARD(x)           _B4(17,x)  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)



typedef struct BGP_UCI_ComputeCard_t
                {                           // "Rxy-Mm-Nnn-Jxx": R<RackRow><RackColumn>-M<Midplane>-N<NodeCard>-J<ComputeCard>
                unsigned Component   :  5;  // when BGP_UCI_Component_ComputeCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned NodeCard    :  4;  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
                unsigned ComputeCard :  6;  // 04..35 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)
                unsigned _zero       :  8;  // zero's
                }
                BGP_UCI_ComputeCard_t;

#define BGP_UCI_COMPUTECARD_COMPONENT(x)       _B5( 4,x)  // when BGP_UCI_Component_ComputeCard
#define BGP_UCI_COMPUTECARD_RACKROW(x)         _B4( 8,x)  // 0..F
#define BGP_UCI_COMPUTECARD_RACKCOLUMN(x)      _B4(12,x)  // 0..F
#define BGP_UCI_COMPUTECARD_MIDPLANE(x)        _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_COMPUTECARD_NODECARD(x)        _B4(17,x)  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
#define BGP_UCI_COMPUTECARD_COMPUTECARD(x)     _B6(23,x)  // 04..35 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)


typedef struct BGP_UCI_IOCard_t
                {                           // "Rxy-Mm-Nnn-Jxx": R<RackRow><RackColumn>-M<Midplane>-N<NodeCard>-J<ComputeCard>
                unsigned Component   :  5;  // when BGP_UCI_Component_IOCard
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned NodeCard    :  4;  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
                unsigned ComputeCard :  6;  // 00..01 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)
                unsigned _zero       :  8;  // zero's
                }
                BGP_UCI_IOCard_t;

#define BGP_UCI_IOCARD_COMPONENT(x)            _B5( 4,x)  // when BGP_UCI_Component_IOCard
#define BGP_UCI_IOCARD_RACKROW(x)              _B4( 8,x)  // 0..F
#define BGP_UCI_IOCARD_RACKCOLUMN(x)           _B4(12,x)  // 0..F
#define BGP_UCI_IOCARD_MIDPLANE(x)             _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_IOCARD_NODECARD(x)             _B4(17,x)  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
#define BGP_UCI_IOCARD_COMPUTECARD(x)          _B6(23,x)  // 00..01 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)



typedef struct BGP_UCI_DDRChip_t
                {                           // "Rxy-Mm-Nnn-Jxx-Uuu": R<RackRow><RackColumn>-M<Midplane>-N<NodeCard>-J<ComputeCard>-U<DDRChip>
                unsigned Component   :  5;  // when BGP_UCI_Component_DDRChip
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned NodeCard    :  4;  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
                unsigned ComputeCard :  6;  // 00..01 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)
                unsigned DDRChip     :  5;  // 00..20
                unsigned _zero       :  3;  // zero's
                }
                BGP_UCI_DDRChip_t;

#define BGP_UCI_DDRCHIP_COMPONENT(x)           _B5( 4,x)  // when BGP_UCI_Component_DDRChip
#define BGP_UCI_DDRCHIP_RACKROW(x)             _B4( 8,x)  // 0..F
#define BGP_UCI_DDRCHIP_RACKCOLUMN(x)          _B4(12,x)  // 0..F
#define BGP_UCI_DDRCHIP_MIDPLANE(x)            _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_DDRCHIP_NODECARD(x)            _B4(17,x)  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
#define BGP_UCI_DDRCHIP_COMPUTECARD(x)         _B6(23,x)  // 00..01 (00-01 IOCard, 02-03 Reserved, 04-35 ComputeCard)
#define BGP_UCI_DDRCHIP_DDRCHIP(x)             _B5(28,x)  // 00..20


typedef struct BGP_UCI_ENetConnector_t
                {                           // "Rxy-Mm-Nnn-ENe": R<RackRow><RackColumn>-M<Midplane>-N<NodeCard>-EN<EN>
                unsigned Component   :  5;  // when BGP_UCI_Component_ENetConnector
                unsigned RackRow     :  4;  // 0..F
                unsigned RackColumn  :  4;  // 0..F
                unsigned Midplane    :  1;  // 0=Bottom, 1=Top
                unsigned NodeCard    :  4;  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
                unsigned EN          :  1;  // 0..1 (Equal to IOCard number)
                unsigned _zero       : 13;  // zero's
                }
                BGP_UCI_ENetConnector_t;

#define BGP_UCI_ENETCONNECTOR_COMPONENT(x)     _B5( 4,x)  // when BGP_UCI_Component_ENetConnector
#define BGP_UCI_ENETCONNECTOR_RACKROW(x)       _B4( 8,x)  // 0..F
#define BGP_UCI_ENETCONNECTOR_RACKCOLUMN(x)    _B4(12,x)  // 0..F
#define BGP_UCI_ENETCONNECTOR_MIDPLANE(x)      _B1(13,x)  // 0=Bottom, 1=Top
#define BGP_UCI_ENETCONNECTOR_NODECARD(x)      _B4(17,x)  // 00..15: 00=BF, 07=TF, 08=BR, 15=TR)
#define BGP_UCI_ENETCONNECTOR_ENETCONNECTOR(x) _B1(18,x)  // 0..1 (Equal to IOCard number)



typedef union  TBGP_UniversalComponentIdentifier
                {
                uint32_t                   UCI;
                BGP_UCI_Rack_t            Rack;
                BGP_UCI_Midplane_t        Midplane;
                BGP_UCI_BulkPowerSupply_t BulkPowerSupply;
                BGP_UCI_PowerCable_t      PowerCable;
                BGP_UCI_PowerModule_t     PowerModule;
                BGP_UCI_ClockCard_t       ClockCard;
                BGP_UCI_FanAssembly_t     FanAssembly;
                BGP_UCI_Fan_t             Fan;
                BGP_UCI_ServiceCard_t     ServiceCard;
                BGP_UCI_LinkCard_t        LinkCard;
                BGP_UCI_LinkChip_t        LinkChip;
                BGP_UCI_LinkPort_t        LinkPort;
                BGP_UCI_NodeCard_t        NodeCard;
                BGP_UCI_ComputeCard_t     ComputeCard;
                BGP_UCI_IOCard_t          IOCard;
                BGP_UCI_DDRChip_t         DDRChip;
                BGP_UCI_ENetConnector_t   ENetConnector;
                }
                BGP_UniversalComponentIdentifier;



#define BGP_PERSONALITY_VERSION (0x0A)

#define BGP_DEFAULT_FREQ (700)  // Match the current DD1 hardware

#define BGP_PERS_PROCESSCONFIG_DIAGS      (0xFF000000) // Diagnostic Mode: All Cores Enabled and Privileged in Process 0
#define BGP_PERS_PROCESSCONFIG_SMP        (0x0F000000) // All Cores Enabled User-Space in Process 0
#define BGP_PERS_PROCESSCONFIG_VNM        (0x08040201) // 4 Single-Core Processes (a.k.a. Virtual Nodes)
#define BGP_PERS_PROCESSCONFIG_2x2        (0x0C030000) // 2 Processes of 2 Cores each in same DP unit
#define BGP_PERS_PROCESSCONFIG_2x2_CROSS1 (0x09060000) // 2 Processes of 2 Cores in different DP units
#define BGP_PERS_PROCESSCONFIG_2x2_CROSS2 (0x0A050000) // 2 Processes of 2 Cores in different DP units
#define BGP_PERS_PROCESSCONFIG_3PLUS1     (0x0E010000) // 3 Cores in one Processes, 4th Core in Separate Process
#define BGP_PERS_PROCESSCONFIG_DEFAULT    (BGP_PERS_PROCESSCONFIG_DIAGS)


// Personality.Kernel_Config.RASPolicy
#define BGP_PERS_RASPOLICY_VERBOSITY(x)   _B2( 1,x)  // Verbosity as shown below
#define BGP_PERS_RASPOLICY_MINIMAL          BGP_PERS_RASPOLICY_VERBOSITY(0) // Benchmarking Level of Capture and Reporting
#define BGP_PERS_RASPOLICY_NORMAL           BGP_PERS_RASPOLICY_VERBOSITY(1) // Normal Production Level of Capture and Reporting
#define BGP_PERS_RASPOLICY_VERBOSE          BGP_PERS_RASPOLICY_VERBOSITY(2) // Manufacturing Test and Diagnostics
#define BGP_PERS_RASPOLICY_EXTREME          BGP_PERS_RASPOLICY_VERBOSITY(3) // Report Every Event Immediately - Thresholds set to 1
#define BGP_PERS_RASPOLICY_FATALEXIT      _BN( 2)   // Fatal is Fatal, so exit.

#define BGP_PERS_RASPOLICY_DEFAULT        (BGP_PERS_RASPOLICY_VERBOSE | BGP_PERS_RASPOLICY_FATALEXIT)


#define BGP_PERSONALITY_LEN_NFSDIR (32) // 32bytes

#define BGP_PERSONALITY_LEN_SECKEY (32) // 32bytes

// Personality.NodeConfig Driver Enables and Configurations
#define BGP_PERS_ENABLE_Simulation      _BN( 0)  // Running on VHDL Simulation
#define BGP_PERS_ENABLE_LockBox         _BN( 1)
#define BGP_PERS_ENABLE_BIC             _BN( 2)
#define BGP_PERS_ENABLE_DDR             _BN( 3)  // DDR Controllers (not Fusion DDR model)
#define BGP_PERS_ENABLE_LoopBack        _BN( 4)  // LoopBack: Internal TS/TR or SerDes Loopback
#define BGP_PERS_ENABLE_GlobalInts      _BN( 5)
#define BGP_PERS_ENABLE_Collective      _BN( 6)  // Enable Collective Network
#define BGP_PERS_ENABLE_Torus           _BN( 7)
#define BGP_PERS_ENABLE_TorusMeshX      _BN( 8)  // Torus is a Mesh in the X-dimension
#define BGP_PERS_ENABLE_TorusMeshY      _BN( 9)  // Torus is a Mesh in the Y-dimension
#define BGP_PERS_ENABLE_TorusMeshZ      _BN(10)  // Torus is a Mesh in the Z-dimension
#define BGP_PERS_ENABLE_TreeA           _BN(11)  // Enable Collective Network A-link
#define BGP_PERS_ENABLE_TreeB           _BN(12)  // Enable Collective Network B-link
#define BGP_PERS_ENABLE_TreeC           _BN(13)  // Enable Collective Network C-link
#define BGP_PERS_ENABLE_DMA             _BN(14)
#define BGP_PERS_ENABLE_SerDes          _BN(15)
#define BGP_PERS_ENABLE_UPC             _BN(16)
#define BGP_PERS_ENABLE_EnvMon          _BN(17)
#define BGP_PERS_ENABLE_Ethernet        _BN(18)
#define BGP_PERS_ENABLE_JTagLoader      _BN(19)  // Converse with JTag Host to load kernel
#define BGP_PERS_ENABLE_MailBoxReceive  BGP_PERS_ENABLE_JTagLoader
#define BGP_PERS_ENABLE_PowerSave       _BN(20)  // Turn off unused devices (Eth on CN, TS on ION)
#define BGP_PERS_ENABLE_FPU             _BN(21)  // Enable Double-Hummers (not supported in EventSim)
#define BGP_PERS_ENABLE_StandAlone      _BN(22)  // Disable "CIOD" interface, Requires Collective!
#define BGP_PERS_ENABLE_TLBMisses       _BN(23)  // TLB Misses vs Wasting Memory (see bgp_AppSetup.c)
#define BGP_PERS_ENABLE_Mambo           _BN(24)  // Running under Mambo? Used by Linux
#define BGP_PERS_ENABLE_TreeBlast       _BN(25)  // Enable Tree "Blast" mode
#define BGP_PERS_ENABLE_BlindStacks     _BN(26)  // For "XB" Tests, Lock 16K Stacks in Blind Device
#define BGP_PERS_ENABLE_CNK_Malloc      _BN(27)  // Enable Malloc Support in CNK.
#define BGP_PERS_ENABLE_Reproducibility _BN(28)  // Enable Cycle Reproducibility

// Configure L1+L2 into BG/L Mode (s/w managed L1 coherence, write-back)
//  This overrides most L1, L2, and Snoop settings. Carefull!!
#define BGP_PERS_ENABLE_BGLMODE      _BN(31)  // (not yet fully implemented)

// Default Setup for Simulation: Torus Meshes, DMA, SerDes, Ethernet, JTagLoader, PowerSave
#define BGP_PERS_NODECONFIG_DEFAULT (BGP_PERS_ENABLE_Simulation  |\
                                      BGP_PERS_ENABLE_LockBox     |\
                                      BGP_PERS_ENABLE_BIC         |\
                                      BGP_PERS_ENABLE_DDR         |\
                                      BGP_PERS_ENABLE_LoopBack    |\
                                      BGP_PERS_ENABLE_GlobalInts  |\
                                      BGP_PERS_ENABLE_Collective  |\
                                      BGP_PERS_ENABLE_Torus       |\
                                      BGP_PERS_ENABLE_UPC         |\
                                      BGP_PERS_ENABLE_EnvMon      |\
                                      BGP_PERS_ENABLE_FPU         |\
                                      BGP_PERS_ENABLE_StandAlone)

// Default Setup for Hardware:
//     Supports Stand-Alone CNA Applications.
//     Bootloader-Extensions and XB's must turn-off JTagLoader
#define BGP_PERS_NODECONFIG_DEFAULT_FOR_HARDWARE (BGP_PERS_ENABLE_JTagLoader  |\
                                                   BGP_PERS_ENABLE_LockBox     |\
                                                   BGP_PERS_ENABLE_BIC         |\
                                                   BGP_PERS_ENABLE_DDR         |\
                                                   BGP_PERS_ENABLE_GlobalInts  |\
                                                   BGP_PERS_ENABLE_Collective  |\
                                                   BGP_PERS_ENABLE_SerDes      |\
                                                   BGP_PERS_ENABLE_UPC         |\
                                                   BGP_PERS_ENABLE_EnvMon      |\
                                                   BGP_PERS_ENABLE_FPU         |\
                                                   BGP_PERS_ENABLE_StandAlone)

// these fields are defined by the control system depending on compute/io node
//                                                   BGP_PERS_ENABLE_Torus       |
//                                                   BGP_PERS_ENABLE_TorusMeshX  |
//                                                   BGP_PERS_ENABLE_TorusMeshY  |
//                                                   BGP_PERS_ENABLE_TorusMeshZ  |



// Personality.L1Config: Controls and Settings for L1 Cache
#define BGP_PERS_L1CONFIG_L1I          _BN( 0)    // L1 Enabled for Instructions
#define BGP_PERS_L1CONFIG_L1D          _BN( 1)    // L1 Enabled for Data
#define BGP_PERS_L1CONFIG_L1SWOA       _BN( 2)    // L1 Store WithOut Allocate
#define BGP_PERS_L1CONFIG_L1Recovery   _BN( 3)    // L1 Full Recovery Mode
#define BGP_PERS_L1CONFIG_L1WriteThru  _BN( 4)    // L1 Write-Thru (not svc_host changeable (yet?))
#define BGP_PERS_L1CONFIG_DO_L1ITrans  _BN( 5)    // Enable L1 Instructions Transient?
#define BGP_PERS_L1CONFIG_DO_L1DTrans  _BN( 6)    // Enable L1 Data         Transient?
                                                   // unused 9bits: 7..15
#define BGP_PERS_L1CONFIG_L1ITrans(x)  _B8(23,x)  // L1 Transient for Instructions in Groups of 16 Lines
#define BGP_PERS_L1CONFIG_L1DTrans(x)  _B8(31,x)  // L1 Transient for Data         in Groups of 16 Lines

#define BGP_PERS_L1CONFIG_DEFAULT (BGP_PERS_L1CONFIG_L1I         |\
                                    BGP_PERS_L1CONFIG_L1D         |\
                                    BGP_PERS_L1CONFIG_L1SWOA      |\
				    BGP_PERS_L1CONFIG_L1Recovery  |\
                                    BGP_PERS_L1CONFIG_L1WriteThru)

typedef union TBGP_Pers_L1Cfg
               {
               uint32_t l1cfg;
               struct {
                      unsigned l1i         :  1;
                      unsigned l1d         :  1;
                      unsigned l1swoa      :  1;
                      unsigned l1recovery  :  1;
                      unsigned l1writethru :  1;
                      unsigned do_l1itrans :  1;
                      unsigned do_l1dtrans :  1;
                      unsigned l1rsvd      :  9;
                      unsigned l1itrans    :  8;
                      unsigned l1dtrans    :  8;
                      };
               }
               BGP_Pers_L1Cfg;

// Personality.L2Config: Controls and Settings for L2 and Snoop
#define BGP_PERS_L2CONFIG_L2I                _BN( 0)  // L2 Instruction Caching Enabled
#define BGP_PERS_L2CONFIG_L2D                _BN( 1)  // L2 Data        Caching Enabled
#define BGP_PERS_L2CONFIG_L2PF               _BN( 2)  // L2 Automatic Prefetching Enabled
#define BGP_PERS_L2CONFIG_L2PFO              _BN( 3)  // L2 Optimistic Prefetching Enabled
#define BGP_PERS_L2CONFIG_L2PFA              _BN( 4)  // L2 Aggressive Prefetching Enabled (fewer deeper streams)
#define BGP_PERS_L2CONFIG_L2PFS              _BN( 5)  // L2 Aggressive Many-Stream Prefetching Enabled (deeper only when available buffers)
#define BGP_PERS_L2CONFIG_Snoop              _BN( 6)  // Just NULL Snoop Filter
#define BGP_PERS_L2CONFIG_SnoopCache         _BN( 7)  // Snoop Caches
#define BGP_PERS_L2CONFIG_SnoopStream        _BN( 8)  // Snoop Stream Registers (Disable for BG/P Rit 1.0 due to PPC450 errata)
#define BGP_PERS_L2CONFIG_SnoopRange         _BN( 9)  // Snoop Range Filter when possible
#define BGP_PERS_L2CONFIG_BUG824LUMPY        _BN(10)  // BPC_BUGS 824: Fix with Lumpy Performance
#define BGP_PERS_L2CONFIG_BUG824SMOOTH       _BN(11)  // BPC_BUGS 824: Fix with Smooth Performance, but -12% Memory
#define BGP_PERS_L2CONFIG_NONCOHERENT_STACKS _BN(12)  // Special for Snoop diagnostics. See bgp_vmm.c
                                              // additional bits may be used for Snoop setting tweaks

// Default L2 Configuration:
//   L2 Enabled with Multi-Stream Aggressive Prefetching
//   Snoop Enabled with all filters except Range
#define BGP_PERS_L2CONFIG_DEFAULT   (BGP_PERS_L2CONFIG_L2I        |\
                                      BGP_PERS_L2CONFIG_L2D        |\
                                      BGP_PERS_L2CONFIG_L2PF       |\
                                      BGP_PERS_L2CONFIG_L2PFO      |\
                                      BGP_PERS_L2CONFIG_L2PFS      |\
                                      BGP_PERS_L2CONFIG_Snoop      |\
                                      BGP_PERS_L2CONFIG_SnoopCache |\
                                      BGP_PERS_L2CONFIG_SnoopStream|\
                                      BGP_PERS_L2CONFIG_BUG824LUMPY)


// Personality.L3Config: Controls and Settings for L3
//   Note: Most bits match BGP_L3x_CTRL DCRs.
//         See arch/include/bpcore/bgl_l3_dcr.h
#define BGP_PERS_L3CONFIG_L3I        _BN( 0)    // L3 Enabled for Instructions
#define BGP_PERS_L3CONFIG_L3D        _BN( 1)    // L3 Enabled for Data
#define BGP_PERS_L3CONFIG_L3PFI      _BN( 2)    // Inhibit L3 Prefetch from DDR
#define BGP_PERS_L3CONFIG_DO_Scratch _BN( 3)    // Set up Scratch?
#define BGP_PERS_L3CONFIG_DO_PFD0    _BN( 4)    // Adjust PFD0?
#define BGP_PERS_L3CONFIG_DO_PFD1    _BN( 5)    // Adjust PFD1?
#define BGP_PERS_L3CONFIG_DO_PFDMA   _BN( 6)    // Adjust PFDMA?
#define BGP_PERS_L3CONFIG_DO_PFQD    _BN( 7)    // Adjust PFQD?
                                      // 8..15 unused/available
#define BGP_PERS_L3CONFIG_Scratch(x) _B4(19,x)  // Scratch 8ths: 0..8
#define BGP_PERS_L3CONFIG_PFD0(x)    _B3(22,x)  // Prefetch Depth for DP0
#define BGP_PERS_L3CONFIG_PFD1(x)    _B3(25,x)  // Prefetch Depth for DP1
#define BGP_PERS_L3CONFIG_PFDMA(x)   _B3(28,x)  // Prefetch Depth for DMA
#define BGP_PERS_L3CONFIG_PFQD(x)    _B3(31,x)  // Prefetch Queue Depth

// General L3 Configuration
typedef union TBGP_Pers_L3Cfg
               {
               uint32_t l3cfg;
               struct {
                      unsigned l3i        :  1;
                      unsigned l3d        :  1;
                      unsigned l3pfi      :  1;
                      unsigned do_scratch :  1;
                      unsigned do_pfd0    :  1;
                      unsigned do_pfd1    :  1;
                      unsigned do_pfdma   :  1;
                      unsigned do_pfqd    :  1;
                      unsigned rsvd       :  8;
                      unsigned scratch    :  4;
                      unsigned pfd0       :  3;
                      unsigned pfd1       :  3;
                      unsigned pfdma      :  3;
                      unsigned pfqd       :  3;
                      };
               }
               BGP_Pers_L3Cfg;

// Default L3 Configuration:
//   L3 Enabled for Instructions and Data
//   No Prefetch Depth overrides, No Scratch, No Scrambling.
#define BGP_PERS_L3CONFIG_DEFAULT    (BGP_PERS_L3CONFIG_L3I |\
                                       BGP_PERS_L3CONFIG_L3D |\
				       BGP_PERS_L3CONFIG_DO_PFDMA |\
                                       BGP_PERS_L3CONFIG_PFDMA(4))


// L3 Cache and Bank Selection, and prefetching tweaks (Recommended for Power-Users)
#define BGP_PERS_L3SELECT_DO_CacheSel _BN( 0)   // Adjust Cache Select setting?
#define BGP_PERS_L3SELECT_DO_BankSel  _BN( 1)   // Adjust Bank  Select setting?
#define BGP_PERS_L3SELECT_Scramble    _BN( 2)   // L3 Scramble
#define BGP_PERS_L3SELECT_PFby2       _BN( 3)   // Prefetch by 2 if set, else by 1 (default) if clear.
#define BGP_PERS_L3SELECT_CacheSel(x) _B5( 8,x) // PhysAddr Bit for L3 Selection (0..26)
#define BGP_PERS_L3SELECT_BankSel(x)  _B5(13,x) // PhysAddr Bit for L3 Bank Selection (0..26) Must be > CacheSel.

typedef union TBGP_Pers_L3Select
               {
               uint32_t l3select;
               struct {
                      unsigned do_CacheSel :  1;
                      unsigned do_BankSel  :  1;
                      unsigned l3Scramble  :  1;
                      unsigned l3_PF_by2   :  1; // default is PreFetch by 1.
                      unsigned CacheSel    :  5; // Physical Address Bit for L3 Selection (0..26)
                      unsigned BankSel     :  5; // 0..26 Must be strictly greater than CacheSel.
                      unsigned rsvd        : 18;
                      };
               }
               BGP_Pers_L3Select;

// Default L3 Selection Configuration: Disable overrides, but set h/w default values.
#define BGP_PERS_L3SELECT_DEFAULT  (BGP_PERS_L3SELECT_CacheSel(21) |\
                                     BGP_PERS_L3SELECT_BankSel(26))

// Tracing Masks and default trace configuration
//   See also arch/include/cnk/bgp_cnk_Trace.h
#define BGP_TRACE_CONFIG    _BN( 0)   // Display Encoded personality config on startup
#define BGP_TRACE_ENTRY     _BN( 1)   // Function enter and exit
#define BGP_TRACE_INTS      _BN( 2)   // Standard Interrupt Dispatch
#define BGP_TRACE_CINTS     _BN( 3)   // Critical Interrupt Dispatch
#define BGP_TRACE_MCHK      _BN( 4)   // Machine Check Dispatch
#define BGP_TRACE_SYSCALL   _BN( 5)   // System Calls
#define BGP_TRACE_VMM       _BN( 6)   // Virtual Memory Manager
#define BGP_TRACE_DEBUG     _BN( 7)   // Debug Events (app crashes etc)
#define BGP_TRACE_TORUS     _BN( 8)   // Torus Init
#define BGP_TRACE_TREE      _BN( 9)   // Tree  Init
#define BGP_TRACE_GLOBINT   _BN(10)   // Global Interrupts
#define BGP_TRACE_DMA       _BN(11)   // DMA Setup
#define BGP_TRACE_SERDES    _BN(12)   // SerDes Init
#define BGP_TRACE_TESTINT   _BN(13)   // Test Interface, ECID, Config
#define BGP_TRACE_ETHTX     _BN(14)   // Ethernet Transmit
#define BGP_TRACE_ETHRX     _BN(15)   // Ethernet Receive
#define BGP_TRACE_POWER     _BN(16)   // Power Control
#define BGP_TRACE_PROCESS   _BN(17)   // Process/Thread Mapping
#define BGP_TRACE_EXIT_SUM  _BN(18)   // Report Per-Core Interrupt and Error Summary on exit()
#define BGP_TRACE_SCHED     _BN(19)   // Report Scheduler Information
#define BGP_TRACE_RAS       _BN(20)   // Report RAS Events (in addition to sending to Host)
#define BGP_TRACE_ECID      _BN(21)   // Report UCI and ECID on boot
#define BGP_TRACE_FUTEX     _BN(22)   // Trace Futex operations
#define BGP_TRACE_MemAlloc  _BN(23)   // Trace MMAP and Shared Memory operations
#define BGP_TRACE_WARNINGS  _BN(30)   // Trace Warnings
#define BGP_TRACE_VERBOSE   _BN(31)   // Verbose Tracing Modifier

// Enable tracking of Regression Suite coverage and report UCI+ECID on boot
#define BGP_PERS_TRACE_DEFAULT (BGP_TRACE_CONFIG | BGP_TRACE_ECID)


typedef struct BGP_Personality_Kernel_t
                {
                uint32_t  UniversalComponentIdentifier; // see include/common/bgp_ras.h

                uint32_t  FreqMHz;                      // Clock_X1 Frequency in MegaHertz (eg 1000)

                uint32_t  RASPolicy;                    // Verbosity level, and other RAS Reporting Controls

                // Process Config:
                //   Each byte represents a process (1 to 4 processes supported)
                //     No core can be assigned to more than 1 process.
                //     Cores assigned to no process are disabled.
                //     Cores with in a process share the same address space.
                //     Separate processes have distinct address spaces.
                //   Within each process (0 to 4 cores assigned to a process):
                //     Lower nibble is bitmask of which core belongs to that process.
                //     Upper nibble is bitmask whether that thread is privileged or user.
                //     Processes with zero cores do not exist.
                //   E.g., for Diagnostics, we use 0xFF000000, which means
                //     that all 4 cores run privileged in process 0.
                uint32_t  ProcessConfig;

                uint32_t  TraceConfig;        // Kernel Tracing Enables
                uint32_t  NodeConfig;         // Kernel Driver Enables
                uint32_t  L1Config;           // L1 Config and setup controls
                uint32_t  L2Config;           // L2 and Snoop Config and setup controls
                uint32_t  L3Config;           // L3 Config and setup controls
                uint32_t  L3Select;           // L3 Cache and Bank Selection controls

                uint32_t  SharedMemMB;        // Memory to Reserve for Sharing among Processes

                uint32_t  ClockStop0;        // Upper 11Bits of ClockStop, enabled if Non-zero
                uint32_t  ClockStop1;        // Lower 32Bits of ClockStop, enabled if Non-zero
                }
                BGP_Personality_Kernel_t;


// Defaults for DDR Config
#define BGP_PERS_DDR_PBX0_DEFAULT             (0x411D1512)    // PBX DCRs setting (in IBM bit numbering)
#define BGP_PERS_DDR_PBX1_DEFAULT             (0x40000000)    // PBX DCRs setting (in IBM bit numbering)
#define BGP_PERS_DDR_MemConfig0_DEFAULT       (0x81fc4080)    // MemConfig
#define BGP_PERS_DDR_MemConfig1_DEFAULT       (0x0C0ff800)    // MemConfig
#define BGP_PERS_DDR_ParmCtl0_DEFAULT         (0x3216c008)    // Parm Control
#define BGP_PERS_DDR_ParmCtl1_DEFAULT         (0x4168c323)    // Parm Control
#define BGP_PERS_DDR_MiscCtl0_DEFAULT         (0)    // Misc. Control
#define BGP_PERS_DDR_MiscCtl1_DEFAULT         (0)    // Misc. Control
#define BGP_PERS_DDR_CmdBufMode0_DEFAULT      (0x00400fdf)    // Command Buffer Mode
#define BGP_PERS_DDR_CmdBufMode1_DEFAULT      (0xffc80600)    // Command Buffer Mode
#define BGP_PERS_DDR_RefrInterval0_DEFAULT    (0xD1000002)    // Refresh Interval
#define BGP_PERS_DDR_RefrInterval1_DEFAULT    (0x04000000)    // Refresh Interval
#define BGP_PERS_DDR_ODTCtl0_DEFAULT          (0)    // ODT Control
#define BGP_PERS_DDR_ODTCtl1_DEFAULT          (0)    // ODT Control
#define BGP_PERS_DDR_DataStrobeCalib0_DEFAULT (0x08028a64)    // Data Strobe Calibration
#define BGP_PERS_DDR_DataStrobeCalib1_DEFAULT (0xa514c805)    // Data Strobe Calibration
#define BGP_PERS_DDR_DQSCtl_DEFAULT           (0x00000168)    // DQS Control
#define BGP_PERS_DDR_Throttle_DEFAULT         (0)    // DDR Throttle
//1#define BGP_PERS_DDR_DDRSizeMB_DEFAULT        (4096) // Total DDR size in MegaBytes (512MB - 16384MB).
#define BGP_PERS_DDR_DDRSizeMB_DEFAULT        (1024) // Total DDR size in MegaBytes (512MB - 16384MB).
//1#define BGP_PERS_DDR_Chips_DEFAULT            (0x0B) // Type of DDR chips
#define BGP_PERS_DDR_Chips_DEFAULT            (0x09) // Type of DDR chips
#define BGP_PERS_DDR_CAS_DEFAULT              (4)    // CAS Latency (3, 4, or 5)


#define BGP_PERS_DDRFLAGS_ENABLE_Scrub        _BN(0) // Enable DDR Slow Scrub when 1

// DDRFLAGS default: Enable Slow Scrub.
#define BGP_PERS_DDRFLAGS_DEFAULT             (BGP_PERS_DDRFLAGS_ENABLE_Scrub)

#define BGP_PERS_SRBS0_DEFAULT                (0)
#define BGP_PERS_SRBS1_DEFAULT                (0)

typedef struct BGP_Personality_DDR_t
                {
                uint32_t  DDRFlags;         // Misc. Flags and Settings
                uint32_t  SRBS0;            // Controller 0 SRBS/CK Settings
                uint32_t  SRBS1;            // Controller 1 SRBS/CK Settings
                uint32_t  PBX0;             // PBX DCRs setting (in IBM bit numbering)
                uint32_t  PBX1;             // PBX DCRs setting (in IBM bit numbering)
                uint32_t  MemConfig0;       // MemConfig
                uint32_t  MemConfig1;       // MemConfig
                uint32_t  ParmCtl0;         // Parm Control
                uint32_t  ParmCtl1;         // Parm Control
                uint32_t  MiscCtl0;         // Misc. Control
                uint32_t  MiscCtl1;         // Misc. Control
                uint32_t  CmdBufMode0;      // Command Buffer Mode
                uint32_t  CmdBufMode1;      // Command Buffer Mode
                uint32_t  RefrInterval0;    // Refresh Interval
                uint32_t  RefrInterval1;    // Refresh Interval
                uint32_t  ODTCtl0;          // ODT Control
                uint32_t  ODTCtl1;          // ODT Control
                uint32_t  DataStrobeCalib0; // Data Strobe Calibration
                uint32_t  DataStrobeCalib1; // Data Strobe Calibration
                uint32_t  DQSCtl;           // DQS Control
                uint32_t  Throttle;         // DDR Throttle
                uint16_t  DDRSizeMB;        // Total DDR size in MegaBytes (512MB - 16384MB).
                uint8_t   Chips;            // Type of DDR chips
                uint8_t   CAS;              // CAS Latency (3, 4, or 5)
                }
                BGP_Personality_DDR_t;


typedef struct BGP_Personality_Networks_t
                {
                uint32_t  BlockID;         // a.k.a. PartitionID

                uint8_t   Xnodes,
                          Ynodes,
                          Znodes,
                          Xcoord,
                          Ycoord,
                          Zcoord;

                // PSet Support
                uint16_t  PSetNum;
                uint32_t  PSetSize;
                uint32_t  RankInPSet;

                uint32_t  IOnodes;
                uint32_t  Rank;               // Rank in Block (or Partition)
                uint32_t  IOnodeRank;         // Rank (and therefore P2P Addr) of my I/O Node
                uint16_t  TreeRoutes[ 16 ];
                }
                BGP_Personality_Networks_t;


typedef struct BGP_IP_Addr_t
                {
                // IPv6 Addresses are 16 bytes, where the
                //  lower 4 (indices 12-15) can be used for IPv4 address.
                uint8_t octet[ 16 ];
                }
                BGP_IP_Addr_t;


typedef struct BGP_Personality_Ethernet_t
                {
                uint16_t       MTU;            // Initial emac MTU size
                uint8_t        EmacID[6];      // MAC address for emac
                BGP_IP_Addr_t IPAddress;      // IPv6/IPv4 address of this node
                BGP_IP_Addr_t IPNetmask;      // IPv6/IPv4 netmask
                BGP_IP_Addr_t IPBroadcast;    // IPv6/IPv4 broadcast address
                BGP_IP_Addr_t IPGateway;      // IPv6/IPv4 initial gateway (zero if none)
                BGP_IP_Addr_t NFSServer;      // IPv6/IPv4 NFS system software server address
                BGP_IP_Addr_t serviceNode;    // IPv6/IPv4 address of service node

                // NFS mount info
                char      NFSExportDir[BGP_PERSONALITY_LEN_NFSDIR];
                char      NFSMountDir[BGP_PERSONALITY_LEN_NFSDIR];

                // Security Key for Service Node authentication
                uint8_t   SecurityKey[BGP_PERSONALITY_LEN_SECKEY ];
                }
                BGP_Personality_Ethernet_t;


typedef struct TBGP_Personality_t
                {
                uint16_t  CRC;
                uint8_t   Version;
                uint8_t   PersonalitySizeWords;

                BGP_Personality_Kernel_t   Kernel_Config;

                BGP_Personality_DDR_t      DDR_Config;

                BGP_Personality_Networks_t Network_Config;

                BGP_Personality_Ethernet_t Ethernet_Config;

                uint32_t padd[2]; // Pad size to multiple of 16 bytes (== width of DEVBUS_DATA tdr)
                                  // to simplify jtag operations. See issue #140.
                }
                BGP_Personality_t;


// Define a static initializer for default configuration. (DEFAULTS FOR SIMULATION)
//  This is used in bootloader:bgp_Personality.c and svc_host:svc_main.c
#define BGP_PERSONALITY_DEFAULT_STATIC_INITIALIZER { \
           0,                                              /* CRC                  */ \
           BGP_PERSONALITY_VERSION,                       /* Version              */ \
           (sizeof(BGP_Personality_t)/sizeof(uint32_t)),  /* PersonalitySizeWords */ \
           {  /* BGP_Personality_Kernel_t: */ \
              0,                                   /* MachineLocation        */ \
              BGP_DEFAULT_FREQ,                   /* FreqMHz       */ \
              BGP_PERS_RASPOLICY_DEFAULT,         /* RASPolicy     */ \
              BGP_PERS_PROCESSCONFIG_DEFAULT,     /* ProcessConfig */ \
              BGP_PERS_TRACE_DEFAULT,             /* TraceConfig   */ \
              BGP_PERS_NODECONFIG_DEFAULT,        /* NodeConfig    */ \
              BGP_PERS_L1CONFIG_DEFAULT,          /* L1Config      */ \
              BGP_PERS_L2CONFIG_DEFAULT,          /* L2Config      */ \
              BGP_PERS_L3CONFIG_DEFAULT,          /* L3Config      */ \
              BGP_PERS_L3SELECT_DEFAULT,          /* L3Select      */ \
              0,                                   /* SharedMemMB   */ \
              0,                                   /* ClockStop0    */ \
              0                                    /* ClockStop1    */ \
              }, \
           {  /* BGP_Personality_DDR_t: */ \
              BGP_PERS_DDRFLAGS_DEFAULT,             /* DDRFlags         */ \
              BGP_PERS_SRBS0_DEFAULT,                /* SRBS0            */ \
              BGP_PERS_SRBS1_DEFAULT,                /* SRBS1            */ \
              BGP_PERS_DDR_PBX0_DEFAULT,             /* PBX0             */ \
              BGP_PERS_DDR_PBX1_DEFAULT,             /* PBX1             */ \
              BGP_PERS_DDR_MemConfig0_DEFAULT,       /* MemConfig0       */ \
              BGP_PERS_DDR_MemConfig1_DEFAULT,       /* MemConfig1       */ \
              BGP_PERS_DDR_ParmCtl0_DEFAULT,         /* ParmCtl0         */ \
              BGP_PERS_DDR_ParmCtl1_DEFAULT,         /* ParmCtl1         */ \
              BGP_PERS_DDR_MiscCtl0_DEFAULT,         /* MiscCtl0         */ \
              BGP_PERS_DDR_MiscCtl1_DEFAULT,         /* MiscCtl1         */ \
              BGP_PERS_DDR_CmdBufMode0_DEFAULT,      /* CmdBufMode0      */ \
              BGP_PERS_DDR_CmdBufMode1_DEFAULT,      /* CmdBufMode1      */ \
              BGP_PERS_DDR_RefrInterval0_DEFAULT,    /* RefrInterval0    */ \
              BGP_PERS_DDR_RefrInterval1_DEFAULT,    /* RefrInterval1    */ \
              BGP_PERS_DDR_ODTCtl0_DEFAULT,          /* ODTCtl0          */ \
              BGP_PERS_DDR_ODTCtl1_DEFAULT,          /* ODTCtl1          */ \
              BGP_PERS_DDR_DataStrobeCalib0_DEFAULT, /* DataStrobeCalib0 */ \
              BGP_PERS_DDR_DataStrobeCalib1_DEFAULT, /* DataStrobeCalib1 */ \
              BGP_PERS_DDR_DQSCtl_DEFAULT,           /* DQSCtl           */ \
              BGP_PERS_DDR_Throttle_DEFAULT,         /* Throttle         */ \
              BGP_PERS_DDR_DDRSizeMB_DEFAULT,        /* DDRSizeMB        */ \
              BGP_PERS_DDR_Chips_DEFAULT,            /* Chips            */ \
              BGP_PERS_DDR_CAS_DEFAULT               /* CAS              */ \
              }, \
           {  /* BGP_Personality_Networks_t: */ \
              0,                                   /* BlockID                */ \
              1, 1, 1,                             /* Xnodes, Ynodes, Znodes */ \
              0, 0, 0,                             /* Xcoord, Ycoord, Zcoord */ \
              0,                                   /* PSetNum                */ \
              0,                                   /* PSetSize               */ \
              0,                                   /* RankInPSet             */ \
              0,                                   /* IOnodes                */ \
              0,                                   /* Rank                   */ \
              0,                                   /* IOnodeRank             */ \
              { 0, }                               /* TreeRoutes[ 16 ]       */ \
              }, \
           {  /* BGP_Personality_Ethernet_t: */ \
              1536,                                /* mtu              */ \
              { 0, },                              /* EmacID[6]        */ \
              { { 0x00,0x00,0x00,0x00,             /* IPAddress        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPNetmask        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0xFF,0xFF,0xFF,0x70  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPBroadcast      */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPGateway        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* NFSServer        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* serviceNode      */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              "",                                  /* NFSExportDir[32] */ \
              "",                                  /* NFSMountDir[32]  */ \
              { 0x00, }                            /* SecurityKey[32]  */ \
              }, \
           { 0, }                                  /* padd[2]          */ \
           }


// Define a static initializer for default configuration. (DEFAULTS FOR HARDWARE)
//  This is used in bootloader:bgp_Personality.c and svc_host:svc_main.c
#define BGP_PERSONALITY_DEFAULT_STATIC_INITIALIZER_FOR_HARDWARE { \
           0,                                             /* CRC                  */ \
           BGP_PERSONALITY_VERSION,                      /* Version              */ \
           (sizeof(BGP_Personality_t)/sizeof(uint32_t)), /* PersonalitySizeWords */ \
           {  /* BGP_Personality_Kernel_t: */ \
              0,                                          /* MachineLocation      */ \
              BGP_DEFAULT_FREQ,                          /* FreqMHz       */ \
              BGP_PERS_RASPOLICY_DEFAULT,                /* RASPolicy     */ \
              BGP_PERS_PROCESSCONFIG_SMP,                /* ProcessConfig */ \
              BGP_PERS_TRACE_DEFAULT,                    /* TraceConfig   */ \
              BGP_PERS_NODECONFIG_DEFAULT_FOR_HARDWARE,  /* NodeConfig    */ \
              BGP_PERS_L1CONFIG_DEFAULT,                 /* L1Config      */ \
              BGP_PERS_L2CONFIG_DEFAULT,                 /* L2Config      */ \
              BGP_PERS_L3CONFIG_DEFAULT,                 /* L3Config      */ \
              BGP_PERS_L3SELECT_DEFAULT,                 /* L3Select      */ \
              0,                                          /* SharedMemMB   */ \
              0,                                          /* ClockStop0    */ \
              0                                           /* ClockStop1    */ \
              }, \
           {  /* BGP_Personality_DDR_t: */ \
              BGP_PERS_DDRFLAGS_DEFAULT,             /* DDRFlags         */ \
              BGP_PERS_SRBS0_DEFAULT,                /* SRBS0            */ \
              BGP_PERS_SRBS1_DEFAULT,                /* SRBS1            */ \
              BGP_PERS_DDR_PBX0_DEFAULT,             /* PBX0             */ \
              BGP_PERS_DDR_PBX1_DEFAULT,             /* PBX1             */ \
              BGP_PERS_DDR_MemConfig0_DEFAULT,       /* MemConfig0       */ \
              BGP_PERS_DDR_MemConfig1_DEFAULT,       /* MemConfig1       */ \
              BGP_PERS_DDR_ParmCtl0_DEFAULT,         /* ParmCtl0         */ \
              BGP_PERS_DDR_ParmCtl1_DEFAULT,         /* ParmCtl1         */ \
              BGP_PERS_DDR_MiscCtl0_DEFAULT,         /* MiscCtl0         */ \
              BGP_PERS_DDR_MiscCtl1_DEFAULT,         /* MiscCtl1         */ \
              BGP_PERS_DDR_CmdBufMode0_DEFAULT,      /* CmdBufMode0      */ \
              BGP_PERS_DDR_CmdBufMode1_DEFAULT,      /* CmdBufMode1      */ \
              BGP_PERS_DDR_RefrInterval0_DEFAULT,    /* RefrInterval0    */ \
              BGP_PERS_DDR_RefrInterval1_DEFAULT,    /* RefrInterval1    */ \
              BGP_PERS_DDR_ODTCtl0_DEFAULT,          /* ODTCtl0          */ \
              BGP_PERS_DDR_ODTCtl1_DEFAULT,          /* ODTCtl1          */ \
              BGP_PERS_DDR_DataStrobeCalib0_DEFAULT, /* DataStrobeCalib0 */ \
              BGP_PERS_DDR_DataStrobeCalib1_DEFAULT, /* DataStrobeCalib1 */ \
              BGP_PERS_DDR_DQSCtl_DEFAULT,           /* DQSCtl           */ \
              BGP_PERS_DDR_Throttle_DEFAULT,         /* Throttle         */ \
              BGP_PERS_DDR_DDRSizeMB_DEFAULT,        /* DDRSizeMB        */ \
              BGP_PERS_DDR_Chips_DEFAULT,            /* Chips            */ \
              BGP_PERS_DDR_CAS_DEFAULT               /* CAS              */ \
              }, \
           {  /* BGP_Personality_Networks_t: */ \
              0,                                   /* BlockID                */ \
              1, 1, 1,                             /* Xnodes, Ynodes, Znodes */ \
              0, 0, 0,                             /* Xcoord, Ycoord, Zcoord */ \
              0,                                   /* PSetNum                */ \
              0,                                   /* PSetSize               */ \
              0,                                   /* RankInPSet             */ \
              0,                                   /* IOnodes                */ \
              0,                                   /* Rank                   */ \
              0,                                   /* IOnodeRank             */ \
              { 0, }                               /* TreeRoutes[ 16 ]       */ \
              }, \
           {  /* BGP_Personality_Ethernet_t: */ \
              1536,                                /* mtu              */ \
              { 0, },                              /* EmacID[6]        */ \
              { { 0x00,0x00,0x00,0x00,             /* IPAddress        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPNetmask        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0xFF,0xFF,0xFF,0x70  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPBroadcast      */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* IPGateway        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* NFSServer        */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              { { 0x00,0x00,0x00,0x00,             /* serviceNode      */ \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00, \
                  0x00,0x00,0x00,0x00  \
                  } }, \
              "",                                  /* NFSExportDir[32] */ \
              "",                                  /* NFSMountDir[32]  */ \
              { 0x00, }                            /* SecurityKey[32]  */ \
              }, \
           { 0, }                                  /* padd[2]          */ \
           }




#endif // Add nothing below this line.
