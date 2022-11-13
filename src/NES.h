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
 *  NES.h - Simulador de la 'NES' escrit en ANSI C.
 *
 */

#ifndef __NES_H__
#define __NES_H__

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


/*********/
/* TIPUS */
/*********/

#if (CHAR_BIT != 8) || (USHRT_MAX != 65535U) || (UINT_MAX != 4294967295U)
#error Arquitectura no suportada
#endif

/* Tipus booleà. */
typedef enum
  {
    NES_FALSE= 0,
    NES_TRUE
  } NES_Bool;

/* Tipus sencers. */
typedef signed char NESs8;
typedef unsigned char NESu8;
typedef unsigned short NESu16;

/* Error */
typedef enum
  {
    NES_NOERROR=0,         /* No hi ha cap error. */
    NES_BADROM,            /* El contingut de la ROM és incoherent. */
    NES_EUNKMAPPER         /* El mapper de la ROM és desconegut. */
  } NES_Error;

/* Color en format RGB. */
typedef struct
{
  NESu8 r;
  NESu8 g;
  NESu8 b;
  
} NES_Color;

/* Funció per a metre avísos. */
typedef void 
(NES_Warning) (
               void       *udata,
               const char *format,
               ...
               );


/*********/
/* DEBUG */
/*********/

/* Identificador d'instruccions. */
typedef enum
  {
    NES_ADC= 0,
    NES_AND,
    NES_ASL,
    NES_BCC,
    NES_BCS,
    NES_BEQ,
    NES_BIT,
    NES_BMI,
    NES_BNE,
    NES_BPL,
    NES_BRK,
    NES_BVC,
    NES_BVS,
    NES_CLC,
    NES_CLD,
    NES_CLI,
    NES_CLV,
    NES_CMP,
    NES_CPX,
    NES_CPY,
    NES_DEC,
    NES_DEX,
    NES_DEY,
    NES_EOR,
    NES_INC,
    NES_INX,
    NES_INY,
    NES_JMP,
    NES_JSR,
    NES_LDA,
    NES_LDX,
    NES_LDY,
    NES_LSR,
    NES_NOP,
    NES_ORA,
    NES_PHA,
    NES_PHP,
    NES_PLA,
    NES_PLP,
    NES_ROL,
    NES_ROR,
    NES_RTI,
    NES_RTS,
    NES_SBC,
    NES_SEC,
    NES_SED,
    NES_SEI,
    NES_STA,
    NES_STX,
    NES_STY,
    NES_TAX,
    NES_TAY,
    NES_TSX,
    NES_TXA,
    NES_TXS,
    NES_TYA,
    NES_UNK
  } NES_Mnemonic;

/* Modes de direccionament de memòria. */
typedef enum
  {
    NES_ABS,
    NES_ABSX,
    NES_ABSY,
    NES_IND,
    NES_INDX,
    NES_INDY,
    NES_INM,
    NES_NONE,
    NES_REL,
    NES_ZPG,
    NES_ZPGX,
    NES_ZPGY
  } NES_AddressMode;

typedef struct
{

  NES_Mnemonic    name;
  NES_AddressMode addr_mode;
  
} NES_InstId;


/* Dades relacionades amb els operadors. */
typedef union
{

  NESu8  valu8; /* INDX, INDY, INM */
  NESu16 valu16; /* ABS, ABSX, ABSY, IND, ZPG, ZPGX, ZPGY */
  struct
  {
    NESu16 addr;
    NESs8  desp;
  }      branch; /* REL */
  
} NES_InstExtra;

/* Estructura per a desar tota la informació relativa a una
 * instrucció.
 */
typedef struct
{
  
  NES_InstId    id;          /* Identificador d'instrucció. */
  NES_InstExtra e;           /* Dades extra segons el tipus de
        			direccionament.. */
  NESu8         bytes[3];    /* Bytes */
  int           nbytes;
  
} NES_Inst;

/* Descodifica la instrucció de l'adreça indicada. */
NESu16
NES_cpu_decode (
                NESu16    addr,
                NES_Inst *inst
                );

void
NES_cpu_init_decode (void);

NESu16
NES_cpu_decode_next_inst (
                          NES_Inst *step
                          );

/* Tipus de funció per a saber quin a sigut l'últim pas d'execució de
 * la UCP.
 */
typedef void (NES_CPUInst) (
                            const NES_Inst *inst,        /* Punter a
                                                            pas
                                                            d'execuió. */
                            const NESu16    nextaddr,    /* Següent
                                                            adreça de
                                                            memòria. */
                            void           *udata
                            );


/*******/
/* ROM */
/*******/

/* Tipus de mapper. */
typedef enum
  {
    NES_AOROM,
    NES_CNROM,
    NES_MMC1,
    NES_MMC2,
    NES_MMC3,
    NES_NROM,
    NES_UNROM,
    NES_UNKMAPPER
  } NES_Mapper;

extern const char *NES_mapper_names_[];
/* Torna una cadena de caràcters, que no s'ha de modificar, amb el nom
 * del 'Mapper' especificat.
 */
#define NES_mapper_name(MAPPER) (NES_mapper_names_[(MAPPER)])

/* Tipus de televisor. */
typedef enum
  {
    NES_PAL= 0,
    NES_NTSC
  } NES_TVMode;

/* Tipus de 'mirroring'. */
typedef enum
  {
    NES_SINGLE, /* Aquest mirroring no el tenes les rom amb mirroring
        	   fixe. */
    NES_HORIZONTAL,
    NES_VERTICAL,
    NES_FOURSCREEN
  } NES_Mirroring;

/* Grandària d'una pàgina CHR. */
#define NES_CHR_SIZE 8192

/* Pàgina CHR. */
typedef NESu8 NES_CHR[NES_CHR_SIZE];

/* Valor nul per al tipus 'NES_CHR'. */
#define NES_CHR_NULL ((NES_CHR *) NULL)

/* Grandària d'una pàgina PRG. */
#define NES_PRG_SIZE 16384

/* Pàgina PRG. */
typedef NESu8 NES_PRG[NES_PRG_SIZE];

/* Valor nul per al tipus 'NES_PRG'. */
#define NES_PRG_NULL ((NES_PRG *) NULL)

/* Estructura per a ser inicialitzada per el frontend. */
typedef struct
{
  
  int            nprg;         /* Número de pàgines de codi de 16K. */
  int            nchr;         /* Número de pàgines d'imatges de
        			  8K. */
  NES_Mapper     mapper;       /* 'Mapper'. */
  NES_TVMode     tvmode;       /* Tipus de televisor. */
  NES_Mirroring  mirroring;    /* Tipus de 'mirroring' inicial. */
  NES_Bool       sram;         /* A cert si hi ha memòria RAM
        			  estàtica. */
  NES_PRG       *prgs;         /* Pàgines PRG. */
  NES_CHR       *chrs;         /* Pàgines CHR. */
  NESu8         *trainer;      /* Els 512 bytes de trainer. NULL si no
        			  en té. */
  
} NES_Rom;

/* Reserva la memòria per a les pàgines PRG d'una variable 'NES_Rom'.
 * Si la variable continua a NULL s'ha produït un error.
 */
#define NES_rom_alloc_prgs(ROM)                                   \
  ((ROM).prgs= (NES_PRG *) malloc ( sizeof(NES_PRG)*(ROM).nprg ))

/* Reserva la memòria per a les pàgines CHR d'una variable 'NES_Rom'.
 * Si la variable continua a NULL s'ha produït un error.
 */
#define NES_rom_alloc_chrs(ROM)                                   \
  ((ROM).chrs= (NES_CHR *) malloc ( sizeof(NES_CHR)*(ROM).nchr ))

/* Reserva memòria per al 'trainer' d'una variables 'NES_Rom'.
 * Si la variable continua a NULL s'ha produït un error.
 */
#define NES_rom_alloc_trainer(ROM)        	\
  ((ROM).trainer= (NESu8 *) malloc ( 512 ))

/* Allibera la memòria utilizada per a es pàgines. Aquesta macro
 * comprova que 'prgs' i 'chrs' no siguen NULL.
 */
#define NES_rom_free(ROM)        			\
  do{        						\
    if ( (ROM).prgs != NULL ) free ( (ROM).prgs );        \
    if ( (ROM).chrs != NULL ) free ( (ROM).chrs );        \
    if ( (ROM).trainer != NULL ) free ( (ROM).trainer); \
  }while(0)

/* Llig una ROM en 'rom' des d'un fitxer en format iNES. 'rom' no cal
 * que estiga inicialitzat. Torna -1 si el format és erroni o no s'ha
 * pogut llegir tot el contingut, 0 si tot ha anat bé.
 * ATENCIÓ!!! Aquesta funció pot tornar una rom amb un mapper desconegut.
 */
int
NES_rom_load_from_ines (
        		FILE    *f,
        		NES_Rom *rom
        		);

int
NES_rom_load_from_ines_mem (
        		    const char *bytes,
        		    size_t      nbytes,
        		    NES_Rom    *rom
        		    );


/**********/
/* MAPPER */
/**********/
/* Mòdul per a mapejar la ROM en la memòria de la NES. */

/* Tipus de la funció que es cridada cada vegada que la disposició del
 * mapper canvia.
 */
typedef void (NES_MapperChanged) (
        			  void *udata
        			  );

/* Inicialitza i reseteja el mapper. Es pot cridar tantes voltes com
 * es vulga, però sempre abans que qualsevol altra funció del mòdul.
 * Torna NES_BADROM si la rom no té un contingut incoherent.
 */
NES_Error
NES_mapper_init (
        	 const NES_Rom     *rom,        /* ROM a mapejar. */
        	 NES_Warning       *warning,    /* Funció on mostrar
        					   avisos. */
        	 NES_MapperChanged *mapper_changed,
        	 void              *udata      /* Dades del usuari. */
        	 );

extern void
(*NES_mapper_init_state) (void);

/* Llig un byte de l'adreça indicada. Comença a contar de 0, màxim
 * valor 0x7FFF.
 */
extern NESu8
(*NES_mapper_read) (
        	    const NESu16 addr
        	    );

/* Reseteja el mòdul sense canviar la rom. */
extern void
(*NES_mapper_reset) (void);

/* Escriu un byte en l'adreça indicada. Comença a contar de 0, màxim
 * valor 0x7FFF.
 */
extern void
(*NES_mapper_write) (
        	     const NESu16 addr,
        	     const NESu8  data
        	     );

/* Escriu un byte en l'adreça indicada de VRAM del mapper. Comença a
 * contar de 0 màxim valor 0x2FFF.
 *
 */
extern NESu8
(*NES_mapper_vram_read) (
        		 const NESu16 addr
        		 );

/* Escriu un byte en l'adreça indicada de VRAM del mapper. Comença a
 * contar de 0, màxim valor 0x2FFF.
 */
extern void
(*NES_mapper_vram_write) (
        		  const NESu16 addr,
        		  const NESu8  data
        		  );

/* Tipus per a emmagatzemar l'estat del mapejador de memòria de ROM
   dividit en pàgines de 8K. */
typedef struct
{
  
  int p0;       /* Pàgina en [$8000-$9FFF]. */
  int p1;       /* Pàgina en [$A000-$BFFF]. */
  int p2;       /* Pàgina en [$C000-$DFFF]. */
  int p3;       /* Pàgina en [$E000-$FFFF]. */
  
} NES_RomMapperState;

/* Obté en la variable indicada l'estat actual del mapejat de la ROM.
 */
extern void
(*NES_mapper_get_rom_mapper_state) (
        			    NES_RomMapperState *state
        			    );

/* Activa/Desactiva el mode traça en el mòdul del mapper. */
extern void
(*NES_mapper_set_mode_trace) (
        		      const NES_Bool val
        		      );

extern int
(*NES_mapper_save_state) (
        		  FILE *f
        		  );

extern int
(*NES_mapper_load_state) (
        		  FILE *f
        		  );


/*******/
/* MEM */
/*******/
/* Mòdul que simula el mapa de memòria, i com es mapeja, de la NES. */

/* Tipus d'accessos a memòria. */
typedef enum
  {
    NES_READ,
    NES_WRITE
  } NES_MemAccessType;


/* Tipus de la funció per a fer una traça dels accessos a
 * memòria. Cada vegada que es produeix un accés a memòria es crida.
 */
typedef void (NES_MemAccess) (
        		      void                    *udata,
                              const NES_MemAccessType type,
                              const NESu16            addr,
                              const NESu8             data
        		      );

/* Inicialitza el mòdul s'ha de cridar abans de les demés funcions. */
void
NES_mem_init (
              NESu8          prgram[0x2000],    /* RAM del
        					   cartutx. Pot ser
        					   NULL per a indicar
        					   que no en té. */
              const NESu8   *trdata,            /* Trainer. A NULL si
        					   no en té. */
              NES_Warning   *warning,           /* Funció on mostrar
        					   avisos. */
              NES_MemAccess *mem_access,        /* Pot ser NULL. */
              void          *udata
              );

void
NES_mem_init_state (void);

/* Llig un byte de l'adreça indicada. */
NESu8
NES_mem_read (
              const NESu16 addr
              );

/* Escriu un byte en l'adreça indicada. */
void
NES_mem_write (
               const NESu16 addr,
               const NESu8  data
               );

/* Activa/Desactiva el mode traça en el mòdul de memòria. */
void
NES_mem_set_mode_trace (
        		const NES_Bool val
        		);

int
NES_mem_save_state (
        	    FILE *f
        	    );

int
NES_mem_load_state (
        	    FILE *f
        	    );


/*******/
/* PPU */
/*******/
/* Mòdul que simula el xip gràfic de la NES. */

#define NES_PALETTE_SIZE 512

/* Paleta de colors de la PPU. */
extern const NES_Color NES_ppu_palette[NES_PALETTE_SIZE];

/* Tipus de la funció que actualitza la pantalla real. */
/* ATENCIÓ!!! Aquesta funció sempre genera frames de 256x240. Ara bé,
   en NTSC les 8 primeres files i les 8 últimes en principi no es
   deurien veure. En alguns jocs es pot apreciar en eixes línies com
   es modifica el scroll. */
typedef void (NES_UpdateScreen) (
        			 const int *fb,
        			 void      *udata
        			 );

/* Files en la pantalla segons el tipus de televisor. */
#define NES_PPU_PAL_ROWS 240
#define NES_PPU_NTSC_ROWS 224

/* Columnes. */
#define NES_PPU_COLS 256

/* Processa clocks de UCP. */
void
NES_ppu_clock (
               int cc
               );

/* Registre de control 1. */
void
NES_ppu_CR1 (
             NESu8 byte
             );

/* Registre de control 2. */
void
NES_ppu_CR2 (
             NESu8 byte
             );

/* Inicialitza PPU, requereix que s'haja incialitzat previament el
 * MAPPER i la MEM.
 */
void
NES_ppu_init (
              const NES_TVMode  tvmode,
              const NES_Mapper  mapper,
              NES_UpdateScreen *update_screen,
              void             *udata
              );

void
NES_ppu_init_state (void);

/* La PPU està implementada de manera què va acumulant cicles i no els
   executa fins que es reconfigura o té prou cicles per produir un
   event com una interrupció. No obstant, algunes parts de l'estat
   intern, com per exemple com està mapejada la VRAM, estan
   controlades per xips externs. Per tant, amb aquesta funció podem
   forçar a la PPU a consumir els cicles que té pendents. */
void
NES_ppu_sync (void);

/* Accés directe a memòria. Copia de $(BYTE)00 256 bytes a la memòria
 * d'objectes.
 */
void
NES_ppu_DMA (
             NESu8 byte
             );

/* Llig un byte de la memòria. */
NESu8
NES_ppu_read (void);

/* Reseteja la PPU. */
void
NES_ppu_reset (void);

/* Llig un byte de la memòria d'objectes. */
NESu8
NES_ppu_SPRAM_read (void);

/* Fixa la posició en la memòria d'objectes on començar a escriure o
 * llegir.
 */
void
NES_ppu_SPRAM_set_offset (
        		  NESu8 byte
        		  );

/* Escriu un byte en la memòria d'objectes. */
void
NES_ppu_SPRAM_write (
        	     NESu8 byte
        	     );

/* Renderitza la següent 'scanline'. */
NES_Bool
NES_ppu_scanline (void);

/* Registre per a controlar el 'scroll'. */
void
NES_ppu_scrolling (
        	   NESu8 byte
        	   );

/* Per a fixar l'adreça d'on llegir/escriure el següent byte. */
void
NES_ppu_set_addr (
        	  NESu8 byte
        	  );

/* Torna l'estat de la PPU. */
NESu8
NES_ppu_status (void);

/* Escriu un byte en la memòria de la PPU. */
void
NES_ppu_write (
               NESu8 byte
               );

/* Llig l'estat actual de la VRAM. És a dir torna el contingut actual
 * entre [0000:3FFF. És posible que internament tinga mirroring o
 * pàgines de VROM.
 */
void
NES_ppu_read_vram (
        	   NESu8 vram[0x4000]
        	   );

void
NES_ppu_read_obj_ram (
        	      NESu8 obj_ram[256]
        	      );

int
NES_ppu_save_state (
        	    FILE *f
        	    );

int
NES_ppu_load_state (
        	    FILE *f
        	    );


/***********/
/* JOYPADS */
/***********/
/* Mòdul que simula la connexió dels mandos. */

/* Botons d'un mando normal. ATENCIÓ!!! no és una mascara de bits.*/
typedef enum
  {
    NES_A= 0,
    NES_B,
    NES_SELECT,
    NES_START,
    NES_UP,
    NES_DOWN,
    NES_LEFT,
    NES_RIGHT
  } NES_PadButton;

/* Tipus d'una funció que comprova l'estat dels botons. */
typedef NES_Bool (NES_CheckPadButton) (
        			       NES_PadButton  button,
        			       void          *udata
        			       );
        	     
/* Inicialitza Joypads. */
void
NES_joypads_init (
        	  NES_CheckPadButton *cpb1,
        	  NES_CheckPadButton *cpb2,
        	  void               *udata
        	  );

void
NES_joypads_init_state (void);
        			      
/* Llig l'estat del mando 1. */
NESu8
NES_joypads_pad1_read (void);

/* Llig l'estat del mando 2. */
NESu8
NES_joypads_pad2_read (void);

/* anipula el 'strobe' que ara mateixa no se que és. */
void
NES_joypads_strobe (
        	    NESu8 data
        	    );

/* El port d'expansió no està suportat. */
void
NES_joypads_EPL (
        	 NESu8 data
        	 );

/* Reseteja el mòdul JOYPADS. */
void
NES_joypads_reset (void);

int
NES_joypads_save_state (
        		FILE *f
        		);

int
NES_joypads_load_state (
        		FILE *f
        		);				      

        			      
/*******/
/* CPU */
/*******/
/* Mòdul que implementa la UCP de la 'NES'. */

/* Velocitat del processador. */
#define NES_CPU_PAL_CYCLES_PER_SEC 1662607
#define NES_CPU_NTSC_CYCLES_PER_SEC 1789773
        			      
/* Realitza una interrupció no enmascarable. */
void
NES_cpu_NMI (void);

/* Realitza una interrupció. */
void
NES_cpu_IRQ (void);

/* Inicialitza el mòdul de la UCP. */
void
NES_cpu_init (
              NES_Warning *warning,    /* Funció per a mostrar
        				  avisos. */
              void        *udata       /* Dades del usuari. */
              );

void
NES_cpu_init_state (void);
        			      
/* Reinicia la UCP. No es pot executar a la vegada que una
 * instrucció. Crida a NES_mapper_reset.
 */
void
NES_cpu_reset (void);

/* Executa la següent instrucció, torna els cicles consumits. */
int
NES_cpu_run (void);

int
NES_cpu_save_state (
        	    FILE *f
        	    );

int
NES_cpu_load_state (
        	    FILE *f
        	    );


/*******/
/* APU */
/*******/
/* Mòdul que simula el xip de sò de la NES. */

/* Número de mostres (cada dura un cicle d'UCP) que té cadascun dels
 * buffers que genera el xip de sò. Al voltant d'una centèsima de
 * segon, depen de si és PAL o NTSC.
 */
#define NES_APU_BUFFER_SIZE 17000

/* Tipus de la funció que reprodueix un 'frame' de sò. */
typedef void (NES_PlayFrame) (
        		      const double  frame[NES_APU_BUFFER_SIZE],
        		      void         *udata
        		      );

/* 'Clockeja' l'apu. Torna cert si s'ha produït una interrupció
 * IRQ. Se li passa un punter amb els cicles que s'ha produït, es a
 * dir els clocks que ha de fer. Este número de cicles pot ser
 * actualitzat per culpa del DMA del DMC que es va executant mentre
 * s'executa.
 */
NES_Bool
NES_apu_clock (
               unsigned int *cc
               );
        			      
/* Configura el 'Frame Sequencer'. */
void
NES_apu_conf_fseq (
        	   NESu8 data
        	   );

/* APU Channel Control. */
void
NES_apu_control (
        	 NESu8 data
        	 );

/* APU Soud/Vertical Clock Signal Register. */
NESu8
NES_apu_CSR (void);
              
/* Inicialitza APU. */
void
NES_apu_init (
              const NES_TVMode  tvmode,
              NES_PlayFrame    *play_frame,
              void             *udata
              );

void
NES_apu_init_state (void);

/* Reseteja l'APU. */
void
NES_apu_reset (void);

/* Pulse #1 Control Register. */
void
NES_apu_pulse1CR (
        	  NESu8 data
        	  );

/* Pulse #1 Ramp Control Register. */
void
NES_apu_pulse1RCR (
        	   NESu8 data
        	   );

/* Pulse #1 Fine Tune Register. */
void
NES_apu_pulse1FTR (
        	   NESu8 data
        	   );

/* Pulse #1 Coarse Tune Register. */
void
NES_apu_pulse1CTR (
        	   NESu8 data
        	   );

/* Pulse #2 Control Register */
void
NES_apu_pulse2CR (
        	  NESu8 data
        	  );

/* Pulse #2 Ramp Control Register. */
void
NES_apu_pulse2RCR (
        	   NESu8 data
        	   );

/* Pulse #2 Fine Tune Register. */
void
NES_apu_pulse2FTR (
        	   NESu8 data
        	   );

/* Pulse #2 Coarse Tune Register. */
void
NES_apu_pulse2CTR (
        	   NESu8 data
        	   );

/* Triangle Control Register #1. */
void
NES_apu_triangleCR1 (
        	     NESu8 data
        	     );

/* Triangle Frequency Register #1. */
void
NES_apu_triangleFR1 (
        	     NESu8 data
        	     );

/* Triangle Frequency Register #2. */
void
NES_apu_triangleFR2 (
        	     NESu8 data
        	     );

/* Noise Control Register #1. */
void
NES_apu_noiseCR (
        	 NESu8 data
        	 );

/* Noise Frequency Register #1. */
void
NES_apu_noiseFR1 (
        	  NESu8 data
        	  );

/* Noise Frequency Register #2. */
void
NES_apu_noiseFR2 (
        	  NESu8 data
        	  );

/* Delta Modulation Control Register. */
void
NES_apu_dmCR (
              NESu8 data
              );

/* Delta Modulation D/A Register. */
void
NES_apu_dmDAR (
               NESu8 data
               );

/* Delta Modulation Address Register. */
void
NES_apu_dmAR (
              NESu8 data
              );

/* Delta Modulation Data Length Register. */
void
NES_apu_dmLR (
              NESu8 data
              );

int
NES_apu_save_state (
        	    FILE *f
        	    );

int
NES_apu_load_state (
        	    FILE *f
        	    );


/*******/
/* DMA */
/*******/
/* Mòdul per a DMA. */

/* Variable global emprada per a emmgatzemar cicles extres deguts a
 * operacions de DMA.
 */
extern int NES_dma_extra_cc;


/********/
/* MAIN */
/********/
/* Funcions que un usuari normal deuria usar. */

/* Tipus de funció amb la que el 'frontend' indica a la llibreria si
 * s'ha produït una senyal de reset o de parada. A més esta funció pot
 * ser emprada per el frontend per a tractar els events pendents.
 */
typedef void (NES_CheckSignals) (
        			 NES_Bool *reset,
        			 NES_Bool *stop,
        			 void     *udata
        			 );

/* No tots els camps tenen que ser distint de NULL. */
typedef struct
{

  NES_MemAccess     *mem_access;
  NES_MapperChanged *mapper_changed;
  NES_CPUInst       *cpu_inst;
  
} NES_TraceCallbacks;

/* Conté la informació necessària per a comunicar-se amb el
 * 'frontend'.
 */
typedef struct
{
  
  NES_Warning              *warning;          /* Funció per a mostrar
        					 avisos. */
  NES_UpdateScreen         *update_screen;    /* Actualitza la pantalla. */
  NES_PlayFrame            *play_frame;       /* Reprodueix un 'frame'
        					 de sò. */
  NES_CheckPadButton       *cpb1;             /* Comprova l'estat del
        					 pad 1 (referint-se al
        					 port, no al mando
        					 físic). */
  NES_CheckPadButton       *cpb2;             /* Comprova l'estat del
        					 pad 2. */
  NES_CheckSignals         *check;            /* Comprova si ha de
        					 parar o reiniciar. */
  const NES_TraceCallbacks *trace;            /* Pot ser NULL si no es
        					 van a gastar les
        					 funcions per a fer
        					 una traça. */
  
} NES_Frontend;

/* Inicialitza la llibreria, s'ha de cridar cada vegada que s'inserte
 * una nova rom. Torna NES_NOERROR si tot ha anat bé.
 */
NES_Error
NES_init (
          const NES_Rom      *rom,               /* ROM. */
          const NES_TVMode    tvmode,            /* PAL/NTSC, ignora
        					    el camp de la
        					    rom. */
          const NES_Frontend *frontend,          /* Frontend. */
          NESu8               prgram[0x2000],    /* RAM del
        					    cartutx. Pot ser
        					    NULL per a indicar
        					    que no en té. */
          void               *udata              /* Dades proporcionades per
        					    l'usuari que són pasades
        					    al 'frontend'. */
          );

/* Executa un cicle de la NES. Aquesta funció executa una
 * iteració de 'NES_loop' i torna els cicles de UCP emprats. Si
 * CHECKSIGNALS en el frontend no és NULL aleshores cada cert temps al
 * cridar a MD_iter es fa una comprovació de CHECKSIGNALS.  La funció
 * CHECKSIGNALS del frontend es crida amb una freqüència suficient per
 * a que el frontend tracte els seus events. La senyal stop de
 * CHECKSIGNALS és llegit en STOP si es crida a CHECKSIGNALS.
 */
int
NES_iter (
          NES_Bool *stop
          );

/* Carrega l'estat de 'f'. Torna 0 si tot ha anat bé. S'espera que el
 * fitxer siga un fitxer d'estat vàlid de NES per a la ROM actual. Si
 * es produeix un error de lectura o es compromet la integritat del
 * simulador, aleshores es reiniciarà el simulador.
 */
int
NES_load_state (
        	FILE *f
        	);

/* Executa la 'NES'. Aquesta funció es bloqueja fins que llig una
 * senyal de parada mitjançant CHECKSIGNALS, si es para es por tornar
 * a cridar i continuarà on s'havia quedat. La funció CHECKSIGNALS del
 * frontend es crida amb una freqüència suficient per a que el
 * frontend tracte els seus events. Si en un mateix instant està actiu
 * RESET i STOP, primer es reinicia i després es para.
 */
void
NES_loop (void);

/* Escriu en 'f' l'estat de la màquina. Torna 0 si tot ha anat bé, -1
 * en cas contrari.
 */
int
NES_save_state (
        	FILE *f
        	);

/* Executa els següent pas de UCP en mode traça. Tots aquelles
 * funcions de 'callback' que no són nul·les es cridaran si és el
 * cas. Torna el clocks de rellotge executats en l'últim pas.
 */
int
NES_trace (void);

#endif /* __NES_H__ */
