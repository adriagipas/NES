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
 *  cpu.c - Implementació del mòdul CPU. És una implementació
 *          en forma d'intèrpret.
 *          Característiques de la 6502 de la NES:
 *            - No té mode decimal.
 *            - El mode indirecte té un bug.
 *              Per a llegir el byte més alt de l'adreça final,
 *              s'incrementa l'adreça intermitga, però sols ho
 *              el byte més baix.
 *            - BRK si està en una interrupció NMI fica el bit 'b'
 *              però no s'executa.
 *
 */
/*
 *  APUNTS extret de nesdev sobre el flag B.
 *
 * - There are six and only six flags in the processor status register
 *   within the CPU. Despite what some 6502 references might appear to
 *   claim on a first reading, there is no "B flag" stored within the
 *   CPU's status register.
  * - Two interrupts (/IRQ and /NMI) and two instructions (PHP and BRK)
 *   push the flags to the stack. In the byte pushed, bit 5 is always
 *   set to 1, and bit 4 is 1 if from an instruction (PHP or BRK) or 0
 *   if from an interrupt line being pulled low (/IRQ or /NMI). This
 *   is the only time and place where the B flag actually exists: not
 *   in the status register itself, but in bit 4 of the copy that is
 *   written to the stack.
 *
 *  - Instruction         Bits 5 and 4 	Side effects after pushing 
 *  - PHP                 11 	None 
 *  - BRK                 11 	I is set to 1 
 *  - /IRQ                 10 	I is set to 1 
 *  - /NMI                 10 	I is set to 1 
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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


#define _CAT(a,b) a ## b
#define CAT(a,b) _CAT(a,b)


/* Auxiliars. */

#define READ NES_mem_read ( _regs.PC++ )

#define GET_IADDR                                                    \
   _vars.addr= NES_mem_read ( _vars.addri );                         \
   _vars.addr|= ((NESu16) NES_mem_read ( (++_vars.addri)&0xFF ))<<8;

#define GET_DATA _vars.data= NES_mem_read ( _vars.addr );
#define PUT_DATA NES_mem_write ( _vars.addr, _vars.data );

#define SET_Z_FROM(VAL)       \
   _regs.P|= (VAL == 0) << 1;

#define SET_NZ_FROM(VAL) \
                         \
   _regs.P|= VAL & 0x80; \
   SET_Z_FROM ( VAL )

#define SET_NZ_FROM_A SET_NZ_FROM(_regs.A)
#define SET_Z_FROM_A SET_Z_FROM(_regs.A)
#define SET_NZ_FROM_DATA SET_NZ_FROM(_vars.data)
#define SET_Z_FROM_DATA SET_Z_FROM(_vars.data)

#define COND(C)        				      \
  if ( (C) )        				      \
    {        					      \
      ++_vars.cc;        			      \
      _vars.addr= _regs.PC;        		      \
      _regs.PC+= _vars.desp;        		      \
      if ( (_vars.addr&0xff00) != (_regs.PC&0xff00) ) \
        ++_vars.cc;				      \
    }

#define PUSH(VAL) NES_mem_write ( 0x0100 | _regs.S--, (VAL) )
#define PULL NES_mem_read ( 0x0100 | ++_regs.S )

#define CP(VAL)                         \
   _vars.data^= 0xff;                   \
   _vars.aux= (VAL) + _vars.data;       \
   ++_vars.aux;                         \
   _vars.C= ((_vars.aux & 0x100) != 0);        \
   _vars.aux&= 0xFF;                    \
   _regs.P&= 0x7C;                      \
   SET_NZ_FROM ( _vars.aux );           \
   _regs.P|= _vars.C;

#define DE(REG)        \
   --(REG);            \
   _regs.P&= 0x7D;     \
   SET_NZ_FROM ( REG )

#define LOG_OP(OP)             \
   _regs.A OP ## = _vars.data; \
   _regs.P&= 0x7D;             \
   SET_NZ_FROM_A

#define IN(REG)        \
   ++(REG);            \
   _regs.P&= 0x7D;     \
   SET_NZ_FROM ( REG )

#define PUSH_PC                        \
   PUSH ( (NESu8) (_regs.PC >> 8) );   \
   PUSH ( (NESu8) (_regs.PC & 0xFF) );

#define LD(REG)        \
   REG= _vars.data;    \
   _regs.P&= 0x7D;     \
   SET_NZ_FROM ( REG )

#define PULL_PC                     \
   _regs.PC= PULL;                  \
   _regs.PC|= (NESu16) (PULL << 8);

#define ADD_DATA                                                         \
   _vars.aux= _regs.A;                                                   \
   _regs.A+= _vars.data;                                                 \
   _regs.A+= _regs.P&0x1;                                                \
   _regs.P&= 0x3C;                                                       \
   _regs.P|= ((~(_vars.aux^_vars.data))&(_regs.A^_vars.data)&0x80) >> 1; \
   _regs.P|= ((_regs.A & 0x100) != 0);                                   \
   _regs.A&= 0xFF;                                                       \
   SET_NZ_FROM_A

#define ST(REG) NES_mem_write ( _vars.addr, (NESu8) (REG) )

#define COPY(FROM,TO)    \
   (TO)= (NESu8) (FROM); \
   _regs.P&= 0x7D;       \
   SET_NZ_FROM ( TO )

#define LOAD_PC_INT(ADDR)                                \
   _regs.PC= NES_mem_read ( (ADDR) );                    \
   _regs.PC|= (NESu16) (NES_mem_read ( (ADDR)+1 ) << 8);

#define ISINT (_regs.P&0x04)

#define INT(ADDR,SS_FLAGS)        		\
  PUSH_PC        				\
  PUSH ( (_regs.P&0xCF) | SS_FLAGS );        	\
  _regs.P|= 0x04;        			\
  LOAD_PC_INT ( (ADDR) )

#define INT_IRQ_NMI(ADDR) INT ( ADDR,0x20 )


/* Direccionaments. */

#define iABS0                       \
   _vars.addr= READ;                \
   _vars.addr|= ((NESu16) READ)<<8;

#define iABS1 \
   iABS0      \
   GET_DATA

#define _ABSX0(REG)        			\
  iABS0        					\
  _vars.addr+= REG;

#define _ABSX1(REG)        	   \
  _ABSX0(REG)        		   \
  if ( (REG) > (_vars.addr&0xff) ) \
    ++_vars.cc;        		   \
  GET_DATA

#define iABSX0 _ABSX0(_regs.X)
#define iABSY0 _ABSX0(_regs.Y)

#define iABSX1 _ABSX1(_regs.X)
#define iABSY1 _ABSX1(_regs.Y)

#define iIND                                                               \
   _vars.data= READ;                                                       \
   _vars.addri= ((NESu16) READ)<<8;                                        \
   _vars.addr= NES_mem_read ( _vars.addri | _vars.data++ );                \
   _vars.addr|= ((NESu16) NES_mem_read ( _vars.addri | _vars.data )) << 8;

#define iINDX0             \
  _vars.addri= READ;           \
  _vars.addri+= _regs.X;   \
  _vars.addri&= 0xFF;           \
  GET_IADDR

#define iINDX1 \
   iINDX0      \
   GET_DATA

#define iINDY0           \
   _vars.addri= READ;    \
   GET_IADDR             \
   _vars.addr+= _regs.Y;

#define iINDY1        		     \
  iINDY0        		     \
  if ( _regs.Y > (_vars.addr&0xff) ) \
    ++_vars.cc;        		     \
   GET_DATA

#define iINM         \
   _vars.data= READ;

#define iNONE

#define iREL                                        \
   _vars.desp= (NESs8) NES_mem_read ( _regs.PC++ );

#define iZPG0        \
   _vars.addr= READ;

#define iZPG1 \
   iZPG0      \
   GET_DATA

#define iZPGX0           \
   iZPG0                 \
   _vars.addr+= _regs.X; \
   _vars.addr&= 0xFF;

#define iZPGX1 \
   iZPGX0      \
   GET_DATA

#define iZPGY0           \
   iZPG0                 \
   _vars.addr+= _regs.Y; \
   _vars.addr&= 0xFF;

#define iZPGY1 \
   iZPGY0      \
   GET_DATA


/* Operacions. */

#define iADC \
   ADD_DATA
   
#define iAND LOG_OP ( & )

#define iASL0                          \
   _regs.A<<= 1;                       \
   _regs.P&= 0x7C;                     \
   _regs.P|= ((_regs.A & 0x100) != 0); \
   _regs.A&= 0xFF;                     \
   SET_NZ_FROM_A

#define iASL1                            \
   GET_DATA                              \
   _regs.P&= 0x7C;                       \
   _regs.P|= ((_vars.data & 0x80) != 0); \
   _vars.data<<= 1;                      \
   SET_NZ_FROM_DATA                      \
   PUT_DATA

#define iBCC COND ( !(_regs.P & 0x01) )
#define iBCS COND ( _regs.P & 0x01 )
#define iBEQ COND ( _regs.P & 0x02 )

#define iBIT                                     \
   _regs.P&= 0x3D;                               \
   _regs.P|= _vars.data & 0xC0;                  \
   _regs.P|= ((_vars.data & _regs.A) == 0) << 1;

#define iBMI COND ( _regs.P & 0x80 )
#define iBNE COND ( !(_regs.P & 0x02) )
#define iBPL COND ( !(_regs.P & 0x80) )

#define iBRK        				\
  ++_regs.PC;        				\
  if ( !ISINT )        				\
    {        					\
      _regs.P|= 0x10;        			\
      if ( !_nmi ) { INT ( 0xFFFE, 0x30 ) }        \
    }

#define iBVC COND ( !(_regs.P & 0x40) )
#define iBVS COND ( _regs.P & 0x40 )

#define iCLC _regs.P&= 0xFE;
#define iCLD _regs.P&= 0xF7;
#define iCLI _regs.P&= 0xFB;
#define iCLV _regs.P&= 0xBF;

#define iCMP CP ( _regs.A )
#define iCPX CP ( _regs.X )
#define iCPY CP ( _regs.Y )

#define iDEC        \
   GET_DATA         \
   --_vars.data;    \
   _regs.P&= 0x7D;  \
   SET_NZ_FROM_DATA \
   PUT_DATA

#define iDEX DE ( _regs.X )
#define iDEY DE ( _regs.Y )

#define iEOR LOG_OP ( ^ )

#define iINC        \
   GET_DATA         \
   ++_vars.data;    \
   _regs.P&= 0x7D;  \
   SET_NZ_FROM_DATA \
   PUT_DATA

#define iINX IN ( _regs.X )
#define iINY IN ( _regs.Y )

#define iJMP _regs.PC= _vars.addr;

#define iJSR             \
   --_regs.PC;           \
   PUSH_PC               \
   _regs.PC= _vars.addr;

#define iLDA LD ( _regs.A )
#define iLDX LD ( _regs.X )
#define iLDY LD ( _regs.Y )

#define iLSR0                         \
   _regs.P&= 0x7C;                    \
   _regs.P|= (NESu8) (_regs.A & 0x1); \
   _regs.A>>= 1;                      \
   SET_Z_FROM_A 

#define iLSR1                  \
   GET_DATA                    \
   _regs.P&= 0x7C;             \
   _regs.P|= _vars.data & 0x1; \
   _vars.data>>= 1;            \
   SET_Z_FROM_DATA             \
   PUT_DATA

#define iNOP

#define iORA LOG_OP ( | )

#define iPHA PUSH ( (NESu8) _regs.A );
#define iPHP PUSH ( (_regs.P&0xCF) | 0x30 );

#define iPLA       \
   _regs.A= PULL;  \
   _regs.P&= 0x7D; \
   SET_NZ_FROM_A

#define iPLP _regs.P= PULL;

#define iROL0                          \
   _regs.A<<= 1;                       \
   _regs.A|= _regs.P & 0x1;            \
   _regs.P&= 0x7C;                     \
   _regs.P|= ((_regs.A & 0x100) != 0); \
   _regs.A&= 0xFF;                     \
   SET_NZ_FROM_A

#define iROL1                            \
   GET_DATA                              \
   _vars.C= _regs.P & 0x1;               \
   _regs.P&= 0x7C;                       \
   _regs.P|= ((_vars.data & 0x80) != 0); \
   _vars.data<<= 1;                      \
   _vars.data|= _vars.C;                 \
   SET_NZ_FROM_DATA                      \
   PUT_DATA

#define iROR0                         \
   _vars.C= _regs.P & 0x1;            \
   _regs.P&= 0x7C;                    \
   _regs.P|= (NESu8) (_regs.A & 0x1); \
   _regs.A>>= 1;                      \
   _regs.A|= _vars.C << 7;            \
   SET_NZ_FROM_A 

#define iROR1                  \
   GET_DATA                    \
   _vars.C= _regs.P & 0x1;     \
   _regs.P&= 0x7C;             \
   _regs.P|= _vars.data & 0x1; \
   _vars.data>>= 1;            \
   _vars.data|= _vars.C << 7;  \
   SET_NZ_FROM_DATA            \
   PUT_DATA

#define iRTI        \
   _nmi= NES_FALSE; \
   _regs.P= PULL;   \
   PULL_PC

#define iRTS   \
   PULL_PC     \
   ++_regs.PC;

#define iSBC          \
   _vars.data^= 0xff; \
   ADD_DATA

#define iSEC _regs.P|= 0x1;
#define iSED _regs.P|= 0x8;
#define iSEI _regs.P|= 0x4;

#define iSTA ST ( _regs.A );
#define iSTX ST ( _regs.X );
#define iSTY ST ( _regs.Y );

#define iTAX COPY ( _regs.A, _regs.X )
#define iTAY COPY ( _regs.A, _regs.Y )
#define iTSX COPY ( _regs.S, _regs.X )
#define iTXA COPY ( _regs.X, _regs.A )
#define iTXS _regs.S= _regs.X;
#define iTYA COPY ( _regs.Y, _regs.A )




/*********/
/* ESTAT */
/*********/

static struct
{
  
  unsigned int A;
  NESu16       PC;
  NESu8        Y;
  NESu8        X;
  NESu8        S;
  NESu8        P;
  
} _regs;


static struct
{
  
  int          cc;
  unsigned int aux;
  int          C;
  NESu16       addr;
  NESu16       addri;
  NESu8        data;
  NESs8        desp;
  
} _vars;

static NES_Bool _nmi;

static void (*_insts[256]) (void);

static NES_Warning *_warning;

static void *_udata;

static NESu8 _opcode;

static int _extra_cc;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

#define OP(OPCODE,NAME,ADDR,CLS) \
                                 \
static void                      \
CAT(NAME,CAT(_,ADDR)) (void)         \
{                                \
  _vars.cc= CLS;        	 \
  CAT(i,ADDR)                    \
  CAT(i,NAME)                    \
}
#include "op.h"
#undef OP

static void
unk (void)
{
  
  _vars.cc= 0;
  _warning ( _udata, "l'opcode '0x%02x' és desconegut", _opcode );
  
} /* end unk */







/****************************/
/* FUNCIONS PÚBLIQUES MÒDUL */
/****************************/

void
NES_cpu_NMI (void)
{
  
  _nmi= NES_TRUE;
  INT_IRQ_NMI ( 0xFFFA )
  _extra_cc+= 7;
  
} /* end NES_cpu_NMI */


void
NES_cpu_IRQ (void)
{
  
  if ( !ISINT )
    {
      INT_IRQ_NMI ( 0xFFFE )
     _extra_cc+= 7;
    }
  
} /* end NES_cpu_IRQ */


void
NES_cpu_init (
              NES_Warning *warning,
              void        *udata
              )
{
  
  int i;
  
  
  _warning= warning;
  _udata= udata;
  
  for ( i= 0; i < 256; ++i )
    _insts[i]= unk;
  
#define OP(OPCODE,NAME,ADDR,CLS)           \
  _insts[(OPCODE)]= CAT(NAME,CAT(_,ADDR));
#include "op.h"
#undef OP
  
  NES_cpu_init_state ();
  
} /* NES_cpu_init */


void
NES_cpu_init_state (void)
{
  
  _extra_cc= 0;
  
  _opcode= 0x00;
  
  _regs.A= 0;
  _regs.PC= 0x0000;
  _regs.Y= 0x00;
  _regs.X= 0x00;
  _regs.S= 0xFD;
  _regs.P= 0x34;
  
  _vars.aux= 0;
  _vars.C= 0;
  _vars.addr= 0x0000;
  _vars.addri= 0x0000;
  _vars.data= 0x00;
  _vars.desp= 0;
  
  _nmi= NES_FALSE;
  NES_mapper_reset ();
  LOAD_PC_INT ( 0xFFFC )
    
} /* end NES_cpu_init_state */


void
NES_cpu_reset (void)
{
  
  _nmi= NES_FALSE;
  _regs.S-= 3;
  _regs.P|= 0x04;
  NES_mapper_reset ();
  LOAD_PC_INT ( 0xFFFC )
  _extra_cc+= 7;
  
} /* end NES_cpu_reset */


int
NES_cpu_run (void)
{
  
  _opcode= NES_mem_read ( _regs.PC++ );
  _insts[_opcode] ();
  _vars.cc+= _extra_cc; _extra_cc= 0;
  
  return _vars.cc;
  
} /* NES_cpu_run */


NESu16
NES_cpu_decode_next_inst (
                          NES_Inst *inst
                          )
{
  return NES_cpu_decode ( _regs.PC, inst );
} /* end NES_cpu_decode_next_inst */


int
NES_cpu_save_state (
        	    FILE *f
        	    )
{

  SAVE ( _regs );
  SAVE ( _vars );
  SAVE ( _nmi );
  SAVE ( _opcode );
  SAVE ( _extra_cc );

  return 0;
  
} /* end NES_cpu_save_state */


int
NES_cpu_load_state (
        	    FILE *f
        	    )
{

  LOAD ( _regs );
  LOAD ( _vars );
  LOAD ( _nmi );
  LOAD ( _opcode );
  LOAD ( _extra_cc );
  
  return 0;
  
} /* end NES_cpu_load_state */
