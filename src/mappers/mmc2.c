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
 *  mmc2.c - Implementació de 'mmc2.h'.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mmc2.h"



/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define GET_PRG_BANK(I) (&(_rom->prgs[0][0]) + 8*1024*(I))

#define GET_CHR_BANK(I) (&(_rom->chrs[0][0]) + 4*1024*(I))

#define MMC2_CHECK_CHR_ACCESS(I)                                        \
  do {                                                                  \
    if ( (I) >= _rom->nchr*2 )        					\
    {                                                                   \
      _warning ( _udata, "Trying to acces MMC2 CHR bank %d", (I) );     \
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
  const NESu8 *prg_bank[4];
  const NESu8 *chr_bank[2];
  int          latch0_FD;
  int          latch0_FE;
  int          latch1_FD;
  int          latch1_FE;
  NES_Bool     latch0_is_FD;
  NES_Bool     latch1_is_FD;
} _state;

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
update_chrs (void)
{
  
  _state.chr_bank[0]=
    _state.latch0_is_FD ?
    GET_CHR_BANK ( _state.latch0_FD ) :
    GET_CHR_BANK ( _state.latch0_FE );
  _state.chr_bank[1]=
    _state.latch1_is_FD ?
    GET_CHR_BANK ( _state.latch1_FD ) :
    GET_CHR_BANK ( _state.latch1_FE );
  
} /* end update_chrs */


static void
update_mirroring (
        	  const NES_Bool is_horizontal
        	  )
{

  if ( is_horizontal )
    {
      _nt[0]= _nt[1]= &(_vram_nt[0]);
      _nt[2]= _nt[3]= &(_vram_nt[0x400]);
    }
  else /* NES_VERTICAL */
    {
      _nt[0]= _nt[2]= &(_vram_nt[0]);
      _nt[1]= _nt[3]= &(_vram_nt[0x400]);
    }
  
} /* end update_mirroring */


static NESu8
mmc2_read (
           const NESu16 addr
           )
{
  return _state.prg_bank[addr>>13][addr&0x1FFF];
} /* end mmc2_read */


static void
mmc2_write (
            const NESu16 addr,
            const NESu8  data
            )
{

  int i;

  
  if ( addr < 0x2000 ) return;

  NES_ppu_sync ();
  
  /* PRG ROM bank select ($A000-$AFFF) */
  if ( addr < 0x3000 )
    {
      i= data&0xF; /* NOTA!!! Són de 8K els prg en MMC2. */
      if ( i >= _rom->nprg*2 )
        {
          _warning ( _udata, "Trying to acces MMC2 PRG bank %d", i );
          return;
        }
      _state.prg_bank[0]= GET_PRG_BANK(i);
    }
  
  /* CHR ROM $FD/0000 bank select ($B000-$BFFF) */
  else if ( addr < 0x4000 )
    {
      i= data&0x1F;
      MMC2_CHECK_CHR_ACCESS ( i ) ;
      _state.latch0_FD= i;
      update_chrs ();
    }
  
  /* CHR ROM $FE/0000 bank select ($C000-$CFFF) */
  else if ( addr < 0x5000 )
    {
      i= data&0x1F;
      MMC2_CHECK_CHR_ACCESS ( i ) ;
      _state.latch0_FE= i;
      update_chrs ();
    }

  /* CHR ROM $FD/1000 bank select ($D000-$DFFF) */
  else if ( addr < 0x6000 )
    {
      i= data&0x1F;
      MMC2_CHECK_CHR_ACCESS ( i ) ;
      _state.latch1_FD= i;
      update_chrs ();
    }

  /* CHR ROM $FE/1000 bank select ($E000-$EFFF) */
  else if ( addr < 0x7000 )
    {
      i= data&0x1F;
      MMC2_CHECK_CHR_ACCESS ( i ) ;
      _state.latch1_FE= i;
      update_chrs ();
    }
  
  /* Mirroring ($F000-$FFFF) */
  else update_mirroring ( (data&0x1)!=0 );
  
} /* end mmc2_write */


static NESu8
vram_read (
           const NESu16 addr
           )
{

  NESu8 ret;

  
  if ( addr < 0x2000 )
    {
      ret= _state.chr_bank[addr>>12][addr&0xFFF];
      if ( (addr&0x0FC0) == 0x0FC0 )
        {
          if ( addr == 0x0FD8 )
            {
              _state.latch0_is_FD= NES_TRUE;
              update_chrs ();
            }
          else if ( addr == 0x0FE8 )
            {
              _state.latch0_is_FD= NES_FALSE;
              update_chrs ();
            }
          else if ( addr >= 0x1FD8 && addr <= 0x1FDF )
            {
              _state.latch1_is_FD= NES_TRUE;
              update_chrs ();
            }
          else if ( addr >= 0x1FE8 && addr <= 0x1FEF )
            {
              _state.latch1_is_FD= NES_FALSE;
              update_chrs ();
            }
        }
      return ret;
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
  
  if ( addr >= 0x2000 )
    _nt[(addr>>10)&0x3][addr&0x3FF]= data;
  
} /* end vram_write */


static void
reset (void)
{

  NES_ppu_sync ();
  
  _state.prg_bank[0]= GET_PRG_BANK ( 0 );
  _state.prg_bank[1]= GET_PRG_BANK ( (_rom->nprg*2)-3 );
  _state.prg_bank[2]= GET_PRG_BANK ( (_rom->nprg*2)-2 );
  _state.prg_bank[3]= GET_PRG_BANK ( (_rom->nprg*2)-1 );
  
  _state.latch0_FD= 0;
  _state.latch0_FE= 0;
  _state.latch0_is_FD= NES_FALSE;
  _state.latch1_FD= 0;
  _state.latch1_FE= 0;
  _state.latch1_is_FD= NES_FALSE;
  update_chrs ();
  
  update_mirroring ( NES_FALSE );
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  mmc2_write ( addr, data );
  if ( addr >= 0x2000 )
    _mapper_changed ( _udata );
  
} /* end write_trace */


static NESu8
vram_read_trace (
        	 const NESu16 addr
        	 )
{

  NESu8 ret;


  ret= vram_read ( addr );
  if ( addr == 0x0FD8 || addr == 0x0FE8 ||
       (addr >= 0x1FD8 && addr <= 0x1FDF) ||
       (addr >= 0x1FE8 && addr <= 0x1FEF) )
    _mapper_changed ( _udata );
  
  return ret;
  
} /* end vram_read_trace */
 

static void
set_mode_trace (
                const NES_Bool val
                )
{
  
  if ( _mapper_changed != NULL )
    {
      _trace_enabled= val;
      NES_mapper_write= val ? write_trace : mmc2_write;
      NES_mapper_vram_read= val ? vram_read_trace : vram_read;
    }
  
} /* end set_mode_trace */


static void
get_rom_mapper_state (
                      NES_RomMapperState *state
                      )
{
  
  state->p0= (_state.prg_bank[0] - ((const NESu8 *)_rom->prgs))/(8*1024);
  state->p1= (_state.prg_bank[1] - ((const NESu8 *)_rom->prgs))/(8*1024);
  state->p2= (_state.prg_bank[2] - ((const NESu8 *)_rom->prgs))/(8*1024);
  state->p3= (_state.prg_bank[3] - ((const NESu8 *)_rom->prgs))/(8*1024);
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{

  memset ( _vram_nt, 0, sizeof(_vram_nt) );
  reset ();
  
} /* end init_state */


static int
save_state (
            FILE *f
            )
{

  ptrdiff_t ptr;
  int i;
  const NESu8 *prg_bank[4],*chr_bank[2];
  size_t ret;
  
  
  /* Info Rom rellevant. */
  SAVE ( _rom->nprg );
  SAVE ( _rom->nchr );
  SAVE ( _rom->mapper );
  SAVE ( _rom->mirroring );

  /* Estat. */
  for ( i= 0; i < 4; ++i )
    {
      prg_bank[i]= _state.prg_bank[i];
      _state.prg_bank[i]=
        (void *) (_state.prg_bank[i] - (const NESu8 *) _rom->prgs);
    }
  for ( i= 0; i < 2; ++i )
    {
      chr_bank[i]= _state.chr_bank[i];
      _state.chr_bank[i]=
        (void *) (_state.chr_bank[i] - (const NESu8 *) _rom->chrs);
    }
  ret= fwrite ( &_state, sizeof(_state), 1, f );
  for ( i= 0; i < 4; ++i ) _state.prg_bank[i]= prg_bank[i];
  for ( i= 0; i < 2; ++i ) _state.chr_bank[i]= chr_bank[i];
  if ( ret != 1 ) return -1;
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
  for ( i= 0; i < 4; ++i )
    {
      ptr= (ptrdiff_t) _state.prg_bank[i];
      CHECK ( ptr >= 0 && ptr <= ((_rom->nprg*2)-1)*8*1024 ); /* pags. 8K */
      _state.prg_bank[i]= ((const NESu8 *) _rom->prgs) + ptr;
    }
  for ( i= 0; i < 2; ++i )
    {
      ptr= (ptrdiff_t) _state.chr_bank[i];
      CHECK ( ptr >= 0 && ptr <= ((_rom->nchr*2)-1)*4*1024 ); /* pags. 4K */
      _state.chr_bank[i]= ((const NESu8 *) _rom->chrs) + ptr;
    }
  CHECK ( _state.latch0_FD >= 0 && _state.latch0_FD < _rom->nchr*2 );
  CHECK ( _state.latch0_FE >= 0 && _state.latch0_FE < _rom->nchr*2 );
  CHECK ( _state.latch1_FD >= 0 && _state.latch1_FD < _rom->nchr*2 );
  CHECK ( _state.latch1_FE >= 0 && _state.latch1_FE < _rom->nchr*2 );
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
NES_mapper_mmc2_init (
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

  if ( _rom->nprg != 8 ||
       _rom->nchr != 16 ||
       (_rom->mirroring != NES_HORIZONTAL &&
        _rom->mirroring != NES_VERTICAL) )
    return NES_BADROM;
  
  /* Callbacks. */
  NES_mapper_read= mmc2_read;
  NES_mapper_write= mmc2_write;
  NES_mapper_vram_read= vram_read;
  NES_mapper_vram_write= vram_write;
  NES_mapper_reset= reset;
  NES_mapper_get_rom_mapper_state= get_rom_mapper_state;
  NES_mapper_set_mode_trace= set_mode_trace;
  NES_mapper_init_state= init_state;
  NES_mapper_save_state= save_state;
  NES_mapper_load_state= load_state;
  
  init_state ();
  
  /* Tracer. */
  _trace_enabled= NES_FALSE;
  
  return NES_NOERROR;
  
} /* end NES_mapper_mmc2_init */


void
NES_mapper_mmc2_save_state (
        		    NES_mmc2_state_t *state
        		    )
{

  state->latch0_is_FD= _state.latch0_is_FD;
  state->latch1_is_FD= _state.latch1_is_FD;
  
} /* end NES_mapper_mmc2_save_state */


void
NES_mapper_mmc2_load_state (
        		    const NES_mmc2_state_t *state
        		    )
{

  _state.latch0_is_FD= state->latch0_is_FD;
  _state.latch1_is_FD= state->latch1_is_FD;
  update_chrs ();
  
} /* end NES_mapper_mmc2_load_state */
