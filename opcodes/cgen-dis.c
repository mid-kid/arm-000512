/* CGEN generic disassembler support code.

   Copyright (C) 1996, 1997, 1998 Free Software Foundation, Inc.

   This file is part of the GNU Binutils and GDB, the GNU debugger.

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
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "opcode/cgen.h"

/* Subroutine of build_dis_hash_table to add INSNS to the hash table.

   COUNT is the number of elements in INSNS.
   ENTSIZE is sizeof (CGEN_INSN) for the target.
   This is a bit tricky as the size of the attribute member of CGEN_INSN
   is variable among architectures.  This code could be moved to
   cgen-asm.in, but I prefer to keep it here for now.
   HTABLE points to the hash table.
   HENTBUF is a pointer to sufficiently large buffer of hash entries.
   The result is a pointer to the next entry to use.

   The table is scanned backwards as additions are made to the front of the
   list and we want earlier ones to be prefered.  */

static CGEN_INSN_LIST *
hash_insn_array (od, insns, count, entsize, htable, hentbuf)
     CGEN_OPCODE_DESC od;
     const CGEN_INSN * insns;
     int count;
     int entsize;
     CGEN_INSN_LIST ** htable;
     CGEN_INSN_LIST * hentbuf;
{
  int big_p = CGEN_OPCODE_ENDIAN (od) == CGEN_ENDIAN_BIG;
  CGEN_INSN * insn;

  for (insn = (CGEN_INSN *) ((char *) insns + entsize * (count - 1));
       insn >= insns;
       insn = (CGEN_INSN *) ((char *) insn - entsize), ++ hentbuf)
    {
      unsigned int hash;
      char buf [4];
      unsigned long value;

      if (! (* CGEN_OPCODE_DIS_HASH_P (od)) (insn))
	continue;

      /* We don't know whether the target uses the buffer or the base insn
	 to hash on, so set both up.  */

      value = CGEN_INSN_BASE_VALUE (insn);
      switch (CGEN_INSN_MASK_BITSIZE (insn))
	{
	case 8:
	  buf[0] = value;
	  break;
	case 16:
	  if (big_p)
	    bfd_putb16 ((bfd_vma) value, buf);
	  else
	    bfd_putl16 ((bfd_vma) value, buf);
	  break;
	case 32:
	  if (big_p)
	    bfd_putb32 ((bfd_vma) value, buf);
	  else
	    bfd_putl32 ((bfd_vma) value, buf);
	  break;
	default:
	  abort ();
	}

      hash = CGEN_OPCODE_DIS_HASH (od) (buf, value);
      hentbuf->next = htable [hash];
      hentbuf->insn = insn;
      htable [hash] = hentbuf;
    }

  return hentbuf;
}

/* Subroutine of build_dis_hash_table to add INSNS to the hash table.
   This function is identical to hash_insn_array except the insns are
   in a list.  */

static CGEN_INSN_LIST *
hash_insn_list (od, insns, htable, hentbuf)
     CGEN_OPCODE_DESC od;
     const CGEN_INSN_LIST * insns;
     CGEN_INSN_LIST ** htable;
     CGEN_INSN_LIST * hentbuf;
{
  int big_p = CGEN_OPCODE_ENDIAN (od) == CGEN_ENDIAN_BIG;
  const CGEN_INSN_LIST * ilist;

  for (ilist = insns; ilist != NULL; ilist = ilist->next, ++ hentbuf)
    {
      unsigned int hash;
      char buf [4];
      unsigned long value;

      if (! (* CGEN_OPCODE_DIS_HASH_P (od)) (ilist->insn))
	continue;

      /* We don't know whether the target uses the buffer or the base insn
	 to hash on, so set both up.  */

      value = CGEN_INSN_BASE_VALUE (ilist->insn);
      switch (CGEN_INSN_MASK_BITSIZE (ilist->insn))
	{
	case 8:
	  buf[0] = value;
	  break;
	case 16:
	  if (big_p)
	    bfd_putb16 ((bfd_vma) value, buf);
	  else
	    bfd_putl16 ((bfd_vma) value, buf);
	  break;
	case 32:
	  if (big_p)
	    bfd_putb32 ((bfd_vma) value, buf);
	  else
	    bfd_putl32 ((bfd_vma) value, buf);
	  break;
	default:
	  abort ();
	}

      hash = (* CGEN_OPCODE_DIS_HASH (od)) (buf, value);
      hentbuf->next = htable [hash];
      hentbuf->insn = ilist->insn;
      htable [hash] = hentbuf;
    }

  return hentbuf;
}

/* Build the disassembler instruction hash table.  */

static void
build_dis_hash_table (od)
     CGEN_OPCODE_DESC od;
{
  int count = cgen_insn_count (od) + cgen_macro_insn_count (od);
  CGEN_INSN_TABLE * insn_table = CGEN_OPCODE_INSN_TABLE (od);
  CGEN_INSN_TABLE * macro_insn_table = CGEN_OPCODE_MACRO_INSN_TABLE (od);
  unsigned int hash_size = CGEN_OPCODE_DIS_HASH_SIZE (od);
  CGEN_INSN_LIST * hash_entry_buf;
  CGEN_INSN_LIST **dis_hash_table;
  CGEN_INSN_LIST *dis_hash_table_entries;

  /* The space allocated for the hash table consists of two parts:
     the hash table and the hash lists.  */

  dis_hash_table = (CGEN_INSN_LIST **)
    xmalloc (hash_size * sizeof (CGEN_INSN_LIST *));
  memset (dis_hash_table, 0, hash_size * sizeof (CGEN_INSN_LIST *));
  dis_hash_table_entries = hash_entry_buf = (CGEN_INSN_LIST *)
    xmalloc (count * sizeof (CGEN_INSN_LIST));

  /* Add compiled in insns.
     Don't include the first one as it is a reserved entry.  */
  /* ??? It was the end of all hash chains, and also the special
     "invalid insn" marker.  May be able to do it differently now.  */

  hash_entry_buf = hash_insn_array (od,
				    (CGEN_INSN *) ((char *) insn_table->init_entries
						   + insn_table->entry_size),
				    insn_table->num_init_entries - 1,
				    insn_table->entry_size,
				    dis_hash_table, hash_entry_buf);

  /* Add compiled in macro-insns.  */

  hash_entry_buf = hash_insn_array (od, macro_insn_table->init_entries,
				    macro_insn_table->num_init_entries,
				    macro_insn_table->entry_size,
				    dis_hash_table, hash_entry_buf);

  /* Add runtime added insns.
     Later added insns will be prefered over earlier ones.  */

  hash_entry_buf = hash_insn_list (od, insn_table->new_entries,
				   dis_hash_table, hash_entry_buf);

  /* Add runtime added macro-insns.  */

  hash_insn_list (od, macro_insn_table->new_entries,
		  dis_hash_table, hash_entry_buf);

  CGEN_OPCODE_DIS_HASH_TABLE (od) = dis_hash_table;
  CGEN_OPCODE_DIS_HASH_TABLE_ENTRIES (od) = dis_hash_table_entries;
}

/* Return the first entry in the hash list for INSN.  */

CGEN_INSN_LIST *
cgen_dis_lookup_insn (od, buf, value)
     CGEN_OPCODE_DESC od;
     const char * buf;
     CGEN_INSN_INT value;
{
  unsigned int hash;

  if (CGEN_OPCODE_DIS_HASH_TABLE (od) == NULL)
    build_dis_hash_table (od);

  hash = (* CGEN_OPCODE_DIS_HASH (od)) (buf, value);

  return CGEN_OPCODE_DIS_HASH_TABLE (od) [hash];
}
