/*
   low.h

   Contains the defines for the target side of the multi-ice server.
   
   Copyright (C) 1999 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */
#include "defs.h"
#include "target.h"

/* These two are used for initializing and shutting down the target */
int low_open_target (char *target_port, char *byte_sex, int query);
int low_close_target ();

/* These are the thread related low-level functions */
int low_set_thread_for_resume (long thread_id);
int low_set_thread_for_query (char *input_buffer);
int low_is_thread_alive (char *input_buffer);
int low_get_current_thread (long *thread_id);

/* These are general queries to the target */
int low_get_offsets (CORE_ADDR *text, CORE_ADDR *data, CORE_ADDR *bss);
enum target_signal low_query_last_signal ();

int low_update_registers();
int low_write_registers();

int low_read_memory_raw (CORE_ADDR start, void *buffer, unsigned int *nbytes);
int low_read_memory (char *buffer, CORE_ADDR start_addr, unsigned int nbytes);
int low_write_memory (char *data, CORE_ADDR start_addr, unsigned int nbytes);

int low_set_breakpoint (CORE_ADDR bp_addr, int size);
int low_delete_breakpoint (CORE_ADDR bp_addr, int size);

int low_set_watchpoint (CORE_ADDR watch_addr, int size, enum watch_type type);
int low_delete_watchpoint (CORE_ADDR watch_addr, int size, enum watch_type type);

int low_resume (enum resume_mode mode, int signo);
int low_test (char *buffer);
