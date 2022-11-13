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
 *  mmc3.c - Implementació de 'mmc3.h'.
 *
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../NES.h"
#include "mmc3.h"



/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define MMC3_CHECK_PRG_ACCESS(I)                                        \
  do {                                                                  \
    if ( (I) >= _state.nprg_banks )        				\
    {                                                                   \
      _warning ( _udata, "Trying to acces MMC3 PRG bank %d %d",        	\
        	 (I), _state.nprg_banks );				\
      return;                                                           \
    }                                                                   \
  } while(0)


#define MMC3_CHECK_CHR_ACCESS(I)                                        \
  do {                                                                  \
    if ( (I) >= _state.nchr_banks )        				\
    {                                                                   \
      _warning ( _udata, "Trying to acces MMC3 CHR bank %d", (I) );     \
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

  /* Memòria. */
  NESu8        regs[8];
  const NESu8 *prg_bank[4];
  const NESu8 *chr_bank[8];
  int          nprg_banks;
  NESu8        prg_mask;
  int          nchr_banks;
  NES_Bool     four_screen;
  int          prg_bank_mode;
  int          sel_reg;
  int          chr_bank_mode;

  /* IRQ. */
  NESu8        irq_counter;
  NESu8        irq_latch;
  NES_Bool     irq_enabled;
  NES_Bool     irq_reload;
  NES_Bool     irq_active;
  
} _state;


/* VRAM ptables. */
static NESu8 _vram_pt[0x2000];

/* VRAM ntables. */
static NESu8 _vram_nt[0x1000];

/* Mapping nametables. */
static NESu8 *_nt[4];

/* Trace. */
static NES_Bool _trace_enabled;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
config_mirroring (
        	  const int is_horizontal
        	  )
{

  if ( is_horizontal )
    {
      _nt[0]= _nt[1]= &(_vram_nt[0]);
      _nt[2]= _nt[3]= &(_vram_nt[0x400]);
    }
  else
    {
      _nt[0]= _nt[2]= &(_vram_nt[0]);
      _nt[1]= _nt[3]= &(_vram_nt[0x400]);
    }
  
} /* end config_mirroring */


static void
update_mmap_prg (void)
{

  MMC3_CHECK_PRG_ACCESS ( _state.regs[6] );
  MMC3_CHECK_PRG_ACCESS ( _state.regs[7] );
  if ( _state.prg_bank_mode == 0 )
    {
      _state.prg_bank[0]= _rom->prgs[0] + _state.regs[6]*8192;
      _state.prg_bank[1]= _rom->prgs[0] + _state.regs[7]*8192;
      _state.prg_bank[2]= _rom->prgs[0] + (_state.nprg_banks-2)*8192;
      _state.prg_bank[3]= _rom->prgs[0] + (_state.nprg_banks-1)*8192;
    }
  else
    {
      _state.prg_bank[0]= _rom->prgs[0] + (_state.nprg_banks-2)*8192;
      _state.prg_bank[1]= _rom->prgs[0] + _state.regs[7]*8192;
      _state.prg_bank[2]= _rom->prgs[0] + _state.regs[6]*8192;
      _state.prg_bank[3]= _rom->prgs[0] + (_state.nprg_banks-1)*8192;
    }
  
} /* end update_mmap_prg */


static void
update_mmap_chr (void)
{

  MMC3_CHECK_CHR_ACCESS ( _state.regs[0]|0x01 );
  MMC3_CHECK_CHR_ACCESS ( _state.regs[1]|0x01 );
  MMC3_CHECK_CHR_ACCESS ( _state.regs[2] );
  MMC3_CHECK_CHR_ACCESS ( _state.regs[3] );
  MMC3_CHECK_CHR_ACCESS ( _state.regs[4] );
  MMC3_CHECK_CHR_ACCESS ( _state.regs[5] );
  if ( _state.chr_bank_mode == 0 )
    {
      _state.chr_bank[0]= _rom->chrs[0] + (_state.regs[0]&0xFE)*1024;
      _state.chr_bank[1]= _rom->chrs[0] + (_state.regs[0]|0x01)*1024;
      _state.chr_bank[2]= _rom->chrs[0] + (_state.regs[1]&0xFE)*1024;
      _state.chr_bank[3]= _rom->chrs[0] + (_state.regs[1]|0x01)*1024;
      _state.chr_bank[4]= _rom->chrs[0] + _state.regs[2]*1024;
      _state.chr_bank[5]= _rom->chrs[0] + _state.regs[3]*1024;
      _state.chr_bank[6]= _rom->chrs[0] + _state.regs[4]*1024;
      _state.chr_bank[7]= _rom->chrs[0] + _state.regs[5]*1024;
    }
  else
    {
      _state.chr_bank[0]= _rom->chrs[0] + _state.regs[2]*1024;
      _state.chr_bank[1]= _rom->chrs[0] + _state.regs[3]*1024;
      _state.chr_bank[2]= _rom->chrs[0] + _state.regs[4]*1024;
      _state.chr_bank[3]= _rom->chrs[0] + _state.regs[5]*1024;
      _state.chr_bank[4]= _rom->chrs[0] + (_state.regs[0]&0xFE)*1024;
      _state.chr_bank[5]= _rom->chrs[0] + (_state.regs[0]|0x01)*1024;
      _state.chr_bank[6]= _rom->chrs[0] + (_state.regs[1]&0xFE)*1024;
      _state.chr_bank[7]= _rom->chrs[0] + (_state.regs[1]|0x01)*1024;
    }
  
} /* end update_mmap_chr */


static void
update_mmap (void)
{

  update_mmap_prg ();
  if ( _rom->nchr )
    update_mmap_chr ();
  
} /* end update_mmap */


static NESu8
mmc3_read (
           const NESu16 addr
           )
{
  return _state.prg_bank[addr>>13][addr&0x1FFF];
} /* end mmc3_read */


static void
mmc3_write (
            const NESu16 addr,
            const NESu8  data
            )
{

  NES_ppu_sync (); /* No és necessari sempre però millor ací. */
  
  if ( addr < 0x2000 )
    {
      
      /* BANK DATA */
      if ( addr&0x1 )
        {
          if ( _state.sel_reg >= 6 )
            _state.regs[_state.sel_reg]= data&_state.prg_mask;
          else
            _state.regs[_state.sel_reg]= data;
          update_mmap ();
        }
      
      /* BANK SELECT */
      else
        {
          _state.sel_reg= data&0x7;
          _state.prg_bank_mode= (data&0x40)!=0;
          _state.chr_bank_mode= (data&0x80)!=0;
          update_mmap ();
        }
      
    }
  else if ( addr < 0x4000 )
    {

      /* PGR RAM PROTECT */
      if ( addr&0x1 )
        {
          /* Per a evitar complicacions, i seguint la recomanació de
             NesDev, he decidit no implementar açò. */
        }

      /* MIRRORING */
      else
        {
          if ( !_state.four_screen )
            config_mirroring ( data&0x1 );
        }
      
    }

  else if ( addr < 0x6000 )
    {

      /* IRQ RELOAD */
      if ( addr&0x1 )
        {
          /*_state.irq_counter= 0;*/ /* <- No està clar. */
          _state.irq_reload= NES_TRUE;
        }

      /* IRQ LATCH */
      else
        {
          _state.irq_latch= data;
        }
      
    }

  else
    {

      /* IRQ ENABLE */
      if ( addr&0x1 )
        {
          _state.irq_enabled= NES_TRUE;
        }

      /* IRQ DISABLE */
      else
        {
          _state.irq_enabled= NES_FALSE;
          _state.irq_active= NES_FALSE;
          /*_state.irq_counter= _state.irq_latch;*/ /* <- No està clar. */
        }
      
    }
  
} /* end mmc3_write */


static NESu8
vram_read (
           const NESu16 addr
           )
{
  
  if ( addr < 0x2000 )
    {
      if ( _rom->nchr ) return _state.chr_bank[addr>>10][addr&0x3FF];
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
  
  /* ROM. */
  _state.prg_bank_mode= 0;
  _state.regs[6]= 0;
  _state.regs[7]= 1;

  /* Vram. */
  _state.chr_bank_mode= 0;
  if ( _rom->nchr )
    {
      _state.regs[0]= 0;
      _state.regs[1]= 2;
      _state.regs[2]= 4;
      _state.regs[3]= 5;
      _state.regs[4]= 6;
      _state.regs[5]= 7;
    }
  if ( !_state.four_screen )
    config_mirroring ( 0 /* VERTICAL */ );

  /* Altres coses d'estat. */
  update_mmap ();
  _state.sel_reg= 0;
  _state.irq_counter= 0;
  _state.irq_latch= 0;
  _state.irq_enabled= NES_FALSE;
  _state.irq_reload= NES_FALSE;
  _state.irq_active= NES_FALSE;
  
} /* end reset */


static void
write_trace (
             const NESu16 addr,
             const NESu8  data
             )
{
  
  mmc3_write ( addr, data );
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
      NES_mapper_write= val ? write_trace : mmc3_write;
    }
  
} /* end set_mode_trace */


static void
get_rom_mapper_state (
                      NES_RomMapperState *state
                      )
{

  state->p0= (_state.prg_bank[0]-((const NESu8 *)_rom->prgs))/(NES_PRG_SIZE>>1);
  state->p1= (_state.prg_bank[1]-((const NESu8 *)_rom->prgs))/(NES_PRG_SIZE>>1);
  state->p2= (_state.prg_bank[2]-((const NESu8 *)_rom->prgs))/(NES_PRG_SIZE>>1);
  state->p3= (_state.prg_bank[3]-((const NESu8 *)_rom->prgs))/(NES_PRG_SIZE>>1);
  
} /* end get_rom_mapper_state */


static void
init_state (void)
{
  
  int aux;
  
  
  memset ( _state.regs, 0, sizeof(_state.regs) );
  
  /* ROM. */
  aux= _state.nprg_banks= _rom->nprg*2;
  _state.prg_mask= 0;
  while ( (aux>>= 1) != 0 )
    _state.prg_mask= _state.prg_mask<<1 | 0x1;
  _state.prg_bank_mode= 0;
  _state.regs[6]= 0;
  _state.regs[7]= 1;
  
  /* Vram. */
  memset ( _vram_pt, 0, sizeof(_vram_pt) );
  memset ( _vram_nt, 0, sizeof(_vram_nt) );
  _state.nchr_banks= _rom->nchr*8;
  _state.chr_bank_mode= 0;
  if ( _rom->nchr )
    {
      _state.regs[0]= 0;
      _state.regs[1]= 2;
      _state.regs[2]= 4;
      _state.regs[3]= 5;
      _state.regs[4]= 6;
      _state.regs[5]= 7;
    }
  if ( _rom->mirroring == NES_FOURSCREEN )
    {
      _state.four_screen= NES_TRUE;
      _nt[0]= &(_vram_nt[0]);
      _nt[1]= &(_vram_nt[0x400]);
      _nt[2]= &(_vram_nt[0x800]);
      _nt[3]= &(_vram_nt[0xC00]);
    }
  else
    {
      _state.four_screen= NES_FALSE;
      config_mirroring ( 0 /* VERTICAL */ );
    }
  
  /* Altres coses d'estat. */
  update_mmap ();
  _state.sel_reg= 0;
  _state.irq_counter= 0;
  _state.irq_latch= 0;
  _state.irq_enabled= NES_FALSE;
  _state.irq_reload= NES_FALSE;
  _state.irq_active= NES_FALSE;
  
} /* end init_state */


static int
save_state (
            FILE *f
            )
{
  
  ptrdiff_t ptr;
  int i;
  const NESu8 *prg_bank[4],*chr_bank[8];
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
  for ( i= 0; i < 8; ++i )
    {
      chr_bank[i]= _state.chr_bank[i];
      if ( _rom->nchr == 0 ) _state.chr_bank[i]= NULL;
      else _state.chr_bank[i]=
             (void *) (_state.chr_bank[i] - (const NESu8 *) _rom->chrs);
    }
  ret= fwrite ( &_state, sizeof(_state), 1, f );
  for ( i= 0; i < 4; ++i ) _state.prg_bank[i]= prg_bank[i];
  for ( i= 0; i < 8; ++i ) _state.chr_bank[i]= chr_bank[i];
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
  NESu8 old_prg_mask;
  

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
  old_prg_mask= _state.prg_mask;
  LOAD ( _state );
  for ( i= 0; i < 4; ++i )
    {
      ptr= (ptrdiff_t) _state.prg_bank[i];
      CHECK ( ptr >= 0 && ptr <= ((_rom->nprg*2)-1)*8*1024 ); /* pags. 8K */
      _state.prg_bank[i]= ((const NESu8 *) _rom->prgs) + ptr;
    }
  if ( _rom->nchr )
    for ( i= 0; i < 8; ++i )
      {
        ptr= (ptrdiff_t) _state.chr_bank[i];
        CHECK ( ptr >= 0 && ptr <= ((_rom->nchr*8)-1)*1024 ); /* pags. 1K */
        _state.chr_bank[i]= ((const NESu8 *) _rom->chrs) + ptr;
      }
  CHECK ( _state.nprg_banks == _rom->nprg*2 );
  CHECK ( _state.prg_mask == old_prg_mask );
  CHECK ( _state.sel_reg >= 0 && _state.sel_reg < 8 );
  LOAD ( _vram_pt );
  LOAD ( _vram_nt );
  for ( i= 0; i < 4; ++i )
    {
      LOAD ( ptr );
      CHECK ( ptr >= 0 && ptr <= (sizeof(_vram_nt)/4)*3 );
      _nt[i]= ((NESu8 *) _vram_nt) + ptr;
    }
  
  return 0;
  
} /* end load_state */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NES_Bool
NES_mapper_mmc3_check_irq (void)
{
  return _state.irq_active;
} /* end return NES_mapper_mmc3_check_irq */


void
NES_mapper_mmc3_clock_counter (void)
{

  if ( _state.irq_counter != 0 )
    --_state.irq_counter;
  else _state.irq_counter= _state.irq_latch; /* Cal fer-ho ací per a
        					què vaja MegaMan 4. */
  if ( _state.irq_reload )
    {
      _state.irq_reload= NES_FALSE;
      _state.irq_counter= _state.irq_latch;
    }
  if ( _state.irq_counter == 0 )
    {
      if ( _state.irq_enabled ) _state.irq_active= NES_TRUE;
    }
  
} /* end NES_mapper_mmc3_clock_counter */


NES_Error
NES_mapper_mmc3_init (
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
       _rom->nchr > 32 ||
       _rom->mirroring == NES_SINGLE )
    return NES_BADROM;

  /* Callbacks. */
  NES_mapper_read= mmc3_read;
  NES_mapper_write= mmc3_write;
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
  
} /* end NES_mapper_mmc3_init */
