/* CPU family header for m32rxf.

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

#ifndef CPU_M32RXF_H
#define CPU_M32RXF_H

/* Maximum number of instructions that are fetched at a time.
   This is for LIW type instructions sets (e.g. m32r).  */
#define MAX_LIW_INSNS 2

/* Maximum number of instructions that can be executed in parallel.  */
#define MAX_PARALLEL_INSNS 2

/* CPU state information.  */
typedef struct {
  /* Hardware elements.  */
  struct {
  /* program counter */
  USI h_pc;
#define GET_H_PC() CPU (h_pc)
#define SET_H_PC(x) (CPU (h_pc) = (x))
  /* general registers */
  SI h_gr[16];
#define GET_H_GR(a1) CPU (h_gr)[a1]
#define SET_H_GR(a1, x) (CPU (h_gr)[a1] = (x))
  /* control registers */
  USI h_cr[16];
/* GET_H_CR macro user-written */
/* SET_H_CR macro user-written */
  /* accumulator */
  DI h_accum;
/* GET_H_ACCUM macro user-written */
/* SET_H_ACCUM macro user-written */
  /* accumulators */
  DI h_accums[2];
/* GET_H_ACCUMS macro user-written */
/* SET_H_ACCUMS macro user-written */
  /* condition bit */
  BI h_cond;
#define GET_H_COND() CPU (h_cond)
#define SET_H_COND(x) (CPU (h_cond) = (x))
  /* psw part of psw */
  UQI h_psw;
/* GET_H_PSW macro user-written */
/* SET_H_PSW macro user-written */
  /* backup psw */
  UQI h_bpsw;
#define GET_H_BPSW() CPU (h_bpsw)
#define SET_H_BPSW(x) (CPU (h_bpsw) = (x))
  /* backup bpsw */
  UQI h_bbpsw;
#define GET_H_BBPSW() CPU (h_bbpsw)
#define SET_H_BBPSW(x) (CPU (h_bbpsw) = (x))
  /* lock */
  BI h_lock;
#define GET_H_LOCK() CPU (h_lock)
#define SET_H_LOCK(x) (CPU (h_lock) = (x))
  } hardware;
#define CPU_CGEN_HW(cpu) (& (cpu)->cpu_data.hardware)
} M32RXF_CPU_DATA;

/* Cover fns for register access.  */
USI m32rxf_h_pc_get (SIM_CPU *);
void m32rxf_h_pc_set (SIM_CPU *, USI);
SI m32rxf_h_gr_get (SIM_CPU *, UINT);
void m32rxf_h_gr_set (SIM_CPU *, UINT, SI);
USI m32rxf_h_cr_get (SIM_CPU *, UINT);
void m32rxf_h_cr_set (SIM_CPU *, UINT, USI);
DI m32rxf_h_accum_get (SIM_CPU *);
void m32rxf_h_accum_set (SIM_CPU *, DI);
DI m32rxf_h_accums_get (SIM_CPU *, UINT);
void m32rxf_h_accums_set (SIM_CPU *, UINT, DI);
BI m32rxf_h_cond_get (SIM_CPU *);
void m32rxf_h_cond_set (SIM_CPU *, BI);
UQI m32rxf_h_psw_get (SIM_CPU *);
void m32rxf_h_psw_set (SIM_CPU *, UQI);
UQI m32rxf_h_bpsw_get (SIM_CPU *);
void m32rxf_h_bpsw_set (SIM_CPU *, UQI);
UQI m32rxf_h_bbpsw_get (SIM_CPU *);
void m32rxf_h_bbpsw_set (SIM_CPU *, UQI);
BI m32rxf_h_lock_get (SIM_CPU *);
void m32rxf_h_lock_set (SIM_CPU *, BI);

/* These must be hand-written.  */
extern CPUREG_FETCH_FN m32rxf_fetch_register;
extern CPUREG_STORE_FN m32rxf_store_register;

typedef struct {
  int empty;
} MODEL_M32RX_DATA;

union sem_fields {
    struct { /* empty sformat for unspecified field list */
      int empty;
    } fmt_empty;
    struct { /* e.g. add $dr,$sr */
      SI * i_dr;
      SI * i_sr;
      unsigned char in_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_add;
    struct { /* e.g. add3 $dr,$sr,$hash$slo16 */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_add3;
    struct { /* e.g. and3 $dr,$sr,$uimm16 */
      UINT f_uimm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_and3;
    struct { /* e.g. or3 $dr,$sr,$hash$ulo16 */
      UINT f_uimm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_or3;
    struct { /* e.g. addi $dr,$simm8 */
      INT f_simm8;
      SI * i_dr;
      unsigned char in_dr;
      unsigned char out_dr;
    } fmt_addi;
    struct { /* e.g. addv $dr,$sr */
      SI * i_dr;
      SI * i_sr;
      unsigned char in_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_addv;
    struct { /* e.g. addv3 $dr,$sr,$simm16 */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_addv3;
    struct { /* e.g. addx $dr,$sr */
      SI * i_dr;
      SI * i_sr;
      unsigned char in_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_addx;
    struct { /* e.g. cmp $src1,$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_cmp;
    struct { /* e.g. cmpi $src2,$simm16 */
      INT f_simm16;
      SI * i_src2;
      unsigned char in_src2;
    } fmt_cmpi;
    struct { /* e.g. cmpeq $src1,$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_cmpeq;
    struct { /* e.g. cmpz $src2 */
      SI * i_src2;
      unsigned char in_src2;
    } fmt_cmpz;
    struct { /* e.g. div $dr,$sr */
      SI * i_dr;
      SI * i_sr;
      unsigned char in_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_div;
    struct { /* e.g. divh $dr,$sr */
      SI * i_dr;
      SI * i_sr;
      unsigned char in_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_divh;
    struct { /* e.g. ld $dr,@$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ld;
    struct { /* e.g. ld $dr,@($slo16,$sr) */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ld_d;
    struct { /* e.g. ldb $dr,@$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ldb;
    struct { /* e.g. ldb $dr,@($slo16,$sr) */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ldb_d;
    struct { /* e.g. ldh $dr,@$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ldh;
    struct { /* e.g. ldh $dr,@($slo16,$sr) */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_ldh_d;
    struct { /* e.g. ld $dr,@$sr+ */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
      unsigned char out_sr;
    } fmt_ld_plus;
    struct { /* e.g. ld24 $dr,$uimm24 */
      ADDR i_uimm24;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_ld24;
    struct { /* e.g. ldi8 $dr,$simm8 */
      INT f_simm8;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_ldi8;
    struct { /* e.g. ldi16 $dr,$hash$slo16 */
      INT f_simm16;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_ldi16;
    struct { /* e.g. lock $dr,@$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_lock;
    struct { /* e.g. machi $src1,$src2,$acc */
      UINT f_acc;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_machi_a;
    struct { /* e.g. mulhi $src1,$src2,$acc */
      UINT f_acc;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_mulhi_a;
    struct { /* e.g. mv $dr,$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_mv;
    struct { /* e.g. mvfachi $dr,$accs */
      UINT f_accs;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_mvfachi_a;
    struct { /* e.g. mvfc $dr,$scr */
      UINT f_r2;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_mvfc;
    struct { /* e.g. mvtachi $src1,$accs */
      UINT f_accs;
      SI * i_src1;
      unsigned char in_src1;
    } fmt_mvtachi_a;
    struct { /* e.g. mvtc $sr,$dcr */
      UINT f_r1;
      SI * i_sr;
      unsigned char in_sr;
    } fmt_mvtc;
    struct { /* e.g. nop */
      int empty;
    } fmt_nop;
    struct { /* e.g. rac $accd,$accs,$imm1 */
      UINT f_accs;
      SI f_imm1;
      UINT f_accd;
    } fmt_rac_dsi;
    struct { /* e.g. seth $dr,$hash$hi16 */
      UINT f_hi16;
      SI * i_dr;
      unsigned char out_dr;
    } fmt_seth;
    struct { /* e.g. sll3 $dr,$sr,$simm16 */
      INT f_simm16;
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_sll3;
    struct { /* e.g. slli $dr,$uimm5 */
      UINT f_uimm5;
      SI * i_dr;
      unsigned char in_dr;
      unsigned char out_dr;
    } fmt_slli;
    struct { /* e.g. st $src1,@$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_st;
    struct { /* e.g. st $src1,@($slo16,$src2) */
      INT f_simm16;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_st_d;
    struct { /* e.g. stb $src1,@$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_stb;
    struct { /* e.g. stb $src1,@($slo16,$src2) */
      INT f_simm16;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_stb_d;
    struct { /* e.g. sth $src1,@$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_sth;
    struct { /* e.g. sth $src1,@($slo16,$src2) */
      INT f_simm16;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_sth_d;
    struct { /* e.g. st $src1,@+$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
      unsigned char out_src2;
    } fmt_st_plus;
    struct { /* e.g. unlock $src1,@$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_unlock;
    struct { /* e.g. satb $dr,$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_satb;
    struct { /* e.g. sat $dr,$sr */
      SI * i_sr;
      SI * i_dr;
      unsigned char in_sr;
      unsigned char out_dr;
    } fmt_sat;
    struct { /* e.g. sadd */
      int empty;
    } fmt_sadd;
    struct { /* e.g. macwu1 $src1,$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_macwu1;
    struct { /* e.g. msblo $src1,$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_msblo;
    struct { /* e.g. mulwu1 $src1,$src2 */
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_mulwu1;
  /* cti insns, kept separately so addr_cache is in fixed place */
  struct {
    union {
    struct { /* e.g. bc.s $disp8 */
      IADDR i_disp8;
    } fmt_bc8;
    struct { /* e.g. bc.l $disp24 */
      IADDR i_disp24;
    } fmt_bc24;
    struct { /* e.g. beq $src1,$src2,$disp16 */
      IADDR i_disp16;
      SI * i_src1;
      SI * i_src2;
      unsigned char in_src1;
      unsigned char in_src2;
    } fmt_beq;
    struct { /* e.g. beqz $src2,$disp16 */
      IADDR i_disp16;
      SI * i_src2;
      unsigned char in_src2;
    } fmt_beqz;
    struct { /* e.g. bl.s $disp8 */
      IADDR i_disp8;
      unsigned char out_h_gr_14;
    } fmt_bl8;
    struct { /* e.g. bl.l $disp24 */
      IADDR i_disp24;
      unsigned char out_h_gr_14;
    } fmt_bl24;
    struct { /* e.g. bcl.s $disp8 */
      IADDR i_disp8;
      unsigned char out_h_gr_14;
    } fmt_bcl8;
    struct { /* e.g. bcl.l $disp24 */
      IADDR i_disp24;
      unsigned char out_h_gr_14;
    } fmt_bcl24;
    struct { /* e.g. bra.s $disp8 */
      IADDR i_disp8;
    } fmt_bra8;
    struct { /* e.g. bra.l $disp24 */
      IADDR i_disp24;
    } fmt_bra24;
    struct { /* e.g. jc $sr */
      SI * i_sr;
      unsigned char in_sr;
    } fmt_jc;
    struct { /* e.g. jl $sr */
      SI * i_sr;
      unsigned char in_sr;
      unsigned char out_h_gr_14;
    } fmt_jl;
    struct { /* e.g. jmp $sr */
      SI * i_sr;
      unsigned char in_sr;
    } fmt_jmp;
    struct { /* e.g. rte */
      int empty;
    } fmt_rte;
    struct { /* e.g. trap $uimm4 */
      UINT f_uimm4;
    } fmt_trap;
    struct { /* e.g. sc */
      int empty;
    } fmt_sc;
    } fields;
#if WITH_SCACHE_PBB
    SEM_PC addr_cache;
#endif
  } cti;
#if WITH_SCACHE_PBB
  /* Writeback handler.  */
  struct {
    /* Pointer to argbuf entry for insn whose results need writing back.  */
    const struct argbuf *abuf;
  } write;
  /* x-before handler */
  struct {
    /*const SCACHE *insns[MAX_PARALLEL_INSNS];*/
    int first_p;
  } before;
  /* x-after handler */
  struct {
    int empty;
  } after;
  /* This entry is used to terminate each pbb.  */
  struct {
    /* Number of insns in pbb.  */
    int insn_count;
    /* Next pbb to execute.  */
    SCACHE *next;
  } chain;
#endif
};

/* The ARGBUF struct.  */
struct argbuf {
  /* These are the baseclass definitions.  */
  IADDR addr;
  const IDESC *idesc;
  char trace_p;
  char profile_p;
  /* cpu specific data follows */
  union sem semantic;
  int written;
  union sem_fields fields;
};

/* A cached insn.

   ??? SCACHE used to contain more than just argbuf.  We could delete the
   type entirely and always just use ARGBUF, but for future concerns and as
   a level of abstraction it is left in.  */

struct scache {
  struct argbuf argbuf;
};

/* Macros to simplify extraction, reading and semantic code.
   These define and assign the local vars that contain the insn's fields.  */

#define EXTRACT_IFMT_EMPTY_VARS \
  /* Instruction fields.  */ \
  unsigned int length;
#define EXTRACT_IFMT_EMPTY_CODE \
  length = 0; \

#define EXTRACT_IFMT_ADD_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_ADD_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_ADD3_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ADD3_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_AND3_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_AND3_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_OR3_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_OR3_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_ADDI_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  INT f_simm8; \
  unsigned int length;
#define EXTRACT_IFMT_ADDI_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_simm8 = EXTRACT_INT (insn, 16, 8, 8); \

#define EXTRACT_IFMT_ADDV3_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ADDV3_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_BC8_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp8; \
  unsigned int length;
#define EXTRACT_IFMT_BC8_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_disp8 = ((((EXTRACT_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4)))); \

#define EXTRACT_IFMT_BC24_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp24; \
  unsigned int length;
#define EXTRACT_IFMT_BC24_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_disp24 = ((((EXTRACT_INT (insn, 32, 8, 24)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BEQ_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  SI f_disp16; \
  unsigned int length;
#define EXTRACT_IFMT_BEQ_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_disp16 = ((((EXTRACT_INT (insn, 32, 16, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BEQZ_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  SI f_disp16; \
  unsigned int length;
#define EXTRACT_IFMT_BEQZ_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_disp16 = ((((EXTRACT_INT (insn, 32, 16, 16)) << (2))) + (pc)); \

#define EXTRACT_IFMT_BCL8_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp8; \
  unsigned int length;
#define EXTRACT_IFMT_BCL8_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_disp8 = ((((EXTRACT_INT (insn, 16, 8, 8)) << (2))) + (((pc) & (-4)))); \

#define EXTRACT_IFMT_BCL24_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  SI f_disp24; \
  unsigned int length;
#define EXTRACT_IFMT_BCL24_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_disp24 = ((((EXTRACT_INT (insn, 32, 8, 24)) << (2))) + (pc)); \

#define EXTRACT_IFMT_CMP_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_CMP_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_CMPI_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_CMPI_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_CMPEQ_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_CMPEQ_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_CMPZ_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_CMPZ_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_DIV_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_DIV_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_DIVH_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_DIVH_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_JC_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_JC_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_JL_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_JL_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_LD24_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_uimm24; \
  unsigned int length;
#define EXTRACT_IFMT_LD24_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_uimm24 = EXTRACT_UINT (insn, 32, 8, 24); \

#define EXTRACT_IFMT_LDI16_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_LDI16_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_MACHI_A_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_acc; \
  UINT f_op23; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_acc = EXTRACT_UINT (insn, 16, 8, 1); \
  f_op23 = EXTRACT_UINT (insn, 16, 9, 3); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_MVFACHI_A_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_op3; \
  unsigned int length;
#define EXTRACT_IFMT_MVFACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_UINT (insn, 16, 12, 2); \
  f_op3 = EXTRACT_UINT (insn, 16, 14, 2); \

#define EXTRACT_IFMT_MVFC_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MVFC_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_MVTACHI_A_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_op3; \
  unsigned int length;
#define EXTRACT_IFMT_MVTACHI_A_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_UINT (insn, 16, 12, 2); \
  f_op3 = EXTRACT_UINT (insn, 16, 14, 2); \

#define EXTRACT_IFMT_MVTC_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_MVTC_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_NOP_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_NOP_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_RAC_DSI_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_accd; \
  UINT f_bits67; \
  UINT f_op2; \
  UINT f_accs; \
  UINT f_bit14; \
  SI f_imm1; \
  unsigned int length;
#define EXTRACT_IFMT_RAC_DSI_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_accd = EXTRACT_UINT (insn, 16, 4, 2); \
  f_bits67 = EXTRACT_UINT (insn, 16, 6, 2); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_accs = EXTRACT_UINT (insn, 16, 12, 2); \
  f_bit14 = EXTRACT_UINT (insn, 16, 14, 1); \
  f_imm1 = ((EXTRACT_UINT (insn, 16, 15, 1)) + (1)); \

#define EXTRACT_IFMT_SETH_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_hi16; \
  unsigned int length;
#define EXTRACT_IFMT_SETH_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_hi16 = EXTRACT_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_SLLI_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_shift_op2; \
  UINT f_uimm5; \
  unsigned int length;
#define EXTRACT_IFMT_SLLI_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_shift_op2 = EXTRACT_UINT (insn, 16, 8, 3); \
  f_uimm5 = EXTRACT_UINT (insn, 16, 11, 5); \

#define EXTRACT_IFMT_ST_D_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  INT f_simm16; \
  unsigned int length;
#define EXTRACT_IFMT_ST_D_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_simm16 = EXTRACT_INT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_TRAP_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_uimm4; \
  unsigned int length;
#define EXTRACT_IFMT_TRAP_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_uimm4 = EXTRACT_UINT (insn, 16, 12, 4); \

#define EXTRACT_IFMT_SATB_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  UINT f_uimm16; \
  unsigned int length;
#define EXTRACT_IFMT_SATB_CODE \
  length = 4; \
  f_op1 = EXTRACT_UINT (insn, 32, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 32, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 32, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 32, 12, 4); \
  f_uimm16 = EXTRACT_UINT (insn, 32, 16, 16); \

#define EXTRACT_IFMT_SADD_VARS \
  /* Instruction fields.  */ \
  UINT f_op1; \
  UINT f_r1; \
  UINT f_op2; \
  UINT f_r2; \
  unsigned int length;
#define EXTRACT_IFMT_SADD_CODE \
  length = 2; \
  f_op1 = EXTRACT_UINT (insn, 16, 0, 4); \
  f_r1 = EXTRACT_UINT (insn, 16, 4, 4); \
  f_op2 = EXTRACT_UINT (insn, 16, 8, 4); \
  f_r2 = EXTRACT_UINT (insn, 16, 12, 4); \

/* Queued output values of an instruction.  */

struct parexec {
  union {
    struct { /* empty sformat for unspecified field list */
      int empty;
    } fmt_empty;
    struct { /* e.g. add $dr,$sr */
      SI dr;
    } fmt_add;
    struct { /* e.g. add3 $dr,$sr,$hash$slo16 */
      SI dr;
    } fmt_add3;
    struct { /* e.g. and3 $dr,$sr,$uimm16 */
      SI dr;
    } fmt_and3;
    struct { /* e.g. or3 $dr,$sr,$hash$ulo16 */
      SI dr;
    } fmt_or3;
    struct { /* e.g. addi $dr,$simm8 */
      SI dr;
    } fmt_addi;
    struct { /* e.g. addv $dr,$sr */
      BI condbit;
      SI dr;
    } fmt_addv;
    struct { /* e.g. addv3 $dr,$sr,$simm16 */
      BI condbit;
      SI dr;
    } fmt_addv3;
    struct { /* e.g. addx $dr,$sr */
      BI condbit;
      SI dr;
    } fmt_addx;
    struct { /* e.g. bc.s $disp8 */
      USI pc;
    } fmt_bc8;
    struct { /* e.g. bc.l $disp24 */
      USI pc;
    } fmt_bc24;
    struct { /* e.g. beq $src1,$src2,$disp16 */
      USI pc;
    } fmt_beq;
    struct { /* e.g. beqz $src2,$disp16 */
      USI pc;
    } fmt_beqz;
    struct { /* e.g. bl.s $disp8 */
      SI h_gr_14;
      USI pc;
    } fmt_bl8;
    struct { /* e.g. bl.l $disp24 */
      SI h_gr_14;
      USI pc;
    } fmt_bl24;
    struct { /* e.g. bcl.s $disp8 */
      SI h_gr_14;
      USI pc;
    } fmt_bcl8;
    struct { /* e.g. bcl.l $disp24 */
      SI h_gr_14;
      USI pc;
    } fmt_bcl24;
    struct { /* e.g. bra.s $disp8 */
      USI pc;
    } fmt_bra8;
    struct { /* e.g. bra.l $disp24 */
      USI pc;
    } fmt_bra24;
    struct { /* e.g. cmp $src1,$src2 */
      BI condbit;
    } fmt_cmp;
    struct { /* e.g. cmpi $src2,$simm16 */
      BI condbit;
    } fmt_cmpi;
    struct { /* e.g. cmpeq $src1,$src2 */
      BI condbit;
    } fmt_cmpeq;
    struct { /* e.g. cmpz $src2 */
      BI condbit;
    } fmt_cmpz;
    struct { /* e.g. div $dr,$sr */
      SI dr;
    } fmt_div;
    struct { /* e.g. divh $dr,$sr */
      SI dr;
    } fmt_divh;
    struct { /* e.g. jc $sr */
      USI pc;
    } fmt_jc;
    struct { /* e.g. jl $sr */
      SI h_gr_14;
      USI pc;
    } fmt_jl;
    struct { /* e.g. jmp $sr */
      USI pc;
    } fmt_jmp;
    struct { /* e.g. ld $dr,@$sr */
      SI dr;
    } fmt_ld;
    struct { /* e.g. ld $dr,@($slo16,$sr) */
      SI dr;
    } fmt_ld_d;
    struct { /* e.g. ldb $dr,@$sr */
      SI dr;
    } fmt_ldb;
    struct { /* e.g. ldb $dr,@($slo16,$sr) */
      SI dr;
    } fmt_ldb_d;
    struct { /* e.g. ldh $dr,@$sr */
      SI dr;
    } fmt_ldh;
    struct { /* e.g. ldh $dr,@($slo16,$sr) */
      SI dr;
    } fmt_ldh_d;
    struct { /* e.g. ld $dr,@$sr+ */
      SI dr;
      SI sr;
    } fmt_ld_plus;
    struct { /* e.g. ld24 $dr,$uimm24 */
      SI dr;
    } fmt_ld24;
    struct { /* e.g. ldi8 $dr,$simm8 */
      SI dr;
    } fmt_ldi8;
    struct { /* e.g. ldi16 $dr,$hash$slo16 */
      SI dr;
    } fmt_ldi16;
    struct { /* e.g. lock $dr,@$sr */
      SI dr;
      BI h_lock_0;
    } fmt_lock;
    struct { /* e.g. machi $src1,$src2,$acc */
      DI acc;
    } fmt_machi_a;
    struct { /* e.g. mulhi $src1,$src2,$acc */
      DI acc;
    } fmt_mulhi_a;
    struct { /* e.g. mv $dr,$sr */
      SI dr;
    } fmt_mv;
    struct { /* e.g. mvfachi $dr,$accs */
      SI dr;
    } fmt_mvfachi_a;
    struct { /* e.g. mvfc $dr,$scr */
      SI dr;
    } fmt_mvfc;
    struct { /* e.g. mvtachi $src1,$accs */
      DI accs;
    } fmt_mvtachi_a;
    struct { /* e.g. mvtc $sr,$dcr */
      USI dcr;
    } fmt_mvtc;
    struct { /* e.g. nop */
      int empty;
    } fmt_nop;
    struct { /* e.g. rac $accd,$accs,$imm1 */
      DI accd;
    } fmt_rac_dsi;
    struct { /* e.g. rte */
      UQI h_bpsw_0;
      USI h_cr_6;
      UQI h_psw_0;
      USI pc;
    } fmt_rte;
    struct { /* e.g. seth $dr,$hash$hi16 */
      SI dr;
    } fmt_seth;
    struct { /* e.g. sll3 $dr,$sr,$simm16 */
      SI dr;
    } fmt_sll3;
    struct { /* e.g. slli $dr,$uimm5 */
      SI dr;
    } fmt_slli;
    struct { /* e.g. st $src1,@$src2 */
      SI h_memory_src2;
      USI h_memory_src2_idx;
    } fmt_st;
    struct { /* e.g. st $src1,@($slo16,$src2) */
      SI h_memory_add__VM_src2_slo16;
      USI h_memory_add__VM_src2_slo16_idx;
    } fmt_st_d;
    struct { /* e.g. stb $src1,@$src2 */
      QI h_memory_src2;
      USI h_memory_src2_idx;
    } fmt_stb;
    struct { /* e.g. stb $src1,@($slo16,$src2) */
      QI h_memory_add__VM_src2_slo16;
      USI h_memory_add__VM_src2_slo16_idx;
    } fmt_stb_d;
    struct { /* e.g. sth $src1,@$src2 */
      HI h_memory_src2;
      USI h_memory_src2_idx;
    } fmt_sth;
    struct { /* e.g. sth $src1,@($slo16,$src2) */
      HI h_memory_add__VM_src2_slo16;
      USI h_memory_add__VM_src2_slo16_idx;
    } fmt_sth_d;
    struct { /* e.g. st $src1,@+$src2 */
      SI h_memory_new_src2;
      USI h_memory_new_src2_idx;
      SI src2;
    } fmt_st_plus;
    struct { /* e.g. trap $uimm4 */
      UQI h_bbpsw_0;
      UQI h_bpsw_0;
      USI h_cr_14;
      USI h_cr_6;
      UQI h_psw_0;
      SI pc;
    } fmt_trap;
    struct { /* e.g. unlock $src1,@$src2 */
      BI h_lock_0;
      SI h_memory_src2;
      USI h_memory_src2_idx;
    } fmt_unlock;
    struct { /* e.g. satb $dr,$sr */
      SI dr;
    } fmt_satb;
    struct { /* e.g. sat $dr,$sr */
      SI dr;
    } fmt_sat;
    struct { /* e.g. sadd */
      DI h_accums_0;
    } fmt_sadd;
    struct { /* e.g. macwu1 $src1,$src2 */
      DI h_accums_1;
    } fmt_macwu1;
    struct { /* e.g. msblo $src1,$src2 */
      DI accum;
    } fmt_msblo;
    struct { /* e.g. mulwu1 $src1,$src2 */
      DI h_accums_1;
    } fmt_mulwu1;
    struct { /* e.g. sc */
      int empty;
    } fmt_sc;
  } operands;
  /* For conditionally written operands, bitmask of which ones were.  */
  int written;
};

/* Collection of various things for the trace handler to use.  */

typedef struct trace_record {
  IADDR pc;
  /* FIXME:wip */
} TRACE_RECORD;

#endif /* CPU_M32RXF_H */
