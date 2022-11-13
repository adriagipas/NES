/*
 * Copyright 2010-2022 Adrià Giménez Pastor.
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
 *  ppu.c - Implementació del mòdul PPU. Per qüestions d'eficiència la
 *          unitat mínima de renderitzat és la línia, es a dir quan es
 *          tenen els cicles necessaris per a una línia es renderitza
 *          de colp. Per tant els canvis a meitat de línia no es tenen
 *          en compter.
 *
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "NES.h"
#include "mappers/mmc2.h"
#include "mappers/mmc3.h"




/**********/
/* MACROS */
/**********/

#define SAVE(VAR)                                               \
  if ( fwrite ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define LOAD(VAR)                                               \
  if ( fread ( &(VAR), sizeof(VAR), 1, f ) != 1 ) return -1

#define CHECK(COND)                             \
  if ( !(COND) ) return -1;


#define BACK 0
#define FORE 1


#define CALC_DIFF (sline+((((int) (*p^0xFF))|0x100))+1)&0x1FF


#define CALC_NT(COUNTS)        			\
  (0x2000 | ((((COUNTS).V<<1)|(COUNTS).H)<<10))


#define CHECK_S0C (_status&0x40)


#define CLOCK              \
  clock ()


#define COLOR_PF(COLOR) ((_palettes[(COLOR)]&_aux.pbitmap)|_aux.emph)
#define COLOR_OBJ(COLOR) ((_palettes[0x10|(COLOR)]&_aux.pbitmap)|_aux.emph)


#define READ_NT_PF_COUNTERS(NT,COUNTS)        			\
  (NES_mapper_vram_read ( (NT)|((COUNTS).VT<<5)|(COUNTS).HT ) )


#define READ_NT_PF(NT) READ_NT_PF_COUNTERS(NT,_counters)


#define GET_ADDR         \
  ((_counters.HT |       \
    (_counters.VT<<5) |  \
    (_counters.H<<10) |  \
    (_counters.V<<11) |  \
    (_counters.FV<<12)))


#define GET_ATR_BYTE(NT)        					\
  (NES_mapper_vram_read ((NT)|0x3C0|((_counters.VT&0x1C)<<1)|(_counters.HT>>2)))


#define GET_ATR(NT)                                             \
  (((GET_ATR_BYTE(NT)>>(((_counters.VT&0x2)|                    \
                         ((_counters.HT&0x2)>>1))<<1))&0x3)<<2)


#define GET_P0_COUNTERS(PAR,COUNTS)        			\
  (NES_mapper_vram_read ( _regs.S|((PAR)<<4)|(COUNTS).FV ))


#define GET_P0(PAR) GET_P0_COUNTERS(PAR,_counters)


#define GET_P1_COUNTERS(PAR,COUNTS)        			\
  (NES_mapper_vram_read ( _regs.S|((PAR)<<4)|0x8|(COUNTS).FV ))


#define GET_P1(PAR) GET_P1_COUNTERS(PAR,_counters)


#define INSERT_STM        					\
  *(t++)= p[1];        						\
  *(t++)= p[3];        						\
  *(t++)= p[2]&0xEF;        					\
  *(t++)= p[2]&0x80?(diff^(_render.size16?0xF:0x7)):diff;        \
  ++_render.scounter


#define MIN(A,B) (((A)<(B))?(A):(B))

#define MMC2_SAVE_STATE(IND)        				\
  do {        							\
    if ( _mmc2.enabled )        				\
      NES_mapper_mmc2_save_state ( &(_mmc2.state ## IND) );        \
  } while(0)
#define MMC2_LOAD_STATE(IND)        				\
  do {        							\
    if ( _mmc2.enabled )        				\
      NES_mapper_mmc2_load_state ( &(_mmc2.state ## IND) );        \
  } while(0)




/*********/
/* ESTAT */
/*********/

/* Per a indicar que la PPU ja està en marxa. */
static int _initialised= 0;

/* Funció per a updatejar la pantalla. */
static NES_UpdateScreen *_update_screen;
static void *_udata;


/* Mode televisió. */
static NES_TVMode _tvmode;

/* Registres interns, seguint la nomenclatura de '2C02 technical
 *  reference.txt'.
 */
static struct
{
  
  NESu16   S;
  unsigned V,H;
  unsigned FV,FH;
  unsigned VT,HT;
  NESu8    obj_ptr;      /* Punter a la memòria d'objectes. */
  int      flip_flop;    /* $2005/6 flip-flop. */
  
} _regs;


/* Variables auxiliars. */
static struct
{
  
  NES_Bool inc1;            /* Increment l'adreça en 1. */
  int      obj_pt;          /* 'Pattern Table' dels
        		       objectes. */
  NES_Bool obj_size16;      /* Els objectes són d'altura
        		       16. */
  NES_Bool NMI;             /* Emiteix interrupció NMI. */
  NESu8    pbitmap;         /* Mascara per a desactivar el
        		       color. */
  NES_Bool pf_clipping;     /* 'Clipping' del fons. */
  NES_Bool obj_clipping;    /* 'Clipping' dels objectes. */
  NES_Bool enable_pf;
  NES_Bool enable_obj;
  int      emph;            /* Emfasis del color. */
  
} _aux;


/* Comptadors interns. */
static struct counters
{
  
  unsigned FV;
  unsigned V,H;
  unsigned VT,HT;
  
} _counters;


/* Estat de la PPU. */
static NESu8 _status;


/* Buffer intern per a llegir. */
static NESu8 _buffer;


/* Estat per a 'renderitzar'. */
static struct
{
  
  int       sline;          /* Següent línia a dibuixar. */
  int       sline_step;     /* Dividix el renderitzat d'una línia en 3
        		       pasos, lectura PF, renderitzat PF i
        		       resta. */
  int       fb[61440];      /* 'Frame Buffer'. */
  int      *p;              /* Punter al següent píxel a dibuixar. */
  NESu16    p0,p1;          /* Registres 'Pattern Tables'. */
  NESu8     atr[2];         /* Atributs. */
  int       scounter;       /* Comptador d'sprites. */
  NES_Bool  size16;         /* Els objectes en STM són d'altura 16. */
  NESu8     stm[32];        /* 'Sprite Temporary Memory'. */
  NESu8     pf[256];        /* 'Playfield'. */
  int       map[16];        /* Mapeja els atributs. */
  NESu8     obj[256];       /* Línia dels objectes. */
  NESu8     objpri[256];    /* Prioritat dels objectes. */
  int       s0c_pos[8];     /* Posicions de la línia on hi han píxels
        		       no transparents del sprite0. */
  int       s0c_N;          /* Número de píxels no transparents del
        		       sprite0. */
  int       s0c_flag;       /* 0 si no està el sprite 0 en aquesta
        		       línia. */
  int       current_pos;    /* Indica el número de píxels dibuixats en
        		       l'actual línia. Sols s'utilitza quan
        		       estem calculant la sol·lissió del
        		       sprite 0. Per a que funcione totes les
        		       'scanline' previes a una scanline
        		       visible tenen que fixar-lo a 0. */
  NES_Bool NMI_occurred;    /* Controla quan s'ha de fer una una
        		       interrupció NMI. Es fica a 1 cert
        		       durant el VBlank. */
  
} _render;


/* Sincronització amb la UCP. */
static struct
{

  int isNTSC;          /* És NTSC. */
  int pputocc;         /* Número de cicles que requereix un cicle de
        		  PPU .*/
  int ccs;             /* Cicles de UCP pendents de ser processats. */
  int ccs_to_end;      /* Cicles de rellotge que falten per a acabar
        		  el frame. */
  int ccperline;       /* Cicles de rellontge per línia. */
  int ccperframe;     /* Cicles de rellotge en un frame normal. */
  int ccpervblank;    /* Cicles de rellotge en les 20 primeres línies. */
  int oddframe;        /* Indicador de frame par/impar. */
  int ccperline_s0;    /* Cicles necessaris per al pas 0. */
  int ccperline_s1;    /* Cicles totals necessaris per al pas 1. */
  int twoCC;          /* Dos cicles de UCP en cicles de rellotge. */
  int cputocc;        /* Passa de cicles de UCP a ppu. */
  
} _timing;


/* Estat especial per al mapper MMC3. */
/*
 * APUNTS DE NESDEV:
 *
 * - If the BG uses $0000, and the sprites use $1000, then the IRQ
 *   will occur after PPU cycle 260 (as in, a little after the visible
 *   part of the target scanline has ended).
 * - If the BG uses $1000, and the sprites use $0000, then the IRQ
 *   will occur after PPU cycle 324 of the previous scanline (as in,
 *   right before the target scanline is about to be drawn).
 * - When using 8x16 sprites: When there are less than 8 sprites on a
 *   scanline, the PPU makes a dummy fetch to tile $FF for each
 *   leftover sprite. In 8x16 sprite mode, tile $FF corresponds to the
 *   right pattern table ($1000).
 * - The counter will not work properly unless you use different
 *   pattern tables for background and sprite data. The standard
 *   configuration is to use PPU $0000-$0FFF for background tiles and
 *   $1000-$1FFF for sprite tiles, whether 8x8 or 8x16.
 * - The counter is clocked on each rising edge of PPU A12, no matter
 *   what caused it, so it is possible to (intentionally or not) clock
 *   the counter by writing to $2006.
 *
 * LA MEUA APROXIMACIÓ:
 *
 *  - Ignorar la configuració de les pattern tables i sprites.
 *  - Cridar al rellotge al final de la secció 1 de cada línia (approx
 *    cicle 256)
 *  - Creuar els dits.
 *
 */
static struct
{

  int      ccs_to_first_clock;
  int      ccs_to_end;
  NES_Bool enabled;
  
} _mmc3;

static struct
{

  NES_mmc2_state_t state0,state1;
  NES_Bool         enabled;
  
} _mmc2;

/* Memòria. */
static NESu8 _palettes[32];
static NESu8 _obj_ram[256];




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
reset_aux (void)
{
  
  _aux.NMI= NES_FALSE;
  _aux.obj_size16= NES_FALSE;
  _aux.obj_pt= 0x0000;
  _aux.inc1= NES_TRUE;
  _aux.emph= 0;
  _aux.enable_obj= NES_FALSE;
  _aux.enable_pf= NES_FALSE;
  _aux.obj_clipping= NES_TRUE;
  _aux.pf_clipping= NES_TRUE;
  _aux.pbitmap= 0x3F;
  
} /* end reset_aux */


static void
reset_regs (void)
{
  
  _regs.S= 0x0000;
  _regs.FV= _regs.VT= _regs.FH= _regs.HT= 0;
  _regs.flip_flop= 0;
  
} /* end reset_regs */


static void
update_counters (void)
{
  
  _counters.FV= _regs.FV;
  _counters.V= _regs.V;
  _counters.H= _regs.H;
  _counters.VT= _regs.VT;
  _counters.HT= _regs.HT;
  
} /* end update_counters */


static void
init_counters (void)
{
  
  _counters.FV= 0;
  _counters.V= 0;
  _counters.H= 0;
  _counters.VT= 0;
  _counters.HT= 0;
  
} /* end init_counters */


static void
init_mem (void)
{
  
  memset ( &(_palettes[0]), 0, 32 );
  memset ( &(_obj_ram[0]), 0, 256 );
  
} /* end init_mem */


static void
init_regs (void)
{
  
  _regs.S= 0x0000;
  _regs.V= _regs.H= 0;
  _regs.FV= _regs.FH= 0;
  _regs.VT= _regs.HT= 0;
  _regs.flip_flop= 0;
  _regs.obj_ptr= 0x00;
  
} /* end init_regs */


static void
init_render (void)
{
  
  int i;
  
  
  _render.p0= _render.p1= 0;
  _render.atr[0]= _render.atr[1]= 0;
  _render.sline= -1;
  for ( i= 0; i < 61440; ++i )
    _render.fb[i]= 0;
  _render.scounter= 0;
  _render.p= &(_render.fb[0]);
  _render.size16= NES_FALSE;
  memset ( &(_render.stm[0]), 0, 32 );
  memset ( &(_render.pf[0]), 0, 256 );
  for ( i= 0; i < 8; ++i )
    _render.map[i]= 0;
  for ( ; i < 16; ++i )
    _render.map[i]= 1;
  memset ( _render.obj, 0, 256 );
  memset ( _render.objpri, BACK, 256 );
  memset ( _render.s0c_pos, 0, sizeof(int)*8 );
  _render.s0c_N= 0;
  _render.s0c_flag= 0;
  _render.current_pos= 0;
  _render.NMI_occurred= NES_TRUE;
  _render.sline_step= 0;
  
} /* end init_render */


static void
init_timing (void)
{
  
  _timing.isNTSC= (_tvmode==NES_NTSC);
  _timing.pputocc= (_tvmode==NES_PAL)?5:4;
  _timing.cputocc= (_tvmode==NES_PAL)?16:12;
  _timing.twoCC= _timing.cputocc*2;
  _timing.ccs= 0;
  _timing.oddframe= 0;
  _timing.ccperline= 341*_timing.pputocc;
  if ( _tvmode == NES_NTSC )
    {
      _timing.ccperframe= _timing.ccperline*262;
      _timing.ccpervblank= _timing.ccperline*20;
    }
  else
    {
      _timing.ccperframe= _timing.ccperline*312;
      _timing.ccpervblank= _timing.ccperline*70;
    }
  _timing.ccs_to_end= _timing.ccperframe;
  _timing.ccperline_s0= 240*_timing.pputocc;
  _timing.ccperline_s1= 256*_timing.pputocc;
  
} /* end init_timing */


static void
inc_addr (void)
{
  
  if ( _aux.inc1 )
    {
      if ( ++_counters.HT == 32 )
        {
          _counters.HT= 0;
          if ( ++_counters.VT == 32 )
            {
              _counters.VT= 0;
              if ( _counters.H == 0 )
                _counters.H= 1;
              else
                {
                  _counters.H= 0;
                  if ( _counters.V == 0 )
                    _counters.V= 1;
                  else
                    {
                      _counters.V= 0;
                      if ( ++_counters.FV == 8 )
                        _counters.FV= 0;
                    }
                }
            }
        }
    }
  else
    {
      if ( ++_counters.VT == 32 )
        {
          _counters.VT= 0;
          if ( _counters.H == 0 )
            _counters.H= 1;
          else
            {
              _counters.H= 0;
              if ( _counters.V == 0 )
                _counters.V= 1;
              else
                {
                  _counters.V= 0;
                  if ( ++_counters.FV == 8 )
                    _counters.FV= 0;
                }
            }
        }
    }
  
} /* end inc_addr */


static void
inc_vscroll (void)
{
  
  if ( ++_counters.FV == 8 )
    {
      _counters.FV= 0;
      if ( ++_counters.VT == 30 )
        {
          _counters.VT= 0;
          _counters.V^= 0x1;
        }
      else if ( _counters.VT == 32 )
        _counters.VT= 0;
    }
  
} /* end inc_vscroll */


static void
init_pf (void)
{
  
  NESu16 NT;
  NESu8 PAR;
  
  
  if ( !_aux.enable_pf ) return;
  
  /* Actualitza comptadors. */
  _counters.H= _regs.H;
  _counters.HT= _regs.HT;
  
  /* Primer tile. */
  NT= CALC_NT ( _counters );
  PAR= READ_NT_PF ( NT );
  _render.atr[0]= GET_ATR ( NT );
  _render.p0= ((NESu16) GET_P0 ( PAR ))<<8;
  _render.p1= ((NESu16) GET_P1 ( PAR ))<<8;
  if ( ++_counters.HT == 32 )
    {
      _counters.HT= 0;
      _counters.H^= 0x1;
    }
  
  /* Segon tile. */
  NT= CALC_NT ( _counters );
  PAR= READ_NT_PF ( NT );
  _render.atr[1]= GET_ATR ( NT );
  _render.p0|= GET_P0 ( PAR );
  _render.p1|= GET_P1 ( PAR );
  if ( ++_counters.HT == 32 )
    {
      _counters.HT= 0;
      _counters.H^= 0x1;
    }
  
} /* end init_pf */


/* 'In-range object evaluation' de la pròxima línia. Es suposa que
 * '_render.scounter' és 0.
 */
static void
render_obj_ioe (
                int sline
                )
{
  
  int diff2, diff;
  NESu8 *t, *p, *end;
  
  
  _render.scounter= 0;
  _render.size16= _aux.obj_size16;
  diff2= _render.size16 ? 16 : 8;
  t= &(_render.stm[0]);
  p= &(_obj_ram[0]);
  end= p + 256;
  diff= CALC_DIFF;
  if ( (_render.s0c_flag= (diff < diff2)) ) { INSERT_STM; }
  for ( p+= 4; _render.scounter < 8 && p != end; p+= 4 )
    {
      diff= CALC_DIFF;
      if ( diff < diff2 ) { INSERT_STM; }
    }
  if ( _render.scounter == 8 ) _status|= 0x20;
  
} /* end render_obj_ioe */


/* Apunta les posicions on es poden produïr col·lisions. Si està
   estarà en la posició 0 del stm. */
static void
render_obj_s0c (void)
{
  
  NESu8 *p, b0, b1;
  int aux, x, end;
  NESu16 pt;
  

  /* NOTA!!! Per a MMC2, si l'ordre de lectura de VRAM és primer
     sprite 0 no cal que emule la lectura de la resta de sprites. */
  
  _render.s0c_N= 0;
  p= &(_render.stm[0]);
  if ( _render.size16 )
    {
      pt= (*p&0x1)!=0 ? 0x1000 : 0x0000;
      aux= ((*p&0xfe)<<4) | ((p[3]&0x8)<<1) | (p[3]&0x7);
    }
  else
    {
      pt= _aux.obj_pt;
      aux= (*p<<4) | p[3];
    }
  b0= NES_mapper_vram_read ( pt|aux );
  b1= NES_mapper_vram_read ( pt|aux|0x8 );
  x= p[1];
  end= MIN(255,x+8); /* En la posició x=255 no es pot produïr una col·lissió. */
  if ( p[2]&0x40 )
    {
      if ( _aux.obj_clipping )
        for ( ; x < end && x < 8; ++x )
          { b1>>= 1; b0>>= 1; }
      for ( ; x < end; ++x )
        {
          if ( ((b1&0x1)<<1)|(b0&0x1) )
            _render.s0c_pos[_render.s0c_N++]= x;
          b1>>= 1; b0>>= 1;
        }
    }
  else
    {
      if ( _aux.obj_clipping )
        for ( ; x < end && x < 8; ++x )
          { b1<<= 1; b0<<= 1; }
      for ( ; x < end; ++x )
        {
          if ( ((b1&0x80)>>6)|((b0&0x80)>>7) )
            _render.s0c_pos[_render.s0c_N++]= x;
          b1<<= 1; b0<<= 1;
        }
    }
  
} /* end render_obj_s0c */


static void
render_obj (void)
{
  
  NESu8 *p, b0, b1, colorh, pri, colorl;
  int pt, aux, x, end, i;
  NESu8 b1s[8],b0s[8];

  
  if ( !_aux.enable_obj ) return;
  
  /* Renderitza línia actual. */
  MMC2_SAVE_STATE ( 0 );

  /* NOTA!!! L'ordre de lectura de VRAM és important per al mapper
     MMC2. Aparentment (no estic 100% ssegur) el primer en llegir-se
     és sempre el sprite 0. Però com per a renderitzar gaste
     l'algorisme del pintor, el que faig es guardar-me abans en uns
     buffer els bytes de VRAM necessaris. */
  p= &(_render.stm[0]);
  for ( i= 0; i < _render.scounter; ++i )
    {
      if ( _render.size16 )
        {
          pt= (*p&0x1)!=0 ? 0x1000 : 0x0000;
          aux= ((*p&0xfe)<<4) | ((p[3]&0x8)<<1) | (p[3]&0x7);
        }
      else
        {
          pt= _aux.obj_pt;
          aux= (*p<<4) | p[3];
        }
      b0s[i]= NES_mapper_vram_read ( pt|aux );
      b1s[i]= NES_mapper_vram_read ( pt|aux|0x8 );
      p+= 4;
    }

  /* Dibuixa. */
  memset ( &(_render.obj[0]), 0, 256 );
  p= &(_render.stm[0])+(_render.scounter<<2);
  while ( _render.scounter != 0 )
    {
      p-= 4;
      --_render.scounter;
      /*
      if ( _render.size16 )
        {
          pt= (*p&0x1)!=0 ? 0x1000 : 0x0000;
          aux= ((*p&0xfe)<<4) | ((p[3]&0x8)<<1) | (p[3]&0x7);
        }
      else
        {
          pt= _aux.obj_pt;
          aux= (*p<<4) | p[3];
        }
      b0= NES_mapper_vram_read ( pt|aux );
      b1= NES_mapper_vram_read ( pt|aux|0x8 );
      */
      b0= b0s[_render.scounter];
      b1= b1s[_render.scounter];
      x= p[1];
      end= MIN(256,x+8);
      colorh= (p[2]&0x3)<<2;
      pri= p[2]&0x20;
      if ( p[2]&0x40 )
        for ( ; x < end; ++x )
          {
            colorl= ((b1&0x1)<<1)|(b0&0x1);
            if ( colorl != 0 )
              {
                _render.obj[x]= colorh | colorl;
                _render.objpri[x]= pri;
              }
            b1>>= 1; b0>>= 1;
          }
      else
        for ( ; x < end; ++x )
          {
            colorl= ((b1&0x80)>>6)|((b0&0x80)>>7);
            if ( colorl != 0 )
              {
                _render.obj[x]= colorh | colorl;
                _render.objpri[x]= pri;
              }
            b1<<= 1; b0<<= 1;
          }
    }
  MMC2_SAVE_STATE ( 1 );
  
  _render.s0c_N= 0;
  if ( !CHECK_S0C && _render.s0c_flag )
    {
      MMC2_LOAD_STATE ( 0 ); /* <-- Com si estaguerem al principi */
      render_obj_s0c ();
      MMC2_LOAD_STATE ( 1 ); /* <-- Tornem a l'estat que toca. */
    }
  else _render.s0c_N= 0;
  
  if ( _aux.obj_clipping )
    memset ( &(_render.obj[0]), 0, 8 );
  
} /* end render_obj */


static void
render_obj_dummy (
                  int sline
                  )
{
  
  if ( !_aux.enable_obj ) return;
  _render.scounter= 0;
  render_obj_ioe ( sline );
  
} /* end render_obj_dummy */


static void
render_pf (void)
{
  
  NESu16 mask, NT;
  NESu8 *p, PAR;
  unsigned desp;
  int i, j, k;
  
  
  if ( !_aux.enable_pf ) return;
  
  mask= 0x8000>>_regs.FH;
  p= &(_render.pf[0]);
  desp= 15-_regs.FH;
  for ( i= 0; i < 32; ++i )
    {
      
      /* Dibuixa tile. */
      for ( j= 0, k= _regs.FH; j < 8;
            ++j, ++k, ++p, _render.p0<<= 1, _render.p1<<= 1 )
        *p=
          ((_render.p0&mask)>>desp) |
          (((_render.p1&mask)>>desp)<<1) |
          _render.atr[_render.map[k]];
      
      /* Llig memòria. */
      NT= CALC_NT ( _counters );
      PAR= READ_NT_PF ( NT );
      _render.atr[0]= _render.atr[1];
      _render.atr[1]= GET_ATR ( NT );
      _render.p0|= GET_P0 ( PAR );
      _render.p1|= GET_P1 ( PAR );
      
      /* Actualitza comptadors. */
      if ( ++_counters.HT == 32 )
        {
          _counters.HT= 0;
          _counters.H^= 0x1;
        }
      
    }
  
  if ( _aux.pf_clipping )
    memset ( &(_render.pf[0]), 0, 8 );
  
} /* end render_pf */


/* Renderitza una línia per a la comprobació de la col·lissió amb el
   sprite 0. Açò no afecta a l'estat de la PPU. Els píxels fora del
   rang es fiquen a 0, d'aquesta manera no es pot produïr col·lissió
   amb el sprite 0 en eixes posicions. */
static void
render_pf_s0c (
               const int begin,
               const int end
               )
{
  
  NESu16 mask, p0, p1, NT;
  NESu8 *p, PAR;
  unsigned desp;
  int i, j, k;
  struct counters counters;
  
  
  mask= 0x8000>>_regs.FH;
  p= &(_render.pf[0]);
  desp= 15-_regs.FH;
  counters= _counters;
  p0= _render.p0; p1= _render.p1;
  for ( i= 0; i < 32; ++i )
    {
      
      /* Dibuixa tile. */
      for ( j= 0, k= _regs.FH; j < 8;
            ++j, ++k, ++p, p0<<= 1, p1<<= 1 )
        *p=
          ((p0&mask)>>desp) |
          (((p1&mask)>>desp)<<1);
      
      /* Llig memòria. */
      NT= CALC_NT ( counters );
      PAR= READ_NT_PF_COUNTERS ( NT, counters );
      p0|= GET_P0_COUNTERS ( PAR, counters );
      p1|= GET_P1_COUNTERS ( PAR, counters );
      
      /* Actualitza comptadors. */
      if ( ++counters.HT == 32 )
        {
          counters.HT= 0;
          counters.H^= 0x1;
        }
      
    }
  
  if ( _aux.pf_clipping )
    memset ( _render.pf, 0, 8 );
  memset ( _render.pf, 0, begin );
  memset ( &(_render.pf[0])+end, 0, 256-end );
  
} /* end render_pf_s0c */


static void
s0c_test (void)
{
  
  int i;
  
  
  for ( i= 0; i < _render.s0c_N; ++i )
    if ( _render.pf[_render.s0c_pos[i]]&0x3 )
      {
        _status|= 0x40;
        break;
      }
  
}

static void
render_pf_dummy (void)
{
  
  if ( !_aux.enable_pf ) return;
  _counters.HT+= 32;
  if ( _counters.HT >= 32 )
    {
      _counters.HT-= 32;
      _counters.H^= 0x1;
    }
  
} /* end render_pf_dummy */


static void
render_line (void)
{
  
  int i;
  NESu8 color_pf, color_obj, color;
  
  
  if ( _aux.enable_pf )
    {
      if ( _aux.enable_obj )
        {
          for ( i= 0; i < 256; ++i, ++_render.p )
            {
              color_pf= _render.pf[i];
              if ( (color_pf&0x3) == 0 ) color_pf= 0;
              color_obj= _render.obj[i];
              *_render.p=
                ((_render.objpri[i]==0 || color_pf==0) && color_obj!=0) ?
                COLOR_OBJ ( color_obj ) :
                COLOR_PF ( color_pf );
            }
        }
      else
        {
          for ( i= 0; i < 256; ++i, ++_render.p )
            {
              color= _render.pf[i];
              *_render.p= COLOR_PF ( color&0x3?color:0 );
            }
        }
    }
  else
    {
      if ( _aux.enable_obj )
        {
          for ( i= 0; i < 256; ++i, ++_render.p )
            {
              color= _render.obj[i];
              *_render.p= (color == 0) ?
                COLOR_PF ( 0 ) :
                COLOR_OBJ ( color );
            }
        }
      else
        {
          for ( i= 0; i < 256; ++i, ++_render.p )
            *_render.p= COLOR_PF ( 0 );
        }
    }
  
} /* end render_line */


static void
scanline_s0 (void)
{
  
  render_pf ();
  render_obj ();
  render_line ();
  
} /* end scanline_s0 */


static void
scanline_s1 (void)
{
  
  render_obj_ioe ( _render.sline );
  if ( _aux.enable_pf && _aux.enable_obj )
    s0c_test ();
  if ( _mmc3.enabled && (_aux.enable_pf || _aux.enable_obj) )
    NES_mapper_mmc3_clock_counter ();
  
} /* end scanline_s1 */


static void
scanline_s2 (void)
{
  
  if ( _aux.enable_pf || _aux.enable_obj )
    inc_vscroll ();
  init_pf ();
  
} /* end scanline_s2 */


/* Quan estem a meitat d'una 'scanline' visible calcula si s'ha
   produït una col·lissió del sprite 0. */
static void
s0c (
     const int cc
     )
{
  
  int old_pos;
  
  
  /* Si en esta línia no apareix el sprite 0 o ja s'ha produït la
     col·lissió en este frame, aleshores és inútil qualsevol tipus
     d'esforç. Si ja no queden píxels que renderitzar més del
     mateix. */
  if ( !_render.s0c_flag || CHECK_S0C || _render.current_pos >= 256 ) return;
  
  /* Calcula nova posició. */
  old_pos= _render.current_pos;
  _render.current_pos= cc/_timing.pputocc;
  if ( _render.current_pos > 256 ) _render.current_pos= 256;
  if ( old_pos == _render.current_pos ) return;
  
  /* Si ara mateix és impossible la col·lissió ens oblidem. */
  if ( !(_aux.enable_pf && _aux.enable_obj) ) return;
  
  /* Calculem les posicions on el sprite 0 pot col·lisionar. */
  MMC2_SAVE_STATE ( 0 );
  if ( _render.sline_step == 0 )
    render_obj_s0c ();
  if ( _render.s0c_N == 0 ||
       _render.s0c_pos[0] >= _render.current_pos ||
       _render.s0c_pos[_render.s0c_N-1] < old_pos ) goto ret;
  
  /* Renderitza una línia per a testejar la col·lissió i testeja. */
  if ( _render.sline_step == 0 )
    render_pf_s0c ( old_pos, _render.current_pos );
  s0c_test ();

 ret:
  MMC2_LOAD_STATE ( 0 );
  
} /* end s0c */


static int
clock_lines (void)
{
  
  int aux;
  
  
  /* -> 20 (o 70 en PAL). */
  if ( _render.sline == 0 )
    {
      aux=
        _timing.oddframe && _timing.isNTSC ?
        _timing.ccperline-_timing.pputocc :
        _timing.ccperline;
      if ( _timing.ccs < aux ) return 1;
      render_pf_dummy ();
      render_obj_dummy ( /*255*/0 );
      _render.current_pos= 0;
      if ( _aux.enable_pf || _aux.enable_obj )
        update_counters ();
      scanline_s2 ();
      _timing.ccs-= aux;
      _timing.ccs_to_end-= aux;
      if ( _mmc3.enabled )
        _mmc3.ccs_to_end-= aux;
      ++_render.sline;
    }
  
  /* Línies visibles 1-240. */
  for ( ; _render.sline < 241; ++_render.sline )
    switch ( _render.sline_step )
      {
      case 0:
        if ( _timing.ccs < _timing.ccperline_s0 )
          { s0c ( _timing.ccs ); return 1; }
        scanline_s0 ();
        _render.sline_step= 1;
      case 1:
        if ( _timing.ccs < _timing.ccperline_s1 )
          { s0c ( _timing.ccs ); return 1; }
        scanline_s1 ();
        _render.sline_step= 2;
        _render.current_pos= 0;
      case 2:
        if ( _timing.ccs < _timing.ccperline ) return 1;
        scanline_s2 ();
        _render.sline_step= 0;
        _timing.ccs-= _timing.ccperline;
        _timing.ccs_to_end-= _timing.ccperline;
        if ( _mmc3.enabled )
          _mmc3.ccs_to_end-= _timing.ccperline + _timing.ccperline_s1;
      }
  
  /* En la última línia no es fa absolutament res. */
  /* Sols es pot aplegar a aquest punt si sline==241. */
  if ( _timing.ccs < _timing.ccperline ) return 1;
  _timing.ccs-= _timing.ccperline;
  _timing.ccs_to_end-= _timing.ccperline;
  /* ACTUALITZAR MMC3 ACÍ ÉS IRRELLEVANT. */
  ++_render.sline;
  
  return 0;
  
} /* end clock_lines */


/* Processa els clocks de UCP pendents. */
static void
clock (void)
{
  
  while ( _timing.ccs >= _timing.pputocc )
    {
      
      /* Si encara no s'ha aplegat al scanline 20. */
      if ( _render.sline == -1 )
        {
          if ( _timing.ccs < _timing.ccpervblank ) return;
          _timing.ccs-= _timing.ccpervblank;
          _timing.ccs_to_end-= _timing.ccpervblank;
          if ( _mmc3.enabled )
            _mmc3.ccs_to_end-= _timing.ccpervblank;
          _render.p= &(_render.fb[0]);
          if ( _aux.enable_pf || _aux.enable_obj )
            _status&= 0x0F;
          else _status&= 0x1F;
          _render.NMI_occurred= NES_FALSE;
          _render.sline= 0;
        }
      
      /* Dibuixa les línies. */
      if ( clock_lines () ) return;
      
      /* Recalcula els límits i acaba. */
      if ( _render.sline == 242 )
        {
          
          _timing.ccs_to_end= _timing.ccperframe;
          _timing.oddframe^= 1;
          if ( _timing.oddframe && _timing.isNTSC )
            _timing.ccs_to_end-= _timing.pputocc;
          if ( _mmc3.enabled )
            {
              _mmc3.ccs_to_end= _mmc3.ccs_to_first_clock;
              if ( _timing.oddframe && _timing.isNTSC )
        	_mmc3.ccs_to_end-= _timing.pputocc;
            }
          
          _status|= 0x90;
          _update_screen ( &(_render.fb[0]), _udata );
          
          if ( _aux.NMI && !_render.NMI_occurred )
            NES_cpu_NMI ();
          _render.NMI_occurred= NES_TRUE;
          
          _render.sline= -1;
          
        }
      
    }
  
} /* end clock */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

void
NES_ppu_clock (
               int cc
               )
{

  _timing.ccs+= cc*_timing.cputocc;
  if ( (_timing.ccs >= _timing.ccs_to_end) ||
       (_mmc3.enabled && _timing.ccs >= _mmc3.ccs_to_end) )
    clock ();
  
} /* end NES_ppu_clock */


void
NES_ppu_CR1 (
             NESu8 byte
             )
{
  
  NES_Bool old_NMI;
  
  
  CLOCK;
  _regs.H= byte&0x1;
  _regs.V= (byte&0x2)>>1;
  _aux.inc1= (byte&0x4) ? NES_FALSE : NES_TRUE;
  _aux.obj_pt= ((byte&0x8)>>3)!=0 ? 0x1000 : 0x0000;
  _regs.S= ((byte&0x10)>>4)!=0 ? 0x1000 : 0x0000;
  _aux.obj_size16= (byte&0x20) ? NES_TRUE : NES_FALSE;
  /* EXT bus direction (0:input; 1:output) ¿¿?? */
  old_NMI= _aux.NMI;
  _aux.NMI= (byte&0x80) ? NES_TRUE : NES_FALSE;
  if ( _aux.NMI && !old_NMI && _render.NMI_occurred ) NES_cpu_NMI ();
  
} /* end NES_ppu_CR1 */


void
NES_ppu_CR2 (
             NESu8 byte
             )
{
  
  CLOCK;
  _aux.pbitmap= (byte&0x1) ? 0x30 : 0x3F;
  _aux.pf_clipping= (byte&0x2) ? NES_FALSE : NES_TRUE;
  _aux.obj_clipping= (byte&0x4) ? NES_FALSE : NES_TRUE;
  _aux.enable_pf= (byte&0x8) ? NES_TRUE : NES_FALSE;
  _aux.enable_obj= (byte&0x10) ? NES_TRUE : NES_FALSE;
  _aux.emph= byte>>5;
  
} /* end NES_ppu_CR2 */


void
NES_ppu_init (
              const NES_TVMode  tvmode,
              const NES_Mapper  mapper,
              NES_UpdateScreen *update_screen,
              void             *udata
              )
{
  
  _update_screen= update_screen;
  _udata= udata;
  _tvmode= tvmode;
  
  /* MMC2. */
  _mmc2.enabled= (mapper == NES_MMC2);
  
  /* MMC3. */
  _mmc3.enabled= (mapper == NES_MMC3);
  
  NES_ppu_init_state ();
  
} /* end NES_ppu_init */


void
NES_ppu_init_state (void)
{
  
  _initialised= 1;
  init_mem ();
  init_render ();
  init_regs ();
  reset_aux ();
  init_counters ();
  _status= 0xE0;
  _buffer= 0x00;
  init_timing ();

  /* MMC3. */
  if ( _mmc3.enabled )
    {
      /* Final del primer scanline step1. Es possible que calga
         començar en la dummy. */
      _mmc3.ccs_to_end= _mmc3.ccs_to_first_clock=
        _timing.ccperline*(1/*Dummy*/+(_tvmode==NES_PAL?70:20)) +
        _timing.ccperline_s1;
    }
  
} /* end NES_ppu_init_state */


void
NES_ppu_DMA (
             NESu8 byte
             )
{

  /*
   * NOTA: Faig un apanyo per a no canviar molt la part esta del
   * DMA. Per a fer que els cicles tinguen efecte fora, el que faig és
   * acumular-los en NES_dma_extra_cc i a la vegada anar fent clock de
   * la PPU després de cada escritura. Per a què la PPU no torne a
   * executar eixos cicles el que faig és restar-los al final.
   */
  NESu16 addr;
  NESu8 data;
  int i;
  
  
  CLOCK;
  addr= ((NESu16) byte) << 8;
  for ( i= 0; i < 256; ++i )
    {
      data= NES_mem_read ( addr++ );
      if ( _regs.obj_ptr%4 == 2 ) data&= 0xE3;
      _obj_ram[_regs.obj_ptr++]= data;
      NES_dma_extra_cc+= 2;
      _timing.ccs+= _timing.twoCC;
      clock ();
    }
  _timing.ccs-= _timing.twoCC*256; /* <-- Apanyo. */
  
} /* end NES_ppu_DMA */


NESu8
NES_ppu_read (void)
{
  /* ATENCIO!!!!!! ACI FALTA ESTUDIAR EL TEMA DE QUE PASSA QUAN S'ESTA
     RENDERITZANT. */
  NESu8 ret;
  NESu16 addr;
  
  
  CLOCK;

  addr= GET_ADDR & 0x3FFF;
  ret= _buffer;
  if ( addr < 0x3000 )
    _buffer= NES_mapper_vram_read ( addr );
  
  else if ( addr < 0x3F00 )
    _buffer= NES_mapper_vram_read ( 0x2000|(addr&0xFFF) );
  
  else
    {
      ret= _palettes[addr&0x1F] & _aux.pbitmap;
      /* Name Table 3. */
      _buffer= NES_mapper_vram_read ( 0x2C00 | (addr&0x03FF) );
    }
  
  /*CLOCK; <-- ESTAVA ACÍ !!! */
  inc_addr ();
  
  return ret;
  
} /* end NES_ppu_read */


void
NES_ppu_reset (void)
{
  
  reset_aux ();
  reset_regs ();
  _buffer= 0x00;
  
} /* end NES_ppu_reset */


NESu8
NES_ppu_SPRAM_read (void)
{
  return _obj_ram[_regs.obj_ptr++];
} /* end NES_ppu_SPRAM_read */


void
NES_ppu_SPRAM_set_offset (
                          NESu8 byte
                          )
{
  _regs.obj_ptr= byte;
} /* end NES_ppu_SPRAM_set_offset */


void
NES_ppu_SPRAM_write (
                     NESu8 byte
                     )
{
  
  CLOCK;
  if ( _regs.obj_ptr%4 == 2 ) byte&= 0xE3;
  _obj_ram[_regs.obj_ptr++]= byte;
  
} /* NES_ppu_SPRAM_write */


void
NES_ppu_scrolling (
                   NESu8 byte
                   )
{
  
  CLOCK;
  if ( _regs.flip_flop )
    {
      _regs.FV= byte&0x7;
      _regs.VT= byte>>3;
    }
  else
    {
      _regs.FH= byte&0x7;
      _regs.HT= byte>>3;
    }
  _regs.flip_flop^= 1;
  
} /* end NES_ppu_scrolling */


void
NES_ppu_set_addr (
                  NESu8 byte
                  )
{
  
  CLOCK;
  if ( _regs.flip_flop )
    {
      _regs.VT&= 0x18;
      _regs.VT|= byte>>5;
      _regs.HT= byte&0x1F;
      update_counters ();
    }
  else
    {
      _regs.FV= (byte&0x30)>>4;
      _regs.V= (byte&0x8)>>3;
      _regs.H= (byte&0x4)>>2;
      _regs.VT&= 0x7;
      _regs.VT|= (byte&0x3)<<3;
    }
  _regs.flip_flop^= 1;
  
} /* end NES_ppu_set_addr */


NESu8
NES_ppu_status (void)
{
  
  NESu8 ret;
  
  
  clock ();
  ret= _status;
  _regs.flip_flop= 0;
  _status&= 0x70;
  _render.NMI_occurred= NES_FALSE;
  
  return ret;
  
} /* end NES_ppu_status */


void
NES_ppu_write (
               NESu8 byte
               )
{
  
  NESu16 aux, addr;
  
  
  CLOCK;
  
  /* ATENCIO!!!!!! ACI FALTA ESTUDIAR EL TEMA DE QUE PASSA QUAN S'ESTA
     RENDERITZANT. */
  addr= GET_ADDR & 0x3FFF;

  if ( addr < 0x3000 )
    NES_mapper_vram_write ( addr, byte );
  
  else if ( addr < 0x3F00 )
    NES_mapper_vram_write ( 0x2000|(addr&0xFFF), byte );
  
  else
    {
      aux= addr & 0x1F;
      _palettes[aux]= byte;
      if ( (aux & 0x3) == 0x0 )
        _palettes[aux^0x10]= byte;
    }
  
  inc_addr ();
  
} /* end NES_ppu_write */


void
NES_ppu_sync (void)
{
  
  if ( !_initialised ) return;
  
  CLOCK;
  
} /* end NES_ppu_sync */


void
NES_ppu_read_vram (
        	   NESu8 vram[0x4000]
        	   )
{

  int i;


  for ( i= 0; i < 0x3000; ++i )
    vram[i]= NES_mapper_vram_read ( i );
  for ( ; i < 0x3F00; ++i )
    vram[i]= NES_mapper_vram_read ( 0x2000|(i&0xFFF) );
  for ( ; i < 0x4000; ++i )
    vram[i]= _palettes[i&0x1F] /*& _aux.pbitmap*/;
  
} /* end NES_ppu_read_vram */


void
NES_ppu_read_obj_ram (
        	      NESu8 obj_ram[256]
        	      )
{
  memcpy ( obj_ram, _obj_ram, sizeof(_obj_ram) );
} /* end NES_ppu_read_obj_ram */


int
NES_ppu_save_state (
        	    FILE *f
        	    )
{

  int *aux;
  size_t ret;
  
  
  SAVE ( _tvmode );
  SAVE ( _regs );
  SAVE ( _aux );
  SAVE ( _counters );
  SAVE ( _status );
  SAVE ( _buffer );
  aux= _render.p;
  _render.p= (void *) (_render.p-&(_render.fb[0]));
  ret= fwrite ( &_render, sizeof(_render), 1, f );
  _render.p= aux;
  if ( ret != 1 ) return -1;
  SAVE ( _timing );
  SAVE ( _mmc3 );
  SAVE ( _mmc2 );
  SAVE ( _palettes );
  SAVE ( _obj_ram );
  
  return 0;
  
} /* end NES_ppu_save_state */


int
NES_ppu_load_state (
        	    FILE *f
        	    )
{

  NES_TVMode fake_tvmode;
  NES_Bool tmp;
  ptrdiff_t diff;
  int i;
  
  
  LOAD ( fake_tvmode );
  CHECK ( fake_tvmode == _tvmode );
  LOAD ( _regs );
  CHECK ( _regs.S == 0x1000 || _regs.S == 0x0000 );
  CHECK ( (_regs.V&0x1) == _regs.V );
  CHECK ( (_regs.H&0x1) == _regs.H );
  CHECK ( (_regs.FV&0x7) == _regs.FV );
  CHECK ( (_regs.FH&0x7) == _regs.FH );
  CHECK ( (_regs.VT&0x1F) == _regs.VT );
  CHECK ( (_regs.HT&0x1F) == _regs.HT );
  LOAD ( _aux );
  CHECK ( _aux.obj_pt == 0x1000 || _aux.obj_pt == 0x0000 );
  LOAD ( _counters );
  CHECK ( (_counters.FV&0x7) == _counters.FV );
  CHECK ( (_counters.V&0x1) == _counters.V );
  CHECK ( (_counters.H&0x1) == _counters.H );
  CHECK ( (_counters.VT&0x1F) == _counters.VT );
  CHECK ( (_counters.HT&0x1F) == _counters.HT );
  LOAD ( _status );
  LOAD ( _buffer );
  LOAD ( _render );
  _render.p= &(_render.fb[0]) + (ptrdiff_t) _render.p;
  diff= _render.p - (&(_render.fb[0]));
  CHECK ( diff < sizeof(_render.fb) && diff >= 0 );
  CHECK ( _render.sline >= -1 && _render.sline <= 241 );
  CHECK ( _render.scounter >= 0 && _render.scounter <= 8 );
  for ( i= 0; i < 16; ++i )
    CHECK ( _render.map[i] == 0 || _render.map[i] == 1 );
  for ( i= 0; i < 8; ++i )
    CHECK ( _render.s0c_pos[i] >= 0 && _render.s0c_pos[i] < 256 );
  CHECK ( _render.s0c_N >= 0 && _render.s0c_N < 8 );
  LOAD ( _timing );
  tmp= _mmc3.enabled;
  LOAD ( _mmc3 );
  if ( _mmc3.enabled != tmp )
    {
      _mmc3.enabled= tmp;
      return -1;
    }
  tmp= _mmc2.enabled;
  LOAD ( _mmc2 );
  if ( _mmc2.enabled != tmp )
    {
      _mmc2.enabled= tmp;
      return -1;
    }
  LOAD ( _palettes );
  LOAD ( _obj_ram );
  
  return 0;
  
} /* end NES_ppu_load_state */
