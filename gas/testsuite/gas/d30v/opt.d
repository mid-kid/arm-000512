#objdump: -dr
#name: D30V optimization test
#as: -O

.*: +file format elf32-d30v

Disassembly of section .text:

0+0000 <.text>:
   0:	08801080 08803100 	abs	r1, r2	||	abs	r3, r4
   8:	02900100 02901080 	notfg	f0, s	||	notfg	f1, f2
  10:	08801080 02901080 	abs	r1, r2	||	notfg	f1, f2
  18:	08001083 82907000 	add.s	r1, r2, r3	->	notfg	c, f0
  20:	08001083 829001c0 	add.s	r1, r2, r3	->	notfg	f0, c
  28:	00080000 88801080 	bra.s	0	\((0x|)28\)	->	abs	r1, r2
  30:	00080000 08801080 	bra.s	0	\((0x|)30\)	||	abs	r1, r2
  38:	00280000 00f00000 	bsr.s	0	\((0x|)38\)	||	nop	
  40:	08801080 88801080 	abs	r1, r2	->	abs	r1, r2
  48:	00280000 08801080 	bsr.s	0	\((0x|)48\)	||	abs	r1, r2
  50:	04001083 85007209 	ldb.s	r1, @\(r2, r3\)	->	stb.s	r7, @\(r8, r9\)
  58:	05007209 84001083 	stb.s	r7, @\(r8, r9\)	->	ldb.s	r1, @\(r2, r3\)
  60:	04007209 84001083 	ldb.s	r7, @\(r8, r9\)	->	ldb.s	r1, @\(r2, r3\)
  68:	05007209 85001083 	stb.s	r7, @\(r8, r9\)	->	stb.s	r1, @\(r2, r3\)
  70:	080030c6 854820c0 	add.s	r3, r3, r6	->	stw.s	r2, @\(r3, 0x0\)
  78:	02c28105 90180000 	cmple.s	f0, r4, r5	->	jmp.s/tx	(0x|)0
  80:	02c28105 a0180000 	cmple.s	f0, r4, r5	->	jmp.s/fx	(0x|)0
  88:	30180000 02c28105 	jmp.s/xt	0	||	cmple.s	f0, r4, r5
  90:	40180000 02c28105 	jmp.s/xf	0	||	cmple.s	f0, r4, r5
  98:	02c28105 d0180000 	cmple.s	f0, r4, r5	->	jmp.s/tt	(0x|)0
  a0:	02c28105 e0180000 	cmple.s	f0, r4, r5	->	jmp.s/tf	(0x|)0
  a8:	10180000 02c29105 	jmp.s/tx	0	||	cmple.s	f1, r4, r5
  b0:	02c29105 b0180000 	cmple.s	f1, r4, r5	->	jmp.s/xt	(0x|)0
  b8:	08084001 82c28105 	add.s	r4, r0, 0x1	->	cmple.s	f0, r4, r5
  c0:	08084001 02c280c5 	add.s	r4, r0, 0x1	||	cmple.s	f0, r3, r5
  c8:	04604006 886054d4 	ld2w.s	r4, @\(r0, r6\)	->	adds.s	r5, r19, r20
  d0:	04604006 88603154 	ld2w.s	r4, @\(r0, r6\)	->	adds.s	r3, r5, r20
  d8:	04604006 886064d4 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r6, r19, r20
  e0:	04604006 086074d4 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r19, r20
  e8:	04604006 08607014 	ld2w.s	r4, @\(r0, r6\)	||	adds.s	r7, 0, r20
  f0:	05604006 886054d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r5, r19, r20
  f8:	05604006 08603154 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r3, r5, r20
 100:	05604006 886064d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r6, r19, r20
 108:	05604006 086074d4 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r7, r19, r20
 110:	05604006 08607014 	st2w.s	r4, @\(r0, r6\)	||	adds.s	r7, 0, r20
 118:	0560a0c4 85628aec 	st2w.s	r10, @\(r3, r4\)	->	st2w.s	r40, @\(r43, r44\)
 120:	05401083 84429aab 	stw.s	r1, @\(r2, r3\)	->	ldw.s	r41, @\(r42, r43\)
 128:	04401083 84029aab 	ldw.s	r1, @\(r2, r3\)	->	ldb.s	r41, @\(r42, r43\)
 130:	0444418b 88689182 	ldw.s	r4, @\(r6\+, r11\)	->	adds.s	r9, r6, 0x2
 138:	044c418b 08689182 	ldw.s	r4, @\(r6-, r11\)	||	adds.s	r9, r6, 0x2
 140:	054c418b 88689182 	stw.s	r4, @\(r6-, r11\)	->	adds.s	r9, r6, 0x2
 148:	0440418b 08689182 	ldw.s	r4, @\(r6, r11\)	||	adds.s	r9, r6, 0x2
 150:	0440418b 08689182 	ldw.s	r4, @\(r6, r11\)	||	adds.s	r9, r6, 0x2
 158:	00180000 88801080 	jmp.s	0x0	->	abs	r1, r2
 160:	00380000 00f00000 	jsr.s	0x0	||	nop	
 168:	08801080 00f00000 	abs	r1, r2	||	nop	
 170:	00080000 88801080 	bra.s	0	\(0x170\)	->	abs	r1, r2
 178:	00280000 00f00000 	bsr.s	0	\(0x178\)	||	nop	
 180:	08801080 00f00000 	abs	r1, r2	||	nop	
