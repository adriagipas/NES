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
 *  mmc1.c - Implementació de 'mmc1.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mmc1.h"



/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define MMC1_CHECK_CHR_ACCESS(I)                                        \
  do {                                                                  \
    if ( (I) >= _rom->nchr )        					\
    {                                                                   \
      _warning ( _udata, "Trying to acces MMC1 CHR bank %d", (I) );        \
      return;                                                           \
    }                                                                   \
  } while(0)


#define MMC1_CHECK_PRG_ACCESS(I)                                        \
  do {                                                                  \
    if ( (I) >= _rom->nprg )        					\
    {                                                                   \
      _warning ( _udata, "Trying to acces MMC1 PRG bank %d", (I) );        \
      return;                                                           \
    }                                                                   \
  } while(0)




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
  const NESu8 *prg_bank[2];
  const NESu8 *chr_bank[2];
  NESu8        load_reg;
  int          counter;
  int          prg_bank_mode;
  int          chr_bank_mode;
} _state;

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
mmc1_read (
           const NESu16 addr
           )
{
  return _state.prg_bank[addr>>14][addr&0x3FFF];
} /* end mmc1_read */


static void
mmc1_set_prg_bank_mode_2 (void)
{
  _state.prg_bank[0]= _rom->prgs[0];
} /* end mmc1_set_prg_bank_mode_2 */


static void
mmc1_set_prg_bank_mode_3 (void)
{
  _state.prg_bank[1]= _rom->prgs[_rom->nprg-1];
} /* end mmc1_set_prg_bank_mode_3 */


static void
mmc1_write (
            const NESu16 addr,
            const NESu8  data
            )
{
  
  int i, prgL, prgH;
  NESu8 reg;
  

  NES_ppu_sync ();
  
  /* Reset i Control=0x0C*/
  if ( data&0x80 )
    {
      _state.load_reg= 0x00;
      _state.counter= 0;
      config_single_screen ( 0 );
      _state.prg_bank_mode= 3;
      mmc1_set_prg_bank_mode_3 ();
      _state.chr_bank_mode= 0;
      return;
    }
  
  /* Normal. */
  if ( data&0x1 ) _state.load_reg|= 0x10;
  if ( ++_state.counter != 5 ) _state.load_reg>>= 1;
  else
    {
      
      reg= _state.load_reg;
      _state.load_reg= 0x00;
      _state.counter= 0;
      
      /* Control. */
      if ( addr < 0x2000 )
        {
          switch ( reg&0x3 )
            {
            case 0: config_single_screen ( 0 ); break;
            case 1: config_single_screen ( 1 ); break;
            case 2: /* VERTICAL */
              _nt[0]= _nt[2]= &(_vram_nt[0]);
              _nt[1]= _nt[3]= &(_vram_nt[0x400]);
              break;
            case 3: /* HORIZONTAL */
              _nt[0]= _nt[1]= &(_vram_nt[0]);
              _nt[2]= _nt[3]= &(_vram_nt[0x400]);
              break;
            }
          _state.prg_bank_mode= (reg&0xC)>>2;
          switch ( _state.prg_bank_mode )
            {
            case 2: mmc1_set_prg_bank_mode_2 (); break;
            case 3: mmc1_set_prg_bank_mode_3 (); break;
            default: break;
            }
          _state.chr_bank_mode= ((reg&0x10)!=0);
        }
      
      /* CHR bank 0. */
      else if ( addr < 0x4000 )
        {
          i= reg>>1;
          if ( _rom->nchr == 0 ) /* RAM */
            {
              if ( _state.chr_bank_mode != 0 || i != 0 )
                _warning ( _udata, "Trying to switch MMC1 CHR RAM bank" );
            }
          else
            {
              MMC1_CHECK_CHR_ACCESS ( i );
              if ( _state.chr_bank_mode == 0 )
                {
                  _state.chr_bank[0]= _rom->chrs[i];
                  _state.chr_bank[1]= _rom->chrs[i]+4096;
                }
              else
                {
                  _state.chr_bank[0]= _rom->chrs[i];
                  if ( reg&0x1 )
                    _state.chr_bank[0]+= 4096;
                }
            }
        }
      
      /* CHR bank 1. */
      else if ( addr < 0x6000 )
        {
          if ( _state.chr_bank_mode == 0 ) return;
          else
            {
              i= reg>>1;
              MMC1_CHECK_CHR_ACCESS ( i );
              _state.chr_bank[1]= _rom->chrs[i];
              if ( reg&0x1 )
                _state.chr_bank[1]+= 4096;
            }
        }
      
      /* PRG bank. */
      else
        {
          /* Ignore el bit 5 que és: 'PRG RAM chip enable (0: enabled;
             1: disabled; ignored on MMC1A)'. */
          prgH= prgL= -1;
          i= reg&0xF;
          MMC1_CHECK_PRG_ACCESS ( i );
          switch ( _state.prg_bank_mode )
            {
            case 0:
            case 1:
              prgL= i&0xE;
              prgH= prgL|0x1;
              MMC1_CHECK_PRG_ACCESS ( prgH );
              break;
            case 2: prgH= i; break;
            case 3: prgL= i; break;
            }
          if ( prgL != -1 ) _state.prg_bank[0]= _rom->prgs[prgL];
          if ( prgH != -1 ) _state.prg_bank[1]= _rom->prgs[prgH];
        }
      
    }
  
} /* end mmc1_write */     


static NESu8
vram_read (
           const NESu16 addr
           )
{

  if ( addr < 0x2000 )
    {
      if ( _rom->nchr ) return _state.chr_bank[addr>>12][addr&0xFFF];
      else              return _vram_pt[addr];
    }
  
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

  NES_ppu_sync ();
  
  _state.prg_bank[0]= _rom->prgs[0];
  _state.prg_bank[1]= _rom->prgs[_rom->nprg-1];
  
  if ( _rom->nchr != 0 )
    {
      _state.chr_bank[0]= _rom->chrs[0];
      _state.chr_bank[1]= _rom->chrs[0]+4096;
    }
  
  _state.load_reg= 0x00;
  _state.counter= 0;
  _state.prg_bank_mode= 3;
  _state.chr_bank_mode= 0;
  config_single_screen ( 0 );
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  mmc1_write ( addr, data );
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
      NES_mapper_write= val ? write_trace : mmc1_write;
    }
  
} /* end set_mode_trace */


static void
get_rom_mapper_state (
                      NES_RomMapperState *state
                      )
{

  ptrdiff_t nbank;


  nbank= (_state.prg_bank[0] - ((const NESu8 *)_rom->prgs))/NES_PRG_SIZE;
  state->p0= nbank*2;
  state->p1= state->p0+1;
  nbank= (_state.prg_bank[1] - ((const NESu8 *)_rom->prgs))/NES_PRG_SIZE;
  state->p2= nbank*2;
  state->p3= state->p2+1;
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{
  
  /* ROM. */
  _state.prg_bank[0]= _rom->prgs[0];
  _state.prg_bank[1]= _rom->prgs[_rom->nprg-1];
  
  /* Vram. */
  memset ( _vram_pt, 0, sizeof(_vram_pt) );
  memset ( _vram_nt, 0, sizeof(_vram_nt) );
  if ( _rom->nchr != 0 )
    {
      _state.chr_bank[0]= _rom->chrs[0];
      _state.chr_bank[1]= _rom->chrs[0]+4096;
    }
  _nt[0]= _nt[1]= _nt[2]= _nt[3]= _vram_nt;
  
  /* Altres coses de l'estat. */
  _state.load_reg= 0x00;
  _state.counter= 0;
  _state.prg_bank_mode= 3;
  _state.chr_bank_mode= 0;
  
} /* end init_state */


static int
save_state (
            FILE *f
            )
{
  
  ptrdiff_t ptr;
  int i;
  const NESu8 *prg_bank[2],*chr_bank[2];
  size_t ret;
  
  
  /* Info Rom rellevant. */
  SAVE ( _rom->nprg );
  SAVE ( _rom->nchr );
  SAVE ( _rom->mapper );
  SAVE ( _rom->mirroring );

  /* Estat. */
  for ( i= 0; i < 2; ++i )
    {
      prg_bank[i]= _state.prg_bank[i];
      _state.prg_bank[i]=
        (void *) (_state.prg_bank[i] - (const NESu8 *) _rom->prgs);
      chr_bank[i]= _state.chr_bank[i];
      if ( _rom->nchr == 0 ) _state.chr_bank[i]= NULL;
      else _state.chr_bank[i]=
             (void *) (_state.chr_bank[i] - (const NESu8 *) _rom->chrs);
    }
  ret= fwrite ( &_state, sizeof(_state), 1, f );
  for ( i= 0; i < 2; ++i )
    {
      _state.prg_bank[i]= prg_bank[i];
      _state.chr_bank[i]= chr_bank[i];
    }
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
  LOAD ( _state );
  for ( i= 0; i < 2; ++i )
    {
      ptr= (ptrdiff_t) _state.prg_bank[i];
      CHECK ( ptr >= 0 && ptr <= (_rom->nprg-1)*16*1024 );
      _state.prg_bank[i]= ((const NESu8 *) _rom->prgs) + ptr;
      if ( _rom->nchr )
        {
          ptr= (ptrdiff_t) _state.chr_bank[i];
          CHECK ( ptr >= 0 && ptr <= ((_rom->nchr*2)-1)*4*1024 ); /* pags. 4K */
          _state.chr_bank[i]= ((const NESu8 *) _rom->chrs) + ptr;
        }
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
NES_mapper_mmc1_init (
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

  if ( _rom->nprg < 1 || _rom->nprg > 32 ||
       _rom->nchr > 16 ||
       _rom->mirroring == NES_FOURSCREEN )
    return NES_BADROM;
  
  /* Callbacks. */
  NES_mapper_read= mmc1_read;
  NES_mapper_write= mmc1_write;
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
  
} /* end NES_mapper_mmc1_init */
