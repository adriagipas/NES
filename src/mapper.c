/*
 * Copyright 2009-2022 Adrià Giménez Pastor.
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
 *  mapper.c - Implementació del mapejador de la ROM en memòria.
 *
 */
/*
 *  NOTA: Per a la implementació m'he basat en mappers.nfo, però sobre
 *  tot en http://wiki.nesdev.com/w/index.php/Mapper
 */


#include <stddef.h>
#include <stdlib.h>

#include "NES.h"
#include "mappers/aorom.h"
#include "mappers/cnrom.h"
#include "mappers/mmc1.h"
#include "mappers/mmc2.h"
#include "mappers/mmc3.h"
#include "mappers/nrom.h"
#include "mappers/unrom.h"




/****************/
/* ESTAT PÚBLIC */
/****************/

void          (*NES_mapper_init_state)           (void);
NESu8         (*NES_mapper_read)                 (const NESu16);
void          (*NES_mapper_reset)                (void);
void          (*NES_mapper_write)                (const NESu16,const NESu8);
NESu8         (*NES_mapper_vram_read)            (const NESu16);
void          (*NES_mapper_vram_write)           (const NESu16,const NESu8);
void          (*NES_mapper_get_rom_mapper_state) (NES_RomMapperState*);
void          (*NES_mapper_set_mode_trace)       (const NES_Bool);
int           (*NES_mapper_save_state)           (FILE *);
int           (*NES_mapper_load_state)           (FILE *);




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NES_Error
NES_mapper_init (
        	 const NES_Rom     *rom,
        	 NES_Warning       *warning,
        	 NES_MapperChanged *mapper_changed,
        	 void              *udata
        	 )
{
  
  int ret;
  
  
  switch ( rom->mapper )
    {
    case NES_AOROM:
      ret= NES_mapper_aorom_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_CNROM:
      ret= NES_mapper_cnrom_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_MMC1:
      ret= NES_mapper_mmc1_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_MMC2:
      ret= NES_mapper_mmc2_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_MMC3:
      ret= NES_mapper_mmc3_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_NROM:
      ret= NES_mapper_nrom_init ( rom, warning, mapper_changed, udata );
      break;
    case NES_UNROM:
      ret= NES_mapper_unrom_init ( rom, warning, mapper_changed, udata );
      break;
    default: return NES_EUNKMAPPER;
    }
  if ( !ret ) NES_mapper_reset ();
  
  return ret;
  
} /* end NES_mapper_init */
