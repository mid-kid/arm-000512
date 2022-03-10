/* CGEN generic opcode support.

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
#include <ctype.h>
#include <stdio.h>
#include "ansidecl.h"
#include "libiberty.h"
#include "bfd.h"
#include "symcat.h"
#include "opcode/cgen.h"

static unsigned int hash_keyword_name
  PARAMS ((const CGEN_KEYWORD *, const char *, int));
static unsigned int hash_keyword_value
  PARAMS ((const CGEN_KEYWORD *, unsigned int));
static void build_keyword_hash_tables
  PARAMS ((CGEN_KEYWORD *));

/* Return number of hash table entries to use for N elements.  */
#define KEYWORD_HASH_SIZE(n) ((n) <= 31 ? 17 : 31)

/* Change the selected machine and endianness.  */

void
cgen_set_cpu (od, mach, endian)
     CGEN_OPCODE_DESC od;
     int mach;
     enum cgen_endian endian;
{
  CGEN_OPCODE_MACH (od) = mach;
  CGEN_OPCODE_ENDIAN (od) = endian;

  if (CGEN_OPCODE_ASM_HASH_TABLE (od))
    {
      free (CGEN_OPCODE_ASM_HASH_TABLE (od));
      free (CGEN_OPCODE_ASM_HASH_TABLE_ENTRIES (od));
      CGEN_OPCODE_ASM_HASH_TABLE (od) = NULL;
      CGEN_OPCODE_ASM_HASH_TABLE_ENTRIES (od) = NULL;
    }

  if (CGEN_OPCODE_DIS_HASH_TABLE (od))
    {
      free (CGEN_OPCODE_DIS_HASH_TABLE (od));
      CGEN_OPCODE_DIS_HASH_TABLE (od) = NULL;
    }
}

/* Look up *NAMEP in the keyword table KT.
   The result is the keyword entry or NULL if not found.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_lookup_name (kt, name)
     CGEN_KEYWORD *kt;
     const char *name;
{
  const CGEN_KEYWORD_ENTRY *ke;
  const char *p,*n;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  ke = kt->name_hash_table[hash_keyword_name (kt, name, 0)];

  /* We do case insensitive comparisons.
     If that ever becomes a problem, add an attribute that denotes
     "do case sensitive comparisons".  */

  while (ke != NULL)
    {
      n = name;
      p = ke->name;

      while (*p
	     && (*p == *n
		 || (isalpha ((unsigned char) *p)
		     && (tolower ((unsigned char) *p)
			 == tolower ((unsigned char) *n)))))
	++n, ++p;

      if (!*p && !*n)
	return ke;

      ke = ke->next_name;
    }

  if (kt->null_entry)
    return kt->null_entry;
  return NULL;
}

/* Look up VALUE in the keyword table KT.
   The result is the keyword entry or NULL if not found.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_lookup_value (kt, value)
     CGEN_KEYWORD *kt;
     int value;
{
  const CGEN_KEYWORD_ENTRY *ke;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  ke = kt->value_hash_table[hash_keyword_value (kt, value)];

  while (ke != NULL)
    {
      if (value == ke->value)
	return ke;
      ke = ke->next_value;
    }

  return NULL;
}

/* Add an entry to a keyword table.  */

void
cgen_keyword_add (kt, ke)
     CGEN_KEYWORD *kt;
     CGEN_KEYWORD_ENTRY *ke;
{
  unsigned int hash;

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  hash = hash_keyword_name (kt, ke->name, 0);
  ke->next_name = kt->name_hash_table[hash];
  kt->name_hash_table[hash] = ke;

  hash = hash_keyword_value (kt, ke->value);
  ke->next_value = kt->value_hash_table[hash];
  kt->value_hash_table[hash] = ke;

  if (ke->name[0] == 0)
    kt->null_entry = ke;
}

/* FIXME: Need function to return count of keywords.  */

/* Initialize a keyword table search.
   SPEC is a specification of what to search for.
   A value of NULL means to find every keyword.
   Currently NULL is the only acceptable value [further specification
   deferred].
   The result is an opaque data item used to record the search status.
   It is passed to each call to cgen_keyword_search_next.  */

CGEN_KEYWORD_SEARCH
cgen_keyword_search_init (kt, spec)
     CGEN_KEYWORD *kt;
     const char *spec;
{
  CGEN_KEYWORD_SEARCH search;

  /* FIXME: Need to specify format of PARAMS.  */
  if (spec != NULL)
    abort ();

  if (kt->name_hash_table == NULL)
    build_keyword_hash_tables (kt);

  search.table = kt;
  search.spec = spec;
  search.current_hash = 0;
  search.current_entry = NULL;
  return search;
}

/* Return the next keyword specified by SEARCH.
   The result is the next entry or NULL if there are no more.  */

const CGEN_KEYWORD_ENTRY *
cgen_keyword_search_next (search)
     CGEN_KEYWORD_SEARCH *search;
{
  /* Has search finished?  */
  if (search->current_hash == search->table->hash_table_size)
    return NULL;

  /* Search in progress?  */
  if (search->current_entry != NULL
      /* Anything left on this hash chain?  */
      && search->current_entry->next_name != NULL)
    {
      search->current_entry = search->current_entry->next_name;
      return search->current_entry;
    }

  /* Move to next hash chain [unless we haven't started yet].  */
  if (search->current_entry != NULL)
    ++search->current_hash;

  while (search->current_hash < search->table->hash_table_size)
    {
      search->current_entry = search->table->name_hash_table[search->current_hash];
      if (search->current_entry != NULL)
	return search->current_entry;
      ++search->current_hash;
    }

  return NULL;
}

/* Return first entry in hash chain for NAME.
   If CASE_SENSITIVE_P is non-zero, return a case sensitive hash.  */

static unsigned int
hash_keyword_name (kt, name, case_sensitive_p)
     const CGEN_KEYWORD *kt;
     const char *name;
     int case_sensitive_p;
{
  unsigned int hash;

  if (case_sensitive_p)
    for (hash = 0; *name; ++name)
      hash = (hash * 97) + (unsigned char) *name;
  else
    for (hash = 0; *name; ++name)
      hash = (hash * 97) + (unsigned char) tolower (*name);
  return hash % kt->hash_table_size;
}

/* Return first entry in hash chain for VALUE.  */

static unsigned int
hash_keyword_value (kt, value)
     const CGEN_KEYWORD *kt;
     unsigned int value;
{
  return value % kt->hash_table_size;
}

/* Build a keyword table's hash tables.
   We probably needn't build the value hash table for the assembler when
   we're using the disassembler, but we keep things simple.  */

static void
build_keyword_hash_tables (kt)
     CGEN_KEYWORD *kt;
{
  int i;
  /* Use the number of compiled in entries as an estimate for the
     typical sized table [not too many added at runtime].  */
  unsigned int size = KEYWORD_HASH_SIZE (kt->num_init_entries);

  kt->hash_table_size = size;
  kt->name_hash_table = (CGEN_KEYWORD_ENTRY **)
    xmalloc (size * sizeof (CGEN_KEYWORD_ENTRY *));
  memset (kt->name_hash_table, 0, size * sizeof (CGEN_KEYWORD_ENTRY *));
  kt->value_hash_table = (CGEN_KEYWORD_ENTRY **)
    xmalloc (size * sizeof (CGEN_KEYWORD_ENTRY *));
  memset (kt->value_hash_table, 0, size * sizeof (CGEN_KEYWORD_ENTRY *));

  /* The table is scanned backwards as we want keywords appearing earlier to
     be prefered over later ones.  */
  for (i = kt->num_init_entries - 1; i >= 0; --i)
    cgen_keyword_add (kt, &kt->init_entries[i]);
}

/* Hardware support.  */

/* Lookup a hardware element by its name.  */

const CGEN_HW_ENTRY *
cgen_hw_lookup_by_name (od, name)
     CGEN_OPCODE_DESC od;
     const char *name;
{
  const CGEN_HW_ENTRY * hw = CGEN_OPCODE_HW_LIST (od);

  while (hw != NULL)
    {
      if (strcmp (name, hw->name) == 0)
	return hw;
      hw = hw->next;
    }

  return NULL;
}

/* Lookup a hardware element by its number.
   Hardware elements are enumerated, however it may be possible to add some
   at runtime, thus HWNUM is not an enum type but rather an int.  */

const CGEN_HW_ENTRY *
cgen_hw_lookup_by_num (od, hwnum)
     CGEN_OPCODE_DESC od;
     int hwnum;
{
  const CGEN_HW_ENTRY * hw = CGEN_OPCODE_HW_LIST (od);

  /* ??? This can be speeded up if we first make a guess into
     the compiled in table.  */
  while (hw != NULL)
    {
      if (hwnum == hw->type)
	return hw;
    }
  return NULL;
}

/* Instruction support.  */

/* Return number of instructions.  This includes any added at runtime.  */

int
cgen_insn_count (od)
     CGEN_OPCODE_DESC od;
{
  int count = CGEN_OPCODE_INSN_TABLE (od)->num_init_entries;
  CGEN_INSN_LIST * insn = CGEN_OPCODE_INSN_TABLE (od)->new_entries;

  for ( ; insn != NULL; insn = insn->next)
    ++count;

  return count;
}

/* Return number of macro-instructions.
   This includes any added at runtime.  */

int
cgen_macro_insn_count (od)
     CGEN_OPCODE_DESC od;
{
  int count = CGEN_OPCODE_MACRO_INSN_TABLE (od)->num_init_entries;
  CGEN_INSN_LIST * insn = CGEN_OPCODE_MACRO_INSN_TABLE (od)->new_entries;

  for ( ; insn != NULL; insn = insn->next)
    ++count;

  return count;
}

/* Cover function to read and properly byteswap an insn value.  */

CGEN_INSN_INT
cgen_get_insn_value (od, buf, length)
     CGEN_OPCODE_DESC od;
     unsigned char *buf;
     int length;
{
  CGEN_INSN_INT value;

  switch (length)
    {
    case 8:
      value = *buf;
      break;
    case 16:
      if (CGEN_OPCODE_INSN_ENDIAN (od) == CGEN_ENDIAN_BIG)
	value = bfd_getb16 (buf);
      else
	value = bfd_getl16 (buf);
      break;
    case 32:
      if (CGEN_OPCODE_INSN_ENDIAN (od) == CGEN_ENDIAN_BIG)
	value = bfd_getb32 (buf);
      else
	value = bfd_getl32 (buf);
      break;
    default:
      abort ();
    }

  return value;
}

/* Cover function to store an insn value properly byteswapped.  */

void
cgen_put_insn_value (od, buf, length, value)
     CGEN_OPCODE_DESC od;
     unsigned char *buf;
     int length;
     CGEN_INSN_INT value;
{
  switch (length)
    {
    case 8:
      buf[0] = value;
      break;
    case 16:
      if (CGEN_OPCODE_INSN_ENDIAN (od) == CGEN_ENDIAN_BIG)
	bfd_putb16 (value, buf);
      else
	bfd_putl16 (value, buf);
      break;
    case 32:
      if (CGEN_OPCODE_INSN_ENDIAN (od) == CGEN_ENDIAN_BIG)
	bfd_putb32 (value, buf);
      else
	bfd_putl32 (value, buf);
      break;
    default:
      abort ();
    }
}
