import NES
import sys

if len(sys.argv)!=2:
    sys.exit('%s <ROM>'%sys.argv[0])
rom_fn= sys.argv[1]

NES.init()
with open(rom_fn,'rb') as f:
    NES.set_rom(f.read())
NES.loop()
NES.close()
