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
 *  op.h - Definició de les operacions de la UCP 6502
 *         de la 'NES'.
 *
 *  Hi han per ahí alguns documents on els cicles no són exactament
 *  iguals, a continuació les diferències que he trobat.
 *
 *   < OP ( 0x05,  ORA,  ZPG1, 3 )
 *   ---
 *   > OP ( 0x05,  ORA,  ZPG1, 2 )
 *
 *   < OP ( 0x15,  ORA, ZPGX1, 4 )
 *   ---
 *   > OP ( 0x15,  ORA, ZPGX1, 3 )
 *
 *   < OP ( 0x25,  AND,  ZPG1, 3 )
 *   ---
 *   > OP ( 0x25,  AND,  ZPG1, 2 )
 *
 *   < OP ( 0x35,  AND, ZPGX1, 4 )
 *   ---
 *   > OP ( 0x35,  AND, ZPGX1, 3 )
 *
 */

/* OP ( opcode, op_name, addr_mode, cicles base ) */

OP ( 0x00,  BRK,  NONE, 7 )
OP ( 0x01,  ORA, INDX1, 6 )

OP ( 0x05,  ORA,  ZPG1, 3 )
OP ( 0x06, ASL1,  ZPG0, 5 )

OP ( 0x08,  PHP,  NONE, 3 )
OP ( 0x09,  ORA,   INM, 2 )
OP ( 0x0A, ASL0,  NONE, 2 )

OP ( 0x0D,  ORA,  ABS1, 4 )
OP ( 0x0E, ASL1,  ABS0, 6 )

OP ( 0x10,  BPL,   REL, 2 )
OP ( 0x11,  ORA, INDY1, 5 )

OP ( 0x15,  ORA, ZPGX1, 4 )
OP ( 0x16, ASL1, ZPGX0, 6 )

OP ( 0x18,  CLC,  NONE, 2 )
OP ( 0x19,  ORA, ABSY1, 4 )

OP ( 0x1D,  ORA, ABSX1, 4 )
OP ( 0x1E, ASL1, ABSX0, 7 )

OP ( 0x20,  JSR,  ABS0, 6 )
OP ( 0x21,  AND, INDX1, 6 )

OP ( 0x24,  BIT,  ZPG1, 3 )
OP ( 0x25,  AND,  ZPG1, 3 )
OP ( 0x26, ROL1,  ZPG0, 5 )

OP ( 0x28,  PLP,  NONE, 4 )
OP ( 0x29,  AND,   INM, 2 )
OP ( 0x2A, ROL0,  NONE, 2 )

OP ( 0x2C,  BIT,  ABS1, 4 )
OP ( 0x2D,  AND,  ABS1, 4 )
OP ( 0x2E, ROL1,  ABS0, 6 )

OP ( 0x30,  BMI,   REL, 2 )
OP ( 0x31,  AND, INDY1, 5 )

OP ( 0x35,  AND, ZPGX1, 4 )
OP ( 0x36, ROL1, ZPGX0, 6 )

OP ( 0x38,  SEC,  NONE, 2 )
OP ( 0x39,  AND, ABSY1, 4 )

OP ( 0x3D,  AND, ABSX1, 4 )
OP ( 0x3E, ROL1, ABSX0, 7 )

OP ( 0x40,  RTI,  NONE, 6 )
OP ( 0x41,  EOR, INDX1, 6 )

OP ( 0x45,  EOR,  ZPG1, 3 )
OP ( 0x46, LSR1,  ZPG0, 5 )

OP ( 0x48,  PHA,  NONE, 3 )
OP ( 0x49,  EOR,   INM, 2 )
OP ( 0x4A, LSR0,  NONE, 2 )

OP ( 0x4C,  JMP,  ABS0, 3 )
OP ( 0x4D,  EOR,  ABS1, 4 )
OP ( 0x4E, LSR1,  ABS0, 6 )

OP ( 0x50,  BVC,   REL, 2 )
OP ( 0x51,  EOR, INDY1, 5 )

OP ( 0x55,  EOR, ZPGX1, 4 )
OP ( 0x56, LSR1, ZPGX0, 6 )

OP ( 0x58,  CLI,  NONE, 2 )
OP ( 0x59,  EOR, ABSY1, 4 )

OP ( 0x5D,  EOR, ABSX1, 4 )
OP ( 0x5E, LSR1, ABSX0, 7 )

OP ( 0x60,  RTS,  NONE, 6 )
OP ( 0x61,  ADC, INDX1, 6 )

OP ( 0x65,  ADC,  ZPG1, 3 )
OP ( 0x66, ROR1,  ZPG0, 5 )

OP ( 0x68,  PLA,  NONE, 4 )
OP ( 0x69,  ADC,   INM, 2 )
OP ( 0x6A, ROR0,  NONE, 2 )

OP ( 0x6C,  JMP,   IND, 5 )
OP ( 0x6D,  ADC,  ABS1, 4 )
OP ( 0x6E, ROR1,  ABS0, 6 )

OP ( 0x70,  BVS,   REL, 2 )
OP ( 0x71,  ADC, INDY1, 5 )

OP ( 0x75,  ADC, ZPGX1, 4 )
OP ( 0x76, ROR1, ZPGX0, 6 )

OP ( 0x78,  SEI,  NONE, 2 )
OP ( 0x79,  ADC, ABSY1, 4 )

OP ( 0x7D,  ADC, ABSX1, 4 )
OP ( 0x7E, ROR1, ABSX0, 7 )

OP ( 0x81,  STA, INDX0, 6 )

OP ( 0x84,  STY,  ZPG0, 3 )
OP ( 0x85,  STA,  ZPG0, 3 )
OP ( 0x86,  STX,  ZPG0, 3 )

OP ( 0x88,  DEY,  NONE, 2 )

OP ( 0x8A,  TXA,  NONE, 2 )

OP ( 0x8C,  STY,  ABS0, 4 )
OP ( 0x8D,  STA,  ABS0, 4 )
OP ( 0x8E,  STX,  ABS0, 4 )

OP ( 0x90,  BCC,   REL, 2 )
OP ( 0x91,  STA, INDY0, 6 )

OP ( 0x94,  STY, ZPGX0, 4 )
OP ( 0x95,  STA, ZPGX0, 4 )
OP ( 0x96,  STX, ZPGY0, 4 )

OP ( 0x98,  TYA,  NONE, 2 )
OP ( 0x99,  STA, ABSY0, 5 )
OP ( 0x9A,  TXS,  NONE, 2 )

OP ( 0x9D,  STA, ABSX0, 5 )

OP ( 0xA0,  LDY,   INM, 2 )
OP ( 0xA1,  LDA, INDX1, 6 )
OP ( 0xA2,  LDX,   INM, 2 )

OP ( 0xA4,  LDY,  ZPG1, 3 )
OP ( 0xA5,  LDA,  ZPG1, 3 )
OP ( 0xA6,  LDX,  ZPG1, 3 )

OP ( 0xA8,  TAY,  NONE, 2 )
OP ( 0xA9,  LDA,   INM, 2 )
OP ( 0xAA,  TAX,  NONE, 2 )

OP ( 0xAC,  LDY,  ABS1, 4 )
OP ( 0xAD,  LDA,  ABS1, 4 )
OP ( 0xAE,  LDX,  ABS1, 4 )

OP ( 0xB0,  BCS,   REL, 2 )
OP ( 0xB1,  LDA, INDY1, 5 )

OP ( 0xB4,  LDY, ZPGX1, 4 )
OP ( 0xB5,  LDA, ZPGX1, 4 )
OP ( 0xB6,  LDX, ZPGY1, 4 )

OP ( 0xB8,  CLV,  NONE, 2 )
OP ( 0xB9,  LDA, ABSY1, 4 )
OP ( 0xBA,  TSX,  NONE, 2 )

OP ( 0xBC,  LDY, ABSX1, 4 )
OP ( 0xBD,  LDA, ABSX1, 4 )
OP ( 0xBE,  LDX, ABSY1, 4 )

OP ( 0xC0,  CPY,   INM, 2 )
OP ( 0xC1,  CMP, INDX1, 6 )

OP ( 0xC4,  CPY,  ZPG1, 3 )
OP ( 0xC5,  CMP,  ZPG1, 3 )
OP ( 0xC6,  DEC,  ZPG0, 5 )

OP ( 0xC8,  INY,  NONE, 2 )
OP ( 0xC9,  CMP,   INM, 2 )
OP ( 0xCA,  DEX,  NONE, 2 )

OP ( 0xCC,  CPY,  ABS1, 4 )
OP ( 0xCD,  CMP,  ABS1, 4 )
OP ( 0xCE,  DEC,  ABS0, 6 )

OP ( 0xD0,  BNE,   REL, 2 )
OP ( 0xD1,  CMP, INDY1, 5 )

OP ( 0xD5,  CMP, ZPGX1, 4 )
OP ( 0xD6,  DEC, ZPGX0, 6 )

OP ( 0xD8,  CLD,  NONE, 2 )
OP ( 0xD9,  CMP, ABSY1, 4 )

OP ( 0xDD,  CMP, ABSX1, 4 )
OP ( 0xDE,  DEC, ABSX0, 7 )

OP ( 0xE0,  CPX,   INM, 2 )
OP ( 0xE1,  SBC, INDX1, 6 )

OP ( 0xE4,  CPX,  ZPG1, 3 )
OP ( 0xE5,  SBC,  ZPG1, 3 )
OP ( 0xE6,  INC,  ZPG0, 5 )

OP ( 0xE8,  INX,  NONE, 2 )
OP ( 0xE9,  SBC,   INM, 2 )
OP ( 0xEA,  NOP,  NONE, 2 )

OP ( 0xEC,  CPX,  ABS1, 4 )
OP ( 0xED,  SBC,  ABS1, 4 )
OP ( 0xEE,  INC,  ABS0, 6 )

OP ( 0xF0,  BEQ,   REL, 2 )
OP ( 0xF1,  SBC, INDY1, 5 )

OP ( 0xF5,  SBC, ZPGX1, 4 )
OP ( 0xF6,  INC, ZPGX0, 6 )

OP ( 0xF8,  SED,  NONE, 2 )
OP ( 0xF9,  SBC, ABSY1, 4 )

OP ( 0xFD,  SBC, ABSX1, 4 )
OP ( 0xFE,  INC, ABSX0, 7 )
