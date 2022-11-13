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
 *  cnrom.c - Implementació de 'cnrom.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cnrom.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define BUS_CONFLICT(ADDR)        				\
  _warning ( _udata, "Bus conflict at $%04x", (ADDR)+0x8000 )


#define NROM_READ128(ADDR)        		\
  ((const NESu8 *) _rom->prgs)[(ADDR)&0x3FFF]


#define NROM_READ256(ADDR)                   \
  ((const NESu8 *) _rom->prgs)[(ADDR)]


/* NOTA!!!! lo dels 3 bits ve de la info de nesdev. */
#define CNROM_WRITE(SIZE)                             \
  NESu8 byte;                                         \
  int ibank;                                          \
                                                      \
                                                      \
  NES_ppu_sync ();        			      \
  byte= NROM_READ ## SIZE ( addr );                   \
  if ( byte != data ) BUS_CONFLICT ( addr );          \
  ibank= data&0x3;        			      \
  if ( ibank >= _rom->nchr )        		      \
    _warning ( _udata,        			      \
               "Trying to acces CNROM bank %d",	      \
               ibank  );			      \
  _vrom= _rom->chrs[ibank]




/*********/
/* ESTAT */
/*********/

/* Rom. */
static const NES_Rom *_rom;

/* Callbacks. */
static void *_udata;
static NES_Warning *_warning;
static NES_MapperChanged *_mapper_changed;

/* VRom. */
const NESu8 *_vrom;

/* VRAM ntables. */
static NESu8 _vram_nt[0x800];

/* Mapping nametables. */
static NESu8 *_nt[4];

/* Trace. */
static NES_Bool _trace_enabled;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static NESu8
nrom128_read (
              const NESu16 addr
              )
{
  return NROM_READ128 ( addr );
} /* end nrom128_read */


static NESu8
nrom256_read (
              const NESu16 addr
              )
{
  return NROM_READ256 ( addr );
} /* end nrom256_read */


static void
cnrom128_write (
                const NESu16 addr,
                const NESu8  data
                )
{
  CNROM_WRITE ( 128 );
} /* end cnrom128_write */


static void
cnrom256_write (
                const NESu16 addr,
                const NESu8  data
                )
{
  CNROM_WRITE ( 256 );
} /* end cnrom256_write */


static NESu8
vram_read (
           const NESu16 addr
           )
{

  if ( addr < 0x2000 )
    return _vrom[addr];
  
  else /* < 0x3000 */
    return _nt[(addr>>10)&0x3][addr&0x3FF];
  
} /* end vram_read */


static void
vram_write (
            const NESu16 addr,
            const NESu8  data
            )
{

  if ( addr >= 0x2000 )
    _nt[(addr>>10)&0x3][addr&0x3FF]= data;
  
} /* end vram_write */


static void
reset (void)
{
  
  NES_ppu_sync ();
  _vrom= (const NESu8 *) _rom->chrs[0];
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  if ( _rom->nprg == 1 ) cnrom128_write ( addr, data );
  else                   cnrom256_write ( addr, data );
  _mapper_changed ( _udata );
  
} /* end write_trace */


static void
set_mode_trace (
                const NES_Bool val
                )
{

  if ( _mapper_changed != NULL )
    {
      _trace_enabled= val;
      NES_mapper_write= val ? write_trace :
        (_rom->nprg == 1 ? cnrom128_write : cnrom256_write);
    }
  
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

  /* Rom. */
  if ( _rom->nprg == 1 )
    {
      NES_mapper_read= nrom128_read;
      NES_mapper_write= cnrom128_write;
    }
  else
    {
      NES_mapper_read= nrom256_read;
      NES_mapper_write= cnrom256_write;
    }

  /* Vram. */
  _vrom= (const NESu8 *) _rom->chrs[0];
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
  
  
  /* Info ROM rellevant. */
  SAVE ( _rom->nprg );
  SAVE ( _rom->nchr );
  SAVE ( _rom->mapper );
  SAVE ( _rom->mirroring );

  /* Estat. */
  ptr= _vrom - (const NESu8 *) _rom->chrs;
  SAVE ( ptr );
  SAVE ( _vram_nt );
  for ( i= 0; i < 4; ++i )
    {
      ptr= _nt[i]- (const NESu8 *) _vram_nt;
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
  LOAD ( ptr );
  CHECK ( ptr >= 0 && ptr <= (_rom->nchr-1)*8*1024 );
  _vrom= ((const NESu8 *) _rom->chrs) + ptr;
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
NES_mapper_cnrom_init (
                       const NES_Rom     *rom,
                       NES_Warning       *warning,
        	       NES_MapperChanged *mapper_changed,
                       void              *udata
                       )
{

  _rom= rom;
  _udata= udata;
  _warning= warning;
  _mapper_changed= mapper_changed;

  if ( _rom->nprg < 1 || _rom->nprg > 2 ||
       _rom->nchr < 1 || _rom->nchr > 4 ||
       (_rom->mirroring != NES_VERTICAL &&
        _rom->mirroring != NES_HORIZONTAL) )
    return NES_BADROM;

  /* Callbacks. */
  if ( _rom->nprg == 1 )
    {
      NES_mapper_read= nrom128_read;
      NES_mapper_write= cnrom128_write;
    }
  else
    {
      NES_mapper_read= nrom256_read;
      NES_mapper_write= cnrom256_write;
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
  
  /* Trace. */
  _trace_enabled= NES_FALSE;
  
  return NES_NOERROR;
  
} /* end NES_mapper_cnrom_init */
