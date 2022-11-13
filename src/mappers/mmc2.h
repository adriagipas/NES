/*
 * Copyright 2017-2022 Adrià Giménez Pastor.
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
 *  mmc2.h - Mapper MMC2.
 *
 */

#ifndef __MAPPERS_MMC2_H__
#define __MAPPERS_MMC2_H__

#include "../NES.h"

typedef struct
{
  NES_Bool latch0_is_FD;
  NES_Bool latch1_is_FD;
} NES_mmc2_state_t;

NES_Error
NES_mapper_mmc2_init (
        	      const NES_Rom     *rom,
        	      NES_Warning       *warning,
        	      NES_MapperChanged *mapper_changed,
        	      void              *udata
        	      );

/* Açò és necessari per el calcul de la col·lissió del sprite 0. */
void
NES_mapper_mmc2_save_state (
        		    NES_mmc2_state_t *state
        		    );

void
NES_mapper_mmc2_load_state (
        		    const NES_mmc2_state_t *state
        		    );

#endif /* __MAPPERS_MMC2_H__ */
