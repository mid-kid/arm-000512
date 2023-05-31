/* Main code for multi-ice server for GDB.
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

#include <getopt.h>
#include <stdio.h>
#include <signals.h>
#include <string.h>
#include <stdlib.h>

#include "defs.h"
#include "server.h"
#include "low.h"
#include "remote-utils.h"

/*
 * These variables are used to track command-line options.
 */
int show_config_dialog = 0;
int debug_on = 0;
int show_help = 0;
int rdi_logging = 0;

/*
 * This is the sticky flag for using extended remote protocol.
 */

int use_extended_ops = 1;

/*
 * Keep track of the command line arguments here.
 */

int exit_status = 0;
int exit_now = 0;

/*
 * This one is set by the Cntrl-C handler, and is used to
 * tell the server to close the connection to the remote
 * GDB.
 */

int close_connection_now = 0;

/*
 * Current thread
 */
int selected_thread = 0;

/*
 * This is the option descriptors, as used by getopt.
 * Remember, if you add an option, be sure to add a
 * help string to the help_strings array below as well.
 */

struct option long_options[] =
{
  {"byte-sex", required_argument, 0, 'b'},
  {"config-dialog", no_argument, &show_config_dialog, 1},
  {"debug", no_argument, &debug_on, 1},
  {"help", no_argument, &show_help, 1},
  {"rdi-logging", no_argument, &rdi_logging, 1},
  {"remote-port", required_argument, 0, 'r'},
  {"stop-others", no_argument, &target_stop_others, 0},
  {"target-port", required_argument, 0, 't'}
};

/*
 * This is what is printed by print_help (and what you see if you
 * do --help.  Be sure to keep it in sync with the options.
 */

char *help_strings[] =
{
  "--byte-sex  {b,l}  - Specify the byte-sex of the target on restart.\n",
  "--config-dialog    - Post configuration dialog for Multi-ICE DLL.\n",
  "--debug            - Turn on debugging of remote protocol requests.\n",
  "--help             - Print out this help message.\n",
  "--rdi-logging      - Turn on RDI logging.\n",
  "--remote-port port - Specify the port to listen for remote connections.\n",
  "--stop-others      - Execution of target stops other modules.\n",
  "--target-port port - Specify the port to which the target is attached.\n",
  (char *) NULL
};

void exit_handler();
int run_test(char *input_buffer);
  
/* ------------------------------------------------------------
 *  main
 *
 * This is the main for the multi-ice variant of gdbserver.  It opens
 * a listening port, and a connection to the debug target, and then
 * waits for a connection on the listener.  When the connection is
 * made, it processes requests from the client till the connection is
 * closed, then goes back to listening.
 * ------------------------------------------------------------
 */

int
main (int argc, char *argv[])
{

#define INITIAL_LENGTH 2000
  
  char initial_buf[INITIAL_LENGTH], *input_buffer = initial_buf;
  int input_buffer_len = INITIAL_LENGTH, message_len;
  char *remote_port = "2331", *target_port = NULL, *byte_sex = NULL;
  char c;

  /*
   * First parse the command line options:
   */

  while (1)
    {
      int option_index;
 
      c = getopt_long_only (argc, argv, "", long_options, &option_index);

      if (c == EOF)
	{
	  break;
	}

      if (c == 0 && long_options[option_index].flag == 0)
	  c = long_options[option_index].val;

	switch (c)
	  {
	  case 0:
	    break;
	  case 'b':
	    byte_sex = optarg;
	    if ((*byte_sex != 'b') && (*byte_sex != 'l')
		&& (*byte_sex != 'B') && (*byte_sex != 'L'))
	      {
		output_error ("Bad value for --byte-sex: \"%s\", ", byte_sex);
		output_error ("should be \"b\" or \"l\"\n");
		exit(1);
	      }
	    break;
	  case 'r':
	    remote_port = optarg;
	    break;
	  case 't':
	    target_port = optarg;
	    break;
  	  case '?':
	    output ("Available options are:\n");
	    print_help ();
	    exit (1);
	  }
      }

  if (show_help)
    {
      print_help ();
      exit (0);
    }


  /*
   * Now do any platform specific initializations
   */

  platform_init();
  
  /*
   * First establish a connection to the target.
   */
  
  if (!low_open_target (target_port, byte_sex, show_config_dialog))
    {
      if (target_port == NULL)
	output("Error opening target.\nExiting...\n");
      else
	output("Error opening target at %s\nExiting...\n", remote_port);
      
      exit(1);
    }

  /*
   * Now open the listening port.  Bag out if we can't open it.
   */
  
  if (!open_listener (remote_port))
    {
      output ("Error opening listener port %s\nExiting...\n", remote_port);
      low_close_target ();
      exit (1);
    }

  output ("GDB Server starting on port %s\n", remote_port);

  signal (SIGINT, exit_handler);
  signal (SIGTERM, exit_handler);
  signal (SIGQUIT, exit_handler);
  
  while (!exit_now)
    {
      if (wait_for_connection())
	{
	  
	  while (!exit_now && !close_connection_now)
	    {
	      if (!handle_system_events ())
		{
		  exit_now = 1;
		  break;
		}
	      else if (close_connection_now)
		{
		  exit_now = 0;
		  close_connection_now = 0;
		  break;
		}
	      
	      message_len = getpkt (input_buffer, &input_buffer_len);
	      if (message_len >= 0)
		{
		  if (!dispatch (input_buffer, message_len))
		    {
		      /* Handle errors here */
		    }
		}
	      else
		{
		  break;
		}
	    }

	  /* If we got here, then the remote user died or detached,
	   * so close the connection, and wait for another...
	   */
	  
	  close_connection ();
	  close_connection_now = 0;
	}
      else
	{
	  output_error ("Attempt to connect failed\n");
	}
    }

  close_listener ();
  low_close_target ();

  output ("Exiting...\n");
  return exit_status;
}

/* ------------------------------------------------------------
 * dispatch
 *
 * This takes a message, decodes the protocol request, and dispatches
 * the message to the appropriate functions that in the target.  It also
 * handles whatever response the message requires before returning.
 * ------------------------------------------------------------
 */

int
dispatch (char *input_buffer, int message_length)
{
    char key;
    char *res;
    int signo;
    enum bp_action_type bp_action = BREAKPOINT_SET;

  if (debug_on)
    {
      output ("Recieved packet: %s\n", input_buffer);
    }

  key = input_buffer[0];
    input_buffer++;
    
    switch (key)
      {

      case 'H': /* Set thread */
	return handle_thread (input_buffer, THREAD_SET);
      case 'g': /* read registers */
          if (selected_thread == 0) {
              return handle_read_registers(input_buffer);
          } else {
              return low_thread_op(input_buffer-1);
          }
      case 'G': /* write registers */
          if (selected_thread == 0) {
              return handle_write_registers (input_buffer);
          } else {
              return low_thread_op(input_buffer-1);
          }
      case 'P': /* write a particular register */
          if (selected_thread == 0) {
              return handle_write_a_register (input_buffer);
          } else {
              return low_thread_op(input_buffer-1);
          }
      case 'm': /* read memory */
	return handle_read_memory (input_buffer);
      case 'M': /* write memory */
	return handle_write_memory (input_buffer);
      case 'X': /* write memory, binary data */
	putpkt ("");
	return 1;
	/* For now, we don't support binary downloads... */
	/* return handle_write_memory (input_buffer); */
      case 'z': /* This is the breakpoint delete packet */
	bp_action = BREAKPOINT_DELETE;
      case 'Z': /* This is the breakpoint set packet */
	key = input_buffer[0];
	input_buffer++;
	if (*input_buffer != ',')
	  {
	    output_error ("Malformed breakpoint set, no \",\".\n");
	    putpkt ("ENN");
	    return 0;
	  }
	input_buffer++;
	switch (key)
	  {
	  case '0':
	    /* JT's breakpoint patches don't explicitly request
	       a HW breakpoint.  We will do our best on this side...

	       putpkt ("ENN");
	       output_error ("Can't do software breakpoints on this side.\n");
	    */
	  case '1':
	    return handle_breakpoint (bp_action, input_buffer);
	  case '2':
	    return handle_watchpoint (bp_action, WATCHPOINT_WRITE,
				      input_buffer);
	  case '3':
	    return handle_watchpoint (bp_action, WATCHPOINT_READ,
				      input_buffer);
	  case '4':
	    return handle_watchpoint (bp_action, WATCHPOINT_ACCESS,
				      input_buffer);
	  default:
	    output_error ("Bad \"Z\" packet index: %c.\n", key);
	    putpkt ("ENN");
	    return 0;
	  }
      case 'c': /* continue */
	return handle_resume (input_buffer, RESUME_CONTINUE, 0);
      case 'C': /* continue with signal */
	input_buffer = remove_signal (input_buffer, &signo);
	if (input_buffer == NULL)
	  {
	    return 0;
	  }
	return handle_resume (input_buffer, RESUME_CONTINUE, signo);
      case 's': /* step */
	return handle_resume (input_buffer, RESUME_STEP, 0);
      case 'S': /* step with signal */
	input_buffer = remove_signal (input_buffer, &signo);
	return handle_resume (input_buffer, RESUME_STEP, signo);
      case '?': /* query last signal */
	return handle_last_signal (input_buffer);
      case 'D': /* detach */
	return handle_detach (input_buffer);
      case 'T': /* query is thread alive */
	return handle_thread (input_buffer, THREAD_ALIVE);
      case 'R': /* restart the remote server */
	return handle_restart (input_buffer);
      case '!': /* use extended ops */
	enable_extended_ops();
	return 1;
      case 'k': /* kill target */
	return handle_kill_target (input_buffer);
      case 'd': /* toggle debug */
	return handle_toggle_debug (input_buffer);
      case 'r': /* reset */
	return handle_reset (input_buffer);
      case 't': /* search memory */
	return handle_search_memory (input_buffer);
      case 'q': /* general query */
	return handle_general_query (input_buffer);
      case 'Q': /* general set */
	return handle_general_set (input_buffer);
      default:  /* Send a packet not recognized reply */
	putpkt("");
	return 1;
      }
}

/*
 * print_help
 *
 * This prints out all the help strings.
 */
void
print_help()
{
    int i;

    for (i = 0; help_strings[i] != NULL; i++)
      {
	output (help_strings[i]);
      }
}


/*
 * handle_thread
 *
 * This handles all the thread queries using the H packed syntax.
 */

int
handle_thread (char *input_buffer, enum thread_mode mode)
{
  char key, *ptr;
  long thread_id;

  switch (mode)
    {
    case THREAD_SET:
      key = input_buffer[0];
      input_buffer++;
      
      thread_id = strtol (input_buffer, &ptr, 0x10);  // Protocol uses hex values
      if (ptr == input_buffer)
	{
	  putpkt ("ENN");
	  output_error ("Got invalid thread id: %s\n", input_buffer);
	  return 0;
	}
      
      switch (key)
	{
	case 'c':
	  if (low_set_thread_for_resume (thread_id))
	    {
	      putpkt("OK");
	      return 1;
	    }
	  else
	    {
	      putpkt("ENN");
	      return 0;
	    }
	case 'g':
          selected_thread = thread_id;
          if (selected_thread) {
              return low_set_thread_for_query (input_buffer-2);
          } else {
	      putpkt("OK");
	      return 1;
          }
	default:
	  putpkt("ENN");
	}
      break;
    case THREAD_ALIVE:
      
      thread_id = strtol (input_buffer, &ptr, 0x10);
      if (ptr == input_buffer)
	{
	  putpkt ("ENN");
	  output_error ("Got invalid thread id: %s\n", input_buffer);
	  return 0;
	}
      if (thread_id) {
          return low_is_thread_alive (input_buffer-1);
      } else {
	  putpkt("OK");
      }
      return 1;
    default:
      putpkt("");
      return 1;
    }
  
  return 1;
}

/*
 * handle_read_registers
 *
 * This returns the entire register set in the form required by
 * the 'g' packet.  ** Only in non-thread mode
 */
int
handle_read_registers (char *input_buffer)
{
  char buffer[(REGISTER_BYTES * 2) + 1];

  if (!low_update_registers ())
    {
        putpkt("ENN");
        return 0;
    }
  
  convert_bytes_to_ascii (registers, buffer, REGISTER_BYTES, 0);
  putpkt(buffer);
  return 1;
}

/*
 * handle_write_registers
 *
 * This stores away all the registers...  In the grand tradition of
 * gdb, we don't actually write down to the board till we restart.
 */

int
handle_write_registers (char *input_buffer)
{

  char buffer[REGISTER_BYTES];
  
  if (!low_update_registers ())
    {
      putpkt ("ENN");
      return 0;
    }
  
  convert_ascii_to_bytes (input_buffer, buffer, REGISTER_BYTES, 0);
  memcpy (&registers, (char *) buffer, REGISTER_BYTES);
  registers_are_dirty = 1;
  low_write_registers ();
  return 1;
}

/*
 * handle_write_a_register
 *
 * This writes a single register into the register array.  The register
 * is not actually written down to the board at this point.
 */

int
handle_write_a_register (char *input_buffer)
{
  char *val_ptr;
  int regno;

  if (!low_update_registers ())
    {
      putpkt ("ENN");
      return 0;
    }

  val_ptr = (char *) strchr (input_buffer, '=');
  if (val_ptr == NULL)
    {
      if (debug_on)
	{
	  output_error ("Malformed P request - no \"=\": %s\n", input_buffer);
	}
      putpkt ("ENN");
      return 0;
    }

  *val_ptr = '\0';
  sscanf (input_buffer, "%x", &regno);
  /* FIXME - Check that regno is a valid register */
  
  val_ptr++;
  if (strlen (val_ptr) != (REGISTER_RAW_SIZE (regno) * 2))
    {
      output_error ("Malformed register value for register %d ", regno);
      output_error ("- wrong size: %s\n", val_ptr);
      return 0;
    }

  convert_ascii_to_bytes (val_ptr, &registers[REGISTER_BYTE (regno)],
			  REGISTER_RAW_SIZE (regno), 0);
  registers_are_dirty = 1;

  /* For now, I am always flushing the changes so I can test the
     code.  Remove this from the production version. */
  
  low_write_registers();
  
  putpkt("OK");
  return 1;
}

int
handle_read_memory (char *input_buffer)
{
  unsigned int len, nbytes, nchars;
  char static_buf[1024];
  char *buffer = static_buf;
  CORE_ADDR start_addr;
  int result;

  decode_m_packet (input_buffer, &start_addr, &nbytes);  
  if (debug_on)
    {
      output ("Reading %d bytes starting at 0x%x\n", nbytes, start_addr);
    }

  nchars = 2 * nbytes + 1;
  if (nchars > 1024)
    {
      buffer = (char *) malloc (nchars);
    }
  
  len = low_read_memory (buffer, start_addr, nbytes);
  if (len > 0)
    {
      result = 1;
    }
  else
    {
      /* HACK ALERT:  When  gdb first tries to connect, to the
	 target, the fp often points into never-never land.  If tries
	 to decode the prolog, and than ends up getting it to read
	 invalid memory, and the whole thing chokes.  To keep this
	 from happening, I will return as many zeros as requested... */
      
      memset (buffer, '0', nchars);
      output_error ("Failed to read memory at address 0x%x\n", start_addr);
      result = 0;
    }

  putpkt (buffer);
  
  if (buffer != static_buf)
    {
      free (buffer);
    }
  
  return result;
}

int
handle_write_memory (char *input_buffer)
{
  unsigned int len, nbytes;
  char data[1024]; /* FIXME -static buffer used! */
  CORE_ADDR start_addr;

  decode_M_packet (input_buffer, data, &start_addr, &nbytes);  
  if (debug_on)
    {
      output ("Writing %d bytes starting at 0x%x\n", nbytes, start_addr);
    }
  
  len = low_write_memory (data, start_addr, nbytes);
  if (len == nbytes)
    {
      putpkt ("OK");
      return 1;
    }
  else
    {
      putpkt ("ENN");
      return 0;
    }
}
/*
 * remove_signal
 *
 * This strips the signal part off the C & S packets, and returns
 * the value in SIGNO.  It returns a pointer to the first character in
 * input_buffer after the ';', or NULL for error.
 */

char *
remove_signal (char *input_buffer, int *signo)
{
  char *p;
  
  p = (char *) strchr (input_buffer, ';');

  /* If there is no ";", then the whole thing is the signal... */
  if (p == NULL)
    {
      sscanf (input_buffer, "%x", signo);
      p = input_buffer + strlen(input_buffer);
    }
  else
    {
      *p = '\0';
      sscanf (input_buffer, "%x", signo);
      
      p = input_buffer++;
    }

  return p;
}

/*
 * handle_resume
 *
 * This resumes the target.  RESUME_MODE is one of RESUME_STEP or
 * RESUME_CONTINUE.  The input buffer can be blank, or can optionally contain
 * the line at which to resume execution.  SIGNO is the signal with which
 * to resume.  It is ignored.
 */

int
handle_resume (char *input_buffer, enum resume_mode mode, int signo)
{
  int stop_signal;
  char return_pkt[4];

  /* Forget about 'selected' thread */
  selected_thread = 0;
  
  /*
   * See if there is an address to resume at, if not default
   * to the current PC.
   */

  if (*input_buffer != '\0')
    {
      convert_ascii_to_bytes (input_buffer,
			      &registers[REGISTER_BYTE (PC_REGNUM)],
			      REGISTER_RAW_SIZE (PC_REGNUM), 0);
    }

  if (debug_on && (mode == RESUME_CONTINUE))
    {
      output ("Executing...\n");
    }
  enable_async_io();
  stop_signal = low_resume (mode, signo);
  disable_async_io();
  if (debug_on && (mode == RESUME_CONTINUE))
    {
      output ("Stopped...\n");
    }
  
  /* We have run, so the registers are no longer up-to-date */

  registers_up_to_date = 0;
  registers_are_dirty = 0;

  /* Now write the return signal. */
  sprintf (return_pkt, "S%02x", stop_signal);

  putpkt (return_pkt);
  
  return 1;
}

/*
 * handle_last_signal
 *
 * Actually, this returns the last reason for stopping.
 */

int
handle_last_signal (char *input_buffer)
{
  enum target_signal signal;
  char buffer[8];
  
  signal = low_query_last_signal ();
  sprintf (buffer, "S%02x", signal);
  putpkt (buffer);
  return 1;
}

/*
 * handle_detach
 *
 * This detaches from the remote connection, and the we wait for
 * another connection.
 */

int
handle_detach (char *input_buffer)
{
  close_connection_now = 1;
  putpkt ("OK");
  return 1;
}

/*
 * handle_restart
 *
 * I am hijacking this to mean kill the server.  Not great, but it
 * is better than kill or detach.
 */

int
handle_restart (char *input_buffer)
{
  close_connection_now = 1;
  exit_now = 1;
  return 1;
}

/*
 * handle_toggle_debug
 *
 * This toggles the internal debugging flag.
 */

int
handle_toggle_debug (char *input_buffer)
{
  if (debug_on)
    debug_on = 0;
  else
    debug_on = 1;
  
  return 1;
}

int
handle_reset (char *input_buffer)
{
  return 1;
}

/*
 * handle_kill_target
 *
 * This actually just disconnects.  Since gdb issues 'k' when it exits
 * it would be very inconvenient if this were to kill the target.
 */

int
handle_kill_target (char *input_buffer)
{

  close_connection_now = 1;

  return 1;
}

/*
 * handle_search_memory
 *
 * Currently a no-op
 */

int
handle_search_memory (char *input_buffer)
{
  return 1;
}

/*
 * handle_general_query
 *
 * This handles the 'q' packets.  Right now it does qOffset, and the
 * thread query (though this returns nothing useful since you can't
 * query threads through the RDI.
 */

int
handle_general_query (char *input_buffer)
{
  char key;
  long thread_id;
  
  /* I couldn't find a general enumeration of the
     general query messages...  So these are all I could find. */
  
  key = input_buffer[0];

  switch (key)
    {
    case 'C': /* This is the Current thread request */
      if (low_get_current_thread(&thread_id))
	{
	  char buffer[32];
	  sprintf (buffer, "QC%x", thread_id);
	  putpkt (buffer);
	}
      else
	{
	  /* The lower layer may not be able to handle return
	     the current thread, in which case, return "". */
	  putpkt ("");
	}
      return 1;
    case 'O': /* The offsets query */
      {
	char buffer[256];
	CORE_ADDR text, data, bss;
	
	if (low_get_offsets (&text, &data, &bss))
	  {
	    sprintf (buffer, "Text=%x;Data=%x;Bss=%x", text, data, bss);
	    putpkt (buffer);
	    return 1;
	  }
	else
	  {
	    putpkt ("");
	    return 0;
	  }
      }
    case 'P': /* This is the thread info request */
      putpkt ("");
      return 1; 
    case 'E': /* This is for private use, to do tests... */
      return run_test(++input_buffer);
    case 'L': /* Get thread list */
      return low_thread_op(input_buffer-1);
    default:
      putpkt ("");
      return 1;
    }
  return 1;
}

/*
 * handle_general_set
 *
 * This would handle the catchall set commands.  For now we do nothing.
 * Set thread is about the only one I currently know what to do anything
 * about, and that doesn't work for RDI1.50, so there...
 */

int
handle_general_set (char *input_buffer)
{
  return 1;
}

/*
 * handle_breakpoint
 *
 * This handles setting and deleting breakpoints.  ACTION is one of
 * BREAKPOINT_SET of BREAKPOINT_DELETE.  The rest of INPUT_BUFFER
 * contains the address.
 */

int
handle_breakpoint (enum bp_action_type action, char *input_buffer)
{
  char ch;
  char *p;
  CORE_ADDR bp_addr;
  int result;
  int size = 0;

  if ((p = (char *) strchr(input_buffer, ',')) != NULL)
    {
      *p = '\0';
      p++;
      sscanf (p, "%d", &size);
    }

  bp_addr = 0;
  while ((ch = *(input_buffer++)) != '\0')
    {
      bp_addr = bp_addr << 4;
      bp_addr |= fromhex (ch) & 0x0f;
    }

  switch (action)
    {
    case BREAKPOINT_SET:
      if (debug_on)
	{
	  output ("Setting breakpoint at %x of length %d\n", bp_addr, size);
	}
      result = low_set_breakpoint (bp_addr, size);
      break;
    case BREAKPOINT_DELETE:
      if (debug_on)
	{
	  output ("Removing breakpoint at %x of length %d\n", bp_addr, size);
	}
      result = low_delete_breakpoint (bp_addr, size);
      break;
    }

  if (result)
    {
      putpkt ("OK");
    }
  else
    {
      putpkt ("ENN");
    }
  
  return result;
}

/*
 * handle_watchpoint
 *
 * This would handle watchpoint requests.  These are not yet wired into
 * the target vector, however, so I don't think that gdb can send them...
 */

int handle_watchpoint (enum bp_action_type action,
		       enum watch_type type,
		       char *input_buffer)
{
  char ch;
  char *p;
  CORE_ADDR watch_addr;
  int result;
  int size = 0;

  if ((p = (char *) strchr(input_buffer, ',')) != NULL)
    {
      *p = '\0';
      p++;
      sscanf (p, "%d", &size);
    }

  watch_addr = 0;
  while ((ch = *(input_buffer++)) != '\0')
    {
      watch_addr = watch_addr << 4;
      watch_addr |= fromhex (ch) & 0x0f;
    }

  switch (action)
    {
    case BREAKPOINT_SET:
      if (debug_on)
	{
	  output ("Setting watchpoint at %x of length %d\n", watch_addr, size);
	}
      result = low_set_watchpoint (watch_addr, size, type);
      break;
    case BREAKPOINT_DELETE:
      if (debug_on)
	{
	  output ("Removing watchpoint at %x of length %d\n", watch_addr, size);
	}
      result = low_delete_watchpoint (watch_addr, size, type);
      break;
    }

  if (result)
    {
      putpkt ("OK");
    }
  else
    {
      putpkt ("ENN");
    }
  
  return result;
}

/*
 * enable_extended_ops
 *
 * Store away the fact that we are using the extended remote protocol.
 */

void enable_extended_ops ()
{
  use_extended_ops = 1;
}

/*
 * exit_handler
 *
 * Flag the main loop to exit.
 */

void
exit_handler()
{
  exit_now = 1;
  close_connection_now = 1;
}

#define ARMword unsigned int
extern void record_register (int regno, ARMword val);
extern ARMword restore_register (int regno);

/* run_test
 *
 * This does nothing now, but you can shove in anything here, and
 * sending an 'qE' packet will activate it.  Just for testing.
 */

int
run_test (char *input_buffer)
{

  return low_test (input_buffer);

}
