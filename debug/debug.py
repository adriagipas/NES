#!/usr/bin/env python3

from array import array
import NES
import sys

class Inst:

    __BYTE_MODES= set([NES.INM,NES.INDY,NES.INDX])
    __WORD_MODES= set([NES.ABS,NES.ABSX,NES.ABSY,NES.IND,
                       NES.ZPG,NES.ZPGX,NES.ZPGY])
    
    __MNEMONIC= {
        NES.ADC: 'ADC',
        NES.AND: 'AND',
        NES.ASL: 'ASL',
        NES.BCC: 'BCC',
        NES.BCS: 'BCS',
        NES.BEQ: 'BEQ',
        NES.BIT: 'BIT',
        NES.BMI: 'BMI',
        NES.BNE: 'BNE',
        NES.BPL: 'BPL',
        NES.BRK: 'BRK',
        NES.BVC: 'BVC',
        NES.BVS: 'BVS',
        NES.CLC: 'CLC',
        NES.CLD: 'CLD',
        NES.CLI: 'CLI',
        NES.CLV: 'CLV',
        NES.CMP: 'CMP',
        NES.CPX: 'CPX',
        NES.CPY: 'CPY',
        NES.DEC: 'DEC',
        NES.DEX: 'DEX',
        NES.DEY: 'DEY',
        NES.EOR: 'EOR',
        NES.INC: 'INC',
        NES.INX: 'INX',
        NES.INY: 'INY',
        NES.JMP: 'JMP',
        NES.JSR: 'JSR',
        NES.LDA: 'LDA',
        NES.LDX: 'LDX',
        NES.LDY: 'LDY',
        NES.LSR: 'LSR',
        NES.NOP: 'NOP',
        NES.ORA: 'ORA',
        NES.PHA: 'PHA',
        NES.PHP: 'PHP',
        NES.PLA: 'PLA',
        NES.PLP: 'PLP',
        NES.ROL: 'ROL',
        NES.ROR: 'ROR',
        NES.RTI: 'RTI',
        NES.RTS: 'RTS',
        NES.SBC: 'SBC',
        NES.SEC: 'SEC',
        NES.SED: 'SED',
        NES.SEI: 'SEI',
        NES.STA: 'STA',
        NES.STX: 'STX',
        NES.STY: 'STY',
        NES.TAX: 'TAX',
        NES.TAY: 'TAY',
        NES.TSX: 'TSX',
        NES.TXA: 'TXA',
        NES.TXS: 'TXS',
        NES.TYA: 'TYA',
        NES.UNK: 'UNK' }

    def __set_extra(self,extra):
        if self.addr_mode in Inst.__BYTE_MODES: self.extra= extra[0]
        elif self.addr_mode in Inst.__WORD_MODES: self.extra= extra[1]
        elif self.addr_mode==NES.REL: self.extra= extra[2]
        else: self.extra= None
    
    def __init__(self,args):
        self.id= args[1]
        self.addr_mode= args[2]
        self.bytes= args[3]
        self.addr= (args[0]-len(self.bytes))&0xFFFF
        self.__set_extra(args[4])

    def __str_op(self):
        if self.addr_mode==NES.ABS: return ' $%04x'%self.extra
        elif self.addr_mode==NES.ABSX: return ' $%04x,X'%self.extra
        elif self.addr_mode==NES.ABSY: return ' $%04x,Y'%self.extra
        elif self.addr_mode==NES.IND: return ' ($%04x)'%self.extra
        elif self.addr_mode==NES.INDX: return ' ($%02x,X)'%self.extra
        elif self.addr_mode==NES.INDY: return ' ($%02x),Y'%self.extra
        elif self.addr_mode==NES.INM: return ' #$%02x'%self.extra
        elif self.addr_mode==NES.NONE: return ''
        elif self.addr_mode==NES.REL: return ' %d ($%04x)'%(self.extra[1],
                                                            self.extra[0])
        elif self.addr_mode==NES.ZPG: return ' $00%02x'%self.extra
        elif self.addr_mode==NES.ZPGX: return ' $00%02x,X'%self.extra
        elif self.addr_mode==NES.ZPGY: return ' $00%02x,Y'%self.extra
        else: return ' WTF!!!!'
        
    def __str__(self):
        ret= '%04X   '%self.addr
        for b in self.bytes: ret+= ' %02x'%b
        for n in range(len(self.bytes),3): ret+= '   '
        ret+= '    '+Inst.__MNEMONIC[self.id]
        ret+= self.__str_op()
        return ret

class Record:
    def __init__(self,inst):
        self.inst= inst
        self.nexec= 0

class Tracer:

    MA_ROM= 0x01
    MA_RAM= 0x02
    MA_PORTS= 0x04
    
    def __init__(self):
        self.next_addr= 0
        self.records= {}
        self.print_insts= False
        self.max_nexec= 0
        self.last_addr= None
        self.bank_map= None
        self.print_insts= False
        self.print_mem_access= 0

    def enable_print_insts(self,enabled):
        self.print_insts= enabled

    def enable_print_mem_access(self,mask):
        self.print_mem_access= mask

    def mapper_changed(self):
        tmp= NES.get_rom_mapper_state()
        self.bank_map=[tmp['bank0'],tmp['bank1'],
                       tmp['bank2'],tmp['bank3']]
        
    def cpu_inst(self,*args):
        self.next_addr= args[0]
        inst= Inst(args)
        self.last_addr= inst.addr
        if inst.addr < 0x2000: # RAM
            aux= self.records.get('ram')
            if aux == None :
                aux= [None]*0x2000
                self.records['ram']= aux
            aux2= aux[inst.addr]
            if aux2 == None :
                aux2= aux[inst.addr]= Record ( inst )
            aux2.nexec+= 1
            if aux2.nexec > self.max_nexec:
                self.max_nexec= aux2.nexec
        elif inst.addr >= 0x8000: # ROM
            aux= self.records.get('rom')
            if aux == None :
                rom= NES.get_rom()
                aux= []
                for n in range(0,rom['nprg']*2):
                    aux.append ( [None]*0x2000)
                self.records['rom']= aux
            if self.bank_map==None:
                self.mapper_changed()
            if inst.addr < 0xA000:
                bank= self.bank_map[0]
                addr= inst.addr-0x8000
            elif inst.addr < 0xC000:
                bank= self.bank_map[1]
                addr= inst.addr-0xA000
            elif inst.addr < 0xE000:
                bank= self.bank_map[2]
                addr= inst.addr-0xC000
            else:
                bank= self.bank_map[3]
                addr= inst.addr-0xE000
            aux2= aux[bank][addr]
            if aux2 == None :
                aux2= aux[bank][addr]= Record ( inst )
            aux2.nexec+= 1
            if aux2.nexec > self.max_nexec:
                self.max_nexec= aux2.nexec
        if self.print_insts:
            print(inst)

    def __dump_code(self,code,title,prec,prec2,nbank):
        print('\n\n## %s ##'%title)
        prev= True
        i= 0
        while i < 0x2000:
            rec= code[i]
            if rec == None :
                prev= False
                i+= 1
            else:
                if not prev: print ( '\n' )
                print ( ('[%'+str(prec)+'d] %0'+
                         str(prec2)+'d:%04X %s')%(rec.nexec,nbank,i,rec.inst) )
                prev= True
                i+= len(rec.inst.bytes)
    
    def dump_insts(self):
        def get_prec(val):
            prec= 0
            while val!=0:
                prec+= 1
                val//= 10
            return prec
        prec= get_prec ( self.max_nexec )
        # Rom
        code_rom= self.records.get('rom')
        if code_rom != None:
            nbanks= len(code_rom)
            prec2= get_prec ( nbanks )
            for n in range(0,nbanks):
                code= code_rom[n]
                self.__dump_code(code,'BANK %d'%n,prec,prec2,n)
        # Ram
        code_ram= self.records.get('ram')
        if code_ram != None:
            self.__dump_code(code_ram,'RAM',prec,1,0)
            
    def __check_mem_access(self,addr):
        if addr<0x2000:
            return self.print_mem_access&Tracer.MA_RAM!=0
        elif addr<0x4000:
            return self.print_mem_access&Tracer.MA_PORTS!=0
        elif addr<0x8000:
            if addr<0x4018: return self.print_mem_access&Tracer.MA_PORTS!=0
        else: return self.print_mem_access&Tracer.MA_ROM!=0
        
    def mem_access(self,typ,addr,data):
        if not self.__check_mem_access(addr): return
        if typ==NES.READ :
            print('MEM[%08X] -> %04X'%(addr,data))
        else:
            print('MEM[%08X]= %04X'%(addr,data))

class Color:
    
    @staticmethod
    def get(r,g,b):
        return (r<<16)|(g<<8)|b
    
    @staticmethod
    def get_components(color):
        return (color>>16,(color>>8)&0xff,color&0xff)
    
class Img:
    
    WHITE= Color.get ( 255, 255, 255 )
    
    def __init__ ( self, width, height ):
        self._width= width
        self._height= height
        self._v= []
        for i in range(0,height):
            self._v.append ( array('i',[Img.WHITE]*width) )
    
    def __getitem__ ( self, ind ):
        return self._v[ind]
    
    def write ( self, to ):
        if type(to) == str :
            to= open ( to, 'wt' )
        to.write ( 'P3\n ')
        to.write ( '%d %d\n'%(self._width,self._height) )
        to.write ( '255\n' )
        for r in self._v:
            for c in r:
                aux= Color.get_components ( c )
                to.write ( '%d %d %d\n'%(aux[0],aux[1],aux[2]) )

def bitplane2color(bp0,bp1,pal4):
    ret= array ( 'i', [0]*8 )
    for i in range(0,8):
        ret[i]= pal4[((bp0>>7)&1)|((bp1>>6)&0x2)]
        bp0<<= 1; bp1<<= 1
    return ret

PAL4= array ( 'i', [Color.get(0,0,0),
                    Color.get(100,100,100),
                    Color.get(200,200,200),
                    Color.get(255,255,255)] )
def tiles2img(vram):
    ntiles= 0x2000/16
    ret= Img ( 8, 8*ntiles ); j= r= 0
    for n in range(0,ntiles):
        for i in range(0,8):
            line= bitplane2color ( mem[j], mem[j+8], PAL4 )
            j+= 1
            for k in range(0,8):
                ret[r][k]= line[k]
            r+= 1
        j+= 8
    return ret

def vramtiles2img(vram):
    ret= Img(32*8,16*8)
    offy= 0; p= 0
    for r in range(0,16):
        offx= 0
        for c in range(0,32):
            for j in range(0,8):
                line= bitplane2color ( vram[p], vram[p+8], PAL4 )
                p+= 1
                r2= offy+j
                for k in range(0,8):
                    ret[r2][offx+k]= line[k]
            p+= 8
            offx+= 8
        offy+= 8
    return ret

def get_img_pal(vram):
    p= 0x3F00
    ret= []
    for i in range(0,4):
        aux= []
        for j in range(0,4):
            color= NES_PALETTE[vram[p]]
            aux.append(Color.get(color[0],color[1],color[2]))
            p+= 1
        ret.append(array('i',aux))
    return ret

def get_obj_pal(vram):
    p= 0x3F10
    ret= []
    for i in range(0,4):
        aux= []
        for j in range(0,4):
            color= NES_PALETTE[vram[p]]
            aux.append(Color.get(color[0],color[1],color[2]))
            p+= 1
        ret.append(array('i',aux))
    return ret

def draw_NT(img,vram,basex,basey,addr,pal):
    NT_addr= addr
    offy= basey
    for r in range(0,30):
        offx= basex
        for c in range(0,32):
            pal_sel= get_attr_color(vram,addr,c,r)
            tile= vram[NT_addr]
            NT_addr+= 1
            p= tile*16
            for j in range(0,8):
                line= bitplane2color(vram[p],vram[p+8],pal[pal_sel])
                p+= 1
                r2= offy+j
                for k in range(0,8):
                    img[r2][offx+k]= line[k]
            offx+= 8
        offy+= 8

def get_attr_color(vram,NT_addr,x,y):
    addr= NT_addr+0x3C0
    ay,ax=y>>2,x>>2
    abyte= vram[addr+ay*8+ax]
    desp= (((y>>1)&0x1)<<1)|((x>>1)&0x1)
    ret= (abyte>>(desp*2))&0x3
    return ret

# Pinta:
#  NT0 NT1
#  NT2 NT3
def vram2img(vram):
    pal= get_img_pal(vram)
    ret= Img(32*2*8,30*2*8)
    offy= 0
    for ntr in range(0,2):
        offx= 0
        for ntc in range(0,2):
            addr= 0x2000|(((ntr<<1)|ntc)<<10)
            draw_NT(ret,vram,offx,offy,addr,pal)
            offx+= 32*8
        offy+= 30*8
    return ret

def draw_sprite(obj_pt_addr,vram,b1,b2,b3,b4,img,width,height,pal):
    pal_sel= pal[b3&0x3]
    for r in range(0,8):
        if (b3&0x80)!=0:
            pt= (b2<<4) | (r^0x7) | obj_pt_addr
        else:
            pt= (b2<<4) | r | obj_pt_addr
        bp0,bp1= vram[pt],vram[pt|0x8]
        if (b3&0x40)==0:
            for c in range(0,8):
                colorl= ((bp1&0x80)>>6)|((bp0&0x80)>>7)
                if colorl != 0:
                    y= b1-1+r
                    x= b4+c
                    if y>=0 and y<height and x>=0 and x<width:
                        img[y][x]= pal_sel[colorl]
                bp1<<= 1
                bp0<<= 1
        else:
            for c in range(0,8):
                colorl= ((bp1&0x1)<<1)|(bp0&0x1)
                if colorl != 0:
                    y= b1-1+r
                    x= b4+c
                    if y>=0 and y<height and x>=0 and x<width:
                        img[y][x]= pal_sel[colorl]
                bp1>>= 1
                bp0>>= 1

def obj_ram2img(obj_ram,vram,bcolor=(255<<16|255)):
    pal= get_obj_pal(vram)
    ret= Img(32*8,32*8)
    for r in range(0,32*8):
        for c in range(0,32*8):
            ret[r][c]= bcolor
    for i in range(0,64):
        b1= obj_ram[4*i]
        b2= obj_ram[4*i+1]
        b3= obj_ram[4*i+2]
        b4= obj_ram[4*i+3]
        draw_sprite(0x0000,vram,b1,b2,b3,b4,ret,32*8,32*8,pal)
    return ret

NES.init()
NES_PALETTE= NES.get_palette()
NES.set_rom(open('ROM.nes','rb').read())
rom= NES.get_rom()
print('NPrg (16K): %d'%rom['nprg'])
print('NChr (8K): %d'%rom['nchr'])
print('Mapper: %s'%rom['mapper'].decode('utf-8'))
print('TV Mode: %s'%rom['tvmode'].decode('utf-8'))
print('SRAM: %s'%rom['sram'])

################################################################################
#NES.loop()
#vram= NES.get_vram()
#obj_ram= NES.get_obj_ram()
#vramtiles2img(vram).write(open('tiles.pnm','w'))
#vram2img(vram).write(open('nts.pnm','w'))
#obj_ram2img(obj_ram,vram).write(open('sprites.pnm','w'))
#NES.close()
#sys.exit(0)
#print([hex(i) for i in rom['prgs'][7][0xFFFA&0x3FFF:(0xFFFA&0x3FFF)+2]]) # NMI
#sys.exit(0)
t= Tracer()
#t.enable_print_insts(True)
t.enable_print_mem_access(Tracer.MA_PORTS|Tracer.MA_RAM)
NES.set_tracer(t)
NES.loop()
#for i in range(0,1000000): NES.trace()
#t.dump_insts()
#print(NES.get_rom_mapper_state())
NES.close()
