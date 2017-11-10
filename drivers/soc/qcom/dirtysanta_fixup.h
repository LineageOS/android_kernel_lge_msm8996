/* Copyright (c) 2017, Elliott Mitchell. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * version 3, or any later versions as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DIRTYSANTA_FIXUP_H_
#define _DIRTYSANTA_FIXUP_H_
#ifdef CONFIG_DIRTYSANTA_FIXUP

int dirtysanta_attach(struct device *dev);

#define DIRTYSANTA_ATTACH(dev) dirtysanta_attach(dev)

#else

#define DIRTYSANTA_ATTACH(dev) (1)

#endif
#endif
