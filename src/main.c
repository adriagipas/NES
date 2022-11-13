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
 *  main.c - Implementació de MAIN.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "NES.h"
#include "mappers/mmc3.h"




/*************/
/* CONSTANTS */
/*************/

static const char NESSTATE[]= "NESSTATE\n";




/*********/
/* ESTAT */
/*********/

static NES_CheckSignals *_check;
static NES_CPUInst *_cpu_inst;
static unsigned int _cc1cs;
static void *_udata;
static NES_Bool _mmc3_irq;
static NES_Bool _reset;
static NES_Warning *_warning;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
reset (void)
{
  
  NES_mapper_reset ();
  NES_ppu_reset ();
  NES_apu_reset ();
  NES_joypads_reset ();
  NES_cpu_reset ();
  _reset= NES_FALSE;
  
} /* end reset */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NES_Error
NES_init (
          const NES_Rom      *rom,
          const NES_TVMode    tvmode,
          const NES_Frontend *frontend,
          NESu8               prgram[0x2000],
          void               *udata
          )
{
  
  NES_Error error;
  
  
  error= NES_mapper_init ( rom,
        		   frontend->warning,
        		   frontend->trace!=NULL?
        		   frontend->trace->mapper_changed:NULL,
        		   udata );
  if ( error != NES_NOERROR ) return error;
  NES_mem_init ( prgram, rom->trainer,
        	 frontend->warning,
        	 frontend->trace!=NULL?
        	 frontend->trace->mem_access:NULL,
        	 udata );
  NES_ppu_init ( tvmode, rom->mapper, frontend->update_screen, udata );
  NES_apu_init ( tvmode, frontend->play_frame, udata );
  NES_joypads_init ( frontend->cpb1, frontend->cpb2, udata );
  NES_cpu_init ( frontend->warning, udata );

  _warning= frontend->warning;
  _check= frontend->check;
  if ( frontend->trace != NULL )
    {
      _cpu_inst= frontend->trace->cpu_inst;
    }
  _udata= udata;
  _cc1cs= (tvmode==NES_PAL) ?
    NES_CPU_PAL_CYCLES_PER_SEC :
    NES_CPU_NTSC_CYCLES_PER_SEC;
  _cc1cs/= 100;
  
  _mmc3_irq= rom->mapper==NES_MMC3;
  
  reset ();
  
  return NES_NOERROR;
  
} /* end NES_init */


int
NES_iter (
          NES_Bool *stop
          )
{

  static int CC= 0;
  NES_Bool irq;
  int cc;
  
  
  /* NOTA: Realment la UCP i la APU són el mateix xip, si un
     s'espera l'altre també. */
  /* Executa següent instrucció. */
  irq= NES_FALSE;
  NES_dma_extra_cc= 0;
  cc= NES_cpu_run ();
  cc+= NES_dma_extra_cc;
  if ( NES_apu_clock ( (unsigned int *) &cc ) )
    irq= NES_TRUE;
  NES_ppu_clock ( cc );
  if ( _mmc3_irq && NES_mapper_mmc3_check_irq () )
    irq= NES_TRUE;
  if ( irq ) NES_cpu_IRQ ();
  CC+= cc;
  
  /* Sincronitza amb el sistema. */
  if ( CC >= _cc1cs )
    {
      CC-= _cc1cs;
      _check ( &_reset, stop, _udata );
      if ( _reset ) reset ();
    }
  
  return cc;
  
} /* end NES_iter */


int
NES_load_state (
        	FILE *f
        	)
{
  
  static char buf[sizeof(NESSTATE)];

  
  _reset= NES_FALSE;
  
  /* NESSTATE. */
  if ( fread ( buf, sizeof(NESSTATE)-1, 1, f ) != 1 ) goto error;
  buf[sizeof(NESSTATE)-1]= '\0';
  if ( strcmp ( buf, NESSTATE ) ) goto error;

  /* Carrega. */
  if ( NES_mapper_load_state ( f ) != 0 ) goto error;
  if ( NES_mem_load_state ( f ) != 0 ) goto error;
  if ( NES_ppu_load_state ( f ) != 0 ) goto error;
  if ( NES_joypads_load_state ( f ) != 0 ) goto error;
  if ( NES_apu_load_state ( f ) != 0 ) goto error;
  if ( NES_cpu_load_state ( f ) != 0 ) goto error;
  
  return 0;

  error:
  _warning ( _udata,
             "error al carregar l'estat del simulador des d'un fitxer" );
  NES_mapper_init_state ();
  NES_mem_init_state ();
  NES_ppu_init_state ();
  NES_joypads_init_state ();
  NES_apu_init_state ();
  NES_cpu_init_state (); /* <-- Al final a propòsit. */
  return -1;
  
} /* end NES_load_state */


void
NES_loop (void)
{
  
  NES_Bool qstop, irq;
  int CC;
  unsigned int ncycles_clock;
  

  _reset= qstop= NES_FALSE;
  ncycles_clock= 0;
  for (;;)
    {
      /* NOTA: Realment la UCP i la APU són el mateix xip, si un
         s'espera l'altre també. */
      /* Executa següent instrucció. */
      irq= NES_FALSE;
      NES_dma_extra_cc= 0;
      CC= NES_cpu_run ();
      CC+= NES_dma_extra_cc;
      if ( NES_apu_clock ( (unsigned int *) &CC ) )
        irq= NES_TRUE;
      NES_ppu_clock ( CC );
      if ( _mmc3_irq && NES_mapper_mmc3_check_irq () )
        irq= NES_TRUE;
      if ( irq ) NES_cpu_IRQ ();
      ncycles_clock+= CC;
      
      /* Sincronitza amb el sistema. */
      if ( ncycles_clock >= _cc1cs )
        {
          ncycles_clock-= _cc1cs;
          _check ( &_reset, &qstop, _udata );
          if ( _reset ) reset ();
          if ( qstop ) return;
        }
      
    }
  
} /* end NES_loop */


int
NES_save_state (
        	FILE *f
        	)
{

  if ( fwrite ( NESSTATE, sizeof(NESSTATE)-1, 1, f ) != 1 ) return -1;
  if ( NES_mapper_save_state ( f ) != 0 ) return -1;
  if ( NES_mem_save_state ( f ) != 0 ) return -1;
  if ( NES_ppu_save_state ( f ) != 0 ) return -1;
  if ( NES_joypads_save_state ( f ) != 0 ) return -1;
  if ( NES_apu_save_state ( f ) != 0 ) return -1;
  if ( NES_cpu_save_state ( f ) != 0 ) return -1;
  
  return 0;
  
} /* end NES_save_state */


int
NES_trace (void)
{
  
  int CC;
  NESu16 addr;
  NES_Bool irq;
  NES_Inst inst;

  
  if ( _cpu_inst != NULL )
    {
      addr= NES_cpu_decode_next_inst ( &inst );
      _cpu_inst ( &inst, addr, _udata );
    }
  NES_mapper_set_mode_trace ( NES_TRUE );
  NES_mem_set_mode_trace ( NES_TRUE );
  irq= NES_FALSE;
  NES_dma_extra_cc= 0;
  CC= NES_cpu_run ();
  CC+= NES_dma_extra_cc;
  if ( NES_apu_clock ( (unsigned int *) &CC ) )
    irq= NES_TRUE;
  NES_ppu_clock ( CC );
  if ( _mmc3_irq && NES_mapper_mmc3_check_irq () )
    irq= NES_TRUE;
  if ( irq ) NES_cpu_IRQ ();
  NES_mem_set_mode_trace ( NES_FALSE );
  NES_mapper_set_mode_trace ( NES_FALSE );
  
  return CC;
  
} /* end NES_trace */
