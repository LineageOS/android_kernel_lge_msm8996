/*
 * Copyright(c) 2016, LG Electronics. All rights reserved.
 *
 * analogix register access by i2c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ANX_I2C_INTF_H
#define __ANX_I2C_INTF_H

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
        ((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
                                      ((_x) & 0x04 ? 2 : 3)) :\
                       ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
                                      ((_x) & 0x40 ? 6 : 7)))
#undef  FFS
#define FFS(_x) \
        ((_x) ? __CONST_FFS(_x) : 0)

#undef  BITS
#define BITS(_end, _start) \
        ((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_byte, _mask, _shift) \
        (((_byte) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_byte, _bit) \
        __BITS_GET(_byte, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_byte, _mask, _shift, _val) \
        (((_byte) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_byte, _bit, _val) \
        __BITS_SET(_byte, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_byte, _bit) \
        (((_byte) & (_bit)) == (_bit)

s32 OhioReadReg(unsigned short addr, u8 reg);
s32 OhioReadWordReg(unsigned short addr, u8 reg);
s32 OhioReadBlockReg(unsigned short addr,
				u8 reg, u8 len, u8 *val);
s32 OhioWriteReg(unsigned short addr, u8 reg, u8 val);
s32 OhioMaskWriteReg(unsigned short addr, u8 reg, u8 mask, u8 val);
s32 OhioWriteWordReg(unsigned short addr, u8 reg, u16 val);
s32 OhioMaskWriteWordReg(unsigned short addr, u8 reg, u16 mask, u16 val);
s32 OhioWriteBlockReg(unsigned short addr, u8 reg, u8 len, u8 *val);

#endif /* __ANX_I2C_INTF_H */
