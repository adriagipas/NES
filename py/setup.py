from distutils.core import setup, Extension

module= Extension ( 'NES',
                    sources= [ '../src/apu.c',
                               '../src/cpu_dis.c',
                               '../src/main.c',
                               '../src/mapper_names.c',
                               '../src/palette.c',
                               '../src/cpu.c',
                               '../src/dma.c',
                               '../src/joypads.c',
                               '../src/mapper.c',
                               '../src/mem.c',
                               '../src/ppu.c',
                               '../src/rom.c',
                               '../src/mappers/aorom.c',
                               '../src/mappers/cnrom.c',
                               '../src/mappers/mmc1.c',
                               '../src/mappers/mmc2.c',
                               '../src/mappers/mmc3.c',
                               '../src/mappers/nrom.c',
                               '../src/mappers/unrom.c',
                               'nesmodule.c'
                                ],
                    depends= [ '../src/NES.h', '../src/op.h',
                               '../src/mappers/aorom.h',
                               '../src/mappers/cnrom.h',
                               '../src/mappers/mmc1.h',
                               '../src/mappers/mmc2.h',
                               '../src/mappers/mmc3.h',
                               '../src/mappers/nrom.h',
                               '../src/mappers/unrom.h' ],
                    libraries= [ 'SDL' ],
                    include_dirs= [ '../src' ] )

setup ( name= 'NES',
        version= '1.0',
        description= 'NES simulator',
        ext_modules= [module] )
