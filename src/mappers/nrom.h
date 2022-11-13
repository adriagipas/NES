/*
 * Copyright 2015-2022 Adrià Giménez Pastor.
 *
 * This file is part of adriagipas/NES.
 *
 * adriagipas/NES is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * adriagipas/NES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with adriagipas/NES.  If not, see <https://www.gnu.org/licenses/>.
 */
/*
 *  nrom.h - Mapper NROM.
 *
 */

#ifndef __MAPPERS_NROM_H__
#define __MAPPERS_NROM_H__

#include "../NES.h"

NES_Error
NES_mapper_nrom_init (
        	      const NES_Rom     *rom,
        	      NES_Warning       *warning,
        	      NES_MapperChanged *mapper_changed,
        	      void              *udata
        	      );

#endif /* __MAPPERS_NROM_H__ */
