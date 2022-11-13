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
 *  aorom.c - Implementació de 'aorom.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "aorom.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;




/*********/
/* ESTAT */
/*********/

/* Rom. */
static const NES_Rom *_rom;

/* Callbacks. */
static void *_udata;
static NES_Warning *_warning;
static NES_MapperChanged *_mapper_changed;

/* Mapeig ROM. */
static struct
{

  const NESu8 *bank;
  int          nbanks;
  
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

static void
config_single_screen (
        	      const int area
        	      )
{

  NESu8 *mem;


  NES_ppu_sync ();
  mem= area ? &(_vram_nt[0x400]) : _vram_nt;
  _nt[0]= _nt[1]= _nt[2]= _nt[3]= mem;
  
} /* end config_single_screen */


static NESu8
aorom_read (
            const NESu16 addr
            )
{
  return _mmap.bank[addr&0x7FFF];
} /* end aorom_read */


/* Com no tots els subtipus d'AxROM tenen conflictes no els
   reporte.*/
static void
aorom_write (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  int ibank;
  
  
  ibank= data&0xF;
  if ( ibank >= _mmap.nbanks )
    _warning ( _udata, "Trying to acces AOROM bank %d", ibank  );
  else _mmap.bank= _rom->prgs[ibank<<1];
  config_single_screen ( (data>>4)&0x1 );
  
} /* end aorom_write */


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
  
  _mmap.bank= (const NESu8 *) *(_rom->prgs);
  config_single_screen ( 0 );
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  aorom_write ( addr, data );
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
      NES_mapper_write= val ? write_trace : aorom_write;
    }
  
} /* end set_mode_trace */


static void
get_rom_mapper_state (
        	      NES_RomMapperState *state
        	      )
{

  ptrdiff_t nbank;


  nbank= (_mmap.bank - ((const NESu8 *)_rom->prgs))/NES_PRG_SIZE;
  state->p0= nbank*2;
  state->p1= state->p0+1;
  state->p2= state->p0+2;
  state->p3= state->p0+3;
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{

  /* Rom. */
  _mmap.nbanks= _rom->nprg>>1;
  _mmap.bank= (const NESu8 *) *(_rom->prgs);
  
  /* Vram. */
  memset ( _vram_pt, 0, sizeof(_vram_pt) );
  memset ( _vram_nt, 0, sizeof(_vram_nt) );
  _nt[0]= _nt[1]= _nt[2]= _nt[3]= _vram_nt;
  
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

  /* Estat. */
  ptr= _mmap.bank- (const NESu8 *) _rom->prgs;
  SAVE ( ptr );
  SAVE ( _mmap.nbanks );
  SAVE ( _vram_pt );
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
  CHECK ( fake_rom.nprg == _rom->nprg &&
          fake_rom.nchr == _rom->nchr &&
          fake_rom.mapper == _rom->mapper );
  
  /* Estat. */
  LOAD ( ptr );
  CHECK ( ptr >= 0 && ptr <= ((_rom->nprg/2-1)*32*1024) ); /* Banks de 32K. */
  _mmap.bank= ((const NESu8 *) _rom->prgs) + ptr;
  LOAD ( _mmap.nbanks );
  CHECK ( _mmap.nbanks == _rom->nprg/2 );
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
NES_mapper_aorom_init (
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

  if ( _rom->nprg % 2 != 0 || _rom->nprg > 16 ||
       _rom->nchr != 0 )
    return NES_BADROM;
  
  /* Callbacks. */
  NES_mapper_init_state= init_state;
  NES_mapper_read= aorom_read;
  NES_mapper_write= aorom_write;
  NES_mapper_vram_read= vram_read;
  NES_mapper_vram_write= vram_write;
  NES_mapper_reset= reset;
  NES_mapper_get_rom_mapper_state= get_rom_mapper_state;
  NES_mapper_set_mode_trace= set_mode_trace;
  NES_mapper_save_state= save_state;
  NES_mapper_load_state= load_state;

  init_state ();

  /* Trace. */
  _trace_enabled= NES_FALSE;
  
  return NES_NOERROR;
  
} /* end NES_mapper_aorom_init */
