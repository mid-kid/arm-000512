/* Decode header for i960base.

THIS FILE IS MACHINE GENERATED WITH CGEN.

Copyright (C) 1996, 1997, 1998, 1999 Free Software Foundation, Inc.

This file is part of the GNU Simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#ifndef I960BASE_DECODE_H
#define I960BASE_DECODE_H

/* Run-time computed instruction descriptor.  */

struct idesc {
#if WITH_SEM_SWITCH_FULL
#ifdef __GNUC__
  void *sem_full_lab;
#endif
#else
  SEMANTIC_FN *sem_full;
#endif

#if WITH_SEM_SWITCH_FAST
#ifdef __GNUC__
  void *sem_fast_lab;
#endif
#else
  SEMANTIC_FN *sem_fast;
#endif

  /* Instruction number (index in IDESC table, profile table).
     Also used to switch on in non-gcc semantic switches.  */
  int num;

  /* opcode table data */
  const CGEN_INSN *opcode;

  /* profiling/modelling support */
  const INSN_TIMING *timing;
};

extern const IDESC *i960base_decode (SIM_CPU *, IADDR,
                                  CGEN_INSN_INT,
                                  ARGBUF *);

/* Enum declaration for instructions in cpu family i960base.  */
typedef enum i960base_insn_type {
  I960BASE_INSN_X_INVALID, I960BASE_INSN_X_AFTER, I960BASE_INSN_X_BEFORE, I960BASE_INSN_X_CTI_CHAIN
 , I960BASE_INSN_X_CHAIN, I960BASE_INSN_X_BEGIN, I960BASE_INSN_MULO, I960BASE_INSN_MULO1
 , I960BASE_INSN_MULO2, I960BASE_INSN_MULO3, I960BASE_INSN_REMO, I960BASE_INSN_REMO1
 , I960BASE_INSN_REMO2, I960BASE_INSN_REMO3, I960BASE_INSN_DIVO, I960BASE_INSN_DIVO1
 , I960BASE_INSN_DIVO2, I960BASE_INSN_DIVO3, I960BASE_INSN_REMI, I960BASE_INSN_REMI1
 , I960BASE_INSN_REMI2, I960BASE_INSN_REMI3, I960BASE_INSN_DIVI, I960BASE_INSN_DIVI1
 , I960BASE_INSN_DIVI2, I960BASE_INSN_DIVI3, I960BASE_INSN_ADDO, I960BASE_INSN_ADDO1
 , I960BASE_INSN_ADDO2, I960BASE_INSN_ADDO3, I960BASE_INSN_SUBO, I960BASE_INSN_SUBO1
 , I960BASE_INSN_SUBO2, I960BASE_INSN_SUBO3, I960BASE_INSN_NOTBIT, I960BASE_INSN_NOTBIT1
 , I960BASE_INSN_NOTBIT2, I960BASE_INSN_NOTBIT3, I960BASE_INSN_AND, I960BASE_INSN_AND1
 , I960BASE_INSN_AND2, I960BASE_INSN_AND3, I960BASE_INSN_ANDNOT, I960BASE_INSN_ANDNOT1
 , I960BASE_INSN_ANDNOT2, I960BASE_INSN_ANDNOT3, I960BASE_INSN_SETBIT, I960BASE_INSN_SETBIT1
 , I960BASE_INSN_SETBIT2, I960BASE_INSN_SETBIT3, I960BASE_INSN_NOTAND, I960BASE_INSN_NOTAND1
 , I960BASE_INSN_NOTAND2, I960BASE_INSN_NOTAND3, I960BASE_INSN_XOR, I960BASE_INSN_XOR1
 , I960BASE_INSN_XOR2, I960BASE_INSN_XOR3, I960BASE_INSN_OR, I960BASE_INSN_OR1
 , I960BASE_INSN_OR2, I960BASE_INSN_OR3, I960BASE_INSN_NOR, I960BASE_INSN_NOR1
 , I960BASE_INSN_NOR2, I960BASE_INSN_NOR3, I960BASE_INSN_NOT, I960BASE_INSN_NOT1
 , I960BASE_INSN_NOT2, I960BASE_INSN_NOT3, I960BASE_INSN_CLRBIT, I960BASE_INSN_CLRBIT1
 , I960BASE_INSN_CLRBIT2, I960BASE_INSN_CLRBIT3, I960BASE_INSN_SHLO, I960BASE_INSN_SHLO1
 , I960BASE_INSN_SHLO2, I960BASE_INSN_SHLO3, I960BASE_INSN_SHRO, I960BASE_INSN_SHRO1
 , I960BASE_INSN_SHRO2, I960BASE_INSN_SHRO3, I960BASE_INSN_SHLI, I960BASE_INSN_SHLI1
 , I960BASE_INSN_SHLI2, I960BASE_INSN_SHLI3, I960BASE_INSN_SHRI, I960BASE_INSN_SHRI1
 , I960BASE_INSN_SHRI2, I960BASE_INSN_SHRI3, I960BASE_INSN_EMUL, I960BASE_INSN_EMUL1
 , I960BASE_INSN_EMUL2, I960BASE_INSN_EMUL3, I960BASE_INSN_MOV, I960BASE_INSN_MOV1
 , I960BASE_INSN_MOVL, I960BASE_INSN_MOVL1, I960BASE_INSN_MOVT, I960BASE_INSN_MOVT1
 , I960BASE_INSN_MOVQ, I960BASE_INSN_MOVQ1, I960BASE_INSN_MODPC, I960BASE_INSN_MODAC
 , I960BASE_INSN_LDA_OFFSET, I960BASE_INSN_LDA_INDIRECT_OFFSET, I960BASE_INSN_LDA_INDIRECT, I960BASE_INSN_LDA_INDIRECT_INDEX
 , I960BASE_INSN_LDA_DISP, I960BASE_INSN_LDA_INDIRECT_DISP, I960BASE_INSN_LDA_INDEX_DISP, I960BASE_INSN_LDA_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LD_OFFSET, I960BASE_INSN_LD_INDIRECT_OFFSET, I960BASE_INSN_LD_INDIRECT, I960BASE_INSN_LD_INDIRECT_INDEX
 , I960BASE_INSN_LD_DISP, I960BASE_INSN_LD_INDIRECT_DISP, I960BASE_INSN_LD_INDEX_DISP, I960BASE_INSN_LD_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDOB_OFFSET, I960BASE_INSN_LDOB_INDIRECT_OFFSET, I960BASE_INSN_LDOB_INDIRECT, I960BASE_INSN_LDOB_INDIRECT_INDEX
 , I960BASE_INSN_LDOB_DISP, I960BASE_INSN_LDOB_INDIRECT_DISP, I960BASE_INSN_LDOB_INDEX_DISP, I960BASE_INSN_LDOB_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDOS_OFFSET, I960BASE_INSN_LDOS_INDIRECT_OFFSET, I960BASE_INSN_LDOS_INDIRECT, I960BASE_INSN_LDOS_INDIRECT_INDEX
 , I960BASE_INSN_LDOS_DISP, I960BASE_INSN_LDOS_INDIRECT_DISP, I960BASE_INSN_LDOS_INDEX_DISP, I960BASE_INSN_LDOS_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDIB_OFFSET, I960BASE_INSN_LDIB_INDIRECT_OFFSET, I960BASE_INSN_LDIB_INDIRECT, I960BASE_INSN_LDIB_INDIRECT_INDEX
 , I960BASE_INSN_LDIB_DISP, I960BASE_INSN_LDIB_INDIRECT_DISP, I960BASE_INSN_LDIB_INDEX_DISP, I960BASE_INSN_LDIB_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDIS_OFFSET, I960BASE_INSN_LDIS_INDIRECT_OFFSET, I960BASE_INSN_LDIS_INDIRECT, I960BASE_INSN_LDIS_INDIRECT_INDEX
 , I960BASE_INSN_LDIS_DISP, I960BASE_INSN_LDIS_INDIRECT_DISP, I960BASE_INSN_LDIS_INDEX_DISP, I960BASE_INSN_LDIS_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDL_OFFSET, I960BASE_INSN_LDL_INDIRECT_OFFSET, I960BASE_INSN_LDL_INDIRECT, I960BASE_INSN_LDL_INDIRECT_INDEX
 , I960BASE_INSN_LDL_DISP, I960BASE_INSN_LDL_INDIRECT_DISP, I960BASE_INSN_LDL_INDEX_DISP, I960BASE_INSN_LDL_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDT_OFFSET, I960BASE_INSN_LDT_INDIRECT_OFFSET, I960BASE_INSN_LDT_INDIRECT, I960BASE_INSN_LDT_INDIRECT_INDEX
 , I960BASE_INSN_LDT_DISP, I960BASE_INSN_LDT_INDIRECT_DISP, I960BASE_INSN_LDT_INDEX_DISP, I960BASE_INSN_LDT_INDIRECT_INDEX_DISP
 , I960BASE_INSN_LDQ_OFFSET, I960BASE_INSN_LDQ_INDIRECT_OFFSET, I960BASE_INSN_LDQ_INDIRECT, I960BASE_INSN_LDQ_INDIRECT_INDEX
 , I960BASE_INSN_LDQ_DISP, I960BASE_INSN_LDQ_INDIRECT_DISP, I960BASE_INSN_LDQ_INDEX_DISP, I960BASE_INSN_LDQ_INDIRECT_INDEX_DISP
 , I960BASE_INSN_ST_OFFSET, I960BASE_INSN_ST_INDIRECT_OFFSET, I960BASE_INSN_ST_INDIRECT, I960BASE_INSN_ST_INDIRECT_INDEX
 , I960BASE_INSN_ST_DISP, I960BASE_INSN_ST_INDIRECT_DISP, I960BASE_INSN_ST_INDEX_DISP, I960BASE_INSN_ST_INDIRECT_INDEX_DISP
 , I960BASE_INSN_STOB_OFFSET, I960BASE_INSN_STOB_INDIRECT_OFFSET, I960BASE_INSN_STOB_INDIRECT, I960BASE_INSN_STOB_INDIRECT_INDEX
 , I960BASE_INSN_STOB_DISP, I960BASE_INSN_STOB_INDIRECT_DISP, I960BASE_INSN_STOB_INDEX_DISP, I960BASE_INSN_STOB_INDIRECT_INDEX_DISP
 , I960BASE_INSN_STOS_OFFSET, I960BASE_INSN_STOS_INDIRECT_OFFSET, I960BASE_INSN_STOS_INDIRECT, I960BASE_INSN_STOS_INDIRECT_INDEX
 , I960BASE_INSN_STOS_DISP, I960BASE_INSN_STOS_INDIRECT_DISP, I960BASE_INSN_STOS_INDEX_DISP, I960BASE_INSN_STOS_INDIRECT_INDEX_DISP
 , I960BASE_INSN_STL_OFFSET, I960BASE_INSN_STL_INDIRECT_OFFSET, I960BASE_INSN_STL_INDIRECT, I960BASE_INSN_STL_INDIRECT_INDEX
 , I960BASE_INSN_STL_DISP, I960BASE_INSN_STL_INDIRECT_DISP, I960BASE_INSN_STL_INDEX_DISP, I960BASE_INSN_STL_INDIRECT_INDEX_DISP
 , I960BASE_INSN_STT_OFFSET, I960BASE_INSN_STT_INDIRECT_OFFSET, I960BASE_INSN_STT_INDIRECT, I960BASE_INSN_STT_INDIRECT_INDEX
 , I960BASE_INSN_STT_DISP, I960BASE_INSN_STT_INDIRECT_DISP, I960BASE_INSN_STT_INDEX_DISP, I960BASE_INSN_STT_INDIRECT_INDEX_DISP
 , I960BASE_INSN_STQ_OFFSET, I960BASE_INSN_STQ_INDIRECT_OFFSET, I960BASE_INSN_STQ_INDIRECT, I960BASE_INSN_STQ_INDIRECT_INDEX
 , I960BASE_INSN_STQ_DISP, I960BASE_INSN_STQ_INDIRECT_DISP, I960BASE_INSN_STQ_INDEX_DISP, I960BASE_INSN_STQ_INDIRECT_INDEX_DISP
 , I960BASE_INSN_CMPOBE_REG, I960BASE_INSN_CMPOBE_LIT, I960BASE_INSN_CMPOBNE_REG, I960BASE_INSN_CMPOBNE_LIT
 , I960BASE_INSN_CMPOBL_REG, I960BASE_INSN_CMPOBL_LIT, I960BASE_INSN_CMPOBLE_REG, I960BASE_INSN_CMPOBLE_LIT
 , I960BASE_INSN_CMPOBG_REG, I960BASE_INSN_CMPOBG_LIT, I960BASE_INSN_CMPOBGE_REG, I960BASE_INSN_CMPOBGE_LIT
 , I960BASE_INSN_CMPIBE_REG, I960BASE_INSN_CMPIBE_LIT, I960BASE_INSN_CMPIBNE_REG, I960BASE_INSN_CMPIBNE_LIT
 , I960BASE_INSN_CMPIBL_REG, I960BASE_INSN_CMPIBL_LIT, I960BASE_INSN_CMPIBLE_REG, I960BASE_INSN_CMPIBLE_LIT
 , I960BASE_INSN_CMPIBG_REG, I960BASE_INSN_CMPIBG_LIT, I960BASE_INSN_CMPIBGE_REG, I960BASE_INSN_CMPIBGE_LIT
 , I960BASE_INSN_BBC_REG, I960BASE_INSN_BBC_LIT, I960BASE_INSN_BBS_REG, I960BASE_INSN_BBS_LIT
 , I960BASE_INSN_CMPI, I960BASE_INSN_CMPI1, I960BASE_INSN_CMPI2, I960BASE_INSN_CMPI3
 , I960BASE_INSN_CMPO, I960BASE_INSN_CMPO1, I960BASE_INSN_CMPO2, I960BASE_INSN_CMPO3
 , I960BASE_INSN_TESTNO_REG, I960BASE_INSN_TESTG_REG, I960BASE_INSN_TESTE_REG, I960BASE_INSN_TESTGE_REG
 , I960BASE_INSN_TESTL_REG, I960BASE_INSN_TESTNE_REG, I960BASE_INSN_TESTLE_REG, I960BASE_INSN_TESTO_REG
 , I960BASE_INSN_BNO, I960BASE_INSN_BG, I960BASE_INSN_BE, I960BASE_INSN_BGE
 , I960BASE_INSN_BL, I960BASE_INSN_BNE, I960BASE_INSN_BLE, I960BASE_INSN_BO
 , I960BASE_INSN_B, I960BASE_INSN_BX_INDIRECT_OFFSET, I960BASE_INSN_BX_INDIRECT, I960BASE_INSN_BX_INDIRECT_INDEX
 , I960BASE_INSN_BX_DISP, I960BASE_INSN_BX_INDIRECT_DISP, I960BASE_INSN_CALLX_DISP, I960BASE_INSN_CALLX_INDIRECT
 , I960BASE_INSN_CALLX_INDIRECT_OFFSET, I960BASE_INSN_RET, I960BASE_INSN_CALLS, I960BASE_INSN_FMARK
 , I960BASE_INSN_FLUSHREG, I960BASE_INSN_MAX
} I960BASE_INSN_TYPE;

#if ! WITH_SEM_SWITCH_FULL
#define SEMFULL(fn) extern SEMANTIC_FN CONCAT3 (i960base,_sem_,fn);
#else
#define SEMFULL(fn)
#endif

#if ! WITH_SEM_SWITCH_FAST
#define SEMFAST(fn) extern SEMANTIC_FN CONCAT3 (i960base,_semf_,fn);
#else
#define SEMFAST(fn)
#endif

#define SEM(fn) SEMFULL (fn) SEMFAST (fn)

/* The function version of the before/after handlers is always needed,
   so we always want the SEMFULL declaration of them.  */
extern SEMANTIC_FN CONCAT3 (i960base,_sem_,x_before);
extern SEMANTIC_FN CONCAT3 (i960base,_sem_,x_after);

SEM (x_invalid)
SEM (x_after)
SEM (x_before)
SEM (x_cti_chain)
SEM (x_chain)
SEM (x_begin)
SEM (mulo)
SEM (mulo1)
SEM (mulo2)
SEM (mulo3)
SEM (remo)
SEM (remo1)
SEM (remo2)
SEM (remo3)
SEM (divo)
SEM (divo1)
SEM (divo2)
SEM (divo3)
SEM (remi)
SEM (remi1)
SEM (remi2)
SEM (remi3)
SEM (divi)
SEM (divi1)
SEM (divi2)
SEM (divi3)
SEM (addo)
SEM (addo1)
SEM (addo2)
SEM (addo3)
SEM (subo)
SEM (subo1)
SEM (subo2)
SEM (subo3)
SEM (notbit)
SEM (notbit1)
SEM (notbit2)
SEM (notbit3)
SEM (and)
SEM (and1)
SEM (and2)
SEM (and3)
SEM (andnot)
SEM (andnot1)
SEM (andnot2)
SEM (andnot3)
SEM (setbit)
SEM (setbit1)
SEM (setbit2)
SEM (setbit3)
SEM (notand)
SEM (notand1)
SEM (notand2)
SEM (notand3)
SEM (xor)
SEM (xor1)
SEM (xor2)
SEM (xor3)
SEM (or)
SEM (or1)
SEM (or2)
SEM (or3)
SEM (nor)
SEM (nor1)
SEM (nor2)
SEM (nor3)
SEM (not)
SEM (not1)
SEM (not2)
SEM (not3)
SEM (clrbit)
SEM (clrbit1)
SEM (clrbit2)
SEM (clrbit3)
SEM (shlo)
SEM (shlo1)
SEM (shlo2)
SEM (shlo3)
SEM (shro)
SEM (shro1)
SEM (shro2)
SEM (shro3)
SEM (shli)
SEM (shli1)
SEM (shli2)
SEM (shli3)
SEM (shri)
SEM (shri1)
SEM (shri2)
SEM (shri3)
SEM (emul)
SEM (emul1)
SEM (emul2)
SEM (emul3)
SEM (mov)
SEM (mov1)
SEM (movl)
SEM (movl1)
SEM (movt)
SEM (movt1)
SEM (movq)
SEM (movq1)
SEM (modpc)
SEM (modac)
SEM (lda_offset)
SEM (lda_indirect_offset)
SEM (lda_indirect)
SEM (lda_indirect_index)
SEM (lda_disp)
SEM (lda_indirect_disp)
SEM (lda_index_disp)
SEM (lda_indirect_index_disp)
SEM (ld_offset)
SEM (ld_indirect_offset)
SEM (ld_indirect)
SEM (ld_indirect_index)
SEM (ld_disp)
SEM (ld_indirect_disp)
SEM (ld_index_disp)
SEM (ld_indirect_index_disp)
SEM (ldob_offset)
SEM (ldob_indirect_offset)
SEM (ldob_indirect)
SEM (ldob_indirect_index)
SEM (ldob_disp)
SEM (ldob_indirect_disp)
SEM (ldob_index_disp)
SEM (ldob_indirect_index_disp)
SEM (ldos_offset)
SEM (ldos_indirect_offset)
SEM (ldos_indirect)
SEM (ldos_indirect_index)
SEM (ldos_disp)
SEM (ldos_indirect_disp)
SEM (ldos_index_disp)
SEM (ldos_indirect_index_disp)
SEM (ldib_offset)
SEM (ldib_indirect_offset)
SEM (ldib_indirect)
SEM (ldib_indirect_index)
SEM (ldib_disp)
SEM (ldib_indirect_disp)
SEM (ldib_index_disp)
SEM (ldib_indirect_index_disp)
SEM (ldis_offset)
SEM (ldis_indirect_offset)
SEM (ldis_indirect)
SEM (ldis_indirect_index)
SEM (ldis_disp)
SEM (ldis_indirect_disp)
SEM (ldis_index_disp)
SEM (ldis_indirect_index_disp)
SEM (ldl_offset)
SEM (ldl_indirect_offset)
SEM (ldl_indirect)
SEM (ldl_indirect_index)
SEM (ldl_disp)
SEM (ldl_indirect_disp)
SEM (ldl_index_disp)
SEM (ldl_indirect_index_disp)
SEM (ldt_offset)
SEM (ldt_indirect_offset)
SEM (ldt_indirect)
SEM (ldt_indirect_index)
SEM (ldt_disp)
SEM (ldt_indirect_disp)
SEM (ldt_index_disp)
SEM (ldt_indirect_index_disp)
SEM (ldq_offset)
SEM (ldq_indirect_offset)
SEM (ldq_indirect)
SEM (ldq_indirect_index)
SEM (ldq_disp)
SEM (ldq_indirect_disp)
SEM (ldq_index_disp)
SEM (ldq_indirect_index_disp)
SEM (st_offset)
SEM (st_indirect_offset)
SEM (st_indirect)
SEM (st_indirect_index)
SEM (st_disp)
SEM (st_indirect_disp)
SEM (st_index_disp)
SEM (st_indirect_index_disp)
SEM (stob_offset)
SEM (stob_indirect_offset)
SEM (stob_indirect)
SEM (stob_indirect_index)
SEM (stob_disp)
SEM (stob_indirect_disp)
SEM (stob_index_disp)
SEM (stob_indirect_index_disp)
SEM (stos_offset)
SEM (stos_indirect_offset)
SEM (stos_indirect)
SEM (stos_indirect_index)
SEM (stos_disp)
SEM (stos_indirect_disp)
SEM (stos_index_disp)
SEM (stos_indirect_index_disp)
SEM (stl_offset)
SEM (stl_indirect_offset)
SEM (stl_indirect)
SEM (stl_indirect_index)
SEM (stl_disp)
SEM (stl_indirect_disp)
SEM (stl_index_disp)
SEM (stl_indirect_index_disp)
SEM (stt_offset)
SEM (stt_indirect_offset)
SEM (stt_indirect)
SEM (stt_indirect_index)
SEM (stt_disp)
SEM (stt_indirect_disp)
SEM (stt_index_disp)
SEM (stt_indirect_index_disp)
SEM (stq_offset)
SEM (stq_indirect_offset)
SEM (stq_indirect)
SEM (stq_indirect_index)
SEM (stq_disp)
SEM (stq_indirect_disp)
SEM (stq_index_disp)
SEM (stq_indirect_index_disp)
SEM (cmpobe_reg)
SEM (cmpobe_lit)
SEM (cmpobne_reg)
SEM (cmpobne_lit)
SEM (cmpobl_reg)
SEM (cmpobl_lit)
SEM (cmpoble_reg)
SEM (cmpoble_lit)
SEM (cmpobg_reg)
SEM (cmpobg_lit)
SEM (cmpobge_reg)
SEM (cmpobge_lit)
SEM (cmpibe_reg)
SEM (cmpibe_lit)
SEM (cmpibne_reg)
SEM (cmpibne_lit)
SEM (cmpibl_reg)
SEM (cmpibl_lit)
SEM (cmpible_reg)
SEM (cmpible_lit)
SEM (cmpibg_reg)
SEM (cmpibg_lit)
SEM (cmpibge_reg)
SEM (cmpibge_lit)
SEM (bbc_reg)
SEM (bbc_lit)
SEM (bbs_reg)
SEM (bbs_lit)
SEM (cmpi)
SEM (cmpi1)
SEM (cmpi2)
SEM (cmpi3)
SEM (cmpo)
SEM (cmpo1)
SEM (cmpo2)
SEM (cmpo3)
SEM (testno_reg)
SEM (testg_reg)
SEM (teste_reg)
SEM (testge_reg)
SEM (testl_reg)
SEM (testne_reg)
SEM (testle_reg)
SEM (testo_reg)
SEM (bno)
SEM (bg)
SEM (be)
SEM (bge)
SEM (bl)
SEM (bne)
SEM (ble)
SEM (bo)
SEM (b)
SEM (bx_indirect_offset)
SEM (bx_indirect)
SEM (bx_indirect_index)
SEM (bx_disp)
SEM (bx_indirect_disp)
SEM (callx_disp)
SEM (callx_indirect)
SEM (callx_indirect_offset)
SEM (ret)
SEM (calls)
SEM (fmark)
SEM (flushreg)

#undef SEMFULL
#undef SEMFAST
#undef SEM

/* Function unit handlers (user written).  */

extern int i960base_model_i960KA_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);
extern int i960base_model_i960CA_u_exec (SIM_CPU *, const IDESC *, int /*unit_num*/, int /*referenced*/);

/* Profiling before/after handlers (user written) */

extern void i960base_model_insn_before (SIM_CPU *, int /*first_p*/);
extern void i960base_model_insn_after (SIM_CPU *, int /*last_p*/, int /*cycles*/);

#endif /* I960BASE_DECODE_H */
