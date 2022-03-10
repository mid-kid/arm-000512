# fragment for PRO dos install
DEMODIR=hppa1.1-proelf
OKI_LDFLAGS= -L${srcdir} -Top50n.ld $(LDFLAGS_FOR_TARGET) -Ttext 40000
WEC_LDFLAGS= -L${srcdir} -Tw89k.ld $(LDFLAGS_FOR_TARGET) -Ttext 100000
HP_LDFLAGS=  -L${srcdir} -Thp74x.ld -N -R 10000 -a archive

