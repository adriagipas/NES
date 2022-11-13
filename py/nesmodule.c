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
 *  mdmodule.c - Mòdul que implementa una NES en Python per a debug.
 *
 */

#include <Python.h>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "NES.h"




/**********/
/* MACROS */
/**********/

#define CHECK_INITIALIZED                                               \
  do {                                                                  \
    if ( !_initialized )                                                \
      {                                                                 \
        PyErr_SetString ( NESError, "Module must be initialized" );        \
        return NULL;                                                    \
      }                                                                 \
  } while(0)


#define CHECK_ROM                                                       \
  do {                                                                  \
    if ( _rom.prgs ==NULL )        					\
      {                                                                 \
        PyErr_SetString ( NESError, "There is no ROM inserted"        	\
                          " into the simulator" );                      \
        return NULL;                                                    \
      }                                                                 \
  } while(0)

#define NBUFF 4




/*********/
/* TIPUS */
/*********/

enum { FALSE= 0, TRUE };

typedef struct
{
  
  double       *v;
  volatile int  full;
  
} buffer_t;




/*********/
/* ESTAT */
/*********/

/* Error. */
static PyObject *NESError;

/* Inicialitzat. */
static char _initialized;

/* Rom. */
static NES_Rom _rom;

/* Tracer. */
static struct
{
  PyObject *obj;
  int       has_cpu_inst;
  int       has_mem_access;
  int       has_mapper_changed;
} _tracer;

/* Pantalla. */
static struct
{
  
  int          width;
  int          height;
  SDL_Surface *surface;
  int          fb_off;
  
} _screen;

/* Paleta de colors. */
static Uint32 _palette[NES_PALETTE_SIZE];

/* Control. */
static int _control;

/* Estat so. */
static struct
{
  
  buffer_t buffers[NBUFF];
  int      buff_in;
  int      buff_out;
  char     silence;
  int      pos;
  int      size;
  int      nsamples;
  int      ccpersec;
  double   ratio;
  double   pos2;
  double   sysfreq;
  
} _audio;




/*********************/
/* FUNCIONS PRIVADES */
/*********************/

static int
has_method (
            PyObject   *obj,
            const char *name
            )
{
  
  PyObject *aux;
  int ret;
  
  if ( !PyObject_HasAttrString ( obj, name ) ) return 0;
  aux= PyObject_GetAttrString ( obj, name );
  ret= PyMethod_Check ( aux );
  Py_DECREF ( aux );
  
  return ret;
  
} /* end has_method */


static void
init_palette (void)
{
  
  NES_Color ret;
  int i;
  
  
  for ( i= 0; i < NES_PALETTE_SIZE; ++i )
    {
      ret= NES_ppu_palette[i];
      _palette[i]= SDL_MapRGB ( _screen.surface->format,
        			ret.r, ret.g, ret.b );
    }
  
} /* end init_palette */


static void
audio_callback (
                void  *userdata,
                Uint8 *stream,
                int    len
                )
{
  
  int i;
  const double *buffer;
  
  
  assert ( _audio.size == len );
  if ( _audio.buffers[_audio.buff_out].full )
    {
      buffer= _audio.buffers[_audio.buff_out].v;
      for ( i= 0; i < len; ++i )
        stream[i]= 127 + (Uint8) ((128*buffer[i]) + 0.5);
      _audio.buffers[_audio.buff_out].full= 0;
      _audio.buff_out= (_audio.buff_out+1)%NBUFF;
    }
  else for ( i= 0; i < len; ++i ) stream[i]= _audio.silence;
  
} /* end audio_callback */


/* Torna 0 si tot ha anat bé. */
static const char *
init_audio (void)
{
  
  SDL_AudioSpec desired, obtained;
  int n;
  double *mem;
  
  
  /* Únic camp de l'estat que s'inicialitza abans. */
  _audio.buff_out= _audio.buff_in= 0;
  for ( n= 0; n < NBUFF; ++n ) _audio.buffers[n].full= 0;
  
  /* Inicialitza. */
  desired.freq= 44100;
  desired.format= AUDIO_U8;
  desired.channels= 1;
  desired.samples= 2048;
  desired.size= 2048;
  desired.callback= audio_callback;
  desired.userdata= NULL;
  if ( SDL_OpenAudio ( &desired, &obtained ) == -1 )
    return SDL_GetError ();
  if ( obtained.format != desired.format )
    {
      fprintf ( stderr, "Força format audio\n" );
      SDL_CloseAudio ();
      if ( SDL_OpenAudio ( &desired, NULL ) == -1 )
        return SDL_GetError ();
      obtained= desired;
    }
  
  /* Inicialitza estat. */
  mem= (double *) malloc ( sizeof(double)*obtained.size*NBUFF );
  for ( n= 0; n < NBUFF; ++n, mem+= obtained.size )
    _audio.buffers[n].v= (double *) mem;
  _audio.silence= (char) obtained.silence;
  _audio.pos= 0;
  _audio.size= obtained.size;
  _audio.nsamples= _audio.size;
  if ( obtained.freq >= NES_CPU_NTSC_CYCLES_PER_SEC )
    {
      SDL_CloseAudio ();
      return "Freqüència massa gran";
    }
  _audio.sysfreq= (double) obtained.freq;
  _audio.ratio= 1.0;
  _audio.ccpersec= 0;
  /*_audio.ratio=  / (double) obtained.freq;*/
  _audio.pos2= 0.0;
  
  return NULL;
  
} /* end init_audio */


static void
close_audio (void)
{
  
  SDL_CloseAudio ();
  free ( _audio.buffers[0].v );
  
} /* end close_audio */


static void
update_tvmode (void)
{

  SDL_Surface *prev;
  
  
  /* Nou surface. */
  _screen.width= NES_PPU_COLS;
  _screen.height= _rom.tvmode==NES_PAL ? NES_PPU_PAL_ROWS : NES_PPU_NTSC_ROWS;
  _screen.fb_off= _rom.tvmode==NES_PAL ? 0 : 8*NES_PPU_COLS;
  prev= _screen.surface;
  _screen.surface= SDL_SetVideoMode ( _screen.width, _screen.height, 32,
                                      SDL_HWSURFACE | SDL_GL_DOUBLEBUFFER );
  if ( prev == NULL ) init_palette ();
  if ( _screen.surface == NULL )
    {
      fprintf ( stderr, "FATAL ERROR!!!: %s", SDL_GetError () );
      SDL_Quit ();
      return;
    }
  SDL_WM_SetCaption ( "NES", "NES" );

  /* Audio. */
  _audio.ccpersec=
    _rom.tvmode==NES_PAL ?
    NES_CPU_PAL_CYCLES_PER_SEC : NES_CPU_NTSC_CYCLES_PER_SEC;
  _audio.ratio= _audio.ccpersec / _audio.sysfreq;
  
} /* end update_tvmode */




/************/
/* FRONTEND */
/************/

static void
warning (
         void       *udata,
         const char *format,
         ...
         )
{
  
  va_list ap;
  
  
  va_start ( ap, format );
  fprintf ( stderr, "Warning: " );
  vfprintf ( stderr, format, ap );
  putc ( '\n', stderr );
  va_end ( ap );
  
} /* end warning */


static void
update_screen (
               const int  fb[],
               void      *udata
               )
{
  
  Uint32 *data;
  int i;
  
  
  if ( _screen.surface == NULL ) return;
  if ( SDL_MUSTLOCK ( _screen.surface ) )
    SDL_LockSurface ( _screen.surface );
  
  data= _screen.surface->pixels;
  for ( i= 0; i < _screen.width*_screen.height; ++i )
    data[i]= _palette[fb[i+_screen.fb_off]];
  
  if ( SDL_MUSTLOCK ( _screen.surface ) )
    SDL_UnlockSurface ( _screen.surface );
  
  if ( SDL_Flip ( _screen.surface ) == -1 )
    {
      fprintf ( stderr, "ERROR FATAL !!!: %s\n", SDL_GetError () );
      SDL_Quit ();
    }
  
} /* end update_screen */


static void
play_frame (
            const double  frame[NES_APU_BUFFER_SIZE],
            void         *udata
            )
{
  
  int nofull, j;
  double *buffer;
  
  
  for (;;)
    {
      
      while ( _audio.buffers[_audio.buff_in].full ) SDL_Delay ( 1 );
      buffer= _audio.buffers[_audio.buff_in].v;
      
      j= (int) (_audio.pos2 + 0.5);
      while ( (nofull= (_audio.pos != _audio.nsamples)) &&
              j < NES_APU_BUFFER_SIZE )
        {
          buffer[_audio.pos++]= frame[j];
          _audio.pos2+= _audio.ratio;
          j= (int) (_audio.pos2 + 0.5);
        }
      if ( !nofull )
        {
          _audio.pos= 0;
          _audio.buffers[_audio.buff_in].full= 1;
          _audio.buff_in= (_audio.buff_in+1)%NBUFF;
        }
      if ( j >= NES_APU_BUFFER_SIZE )
        {
          _audio.pos2-= NES_APU_BUFFER_SIZE;
          break;
        }
      
    }
  
} /* end play_frame */


static NES_Bool
check_pad_button1 (
        	   NES_PadButton  button,
        	   void          *udata
        	   )
{

  return (_control&(1<<button))!=0;
  
} /* end check_pad_button1 */


static NES_Bool
check_pad_button2 (
        	   NES_PadButton  button,
        	   void          *udata
        	   )
{

  return NES_FALSE;
  
} /* end check_pad_button2 */


static void
check_signals (
               NES_Bool *reset,
               NES_Bool *stop,
               void     *udata
               )
{
  
  SDL_Event event;
  
  
  *stop= *reset= NES_FALSE;
  while ( SDL_PollEvent ( &event ) )
    switch ( event.type )
      {
      case SDL_ACTIVEEVENT:
        if ( event.active.state&SDL_APPINPUTFOCUS &&
             !event.active.gain )
          _control= 0;
        break;
      case SDL_KEYDOWN:
        if ( event.key.keysym.mod&KMOD_CTRL )
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_q: *stop= NES_TRUE; break;
              case SDLK_r: *reset= NES_TRUE;
              default: break;
              }
          }
        else
          {
            switch ( event.key.keysym.sym )
              {
              case SDLK_SPACE: _control|= (1<<NES_START); break;
              case SDLK_RETURN: _control|= (1<<NES_SELECT); break;
              case SDLK_UP: _control|= (1<<NES_UP); break;
              case SDLK_DOWN: _control|= (1<<NES_DOWN); break;
              case SDLK_LEFT: _control|= (1<<NES_LEFT); break;
              case SDLK_RIGHT: _control|= (1<<NES_RIGHT); break;
              case SDLK_z: _control|= (1<<NES_B); break;
              case SDLK_x: _control|= (1<<NES_A); break;
              default: break;
              }
          }
        break;
      case SDL_KEYUP:
        switch ( event.key.keysym.sym )
          {
          case SDLK_SPACE: _control&= ~(1<<NES_START); break;
          case SDLK_RETURN: _control&= ~(1<<NES_SELECT); break;
          case SDLK_UP: _control&= ~(1<<NES_UP); break;
          case SDLK_DOWN: _control&= ~(1<<NES_DOWN); break;
          case SDLK_LEFT: _control&= ~(1<<NES_LEFT); break;
          case SDLK_RIGHT: _control&= ~(1<<NES_RIGHT); break;
          case SDLK_z: _control&= ~(1<<NES_B); break;
          case SDLK_x: _control&= ~(1<<NES_A); break;
          default: break;
          }
        break;
      default: break;
      }
  
} /* end check_signals */

static void
mem_access (
            void                    *udata,
            const NES_MemAccessType  type,
            const NESu16             addr,
            const NESu8              data
            )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mem_access ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mem_access",
                             "iHB", type, addr, data );
  Py_XDECREF ( ret );
  
} /* end mem_access */


static void
mapper_changed (
                void *udata
                )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_mapper_changed ||
       PyErr_Occurred () != NULL ) return;
  ret= PyObject_CallMethod ( _tracer.obj, "mapper_changed", "" );
  Py_XDECREF ( ret );
  
} /* end mapper_changed */


static void
cpu_inst (
          const NES_Inst *inst,
          const NESu16    nextaddr,
          void           *udata
          )
{
  
  PyObject *ret;
  
  
  if ( _tracer.obj == NULL ||
       !_tracer.has_cpu_inst ||
       PyErr_Occurred () != NULL ) return;
  
  ret= PyObject_CallMethod ( _tracer.obj, "cpu_inst",
        		     "Hiiy#(BH(Hb))",
        		     nextaddr,
        		     inst->id.name,
        		     inst->id.addr_mode,
        		     inst->bytes,
        		     inst->nbytes,
        		     inst->e.valu8,
        		     inst->e.valu16,
        		     inst->e.branch.addr,
        		     inst->e.branch.desp );
  Py_XDECREF ( ret );
  
} /* end cpu_inst */




/******************/
/* FUNCIONS MÒDUL */
/******************/

static PyObject *
NES_close (
           PyObject *self,
           PyObject *args
           )
{
  
  if ( !_initialized ) Py_RETURN_NONE;

  close_audio ();
  SDL_Quit ();
  NES_rom_free ( _rom );
  _initialized= FALSE;
  Py_XDECREF ( _tracer.obj );
  
  Py_RETURN_NONE;
  
} /* end NES_close */


static PyObject *
NES_get_rom_mapper_state (
        		  PyObject *self,
        		  PyObject *args
        		  )
{
  
  PyObject *dict, *aux;
  NES_RomMapperState state;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  NES_mapper_get_rom_mapper_state ( &state );
  
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  aux= PyLong_FromLong ( state.p0 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "bank0", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.p1 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "bank1", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.p2 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "bank2", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  aux= PyLong_FromLong ( state.p3 );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "bank3", aux ) == -1 )
    { Py_DECREF ( aux ); goto error; }
  Py_DECREF ( aux );
  
  return dict;

 error:
  Py_DECREF ( dict );
  return NULL;  
  
} /* end NES_get_rom_mapper_state */


static PyObject *
NES_get_rom (
             PyObject *self,
             PyObject *args
             )
{
  
  PyObject *dict, *aux, *aux2;
  const char *tmp_str;
  int n;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  dict= PyDict_New ();
  if ( dict == NULL ) return NULL;
  
  /* Número de pàgines de codi 16K. */
  aux= PyLong_FromLong ( _rom.nprg );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "nprg", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* Número de pàgines de vrom 8K. */
  aux= PyLong_FromLong ( _rom.nchr );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "nchr", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* Mapper. */
  tmp_str= NES_mapper_name ( _rom.mapper );
  aux= PyByteArray_FromStringAndSize ( tmp_str, strlen ( tmp_str ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "mapper", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* TVMode. */
  tmp_str= _rom.tvmode==NES_PAL ? "PAL" : "NTSC";
  aux= PyByteArray_FromStringAndSize ( tmp_str, strlen ( tmp_str ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "tvmode", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* Mirroring. */
  switch ( _rom.mirroring )
    {
    case NES_HORIZONTAL: tmp_str= "horizontal"; break;
    case NES_VERTICAL: tmp_str= "vertical"; break;
    case NES_FOURSCREEN: tmp_str= "four screen"; break;
    default:
    case NES_SINGLE: tmp_str= "single"; break;
    }
  aux= PyByteArray_FromStringAndSize ( tmp_str, strlen ( tmp_str ) );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "mirroring", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* SRAM. */
  aux= PyBool_FromLong ( _rom.sram );
  if ( PyDict_SetItemString ( dict, "sram", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );

  /* PRGS. */
  aux= PyTuple_New ( _rom.nprg );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "prgs", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  for ( n= 0; n < _rom.nprg; ++n )
    {
      aux2= PyBytes_FromStringAndSize ( ((const char *) _rom.prgs[n]),
                                        NES_PRG_SIZE );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, n, aux2 );
    }

  /* CHRS. */
  aux= PyTuple_New ( _rom.nchr );
  if ( aux == NULL ) goto error;
  if ( PyDict_SetItemString ( dict, "chrs", aux ) == -1 )
    { Py_XDECREF ( aux ); goto error; }
  Py_XDECREF ( aux );
  for ( n= 0; n < _rom.nchr; ++n )
    {
      aux2= PyBytes_FromStringAndSize ( ((const char *) _rom.chrs[n]),
                                        NES_CHR_SIZE );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, n, aux2 );
    }
  
  return dict;
  
 error:
  Py_XDECREF ( dict );
  return NULL;
  
} /* end NES_get_rom */


static PyObject *
NES_get_vram (
              PyObject *self,
              PyObject *args
              )
{

  static NESu8 vram[0x4000];
  
  PyObject *ret;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  NES_ppu_read_vram ( vram );
  ret= PyBytes_FromStringAndSize ( (const char *) &(vram[0]), 0x4000 );
  
  return ret;
  
} /* end NES_get_vram */


static PyObject *
NES_get_obj_ram (
        	 PyObject *self,
        	 PyObject *args
        	 )
{

  static NESu8 obj_ram[256];
  
  PyObject *ret;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  NES_ppu_read_obj_ram ( obj_ram );
  ret= PyBytes_FromStringAndSize ( (const char *) &(obj_ram[0]), 256 );
  
  return ret;
  
} /* end NES_get_obj_ram */


static PyObject *
NES_get_palette (
        	 PyObject *self,
        	 PyObject *args
        	 )
{

  PyObject *ret, *aux, *aux2;
  NES_Color color;
  int i;
  
  
  CHECK_INITIALIZED;
  
  ret= PyTuple_New ( NES_PALETTE_SIZE );
  if ( ret == NULL ) goto error;
  for ( i= 0; i < NES_PALETTE_SIZE; ++i )
    {
      
      aux= PyTuple_New ( 3 );
      if ( aux == NULL ) goto error;
      PyTuple_SET_ITEM ( ret, i, aux );
      
      color= NES_ppu_palette[i];
      aux2= PyLong_FromLong ( color.r );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, 0, aux2 );
      aux2= PyLong_FromLong ( color.g );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, 1, aux2 );
      aux2= PyLong_FromLong ( color.b );
      if ( aux2 == NULL ) goto error;
      PyTuple_SET_ITEM ( aux, 2, aux2 );
      
    }

  return ret;
  
 error:
  Py_DECREF ( ret );
  return NULL;
  
} /* end NES_get_palette */


static PyObject *
NES_init_module (
        	 PyObject *self,
        	 PyObject *args
        	 )
{
  
  const char *err;
  
  
  if ( _initialized ) Py_RETURN_NONE;
  
  /* SDL */
  if ( SDL_Init ( SDL_INIT_VIDEO |
                  SDL_INIT_NOPARACHUTE |
                  SDL_INIT_AUDIO ) == -1 )
    {
      PyErr_SetString ( NESError, SDL_GetError () );
      return NULL;
    }
  _screen.surface= NULL;
  if ( (err= init_audio ()) != NULL )
    {
      PyErr_SetString ( NESError, err );
      SDL_Quit ();
      return NULL; 
    }
  
  /* ROM */
  _rom.prgs= NULL;
  
  /* Tracer. */
  _tracer.obj= NULL;
  
  /* Pad. */
  _control= 0;

  /* Debug. */
  NES_cpu_init_decode ();
  
  _initialized= TRUE;
  
  Py_RETURN_NONE;
  
} /* end NES_init_module */


static PyObject *
NES_loop_module (
        	 PyObject *self,
        	 PyObject *args
        	 )
{
  
  int n;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  for ( n= 0; n < NBUFF; ++n ) _audio.buffers[n].full= 0;
  SDL_PauseAudio ( 0 );
  NES_loop ();
  SDL_PauseAudio ( 1 );
  
  Py_RETURN_NONE;
  
} /* end NES_loop_module */


static PyObject *
NES_set_rom (
             PyObject *self,
             PyObject *args
             )
{
  
  static const NES_TraceCallbacks trace_callbacks=
    {
      mem_access,
      mapper_changed,
      cpu_inst
    };
  static NES_Frontend frontend=
    {
      warning,
      update_screen,
      play_frame,
      check_pad_button1,
      check_pad_button2,
      check_signals,
      &trace_callbacks
    };

  static NESu8 prgram[0x2000];
  
  PyObject *bytes;
  Py_ssize_t size;
  NES_Error err;
  const char *data;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O!", &PyBytes_Type, &bytes ) )
    return NULL;
  
  size= PyBytes_Size ( bytes );
  NES_rom_free ( _rom );
  data= PyBytes_AS_STRING ( bytes );
  if ( NES_rom_load_from_ines_mem ( data, (size_t ) size, &_rom ) != 0 )
    {
      PyErr_SetString ( NESError, "Unable to load the iNES rom" );
      return NULL;
    }
  
  /* Inicialitza el simulador. */
  _control= 0;
  memset ( prgram, 0, sizeof(prgram) );
  update_tvmode ();
  err= NES_init ( &_rom, NES_NTSC, &frontend, prgram, NULL );
  switch ( err )
    {
    case NES_BADROM:
      PyErr_SetString ( NESError, "Bad ROM" );
      goto error;
    case NES_EUNKMAPPER:
      PyErr_SetString ( NESError, "Unknown mapper" );
      goto error;
    default: break;
    }
  
  Py_RETURN_NONE;

 error:
  NES_rom_free ( _rom );
  return NULL;
  
} /* end NES_set_rom */


static PyObject *
NES_set_tracer (
               PyObject *self,
               PyObject *args
               )
{
  
  PyObject *aux;
  
  
  CHECK_INITIALIZED;
  if ( !PyArg_ParseTuple ( args, "O", &aux ) )
    return NULL;
  Py_XDECREF ( _tracer.obj );
  _tracer.obj= aux;
  Py_INCREF ( _tracer.obj );
  
  if ( _tracer.obj != NULL )
    {
      _tracer.has_mem_access= has_method ( _tracer.obj, "mem_access" );
      _tracer.has_mapper_changed= has_method ( _tracer.obj, "mapper_changed" );
      _tracer.has_cpu_inst= has_method ( _tracer.obj, "cpu_inst" );
    }
  
  Py_RETURN_NONE;
  
} /* end NES_set_tracer */


static PyObject *
NES_trace_module (
        	  PyObject *self,
        	  PyObject *args
        	  )
{
  
  int cc;
  
  
  CHECK_INITIALIZED;
  CHECK_ROM;
  
  SDL_PauseAudio ( 0 );
  cc= NES_trace ();
  SDL_PauseAudio ( 1 );
  if ( PyErr_Occurred () != NULL ) return NULL;
  
  return PyLong_FromLong ( cc );
  
} /* end NES_trace_module */




/************************/
/* INICIALITZACIÓ MÒDUL */
/************************/

static PyMethodDef NESMethods[]=
  {
    { "close", NES_close, METH_VARARGS,
      "Free module resources and close the module" },
    { "get_rom", NES_get_rom, METH_VARARGS,
      "Get the ROM structured into a dictionary" },
    { "get_vram", NES_get_vram, METH_VARARGS,
      "Get the current VRAM as a sequence of bytes" },
    { "get_obj_ram", NES_get_obj_ram, METH_VARARGS,
      "Get the current sprite RAM as a sequence of bytes" },
    { "get_rom_mapper_state", NES_get_rom_mapper_state, METH_VARARGS,
      "Get the current state of the ROM mapping into a dictionary" },
    { "get_palette", NES_get_palette, METH_VARARGS,
      "Get the NES palette as a tuple of 512 RGB colors" },
    { "init", NES_init_module, METH_VARARGS,
      "Initialize the module" },
    { "loop", NES_loop_module, METH_VARARGS,
      "Run the simulator into a loop and block" },
    { "set_rom", NES_set_rom, METH_VARARGS,
      "Set a iNES ROM into the simulator. The ROM should be of type bytes."},
    { "set_tracer", NES_set_tracer, METH_VARARGS,
      "Set a python object to trace the execution. The object can"
      " implement one of these methods:\n"
      " - cpu_inst: To trace the cpu execution. Parameters:"
      " --      next addr, instruction mnemonic., address mode., bytes,"
      " tuple with extra info containing: (byte,word,(addr,desp))\n"
      " - mapper_changed: Called every time the memory mapper is reconfigured."
      " No arguments are passed\n"
      " - mem_access: Called every time a memory access is done. The type"
      " of access, the address and the transmitted data "
      "is passed as arguments\n"
    },
    { "trace", NES_trace_module, METH_VARARGS,
      "Executes the next instruction or interruption in trace mode" },
    { NULL, NULL, 0, NULL }
  };


static struct PyModuleDef NESmodule=
  {
    PyModuleDef_HEAD_INIT,
    "NES",
    NULL,
    -1,
    NESMethods
  };


PyMODINIT_FUNC
PyInit_NES (void)
{
  
  PyObject *m;
  
  
  m= PyModule_Create ( &NESmodule );
  if ( m == NULL ) return NULL;
  
  _initialized= FALSE;
  NESError= PyErr_NewException ( "NES.error", NULL, NULL );
  Py_INCREF ( NESError );
  PyModule_AddObject ( m, "error", NESError );
  
  /* Mnemonics. */
  PyModule_AddIntConstant ( m, "ADC", NES_ADC );
  PyModule_AddIntConstant ( m, "AND", NES_AND );
  PyModule_AddIntConstant ( m, "ASL", NES_ASL );
  PyModule_AddIntConstant ( m, "BCC", NES_BCC );
  PyModule_AddIntConstant ( m, "BCS", NES_BCS );
  PyModule_AddIntConstant ( m, "BEQ", NES_BEQ );
  PyModule_AddIntConstant ( m, "BIT", NES_BIT );
  PyModule_AddIntConstant ( m, "BMI", NES_BMI );
  PyModule_AddIntConstant ( m, "BNE", NES_BNE );
  PyModule_AddIntConstant ( m, "BPL", NES_BPL );
  PyModule_AddIntConstant ( m, "BRK", NES_BRK );
  PyModule_AddIntConstant ( m, "BVC", NES_BVC );
  PyModule_AddIntConstant ( m, "BVS", NES_BVS );
  PyModule_AddIntConstant ( m, "CLC", NES_CLC );
  PyModule_AddIntConstant ( m, "CLD", NES_CLD );
  PyModule_AddIntConstant ( m, "CLI", NES_CLI );
  PyModule_AddIntConstant ( m, "CLV", NES_CLV );
  PyModule_AddIntConstant ( m, "CMP", NES_CMP );
  PyModule_AddIntConstant ( m, "CPX", NES_CPX );
  PyModule_AddIntConstant ( m, "CPY", NES_CPY );
  PyModule_AddIntConstant ( m, "DEC", NES_DEC );
  PyModule_AddIntConstant ( m, "DEX", NES_DEX );
  PyModule_AddIntConstant ( m, "DEY", NES_DEY );
  PyModule_AddIntConstant ( m, "EOR", NES_EOR );
  PyModule_AddIntConstant ( m, "INC", NES_INC );
  PyModule_AddIntConstant ( m, "INX", NES_INX );
  PyModule_AddIntConstant ( m, "INY", NES_INY );
  PyModule_AddIntConstant ( m, "JMP", NES_JMP );
  PyModule_AddIntConstant ( m, "JSR", NES_JSR );
  PyModule_AddIntConstant ( m, "LDA", NES_LDA );
  PyModule_AddIntConstant ( m, "LDX", NES_LDX );
  PyModule_AddIntConstant ( m, "LDY", NES_LDY );
  PyModule_AddIntConstant ( m, "LSR", NES_LSR );
  PyModule_AddIntConstant ( m, "NOP", NES_NOP );
  PyModule_AddIntConstant ( m, "ORA", NES_ORA );
  PyModule_AddIntConstant ( m, "PHA", NES_PHA );
  PyModule_AddIntConstant ( m, "PHP", NES_PHP );
  PyModule_AddIntConstant ( m, "PLA", NES_PLA );
  PyModule_AddIntConstant ( m, "PLP", NES_PLP );
  PyModule_AddIntConstant ( m, "ROL", NES_ROL );
  PyModule_AddIntConstant ( m, "ROR", NES_ROR );
  PyModule_AddIntConstant ( m, "RTI", NES_RTI );
  PyModule_AddIntConstant ( m, "RTS", NES_RTS );
  PyModule_AddIntConstant ( m, "SBC", NES_SBC );
  PyModule_AddIntConstant ( m, "SEC", NES_SEC );
  PyModule_AddIntConstant ( m, "SED", NES_SED );
  PyModule_AddIntConstant ( m, "SEI", NES_SEI );
  PyModule_AddIntConstant ( m, "STA", NES_STA );
  PyModule_AddIntConstant ( m, "STX", NES_STX );
  PyModule_AddIntConstant ( m, "STY", NES_STY );
  PyModule_AddIntConstant ( m, "TAX", NES_TAX );
  PyModule_AddIntConstant ( m, "TAY", NES_TAY );
  PyModule_AddIntConstant ( m, "TSX", NES_TSX );
  PyModule_AddIntConstant ( m, "TXA", NES_TXA );
  PyModule_AddIntConstant ( m, "TXS", NES_TXS );
  PyModule_AddIntConstant ( m, "TYA", NES_TYA );
  PyModule_AddIntConstant ( m, "UNK", NES_UNK );
  
  /* MODES */
  PyModule_AddIntConstant ( m, "ABS", NES_ABS );
  PyModule_AddIntConstant ( m, "ABSX", NES_ABSX );
  PyModule_AddIntConstant ( m, "ABSY", NES_ABSY );
  PyModule_AddIntConstant ( m, "IND", NES_IND );
  PyModule_AddIntConstant ( m, "INDX", NES_INDX );
  PyModule_AddIntConstant ( m, "INDY", NES_INDY );
  PyModule_AddIntConstant ( m, "INM", NES_INM );
  PyModule_AddIntConstant ( m, "NONE", NES_NONE );
  PyModule_AddIntConstant ( m, "REL", NES_REL );
  PyModule_AddIntConstant ( m, "ZPG", NES_ZPG );
  PyModule_AddIntConstant ( m, "ZPGX", NES_ZPGX );
  PyModule_AddIntConstant ( m, "ZPGY", NES_ZPGY );
  
  /* TIPUS D'ACCESSOS A MEMÒRIA. */
  PyModule_AddIntConstant ( m, "READ", NES_READ );
  PyModule_AddIntConstant ( m, "WRITE", NES_WRITE );
  
  return m;

} /* end PyInit_NES */
