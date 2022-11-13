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
 *  unrom.c - Implementació de 'unrom.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "unrom.h"



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


#define UNROM_READ(ADDR)        		\
  _mmap.bank[(ADDR)>>14][(ADDR)&0x3FFF]


#define UNROM_WRITE(MASK)                    \
                                             \
  NESu8 byte;                                \
                                             \
                                             \
  byte= UNROM_READ ( addr );                 \
  if ( byte != data ) BUS_CONFLICT ( addr ); \
  _mmap.ibank= data&(MASK);        	     \
  _mmap.bank[0]=        		     \
    (const NESu8 *) _rom->prgs[_mmap.ibank]




/*********/
/* ESTAT */
/*********/

/* Rom. */
static const NES_Rom *_rom;

/* Callbacks. */
static void *_udata;
static NES_Warning *_warning;
static NES_MapperChanged *_mapper_changed;

/* Estat. */
static struct
{
  
  const NESu8 *bank[2];
  int          ibank;
  
} _mmap;

/* VRAM ptables. */
static NESu8 _vram_pt[0x2000];

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
unrom_read (
            const NESu16 addr
            )
{
  return UNROM_READ ( addr );
} /* end read_rom */


static void
unrom_write (
             const NESu16 addr,
             const NESu8  data
             )
{
  UNROM_WRITE(0x7);
} /* end write_rom */


static void
uorom_write (
             const NESu16 addr,
             const NESu8  data
             )
{
  UNROM_WRITE(0xF);
} /* end uorom_write */


static NESu8
vram_read (
           const NESu16 addr
           )
{

  if ( addr < 0x2000 )
    return _vram_pt[addr];
  
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
    _vram_pt[addr]= data;
  
  else /* < 0x3000 */
    _nt[(addr>>10)&0x3][addr&0x3FF]= data;
  
} /* end vram_write */


static void
reset (void)
{
  
  _mmap.ibank= 0;
  _mmap.bank[0]=
    (const NESu8 *) _rom->prgs[0];
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{

  if ( _rom->nprg == 8 ) unrom_write ( addr, data );
  else                   uorom_write ( addr, data );
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
        ((_rom->nprg == 8) ? unrom_write : uorom_write);
    }
  
} /* end set_mode_trace */


static void
get_rom_mapper_state (
                      NES_RomMapperState *state
                      )
{

  ptrdiff_t nbank;


  nbank= (_mmap.bank[0] - ((const NESu8 *)_rom->prgs))/NES_PRG_SIZE;
  state->p0= nbank*2;
  state->p1= state->p0+1;
  nbank= (_mmap.bank[1] - ((const NESu8 *)_rom->prgs))/NES_PRG_SIZE;
  state->p2= nbank*2;
  state->p3= state->p2+1;
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{
  
  _mmap.bank[1]=
    (const NESu8 *) _rom->prgs[_rom->nprg-1];

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
  const NESu8 *bank[2];
  size_t ret;
  
  
  /* Info Rom rellevant. */
  SAVE ( _rom->nprg );
  SAVE ( _rom->nchr );
  SAVE ( _rom->mapper );
  SAVE ( _rom->mirroring );

  /* Estat. */
  for ( i= 0; i < 2; ++i )
    {
      bank[i]= _mmap.bank[i];
      _mmap.bank[i]= (void *) (_mmap.bank[i] - (const NESu8 *) _rom->prgs);
    }
  ret= fwrite ( &_mmap, sizeof(_mmap), 1, f );
  for ( i= 0; i < 2; ++i ) _mmap.bank[i]= bank[i];
  if ( ret != 1 ) return -1;
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
  LOAD ( _mmap );
  for ( i= 0; i < 2; ++i )
    {
      ptr= (ptrdiff_t) _mmap.bank[i];
      CHECK ( ptr >= 0 && ptr <= (_rom->nprg-1)*16*1024 );
      _mmap.bank[i]= ((const NESu8 *) _rom->prgs) + ptr;
    }
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
NES_mapper_unrom_init (
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

  if ( (_rom->nprg != 8 && _rom->nprg != 16) ||
       _rom->nchr != 0 ||
       (_rom->mirroring != NES_VERTICAL && _rom->mirroring != NES_HORIZONTAL) )
    return NES_BADROM;
  
  /* Callbacks. */
  NES_mapper_read= unrom_read;
  NES_mapper_write= (_rom->nprg == 8) ? unrom_write : uorom_write;
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
  
} /* end NES_mapper_unrom_init */
