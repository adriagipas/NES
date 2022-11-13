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
 *  nrom.c - Implementació de 'nrom.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "nrom.h"




/*********/
/* ESTAT */
/*********/

/* Rom. */
static const NES_Rom *_rom;

/* Callbacks. */
static void *_udata;
static NES_Warning *_warning;

/* VRAM ptables. */
static NESu8 _vram_pt[0x2000];

/* VRAM ntables. */
static NESu8 _vram_nt[0x800];

/* Mapping nametables. */
static NESu8 *_nt[4];




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define NROM_READ128(ADDR)        			\
  ((const NESu8 *) _rom->prgs)[(ADDR)&0x3FFF]


#define NROM_READ256(ADDR)        		\
  ((const NESu8 *) _rom->prgs)[(ADDR)]


#define BUS_CONFLICT(ADDR)        				\
  _warning ( _udata, "Bus conflict at $%04x", (ADDR)+0x8000 )




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static NESu8
nrom128_read (
              NESu16 addr
              )
{
  return NROM_READ128 ( addr );
} /* end nrom256_read */


static NESu8
nrom256_read (
              NESu16 addr
              )
{
  return NROM_READ256 ( addr );
} /* end nrom128_read */


static void
nrom128_write (
               NESu16 addr,
               NESu8  data
               )
{
  
  NESu8 byte;
  
  
  byte= NROM_READ128 ( addr );
  if ( byte != data ) BUS_CONFLICT ( addr );
  
} /* end nrom128_write */


static void
nrom256_write (
               NESu16 addr,
               NESu8  data
               )
{
  
  NESu8 byte;
  
  
  byte= NROM_READ256 ( addr );
  if ( byte != data ) BUS_CONFLICT ( addr );
  
} /* end nrom256_write */


static NESu8
vram_read (
           const NESu16 addr
           )
{

  if ( addr < 0x2000 )
    return _rom->nchr ? _rom->chrs[0][addr] : _vram_pt[addr];
  
  else /* < 0x3000 */
    return _nt[(addr>>10)&0x3][addr&0x3FF];
  
} /* end vram_read */


static void
vram_write (
            const NESu16 addr,
            const NESu8  data
            )
{

  if ( addr < 0x2000 )
    {
      if ( _rom->nchr == 0 ) _vram_pt[addr]= data;
    }

  else /* < 0x3000 */
    _nt[(addr>>10)&0x3][addr&0x3FF]= data;
  
} /* end vram_write */


static void
reset (void)
{
} /* end reset */


static void
set_mode_trace (
                const NES_Bool val
                )
{
} /* end set_mode_trace */


static void
get_rom_mapper_state (
                      NES_RomMapperState *state
                      )
{

  if ( _rom->nprg == 1 ) /* 128Mb */
    {
      state->p0= 0;
      state->p1= 1;
      state->p2= 0;
      state->p3= 1;
    }
  else /* 256Mb */
    {
      state->p0= 0;
      state->p1= 1;
      state->p2= 2;
      state->p3= 3;
    }
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{
  
  /* Vram. */
  memset ( _vram_pt, 0, sizeof(_vram_pt) );
  memset ( _vram_nt, 0, sizeof(_vram_nt) );
  if ( _rom->mirroring == NES_HORIZONTAL )
    {
      _nt[0]= _nt[1]= &(_vram_nt[0]);
      _nt[2]= _nt[3]= &(_vram_nt[0x400]);
    }
  else /* NES_VERTICAL */
    {
      _nt[0]= _nt[2]= &(_vram_nt[0]);
      _nt[1]= _nt[3]= &(_vram_nt[0x400]);
    }
  
} /* end init_state */


static int
save_state (
            FILE *f
            )
{

  ptrdiff_t ptr;
  int i;
  
  
  /* Info Rom rellevant. */
  SAVE ( _rom->nprg );
  SAVE ( _rom->nchr );
  SAVE ( _rom->mapper );
  SAVE ( _rom->mirroring );

  /* Estat. */
  SAVE ( _vram_pt );
  SAVE ( _vram_nt );
  for ( i= 0; i < 4; ++i )
    {
      ptr= _nt[i] - (const NESu8 *) _vram_nt;
      SAVE ( ptr );
    }

  return 0;
  
} /* end save_state */


static int
load_state (
            FILE *f
            )
{

  NES_Rom fake_rom;
  ptrdiff_t ptr;
  int i;
  

  /* Info rellevant ROM. */
  LOAD ( fake_rom.nprg );
  LOAD ( fake_rom.nchr );
  LOAD ( fake_rom.mapper );
  LOAD ( fake_rom.mirroring );
  CHECK ( fake_rom.nprg == _rom->nprg &&
          fake_rom.nchr == _rom->nchr &&
          fake_rom.mapper == _rom->mapper &&
          fake_rom.mirroring == _rom->mirroring );

  /* Estat. */
  LOAD ( _vram_pt );
  LOAD ( _vram_nt );
  for ( i= 0; i < 4; ++i )
    {
      LOAD ( ptr );
      CHECK ( ptr >= 0 && ptr <= (sizeof(_vram_nt)/2)*1 );
      _nt[i]= ((NESu8 *) _vram_nt) + ptr;
    }
  
  return 0;
  
} /* end load_state */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NES_Error
NES_mapper_nrom_init (
        	      const NES_Rom     *rom,
        	      NES_Warning       *warning,
        	      NES_MapperChanged *mapper_changed,
        	      void              *udata
        	      )
{

  _rom= rom;
  _udata= udata;
  _warning= warning;

  if ( rom->nprg < 1 || rom->nprg > 2 || rom->nchr > 1 ||
       (rom->mirroring != NES_VERTICAL && rom->mirroring != NES_HORIZONTAL) )
    return NES_BADROM;

  /* Callbacks. */
  if ( _rom->nprg == 1 )
    {
      NES_mapper_read= nrom128_read;
      NES_mapper_write= nrom128_write;
    }
  else
    {
      NES_mapper_read= nrom256_read;
      NES_mapper_write= nrom256_write;
    }
  NES_mapper_vram_read= vram_read;
  NES_mapper_vram_write= vram_write;
  NES_mapper_reset= reset;
  NES_mapper_get_rom_mapper_state= get_rom_mapper_state;
  NES_mapper_set_mode_trace= set_mode_trace;
  NES_mapper_init_state= init_state;
  NES_mapper_save_state= save_state;
  NES_mapper_load_state= load_state;

  init_state ();
  
  return NES_NOERROR;
  
} /* end NES_mapper_nrom_init */
