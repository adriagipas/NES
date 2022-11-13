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
 *  rom.c - Implementació de funcions relacionades amb les ROMs.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "NES.h"




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static NES_Mapper
ines2nes_mapper (
        	 const NESu8 mapper
        	 )
{

  switch ( mapper )
    {
    case 0: return NES_NROM;
    case 1: return NES_MMC1;
    case 2: return NES_UNROM;
    case 3: return NES_CNROM;
    case 4: return NES_MMC3;
    case 7: return NES_AOROM;
    case 9: return NES_MMC2;
    default: return NES_UNKMAPPER;
    }
  
} /* end ines2nes_mapper */


/* No llig el trainer ni les pàgines, però si reserva memòria. */
static int
read_header_ines (
        	  const NESu8 *bytes,
        	  NES_Rom     *rom
        	  )
{

  NESu8 mapper;

  
  /* Capçalera. */
  if ( bytes[0] != 'N' || bytes[1] != 'E' ||
       bytes[2] != 'S' || bytes[3] != 0x1A )
    return -1;
  rom->nprg= bytes[4];
  rom->nchr= bytes[5];
  rom->mirroring= (bytes[6] & 0x8) ? NES_FOURSCREEN :
    ((bytes[6] & 0x1) ? NES_VERTICAL : NES_HORIZONTAL);
  rom->sram= (bytes[6] & 0x2) ? NES_TRUE : NES_FALSE;
  if ( bytes[6]&0x4 )
    {
      NES_rom_alloc_trainer ( *rom );
      if ( rom->trainer == NULL ) return -1;
    }
  else rom->trainer= NULL;
  mapper= (NESu8 ) ((bytes[7]&0xF0) | (bytes[6]>>4));
  rom->mapper= ines2nes_mapper ( mapper );
  rom->tvmode= (bytes[9] & 0x1) ? NES_PAL : NES_NTSC;

  /* Memòria pàgines. */
  rom->prgs= NES_PRG_NULL;
  rom->chrs= NES_CHR_NULL;
  if ( rom->nprg > 0 )
    {
      NES_rom_alloc_prgs ( *rom );
      if ( rom->prgs == NULL ) return -1;
    }
  if ( rom->nchr > 0 )
    {
      NES_rom_alloc_chrs ( *rom );
      if ( rom->chrs == NULL ) return -1;
    }
  
  return 0;
  
} /* end read_header_ines */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

int
NES_rom_load_from_ines (
        		FILE    *f,
        		NES_Rom *rom
        		)
{

  NESu8 aux[16];
  size_t nbytes;
  
  
  /* Prepara. */
  rom->prgs= NULL;
  rom->chrs= NULL;
  rom->trainer= NULL;
  
  /* Llig capçalera. */
  if ( fread ( aux, 1, 16, f ) != 16 )
    goto error;
  if ( read_header_ines ( aux, rom ) != 0 )
    goto error;
  if ( rom->trainer != NULL &&
       fread ( rom->trainer, 1, 512, f ) != 512 )
    goto error;
  
  /* Llig pàgines. */
  if ( rom->prgs != NES_PRG_NULL )
    {
      nbytes= rom->nprg*NES_PRG_SIZE;
      if ( fread ( rom->prgs, 1, nbytes, f ) != nbytes )
        goto error;
    }
  if ( rom->chrs != NES_CHR_NULL )
    {
      nbytes= rom->nchr*NES_CHR_SIZE;
      if ( fread ( rom->chrs, 1, nbytes, f ) != nbytes )
        goto error;
    }
  
  return 0;
  
 error:
  NES_rom_free ( *rom );
  return -1;
  
} /* end NES_rom_load_from_ines */


int
NES_rom_load_from_ines_mem (
        		    const char *bytes,
        		    size_t      nbytes,
        		    NES_Rom    *rom
        		    )
{

  size_t aux;

  
  /* Prepara. */
  rom->prgs= NULL;
  rom->chrs= NULL;
  rom->trainer= NULL;
  
  /* Capçalera. */
  if ( nbytes < 16 ) goto error;
  if ( read_header_ines ( (const NESu8 *) bytes, rom ) != 0 )
    goto error;
  bytes+= 16; nbytes-= 16;
  if ( rom->trainer != NULL )
    {
      if ( nbytes < 512 ) goto error;
      memcpy ( rom->trainer, bytes, 512 );
      bytes+= 512; nbytes-= 512;
    }
  
  /* Llig pàgines. */
  if ( rom->prgs != NES_PRG_NULL )
    {
      aux= rom->nprg*NES_PRG_SIZE;
      if ( nbytes < aux ) goto error;
      memcpy ( rom->prgs, bytes, aux );
      bytes+= aux; nbytes-= aux;
    }
  if ( rom->chrs != NES_CHR_NULL )
    {
      aux= rom->nchr*NES_CHR_SIZE;
      if ( nbytes < aux ) goto error;
      memcpy ( rom->chrs, bytes, aux );
    }
  
  return 0;
  
 error:
  NES_rom_free ( *rom );
  return -1;
  
} /* end NES_rom_load_from_ines_mem */
