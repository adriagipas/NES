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
 *  apu.c - Implementació del mòdul APU.
 *
 *  NOTA: No tinc clar si hi ha que actualitzar els timer cada vegada
 *  que es canvia el periode.
 *
 */


#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

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


#define LC_CLOCK(LC)                                  \
  if ( (LC).count != 0 && !(LC).halted ) --(LC).count


#define LC_UPDATE_INDEX(LC,INDEX)   \
  (LC).index= (INDEX);              \
  (LC).count= _lc_table[(LC).index]


#define LC_HALT(LC,FLAG) (LC).halted= ((FLAG)!=0)


#define LC_CONF(LC,DISABLED)          \
  if ( !((LC).disabled= (DISABLED)) ) \
    (LC).count= 0


#define EG_GET_VOL(EG) ((EG).disabled ? (EG).n : (EG).counter)


#define SWEEP_CLOCK_SQ1 sweep_clock ( &_sq1, 0 )
#define SWEEP_CLOCK_SQ2 sweep_clock ( &_sq2, 1 )


#define CALC_TRG_OUT ((_trg.period<2)?7:_trg_seq[_trg.step])


#define DMC_RESTART                              \
  _dmc.dma.addr= (_dmc.dma.init_addr<<6)+0xC000; \
  _dmc.dma.remain= (_dmc.dma.length<<4)+1




/*********/
/* TIPUS */
/*********/

typedef struct
{
  
  int      index;       /* Índex de la taula amb el valor a
        		   carregar. */
  int      count;       /* Compter que porta. */
  NES_Bool halted;      /* Està aturat. */
  
} LengthCounter;


typedef struct
{
  
  NES_Bool loop;             /* Si està activat el bucle. */
  NES_Bool disabled;         /* Si està activat o no. */
  int      n;                /* Pot ser el valor d'eixida o el
        			periode-1. */
  NES_Bool thereisawrite;    /* Si s'ha produït una escritura al quart
        			registre des de l'últim clock del frame
        			sequencer. */
  int      divider;          /* El divisor. */
  int      counter;          /* El comptador. */
  
} EnvelopeGenerator;


typedef struct
{
  
  EnvelopeGenerator envelope;
  LengthCounter     length;
  struct
  {
    int      period;           /* Periode. */
    int      divider;          /* El divisor. */
    int      shift;            /* Valor que es desplaça el periode. */
    int      result;           /* Resultat. */
    NES_Bool enabled;          /* Està actiu. */
    NES_Bool negated;          /* Hi ha que negar el resultat. */
  }                  sweep;
  int                period;      /* El periode. */
  int                timer;       /* El temporitzador. */
  int                divider2;    /* Divideix el timer en 2. */
  int                step;        /* Pas de la seqüència. */
  const int         *dutyc;       /* Tipus de la senyal. */
  
} SquareChannel;




/************/
/* CONSTATS */
/************/

/* Taula per al Length Counter. */
static const int _lc_table[32]=
  {
    0x0A, 0xFE,
    0x14, 0x02,
    0x28, 0x04,
    0x50, 0x06,
    0xA0, 0x08,
    0x3C, 0x0A,
    0x0E, 0x0C,
    0x1A, 0x0E,
    0x0C, 0x10,
    0x18, 0x12,
    0x30, 0x14,
    0x60, 0x16,
    0xC0, 0x18,
    0x48, 0x1A,
    0x10, 0x1C,
    0x20, 0x1E
  };


/* Taula amb els diferents tipus de senyal per als 'Square
   Channels'. */
static const int _sqdc_table[4][8]=
  {
    { 0, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 1, 1, 1 }
  };


static const double _square_out[31]=
  {
    0, 0.0116529, 0.0230259, 0.0341291, 0.0449719, 0.0555633,
    0.065912, 0.0760263, 0.085914, 0.0955826, 0.105039, 0.114291,
    0.123345, 0.132206, 0.140882, 0.149377, 0.157697, 0.165849,
    0.173836, 0.181663, 0.189336, 0.19686, 0.204237, 0.211473,
    0.218571, 0.225536, 0.232371, 0.23908, 0.245666, 0.252133, 0.258483
  };


static const double _tnd_out[203]=
  {
    0.000000, 0.006700, 0.013345, 0.019936, 0.026474, 0.032959, 0.039393,
    0.045775, 0.052106, 0.058386, 0.064618, 0.070800, 0.076934, 0.083020,
    0.089058, 0.095050, 0.100996, 0.106896, 0.112751, 0.118561, 0.124327,
    0.130049, 0.135728, 0.141365, 0.146959, 0.152512, 0.158024, 0.163494,
    0.168925, 0.174315, 0.179666, 0.184978, 0.190252, 0.195487, 0.200684,
    0.205845, 0.210968, 0.216054, 0.221105, 0.226120, 0.231099, 0.236043,
    0.240953, 0.245828, 0.250669, 0.255477, 0.260252, 0.264993, 0.269702,
    0.274379, 0.279024, 0.283638, 0.288220, 0.292771, 0.297292, 0.301782,
    0.306242, 0.310673, 0.315074, 0.319446, 0.323789, 0.328104, 0.332390,
    0.336649, 0.340879, 0.345083, 0.349259, 0.353408, 0.357530, 0.361626,
    0.365696, 0.369740, 0.373759, 0.377752, 0.381720, 0.385662, 0.389581,
    0.393474, 0.397344, 0.401189, 0.405011, 0.408809, 0.412584, 0.416335,
    0.420064, 0.423770, 0.427454, 0.431115, 0.434754, 0.438371, 0.441966,
    0.445540, 0.449093, 0.452625, 0.456135, 0.459625, 0.463094, 0.466543,
    0.469972, 0.473380, 0.476769, 0.480138, 0.483488, 0.486818, 0.490129,
    0.493421, 0.496694, 0.499948, 0.503184, 0.506402, 0.509601, 0.512782,
    0.515946, 0.519091, 0.522219, 0.525330, 0.528423, 0.531499, 0.534558,
    0.537601, 0.540626, 0.543635, 0.546627, 0.549603, 0.552563, 0.555507,
    0.558434, 0.561346, 0.564243, 0.567123, 0.569988, 0.572838, 0.575673,
    0.578493, 0.581298, 0.584088, 0.586863, 0.589623, 0.592370, 0.595101,
    0.597819, 0.600522, 0.603212, 0.605887, 0.608549, 0.611197, 0.613831,
    0.616452, 0.619059, 0.621653, 0.624234, 0.626802, 0.629357, 0.631899,
    0.634428, 0.636944, 0.639448, 0.641939, 0.644418, 0.646885, 0.649339,
    0.651781, 0.654212, 0.656630, 0.659036, 0.661431, 0.663813, 0.666185,
    0.668544, 0.670893, 0.673229, 0.675555, 0.677869, 0.680173, 0.682465,
    0.684746, 0.687017, 0.689276, 0.691525, 0.693763, 0.695991, 0.698208,
    0.700415, 0.702611, 0.704797, 0.706973, 0.709139, 0.711294, 0.713440,
    0.715576, 0.717702, 0.719818, 0.721924, 0.724021, 0.726108, 0.728186,
    0.730254, 0.732313, 0.734362, 0.736402, 0.738433, 0.740455, 0.742468
  };


/* Valor del seqüènciador del 'Triangle Channel'. */
static const int _trg_seq[32]=
  {
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3,
    0x2, 0x1, 0x0, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
    0xa, 0xb, 0xc, 0xd, 0xe, 0xf
  };


/* Periodes del timer del 'Noise Channel'. */
static const int _noise_periods[2][16]=
  {
    {0x004, 0x008, 0x00E, 0x01E, 0x03C, 0x058, 0x076, 0x094, /* PAL */
     0x0BC, 0x0EC, 0x162, 0x1D8, 0x2C4, 0x3B0, 0x762, 0xEC2},
    {0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 0x080, 0x0A0, /* NTSC */
     0x0CA, 0x0FE, 0x17C, 0x1FC, 0x2FA, 0x3F8, 0x7F2, 0xFE4}
  };


/* Periodes del timer del DMC. */
static const int _dmc_periods[2][16]=
  {
    {0x18E, 0x162, 0x13C, 0x12A, 0x114, 0x0EC, 0x0D2, 0x0C6, /* PAL */
     0x0B0, 0x094, 0x084, 0x076, 0x062, 0x04E, 0x042, 0x032},
    {0x1AC, 0x17C, 0x154, 0x140, 0x11E, 0x0FE, 0x0E2, 0x0D6, /* NTSC */
     0x0BE, 0x0A0, 0x08E, 0x080, 0x06A, 0x054, 0x048, 0x036}
  };




/*********/
/* ESTAT */
/*********/

/* Dades de l'usuari. */
static NES_PlayFrame *_play_frame;
static void *_udata;

/* Frame de sò resultant. */
static double _frame[NES_APU_BUFFER_SIZE];


/* Número de mostres generades. */
static unsigned int _nsamples;


/* Frame Sequencer. NOTA: no implemente el divisor, per que
 * directament cridaré la funció en la freqüència necessària.
 */
static struct
{
  
  void     (*clock) (void);    /* Fa un clock del frame sequencer, que a
        			  la vegada faràun clock d'altres
        			  components. */
  NES_Bool   irq;              /* A cert si està habilitat IRQ. */
  NES_Bool   iflag;            /* Flag d'interrupció. */
  int        step;             /* Pas actual. */
  int        ccperframe;       /* Cicles d'UCP per frame. */
  int        cc;               /* Cicles acumulats. */
  
} _fseq;


/* 'Square Channels'. */
static SquareChannel _sq1, _sq2;


/* 'Triangle Channel'. */
static struct
{
  
  LengthCounter length;
  struct
  {
    NES_Bool haltf;
    NES_Bool controlf;
    int      rvalue;
    int      counter;
  }             linearctr;
  int           step;
  int           period;
  int           timer;
  
} _trg;


/* 'Noise Channel'. */
static struct
{
  
  EnvelopeGenerator  envelope;
  LengthCounter      length;
  int                index;
  int                timer;
  int                shiftr;
  NES_Bool           mode0;
  const int         *periods;
  
} _noise;


/* 'DMC'. */
static struct
{
  
  int        timer;
  int        index;
  int        counter_dac;
  NES_Bool   ienabled;
  NES_Bool   iflag;
  struct
  {
    NESu8    shiftr;
    int      counter;
    NES_Bool silenced;
  }          output;
  struct
  {
    NESu8    sample;
    NES_Bool empty;
  }          buffer;
  struct
  {
    int      init_addr;
    NESu16   addr;
    int      length;
    int      remain;
    NES_Bool loop;
  }          dma;
  const int *periods;
  
} _dmc;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static void
eg_clock (
          EnvelopeGenerator *eg
          )
{

  if ( eg->thereisawrite )
    {
      eg->counter= 15;
      eg->divider= eg->n+1;
      eg->thereisawrite= NES_FALSE;
    }
  else if ( --(eg->divider) == 0 )
    {
      eg->divider= eg->n+1;
      if ( eg->counter == 0 )
        {
          if ( eg->loop ) eg->counter= 15;
        }
      else --(eg->counter);
    }
  
} /* end eg_clock */


static void
sweep_clock (
             SquareChannel *sq,
             int            C
             )
{
  
  int result;
  
  
  /* Calcula. */
  if ( sq->sweep.enabled )
    {
      result= (sq->period)>>(sq->sweep.shift);
      if ( sq->sweep.negated )
        {
          result+= C;
          result= -result;
        }
      result+= sq->period;
      sq->sweep.result= result;
    }
  
  /* Fixa nou valor. */
  if ( --(sq->sweep.divider) == 0 )
    {
      sq->sweep.divider= sq->sweep.period+1;
      if ( sq->sweep.enabled &&
           sq->sweep.shift != 0 &&
           sq->length.count != 0 &&
           sq->period >= 8 )
        {
          if ( sq->sweep.result <= 0x7FF || sq->sweep.negated )
            sq->period= sq->sweep.result;
        }
    }
  
} /* end sweep_clock */


static void
linearctr_clock (void)
{
  
  if ( _trg.linearctr.haltf )
    _trg.linearctr.counter= _trg.linearctr.rvalue;
  else if ( _trg.linearctr.counter > 0 )
    --_trg.linearctr.counter;
  if ( !_trg.linearctr.controlf )
    _trg.linearctr.haltf= NES_FALSE;
  
} /* end linearctr_clock */


static void
fseq_clock_mode1 (void)
{
  
  if ( _fseq.step != 4 )
    {
      if ( _fseq.step == 0 || _fseq.step == 2 )
        {
          /* clock length counters. */
          LC_CLOCK ( _sq1.length );
          LC_CLOCK ( _sq2.length );
          LC_CLOCK ( _trg.length );
          LC_CLOCK ( _noise.length );
          
          /* clock sweep units. */
          SWEEP_CLOCK_SQ1;
          SWEEP_CLOCK_SQ2;
          
        }
        
      /* clock envelopes and triangle's linear counter */
      eg_clock ( &(_sq1.envelope) );
      eg_clock ( &(_sq2.envelope) );
      linearctr_clock ();
      eg_clock ( &(_noise.envelope) );
      
    }
  if ( ++_fseq.step == 5 ) _fseq.step= 0;
  
} /* end fseq_clock_mode1 */


static void
fseq_clock_mode0 (void)
{
  
  switch ( _fseq.step )
    {
    case 3: _fseq.iflag= NES_TRUE;
    case 1:
      
      /* clock length counters. */
      LC_CLOCK ( _sq1.length );
      LC_CLOCK ( _sq2.length );
      LC_CLOCK ( _trg.length );
      LC_CLOCK ( _noise.length );
      
      /* clock sweep units */
      SWEEP_CLOCK_SQ1;
      SWEEP_CLOCK_SQ2;
      
      break;
    default: break;
    }
  
  /* clock envelopes and triangle's linear counter */
  eg_clock ( &(_sq1.envelope) );
  eg_clock ( &(_sq2.envelope) );
  linearctr_clock ();
  eg_clock ( &(_noise.envelope) );
  
  if ( ++_fseq.step == 4 ) _fseq.step= 0;
  
} /* end fseq_clock_mode0 */


static void
fseq_reset (void)
{
  
  _fseq.clock= fseq_clock_mode0;
  _fseq.irq= NES_FALSE;
  _fseq.iflag= NES_FALSE;
  _fseq.step= 0;
  _fseq.cc= 0;
  
} /* end fseq_reset */


static void
length_reset (
              LengthCounter *lc
              )
{
  
  lc->index= 0;
  lc->count= 0;
  lc->halted= NES_TRUE;
  
} /* end length_reset */


static void
envelope_conf (
               EnvelopeGenerator *eg,
               NES_Bool           loop,
               NES_Bool           disabled,
               int                n
               )
{
  
  eg->loop= loop;
  eg->disabled= disabled;
  eg->n= n;
  eg->divider= n+1;
  
} /* end envelope_conf */


static void
envelope_reset (
        	EnvelopeGenerator *eg
        	)
{
  
  eg->loop= NES_FALSE;
  eg->disabled= NES_TRUE;
  eg->n= 0;
  eg->thereisawrite= NES_FALSE;
  eg->divider= 0;
  eg->counter= 0;
  
} /* end envelope_reset */


static void
sq_reset (
          SquareChannel *sq
          )
{
  
  envelope_reset ( &(sq->envelope) );
  length_reset ( &(sq->length) );
  sq->sweep.period= 0;
  sq->sweep.divider= 1;
  sq->sweep.shift= 0;
  sq->sweep.result= 0;
  sq->sweep.enabled= NES_FALSE;
  sq->sweep.negated= NES_FALSE;
  sq->period= 0;
  sq->timer= 1;
  sq->divider2= 0;
  sq->step= 0;
  sq->dutyc= (const int *) &(_sqdc_table[0]);
  
} /* end sq_reset */


static void
clock_sq_timer (
        	SquareChannel *sq
        	)
{
  
  if ( --(sq->timer) == 0 )
    {
      sq->timer= sq->period+1;
      if ( sq->divider2 )
        {
          if ( ++(sq->step) == 8 )
            sq->step= 0;
        }
      sq->divider2^= 1;
    }
  
} /* end clock_sq_timer */


static int
calc_sq_out (
             SquareChannel *sq
             )
{
  
  if ( sq->length.count == 0 || sq->dutyc[sq->step] == 0 ||
       sq->period < 8 ||
       (sq->sweep.result > 0x7FF && !sq->sweep.negated) )
    return 0;
  else return sq->envelope.disabled ?
         sq->envelope.n : sq->envelope.counter;
    
} /* end calc_sq_out */


static void
clock_trg_timer (void)
{
  
  if ( --_trg.timer == 0 )
    {
      _trg.timer= _trg.period+1;
      if ( _trg.length.count != 0 &&
           _trg.linearctr.counter != 0 )
        if ( ++_trg.step == 32 )
          _trg.step= 0;
    }
  
} /* end clock_trg_timer */


static void
linearctr_reset (void)
{
  
  _trg.linearctr.haltf= NES_FALSE;
  _trg.linearctr.controlf= NES_FALSE;
  _trg.linearctr.rvalue= 0;
  _trg.linearctr.counter= 0;
  
} /* end linearctr_reset */


static void
trg_reset (void)
{
  
  length_reset ( &(_trg.length) );
  linearctr_reset ();
  _trg.step= 0;
  _trg.period= 0;
  _trg.timer= 1;
  
} /* end trg_reset */


static int
calc_noise_out (void)
{
  
  if ( _noise.length.count == 0 || !(_noise.shiftr&0x1) )
    return 0;
  else return _noise.envelope.disabled ?
         _noise.envelope.n : _noise.envelope.counter;
  
} /* end calc_noise_out */


static void
clock_noise_timer (void)
{
  
  int aux;
  
  
  if ( --_noise.timer == 0 )
    {
      _noise.timer= _noise.periods[_noise.index];
      if ( _noise.mode0 )
        aux= (_noise.shiftr^(_noise.shiftr>>1))&0x1;
      else
        aux= (_noise.shiftr^(_noise.shiftr>>6))&0x1;
      _noise.shiftr>>= 1;
      _noise.shiftr|= (aux<<14);
    }
  
} /* end clock_noise_timer */


static void
noise_reset (void)
{
  
  envelope_reset ( &(_noise.envelope) );
  length_reset ( &(_noise.length) );
  _noise.index= 0;
  _noise.timer= _noise.periods[_noise.index];
  _noise.shiftr= 1;
  _noise.mode0= NES_TRUE;
  
} /* end noise_reset */


static void
dmc_dma_read (void)
{
  
  _dmc.buffer.sample= NES_mem_read ( _dmc.dma.addr );
  _dmc.buffer.empty= NES_FALSE;
  if ( ++_dmc.dma.addr == 0x0000 ) _dmc.dma.addr= 0x8000;
  if ( --_dmc.dma.remain == 0 )
    {
      if ( _dmc.dma.loop )
        {
          DMC_RESTART;
        }
      else if ( _dmc.ienabled ) _dmc.iflag= NES_TRUE;
    }
  
} /* end dmc_dma_read */


/* Torna cert per a indicar que s'ha produit una operació de DMA que
   gasta 4 cicles de CPU. */
static NES_Bool
clock_dmc_timer (void)
{
 
  if ( --_dmc.timer != 0 ) return NES_FALSE;
  _dmc.timer= _dmc.periods[_dmc.index];
  
  /* Clock. */
  if ( !_dmc.output.silenced )
    {
      if ( _dmc.output.shiftr&0x1 )
        {
          if ( _dmc.counter_dac < 126 )
            _dmc.counter_dac+= 2;
        }
      else
        {
          if ( _dmc.counter_dac > 1 )
            _dmc.counter_dac-= 2;
        }
    }
  _dmc.output.shiftr>>= 1;
  if ( --_dmc.output.counter ) return NES_FALSE;
  
  /* Un nou cicle. OUTPUT.COUNTER==0. */
  _dmc.output.counter= 8;
  if ( _dmc.buffer.empty )
    {
      _dmc.output.silenced= NES_TRUE;
      return NES_FALSE;
    }
  
  /* Es buida el buffer. */
  _dmc.output.silenced= NES_FALSE;
  _dmc.output.shiftr= _dmc.buffer.sample;
  if ( _dmc.dma.remain == 0 )
    {
      _dmc.buffer.empty= NES_TRUE;
      return NES_FALSE;
    }
  else
    {
      /* NOTA!!! Encara que estiga llegint el canal no s'atura, és
         imposible que en 4 cicles torne a executar-se açò. */
      dmc_dma_read ();
      return NES_TRUE;
    }
  
} /* end clock_dmc_timer */


static void
dmc_reset (void)
{
  
  _dmc.index= 0;
  _dmc.timer= _dmc.periods[_dmc.index];
  _dmc.ienabled= NES_FALSE;
  _dmc.iflag= NES_FALSE;
  
  /* Açò no és arbitrari, és el valor que ha de tindre. */
  _dmc.counter_dac= 0;
  
  /* Açò tampoc és arbitrari. */
  _dmc.output.shiftr= 0;
  _dmc.output.counter= 8;
  _dmc.output.silenced= NES_TRUE;
  
  _dmc.buffer.sample= 0;
  _dmc.buffer.empty= NES_TRUE;
  
  /* Açò tampoc és arbitrari. */
  _dmc.dma.init_addr= 0;
  _dmc.dma.addr= 0xC000;
  _dmc.dma.length= 0;
  _dmc.dma.remain= 0;
  _dmc.dma.loop= NES_FALSE;
  
} /* end dmc_reset */


static void
pulseCR (
         SquareChannel *sq,
         NESu8          data
         )
{
  
  NESu8 cflag;
  
  
  cflag= (data&0x20);
  sq->dutyc= (const int *) &(_sqdc_table[data>>6]);
  envelope_conf ( &(sq->envelope), cflag!=0,
        	  (data&0x10)!=0, data&0xF );
  LC_HALT ( sq->length, cflag );
  
} /* end pulseCR */


static void
pulseRCR (
          SquareChannel *sq,
          NESu8          data
          )
{
  
  sq->sweep.enabled= ((data&0x80)!=0);
  sq->sweep.period= (data>>4)&0x7;
  sq->sweep.divider= sq->sweep.period+1;
  sq->sweep.negated= ((data&0x8)!=0);
  sq->sweep.shift= data&0x7;
  
} /* end pulseRCR */


static void
pulseFTR (
          SquareChannel *sq,
          NESu8          data
          )
{
  
  sq->period&= 0x700;
  sq->period|= data;
  /*sq->timer= sq->period+1;*/
  
} /* end pulseFTR */


static void
pulseCTR (
          SquareChannel *sq,
          NESu8          data
          )
{
  
  LC_UPDATE_INDEX ( sq->length, data>>3 );
  sq->period&= 0xFF;
  sq->period|= ((int) (data&0x7))<<8;
  /*sq->timer= sq->period+1;*/
  sq->envelope.thereisawrite= NES_TRUE;
  sq->step= 0;
  
} /* end pulseCTR */


static NES_Bool
clock_frame (void)
{
  
  _fseq.cc= 0;
  _fseq.clock ();
  
  return (_fseq.irq && _fseq.iflag) ?
    NES_TRUE : NES_FALSE;
  
} /* end clock_frame */




/**********************/
/* FUNCIONS PÚBLIQUES */
/**********************/

NES_Bool
NES_apu_clock (
               unsigned int *cc
               )
{
  
  unsigned int CC;
  int sq1_out, sq2_out, trg_out, noise_out, dmc_out;
  NES_Bool ret;
  
  
  ret= NES_FALSE;
  CC= *cc;
  while ( CC )
    {
      
      /* Calcula l'eixida de cada canal. */
      sq1_out= calc_sq_out ( &_sq1 );
      sq2_out= calc_sq_out ( &_sq2 );
      trg_out= CALC_TRG_OUT;
      noise_out= calc_noise_out ();
      dmc_out= _dmc.counter_dac;
      
      /* Calcula valor instant 't'. */
      _frame[_nsamples++]=
        _square_out[sq1_out+sq2_out] +
        _tnd_out[3*trg_out+(noise_out<<1)+dmc_out];
      if ( ++_fseq.cc == _fseq.ccperframe )
        ret|= clock_frame ();
      if ( _nsamples == NES_APU_BUFFER_SIZE )
        {
          _nsamples= 0;
          _play_frame ( _frame, _udata );
        }
      
      /* Clock els canals. */
      clock_sq_timer ( &_sq1 );
      clock_sq_timer ( &_sq2 );
      clock_trg_timer ();
      clock_noise_timer ();
      if ( clock_dmc_timer () )
        {
          (*cc)+= 4;
          CC+= 4;
        }
      ret|= _dmc.iflag;

      --CC;
      
    }
  
  return ret;
  
} /* end NES_apu_clock */


void
NES_apu_conf_fseq (
        	   NESu8 data
        	   )
{
  
  _fseq.step= 0;
  _fseq.clock= data&0x80 ?
    fseq_clock_mode1 : fseq_clock_mode0;
  _fseq.irq= data&0x40 ? NES_FALSE : NES_TRUE;
  
} /* end NES_apu_conf_fseq */


void
NES_apu_control (
        	 NESu8 data
        	 )
{
  
  if ( data&0x10 )
    {
      if ( _dmc.dma.remain == 0 )
        {
          DMC_RESTART;
          if ( _dmc.buffer.empty )
            {
              dmc_dma_read ();
              NES_dma_extra_cc+= 4;
            }
        }
    }
  else _dmc.dma.remain= 0;
  if ( (data&0x08) == 0 ) _noise.length.count= 0;
  if ( (data&0x04) == 0 ) _trg.length.count= 0;
  if ( (data&0x02) == 0 ) _sq2.length.count= 0;
  if ( (data&0x01) == 0 ) _sq1.length.count= 0;
  
} /* NES_apu_control */


NESu8
NES_apu_CSR (void)
{
  
  NESu8 ret;
  
  
  ret= 0x00;
  if ( _dmc.iflag ) ret|= 0x80;
  if ( _fseq.iflag ) ret|= 0x40;
  if ( _dmc.dma.remain > 0 ) ret|= 0x10;
  if ( _noise.length.count > 0 ) ret|= 0x08;
  if ( _trg.length.count > 0 ) ret|= 0x04;
  if ( _sq2.length.count > 0 ) ret|= 0x02;
  if ( _sq1.length.count > 0 ) ret|= 0x01;
  _fseq.iflag= NES_FALSE;
  
  return ret;
  
} /* NES_apu_CSR */


void
NES_apu_init (
              const NES_TVMode  tvmode,
              NES_PlayFrame    *play_frame,
              void             *udata
              )
{
  
  /* Valors estimats empiracament per gent en foros. La idea és que
     PAL~50Hz i NTSC~60Hz. */
  _fseq.ccperframe= tvmode==NES_PAL ? 8313 : 7458;
  _dmc.periods= &(_dmc_periods[tvmode][0]);
  _noise.periods= &(_noise_periods[tvmode][0]);
  
  _play_frame= play_frame;
  _udata= udata;
  
  NES_apu_init_state ();
  
} /* end NES_apu_init */


void
NES_apu_init_state (void)
{

  int i;
  
  
  for ( i= 0; i < NES_APU_BUFFER_SIZE; ++i )
    _frame[i]= 0.0;
  NES_apu_reset ();
  
} /* end NES_apu_init_state */


void
NES_apu_reset (void)
{
  
  _nsamples= 0;
  fseq_reset ();
  sq_reset ( &_sq1 );
  sq_reset ( &_sq2 );
  trg_reset ();
  noise_reset ();
  dmc_reset ();
  
} /* NES_apu_reset */


void
NES_apu_pulse1CR (
        	  NESu8 data
        	  )
{
  pulseCR ( &_sq1, data );
} /* end NES_apu_pulse1CR */


void
NES_apu_pulse1RCR (
        	   NESu8 data
        	   )
{
  pulseRCR ( &_sq1, data );
} /* end NES_apu_pulse1RCR */


/* NOTA: Entenc que 'timer' sols s'actualitza quan s'escriu el quart
   registre. */
void
NES_apu_pulse1FTR (
        	   NESu8 data
        	   )
{
  pulseFTR ( &_sq1, data );
} /* end NES_apu_pulse1FTR */


void
NES_apu_pulse1CTR (
        	   NESu8 data
        	   )
{
  pulseCTR ( &_sq1, data );
} /* end NES_apu_pulse1CTR */


void
NES_apu_pulse2CR (
        	  NESu8 data
        	  )
{
  pulseCR ( &_sq2, data );
} /* end NES_apu_pulse2CR */


void
NES_apu_pulse2RCR (
        	   NESu8 data
        	   )
{
  pulseRCR ( &_sq2, data );
} /* end NES_apu_pulse2RCR */


void
NES_apu_pulse2FTR (
        	   NESu8 data
        	   )
{
  pulseFTR ( &_sq2, data );
} /* end NES_apu_pulse2FTR */


void
NES_apu_pulse2CTR (
        	   NESu8 data
        	   )
{
  pulseCTR ( &_sq2, data );
} /* end NES_apu_pulse2CTR */


void
NES_apu_triangleCR1 (
        	     NESu8 data
        	     )
{
  
  NESu8 cflag;
  
  
  cflag= (data&0x80);
  _trg.linearctr.rvalue= data&0x7F;
  _trg.linearctr.controlf= (cflag!=0);
  LC_HALT ( _trg.length, cflag );
  
} /* end NES_apu_triangleCR1 */


void
NES_apu_triangleFR1 (
        	     NESu8 data
        	     )
{
  
  _trg.period&= 0x700;
  _trg.period|= data;
  /*_trg.timer= _trg.period+1;*/
  
} /* end NES_apu_triangleFR1 */


void
NES_apu_triangleFR2 (
        	     NESu8 data
        	     )
{
  
  LC_UPDATE_INDEX ( _trg.length, data>>3 );
  _trg.period&= 0xFF;
  _trg.period|= ((int) (data&0x7))<<8;
  /*_trg.timer= _trg.period+1;*/
  _trg.linearctr.haltf= NES_TRUE;
  
} /* end NES_apu_triangleFR2 */


void
NES_apu_noiseCR (
        	 NESu8 data
        	 )
{
  
  NESu8 cflag;
  
  
  cflag= (data&0x20);
  envelope_conf ( &(_noise.envelope), cflag!=0,
        	  (data&0x10)!=0, data&0xF );
  LC_HALT ( _noise.length, cflag );
  
} /* end NES_apu_noiseCR */


void
NES_apu_noiseFR1 (
        	  NESu8 data
        	  )
{
  
  _noise.index= data&0xF;
  _noise.mode0= ((data&0x80)==0);
  /*_noise.timer= _noise.periods[_noise.index];*/
  
} /* end NES_apu_noiseFR1 */


void
NES_apu_noiseFR2 (
        	  NESu8 data
        	  )
{
  LC_UPDATE_INDEX ( _noise.length, data>>3 );
} /* end NES_apu_noiseFR2 */


void
NES_apu_dmCR (
              NESu8 data
              )
{
  
  _dmc.ienabled= ((data&0x80)!=0);
  if ( !_dmc.ienabled ) _dmc.iflag= NES_FALSE;
  _dmc.dma.loop= ((data&0x40)!=0);
  _dmc.index= data&0xF;
  /*_dmc.timer= _dmc.periods[_dmc.index];*/
  
} /* NES_apu_dmCR */


void
NES_apu_dmDAR (
               NESu8 data
               )
{
  _dmc.counter_dac= data&0x7F;
} /* NES_apu_dmDAR */


void
NES_apu_dmAR (
              NESu8 data
              )
{
  _dmc.dma.init_addr= data;
} /* end NES_apu_dmAR */


void
NES_apu_dmLR (
              NESu8 data
              )
{
  _dmc.dma.length= data;
} /* end NES_apu_dmLR */


int
NES_apu_save_state (
        	    FILE *f
        	    )
{

  void (*tmp_clock) (void);
  const int *tmp;
  size_t ret;
  
  
  SAVE ( _frame );
  SAVE ( _nsamples );
  
  tmp_clock= _fseq.clock;
  _fseq.clock= (void *) (int64_t) (_fseq.clock==fseq_clock_mode1);
  ret= fwrite ( &_fseq, sizeof(_fseq), 1, f );
  _fseq.clock= tmp_clock;
  if ( ret != 1 ) return -1;

  tmp= _sq1.dutyc;
  _sq1.dutyc= (void *) (int64_t) ((_sq1.dutyc - (const int *) _sqdc_table)/8);
  ret= fwrite ( &_sq1, sizeof(_sq1), 1, f );
  _sq1.dutyc= tmp;
  if ( ret != 1 ) return -1;

  tmp= _sq2.dutyc;
  _sq2.dutyc= (void *) (int64_t) ((_sq2.dutyc - (const int *) _sqdc_table)/8);
  ret= fwrite ( &_sq2, sizeof(_sq2), 1, f );
  _sq2.dutyc= tmp;
  if ( ret != 1 ) return -1;

  SAVE ( _trg );
  
  tmp= _noise.periods;
  _noise.periods= (void *) ((_noise.periods - (const int *) _noise_periods)/16);
  ret= fwrite ( &_noise, sizeof(_noise), 1, f );
  _noise.periods= tmp;
  if ( ret != 1 ) return -1;

  tmp= _dmc.periods;
  _dmc.periods= (void *) ((_dmc.periods - (const int *) _dmc_periods)/16);
  ret= fwrite ( &_dmc, sizeof(_dmc), 1, f );
  _dmc.periods= tmp;
  if ( ret != 1 ) return -1;
  
  return 0;
  
} /* end NES_apu_save_state */


int
NES_apu_load_state (
        	    FILE *f
        	    )
{

  LOAD ( _frame );
  LOAD ( _nsamples );
  CHECK ( _nsamples >= 0 && _nsamples < NES_APU_BUFFER_SIZE );
  LOAD ( _fseq );
  _fseq.clock= ((int64_t) _fseq.clock) ? fseq_clock_mode1 : fseq_clock_mode0;
  LOAD ( _sq1 );
  _sq1.dutyc= &(_sqdc_table[(int64_t) _sq1.dutyc][0]);
  CHECK ( _sq1.envelope.counter >= 0 && _sq1.envelope.counter <= 15 );
  CHECK ( _sq1.envelope.n >= 0 && _sq1.envelope.n <= 15 );
  CHECK ( _sq1.length.index >= 0 && _sq1.length.index < 32 );
  CHECK ( _sq1.step >= 0 && _sq1.step < 8 );
  LOAD ( _sq2 );
  _sq2.dutyc= &(_sqdc_table[(int64_t) _sq2.dutyc][0]);
  CHECK ( _sq2.envelope.counter >= 0 && _sq2.envelope.counter <= 15 );
  CHECK ( _sq2.envelope.n >= 0 && _sq2.envelope.n <= 15 );
  CHECK ( _sq2.length.index >= 0 && _sq2.length.index < 32 );
  CHECK ( _sq2.step >= 0 && _sq2.step < 8 );
  LOAD ( _trg );
  CHECK ( _trg.length.index >= 0 && _trg.length.index < 32 );
  CHECK ( _trg.step >= 0 && _trg.step < 32 );
  LOAD ( _noise );
  _noise.periods= &(_noise_periods[(int64_t) _noise.periods][0]);
  CHECK ( _noise.envelope.counter >= 0 && _noise.envelope.counter <= 15 );
  CHECK ( _noise.envelope.n >= 0 && _noise.envelope.n <= 15 );
  CHECK ( _noise.length.index >= 0 && _noise.length.index < 32 );
  CHECK ( _noise.index >= 0 && _noise.index < 16 );
  LOAD ( _dmc );
  _dmc.periods= &(_dmc_periods[(int64_t) _dmc.periods][0]);
  CHECK ( _dmc.index >= 0 && _dmc.index < 16 );
  CHECK ( _dmc.counter_dac >= 0 && _dmc.counter_dac <= 127 );
  
  return 0;
  
} /* end NES_apu_load_state */
