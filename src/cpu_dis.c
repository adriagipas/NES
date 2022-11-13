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
 *  cpu_dis.c - Implementació de la part de debug de UCP.
 *
 *  Adrià Giménez Pastor, 2015.
 *
 */


#include <stddef.h>
#include <stdlib.h>

#include "NES.h"




/**********/
/* MACROS */
/**********/

#define _CAT(a,b) a ## b
#define CAT(a,b) _CAT(a,b)




/*********/
/* ESTAT */
/*********/

static NES_Mnemonic _inst_ids[256];

static NES_AddressMode _inst_addrms[256];




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static NESu16
get_extra (
           NESu16    addr,
           NES_Inst *inst
           )
{

  switch ( inst->id.addr_mode )
    {
      
    case NES_ABS:
    case NES_ABSX:
    case NES_ABSY:
    case NES_IND:
      inst->bytes[1]= NES_mem_read ( addr++ );
      inst->bytes[2]= NES_mem_read ( addr++ );
      inst->e.valu16= ((NESu16)inst->bytes[1])|(((NESu16)inst->bytes[2])<<8);
      inst->nbytes+= 2;
      break;

    case NES_INM:
    case NES_INDY:
    case NES_INDX:
      inst->e.valu8= inst->bytes[1]= NES_mem_read ( addr++ );
      ++(inst->nbytes);
      break;

    case NES_NONE:
      break;
      
    case NES_REL:
      inst->bytes[1]= NES_mem_read ( addr++ );
      inst->e.branch.addr= addr + (NESs8) inst->bytes[1];
      inst->e.branch.desp= (NESs8) inst->bytes[1];
      ++(inst->nbytes);
      break;

    case NES_ZPG:
    case NES_ZPGX:
    case NES_ZPGY:
      inst->bytes[1]= NES_mem_read ( addr++ );
      inst->e.valu16= (NESu16) inst->bytes[1];
      ++(inst->nbytes);
      break;
      
    }

  return addr;
  
} /* end get_extra */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NESu16
NES_cpu_decode (
                NESu16    addr,
                NES_Inst *inst
                )
{
  
  NESu8 opcode;
  
  
  opcode= inst->bytes[0]= NES_mem_read ( addr++ );
  inst->id.name= _inst_ids[opcode];
  inst->id.addr_mode= _inst_addrms[opcode];
  inst->nbytes= 1;
  
  return get_extra ( addr, inst );
  
} /* end NES_cpu_decode */


void
NES_cpu_init_decode (void)
{

  int i;
  
  
  for ( i= 0; i < 256; ++i )
    _inst_ids[i]= NES_UNK;
  for ( i= 0; i < 256; ++i )
    _inst_addrms[i]= NES_NONE;
  
#define ABS0 ABS
#define ABS1 ABS
#define ABSX0 ABSX
#define ABSX1 ABSX
#define ABSY0 ABSY
#define ABSY1 ABSY
#define INDX0 INDX
#define INDX1 INDX
#define INDY0 INDY
#define INDY1 INDY
#define ZPG0 ZPG
#define ZPG1 ZPG
#define ZPGX0 ZPGX
#define ZPGX1 ZPGX
#define ZPGY0 ZPGY
#define ZPGY1 ZPGY
#define ASL0 ASL
#define ASL1 ASL
#define ROL0 ROL
#define ROL1 ROL
#define LSR0 LSR
#define LSR1 LSR
#define ROR0 ROR
#define ROR1 ROR
#define OP(OP,NAME,ADDRM,CC)        		\
  _inst_ids[OP]= CAT(NES_,NAME);        	\
  _inst_addrms[OP]= CAT(NES_,ADDRM);
#include "op.h"
#undef OP
#undef ABS0
#undef ABS1
#undef ABSX0
#undef ABSX1
#undef ABSY0
#undef ABSY1
#undef INDX0
#undef INDX1
#undef INDY0
#undef INDY1
#undef ZPG0
#undef ZPG1
#undef ZPGX0
#undef ZPGX1
#undef ZPGY0
#undef ZPGY1
#undef ASL0
#undef ASL1
#undef ROL0
#undef ROL1
#undef LSR0
#undef LSR1
#undef ROR0
#undef ROR1

} /* end NES_cpu_init_decode */
