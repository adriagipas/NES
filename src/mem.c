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
 *  memory.c - Implementació del mòdul MEM.
 *
 */


#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NES.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;

#define WARNING(MSG)        			\
  fprintf ( stderr, "Warning: %s\n", (MSG) )




/*********/
/* ESTAT */
/*********/

/* Callbacks. */
static NES_Warning *_warning;
static NES_MemAccess *_mem_access;
static void *_udata;

/* Memòria. */
static NESu8 _ram[0x800];

/* Trainer. */
static const NESu8 *_trdata;
  
/* Memòria RAM del cartutx. */
static NESu8 *_prgram;

/* Funcions. */
static NESu8 (*_mem_read) (const NESu16 addr);
static void (*_mem_write) (const NESu16 addr,const NESu8 data);




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static NESu8
mem_read (
          const NESu16 addr
          )
{
  
  if ( addr < 0x2000 )
    return _ram[addr & 0x7FF];
  
  else if ( addr < 0x4000 )
    {
      switch ( addr & 0x7 )
        {
        case 0: return 0x00;
        case 1: return 0x00;
        case 2: return NES_ppu_status ();
        case 3: return 0x00;
        case 4: return NES_ppu_SPRAM_read ();
        case 5: return 0x00;
        case 6: return 0x00;
        case 7: return NES_ppu_read ();
        default: return 0x00;
        }
    }
  
  else if ( addr < 0x8000 )
    {
      if ( addr < 0x4018 )
        {
          switch ( addr )
            {
            case 0x4000: return 0x00;
            case 0x4001: return 0x00;
            case 0x4002: return 0x00;
            case 0x4003: return 0x00;
            case 0x4004: return 0x00;
            case 0x4005: return 0x00;
            case 0x4006: return 0x00;
            case 0x4007: return 0x00;
            case 0x4008: return 0x00;
            case 0x4009: return 0x00;
            case 0x400A: return 0x00;
            case 0x400B: return 0x00;
            case 0x400C: return 0x00;
            case 0x400D: return 0x00;
            case 0x400E: return 0x00;
            case 0x400F: return 0x00;
            case 0x4010: return 0x00;
            case 0x4011: return 0x00;
            case 0x4012: return 0x00;
            case 0x4013: return 0x00;
            case 0x4014: return 0x00;
            case 0x4015: return NES_apu_CSR ();
            case 0x4016: return NES_joypads_pad1_read ();
            case 0x4017: return NES_joypads_pad2_read ();
            default: return 0x00;
            }
        }
      else if ( addr < 0x6000 )
        {
          WARNING ( "MEM: No està implementat"
        	    " el suport de Expansion ROM " );
          return 0x00;
          /*
          if ( _mmc->erom != NULL )
            return _mmc->erom[addr-0x4018];
          else
            {
              _warning ( "Aquest mapper no té Expansion ROM" );
              return 0x00;
            }
          */
        }
      else
        {
          if ( _prgram == NULL )
            {
              _warning ( _udata, "Aquest mapper no té PRGRAM" );
              return 0x00;
            }
          else return _prgram[addr&0x1FFF];
        }
    }
  
  else return NES_mapper_read ( addr&0x7FFF );
  
} /* end mem_read */


static void
mem_write (
           const NESu16 addr,
           const NESu8  byte
           )
{
  
  if ( addr < 0x2000 )
    _ram[addr & 0x7FF]= byte;
  
  else if ( addr < 0x4000 )
    {
      switch ( addr & 0x7 )
        {
        case 0: NES_ppu_CR1 ( byte ); break;
        case 1: NES_ppu_CR2 ( byte ); break;
        case 2: break;
        case 3: NES_ppu_SPRAM_set_offset ( byte ); break;
        case 4: NES_ppu_SPRAM_write ( byte ); break;
        case 5: NES_ppu_scrolling ( byte ); break;
        case 6: NES_ppu_set_addr ( byte ); break;
        case 7: NES_ppu_write ( byte ); break;
        }
    }
  
  else if ( addr < 0x8000 )
    {
      if ( addr < 0x4018 )
        {
          switch ( addr )
            {
            case 0x4000: NES_apu_pulse1CR ( byte ); break;
            case 0x4001: NES_apu_pulse1RCR ( byte ); break;
            case 0x4002: NES_apu_pulse1FTR ( byte ); break;
            case 0x4003: NES_apu_pulse1CTR ( byte ); break;
            case 0x4004: NES_apu_pulse2CR ( byte ); break;
            case 0x4005: NES_apu_pulse2RCR ( byte ); break;
            case 0x4006: NES_apu_pulse2FTR ( byte ); break;
            case 0x4007: NES_apu_pulse2CTR ( byte ); break;
            case 0x4008: NES_apu_triangleCR1 ( byte ); break;
            case 0x4009: break;
            case 0x400A: NES_apu_triangleFR1 ( byte ); break;
            case 0x400B: NES_apu_triangleFR2 ( byte ); break;
            case 0x400C: NES_apu_noiseCR ( byte ); break;
            case 0x400D: break;
            case 0x400E: NES_apu_noiseFR1 ( byte ); break;
            case 0x400F: NES_apu_noiseFR2 ( byte ); break;
            case 0x4010: NES_apu_dmCR ( byte ); break;
            case 0x4011: NES_apu_dmDAR ( byte ); break;
            case 0x4012: NES_apu_dmAR ( byte ); break;
            case 0x4013: NES_apu_dmLR ( byte ); break;
            case 0x4014: NES_ppu_DMA ( byte ); break;
            case 0x4015: NES_apu_control ( byte ); break;
            case 0x4016: NES_joypads_strobe ( byte ); break;
            case 0x4017:
              NES_joypads_EPL ( byte );
              NES_apu_conf_fseq ( byte );
              break;
            }
        }
      else if ( addr < 0x6000 )
        {
          WARNING ( "MEM: No hi ha suport per a Expansion ROM" );
          /*
          if ( _mmc->erom != NULL )
            _mmc->erom[addr-0x4018]= byte;
          else
            _warning ( "Aquest mapper no té Expansion ROM" );
          */
        }
      else
        {
          if ( _prgram == NULL )
            _warning ( _udata, "Aquest mapper no té PRGRAM" );
          else _prgram[addr&0x1FFF]= byte;
        }
    }
  
  else NES_mapper_write ( addr&0x7FFF, byte );
  
} /* end mem_write */


static NESu8
mem_read_trace (
        	const NESu16 addr
        	)
{

  NESu8 data;


  data= mem_read ( addr );
  _mem_access ( _udata, NES_READ, addr, data );

  return data;
  
} /* end mem_read_trace */


static void
mem_write_trace (
        	 const NESu16 addr,
        	 const NESu8  byte
        	 )
{

  mem_write ( addr, byte );
  _mem_access ( _udata, NES_WRITE, addr, byte );
  
} /* end mem_write_trace */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
NES_mem_init (
              NESu8          prgram[0x2000],
              const NESu8   *trdata,
              NES_Warning   *warning,
              NES_MemAccess *mem_access,
              void          *udata
              )
{
  
  /* Callbacks. */
  _warning= warning;
  _udata= udata;
  _mem_access= mem_access;

  /* Funcions. */
  _mem_read= mem_read;
  _mem_write= mem_write;
  
  /* Altres. */
  _prgram= prgram;
  _trdata= trdata;
  
  NES_mem_init_state ();
  
} /* end NES_mem_init  */


void
NES_mem_init_state (void)
{
  
  int i;

  
  memset ( &(_ram[0]), 0xFF, 0x800 );
  _ram[0x0008]= 0xF7;
  _ram[0x0009]= 0xEF;
  _ram[0x000A]= 0xDF;
  _ram[0x000F]= 0xBF;
  if ( _trdata != NULL )
    for ( i= 0; i < 512; ++i )
      _prgram[i]= _trdata[i];
  
} /* end NES_mem_init_state */


NESu8
NES_mem_read (
              NESu16 addr
              )
{
  return _mem_read ( addr );
} /* end NES_mem_read */


void
NES_mem_write (
               NESu16 addr,
               NESu8  byte
               )
{
  _mem_write ( addr, byte );
} /* end NES_mem_write */


void
NES_mem_set_mode_trace (
        		const NES_Bool val
        		)
{

  if ( _mem_access != NULL )
    {
      if ( val )
        {
          _mem_read= mem_read_trace;
          _mem_write= mem_write_trace;
        }
      else
        {
          _mem_read= mem_read;
          _mem_write= mem_write;
        }
    }
  
} /* end NES_mem_set_mode_trace */


int
NES_mem_save_state (
        	    FILE *f
        	    )
{

  bool aux;
  size_t ret;
  
  
  SAVE ( _ram );
  aux= _trdata!=NULL;
  SAVE ( aux );
  ret= fwrite ( _prgram, 0x2000, 1, f );
  if ( ret != 1 ) return -1;
  
  return 0;
  
} /* end NES_mem_save_state */


int
NES_mem_load_state (
        	    FILE *f
        	    )
{

  bool aux;
  size_t ret;
  
  
  LOAD ( _ram );
  LOAD ( aux );
  CHECK ( aux == (_trdata!=NULL) );
  ret= fread ( _prgram, 0x2000, 1, f );
  if ( ret != 1 ) return -1;
  
  return 0;
  
} /* end NES_mem_load_state */
