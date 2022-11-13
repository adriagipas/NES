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
 *  joypads.c - Implementació del mòdul JOYPADS.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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




/*********/
/* ESTAT */
/*********/

/* Funcions per a vore l'estat dels mandos. */
static NES_CheckPadButton *_cpb1;
static NES_CheckPadButton *_cpb2;
static void *_udata;

/* Estat lectura botons. */
static NES_Bool _strobe;
static int _shift1;
static int _shift2;




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
NES_joypads_init (
        	  NES_CheckPadButton *cpb1,
        	  NES_CheckPadButton *cpb2,
        	  void               *udata
        	  )
{
  
  _cpb1= cpb1;
  _cpb2= cpb2;
  _udata= udata;
  NES_joypads_init_state ();
  
} /* end NES_joypads_init */


void
NES_joypads_init_state (void)
{
  NES_joypads_reset ();
} /* end NES_joypads_init_state */


NESu8
NES_joypads_pad1_read ()
{
  
  NESu8 ret;
  
  
  if ( !_strobe )
    {
      fprintf ( stderr, "PAD1: Half-strobing not implemented\n" );
      return 0x00;
    }
  
  if ( _shift1 < 8 )
    ret= _cpb1 ( _shift1, _udata ) ? 0x01 : 0x00;
  else if ( _shift1 == 19 ) ret= 0x1;
  else                      ret= 0x00;
  
  if (++_shift1 == 24 ) _shift1= 0;
  
  return ret;
  
} /* end NES_joypads_pad1_read */


NESu8
NES_joypads_pad2_read ()
{
  
  NESu8 ret;
  
  
  if ( !_strobe )
    {
      fprintf ( stderr, "PAD2: Half-strobing not implemented\n" );
      return 0x00;
    }
  
  if ( _shift2 < 8 )
    ret= _cpb2 ( _shift2, _udata ) ? 0x01 : 0x00;
  else if ( _shift2 == 18 ) ret= 0x1;
  else                      ret= 0x00;
  
  if (++_shift2 == 24 ) _shift2= 0;
  
  return ret;
  
} /* end NES_joypads_pad2_read */


void
NES_joypads_strobe (
        	    NESu8 data
        	    )
{
  
  data&=0x01;
  if ( data == 1 )
    {
      _shift1= 0;
      _shift2= 0;
      _strobe= NES_FALSE;
    }
  else if ( data == 0 )
    _strobe= NES_TRUE;
  
} /* end NES_joypads_strobe */


void
NES_joypads_EPL (
        	 NESu8 data
        	 )
{
  /* DE MOMENT NO ES SUPORTA RES RELACIONAT AMB EL EXPANSION PORT
     LATCH */
} /* end NES_joypads_EPL */


void
NES_joypads_reset ()
{
  
  _strobe= NES_TRUE;
  _shift1= 0;
  _shift2= 0;
  
} /* end NES_joypads_reset */


int
NES_joypads_save_state (
        		FILE *f
        		)
{

  SAVE ( _strobe );
  SAVE ( _shift1 );
  SAVE ( _shift2 );

  return 0;
  
} /* end NES_joypads_save_state */


int
NES_joypads_load_state (
        		FILE *f
        		)
{

  LOAD ( _strobe );
  LOAD ( _shift1 );
  CHECK ( _shift1 >= 0 && _shift1 < 24 );
  LOAD ( _shift2 );
  CHECK ( _shift2 >= 0 && _shift2 < 24 );
  
  return 0;
  
} /* end NES_joypads_load_state */
