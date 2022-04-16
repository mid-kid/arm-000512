#objdump: -dr
#name: D30V relocation test
#as:

.*: +file format elf32-d30v

Disassembly of section .text:

0+0000 <start>:
       0:	88082000 80000028 	add.l	r2, r0, 0x28
			0: R_D30V_32	.text
       8:	88084000 80000000 	add.l	r4, r0, 0x0
			8: R_D30V_32	.data
      10:	88084000 80000006 	add.l	r4, r0, 0x6
			10: R_D30V_32	.data
      18:	88084000 80000000 	add.l	r4, r0, 0x0
			18: R_D30V_32	unk
      20:	80080000 80000018 	bra.l	18	\(38 <cont>\)

0+0028 <hello>:
      28:	48656c6c 6f20576f 	.long	0x48656c6c	||	.long	0x6f20576f
      30:	726c640a 00f00000 	.long	0x726c640a	||	nop	
0+0038 <cont>:
      38:	80180000 80000048 	jmp.l	48 <cont2>
			38: R_D30V_32	.text
      40:	088020c0 00f00000 	abs	r2, r3	||	nop	

0+0048 <cont2>:
      48:	000bfff7 00f00000 	bra.s	-48	\(0 <start>\)	||	nop	
      50:	000801fe 800801fe 	bra.s	ff0	\(1040 <exit>\)	->	bra.s	ff0	\(1040 <exit>\)
      58:	00180000 00f00000 	jmp.s	0 <start>	||	nop	
      60:	006c1ffb 00f00000 	bsrtnz.s	r1, -28	\(38 <cont>\)	||	nop	
      68:	006c1ffa 00f00000 	bsrtnz.s	r1, -30	\(38 <cont>\)	||	nop	
      70:	004c1ff9 804c1ff9 	bratnz.s	r1, -38	\(38 <cont>\)	->	bratnz.s	r1, -38	\(38 <cont>\)
      78:	005c1007 806c11ee 	jmptnz.s	r1, 38 <cont>	->	bsrtnz.s	r1, f70	\(fe8 <foo>\)
			78: R_D30V_15	.text
      80:	005c1000 806c1000 	jmptnz.s	r1, 0 <start>	->	bsrtnz.s	r1, 0	\(80 <cont2\+0x38>\)
			80: R_D30V_15	unk
			84: R_D30V_15_PCREL_R	unk
      88:	805c1000 80000000 	jmptnz.l	r1, 0 <start>
			88: R_D30V_32	unk
      90:	806c1000 80000000 	bsrtnz.l	r1, 0	\(90 <cont2\+0x48>\)
			90: R_D30V_32_PCREL	unk
      98:	000801ea 00f00000 	bra.s	f50	\(fe8 <foo>\)	||	nop	
      a0:	80080000 80000f48 	bra.l	f48	\(fe8 <foo>\)
      a8:	000bffeb 00f00000 	bra.s	-a8	\(0 <start>\)	||	nop	
      b0:	80180000 80000000 	jmp.l	0 <start>
			b0: R_D30V_32	.text
      b8:	80180000 80000000 	jmp.l	0 <start>
			b8: R_D30V_32	.text
      c0:	00180000 801801fd 	jmp.s	0 <start>	->	jmp.s	fe8 <foo>
			c0: R_D30V_21	.text
			c4: R_D30V_21	.text
      c8:	000bffe7 00f00000 	bra.s	-c8	\(0 <start>\)	||	nop	
      d0:	80080000 80000000 	bra.l	0	\(d0 <cont2\+0x88>\)
			d0: R_D30V_32_PCREL	unknown
      d8:	80180000 80000000 	jmp.l	0 <start>
			d8: R_D30V_32	unknown
      e0:	00180000 80080000 	jmp.s	0 <start>	->	bra.s	0	\(e0 <cont2\+0x98>\)
			e0: R_D30V_21	unknown
			e4: R_D30V_21_PCREL_R	unknown
	...

0+0fe8 <foo>:
     fe8:	08001000 00f00000 	add.s	r1, r0, r0	||	nop	
     ff0:	846bc000 80001038 	ld2w.l	r60, @\(r0, 0x1038\)
			ff0: R_D30V_32	.text
     ff8:	0803e000 80280009 	add.s	r62, r0, r0	->	bsr.s	48	\(1040 <exit>\)
    1000:	002bfffd 00f00000 	bsr.s	-18	\(fe8 <foo>\)	||	nop	
    1008:	000bfe08 800bfe08 	bra.s	-fc0	\(48 <cont2>\)	->	bra.s	-fc0	\(48 <cont2>\)
    1010:	00280006 00f00000 	bsr.s	30	\(1040 <exit>\)	||	nop	
    1018:	00180208 80180208 	jmp.s	1040 <exit>	->	jmp.s	1040 <exit>
			1018: R_D30V_21	.text
			101c: R_D30V_21	.text
    1020:	00180208 00f00000 	jmp.s	1040 <exit>	||	nop	
			1020: R_D30V_21	.text
    1028:	80280000 80000018 	bsr.l	18	\(1040 <exit>\)
    1030:	80180000 80001040 	jmp.l	1040 <exit>
			1030: R_D30V_32	.text

0+1038 <longzero>:
	...

0+1040 <exit>:
    1040:	0010003e 00f00000 	jmp.s	r62	||	nop	
