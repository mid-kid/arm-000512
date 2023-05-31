/* RDI 1.5 translation code for Arm Multi-ice server for GDB.
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
#include "target.h" /* For "enum target_signal" */
#include "server.h"
#include "low.h"
#include "remote-utils.h"

/* These are all the WinRDI defines that we need. */

#include "windows.h"
#include "host.h"
#include "rdi.h"
#include "rdi150.h"
#include "rdi_hif.h"
#include "rdi_conf.h"
#include "rdi_info.h"
#include "winrdi.h"

/* This is set in low_resume, it indicates when we are running.  This is so
   that a call to low_close_target can know to stop the target before it tries
   to shut down.
*/

int currently_running = 0;

/*
 * We will keep a linked list of the RDI "points" that have been set.
 * We need these tokens to delete the points again.
 */

struct rdi_points {
  RDI_PointHandle handle;
  CORE_ADDR addr;
  struct rdi_points *next;
};

struct rdi_points *breakpoint_list; /* This one keeps the breakpoints. */
struct rdi_points *watchpoint_list; /* This one the watchpoints. */

/*
 * Really have to do better than this...  I don't want to put routines
 * that need platform types in server.h, so I need to abstract the "window"
 * type, and coerce on the other end.  For another day.
 */

extern HWND get_main_window (void);

/* Pointers to the functions we have dug out of the Multi-ICE DLL. */

WinRDI_funcGetVersion winRDI_GetVersion;
WinRDI_funcValid_RDI_DLL winRDI_Valid_RDI_DLL;
WinRDI_funcGet_DLL_Description winRDI_Get_DLL_Description;
WinRDI_funcGetRDIProcVec winRDI_GetRDIProcVec;
WinRDI_funcInitialise winRDI_Initialise;
WinRDI_funcConsoleInitialise winRDI_ConsoleInitialise;
WinRDI_funcConfig winRDI_Config;
WinRDI_funcRegister_Yield_Callback winRDI_Register_Yield_Callback;

/*
 * This is the RDI proc vector
 */

struct RDI_ProcVec * rdi_proc_vec;

/* This is the that we need to pass to WinRDI_Initialise.  There are a lot of
 * other calls that this also gets passed to, but according to the docs, it is
 * ignored in all of them.
 */

RDI_ConfigPointer gdb_config;

/*
 * This is the agent handle for the arm processor.  It is filled out
 * in the initialisation section.
 */

RDI_AgentHandle gdb_agent;

RDI_DbgState *gdb_debug_state;

/*
 * This is the interface for debug messages used by the Multi-ICE.
 */

struct RDI_HostosInterface gdb_IO_struct;


/*
 * proc_desc_array is the Array of processor modules for the ARM's
 * et al on the
 * board.  num_procs is the number of processers on board.
 */

unsigned num_procs;

RDI_ModuleDesc *proc_desc_array = NULL;

/*
 * TARGET_ARM_CORE is the index in the proc_desc_array for the arm
 * core we are debugging.  We are not currently prepared to debug more
 * than one Arm core, and have no interface to set which core
 * to debug, so for now this is just set to the first arm we find.
 * Set it to -1 on startup, and then low_open_target will set it to
 * the correct value.
 *
 * TARGET_ARM_MODULE is just a convenience for
 *
 * proc_desc_array[target_arm_core].handle
 *
 * which we use all the time.
 */

int target_arm_core = -1;
RDI_ModuleHandle target_arm_module = NULL;

/* Gives the target byte order returned from the RDI initialization
   for the "target_arm_core" processor. I don't understand the response
   that I get back from the RDI info call for ByteSex, so for now, 
   initialize this to the same value you put in the cfnArray's BYTESEX
   parameter below. */

int target_byte_order = BIG_ENDIAN;

/* Does the target_arm_core support single-stepping? */

int target_step = 0;

/* TARGET_LOAD_SIZE is the largest Memory write that the target supports
   Not all targets support reporting this, -1 indicates unknown. */
int target_load_size = -1;

/* If TARGET_STOP_OTHERS = 1, then a resume stops all the other
   processors. */
int target_stop_others = 0;

/*
 * This is the default Toolconf database that we will use.
 * Because the RDIinfo ByteSex call comes back with some
 * response I don't understand, be sure to initialize 
 * target_byte_order to the value in the BYTESEX field below.
 * Also, the array needs to end with a NULL element.
 */

/* This was the original version of the array that I cooked up based
   on Multi-ICE 1.0.  It doesn't work with versions >=  1.3, however.
*/

char *cfnArray[] = {
"MEMORYSIZE=0",
"SERIALPORT=1",
"SERIALLINESPEED=0",
"PARALLELPORT=1",
"PARALLELLINESPEED=1",
"LLSYMSNEEDPREFIX=TRUE",
"RDITYPE=0",
"HEARTBEAT=TRUE",
"MICETAPPOS1=0",
"RESET=FALSE",
"MICEDBGCONN=",
"MICEDRIVER1=",
"MICESERVER=localhost",
"MICETAPPOS0=0",
"BYTESEX=BIG",
"MICEDRIVER0=ARM7TDMI",
"FPE=TRUE",
/* These fields were added from on the advice of David Adshead from ARM.
   They work with versions > 1.3 of the Multi-ICE DLL.  Some of these
   fields need to get filled in for the DLL to start properly.  If they
   are not filled the get_agent call will report success, but will return
   a null agent token, which is not good.  I haven't played around with
   which ones are needed, however.
*/
"Multi-ICE_Tap1_Position=0",
"Multi-ICE_Connection_Name=",
"Multi-ICE_Driver1_Name=",
"Multi-ICE_Server_Location=localhost",
"Multi-ICE_Tap0_Position=0",
"Multi-ICE_Driver0_Name=ARM7TDMI",
"Multi-ICE_DLL_Settings=0",
NULL
};

/* Define the registers array here. */

char registers[REGISTER_BYTES];
char hold_registers[REGISTER_BYTES];   /* Used while running thread code on target */

int registers_up_to_date = 0;
int registers_are_dirty = 0;

int need_to_abort = 0;

/*
 * Defines for functions used only in this file.
 */

RDI_Hif_DbgPrint my_IO_dbgprint;
RDI_Hif_DbgPause my_IO_dbgpause;
void my_IO_dbgarg(RDI_Hif_DbgArg *arg);

RDI_Hif_WriteC my_IO_writec;
RDI_Hif_ReadC my_IO_readc;
RDI_Hif_Write my_IO_write;
RDI_Hif_GetS my_IO_gets;

RDI_Hif_ResetProc my_IO_reset;
void my_IO_resetarg(RDI_Hif_ResetArg *arg);
  
char *rdi_error_message (int err);
static int rdi_to_gdb_signal (int rdi_error);
void record_register (int regno, ARMword val);
ARMword restore_register (int regno);

/* These functions come from arm-singlestep.c */

extern CORE_ADDR server_arm_get_next_pc (CORE_ADDR pc,
					 unsigned short *is_thumb);
extern int arm_pc_is_thumb (bfd_vma memaddr);

/* Yield callback */
static int yield_arg;
static int yield_count;
#define YIELD_PERIOD 10

void
yield_func(WinRDI_YieldArg *arg)
{
    ARMword empty;
    if (++yield_count == YIELD_PERIOD) {
        yield_count = 0;
        check_for_SIGIO();
    }
    if (need_to_abort) {
        need_to_abort = 0;
        rdi_proc_vec->info (target_arm_module, RDISignal_Stop, &empty, &empty);
    }
}


/*
 * low_open_target
 *
 * This opens the connection to the target board.  TARGET_PORT
 * gives the port address for the board.  If QUERY is 1, the
 * user is queried for the setup of the connection parameters.
 */

int
low_open_target (char *target_port, char *byte_sex, int query)
{
  int vers;
  int valid, result = 0;
  unsigned i;
  unsigned write_mask = 0;
  ARMword null_value[2] = {0, 0};
  unsigned open_type;
  ARMword flags, dummy;
  HINSTANCE handle;
  HWND main_win;

  /*
   * First, we will open the Multi-ICE DLL and dig all the procs
   * that we need out of it.
   */
  
  handle = LoadLibrary ("Multi-ICE.dll");
  if (handle == NULL)
    {
      output_error ("Could not load the Multi-ICE DLL\n");
      return 0;
    }
  
  winRDI_GetVersion = WinRDI_GetProcAddress (handle, GetVersion);
  if (winRDI_GetVersion == NULL)
    {
      output_error ("Could not get GetVersion from the Multi-ICE DLL\n");
      return 0;
    }
  
  winRDI_Valid_RDI_DLL = WinRDI_GetProcAddress (handle, Valid_RDI_DLL);
  if (winRDI_Valid_RDI_DLL == NULL)
    {
      output_error ("Could not get Valid_RDI_DLL from the Multi-ICE DLL\n");
      return 0;
    }

  winRDI_Get_DLL_Description = WinRDI_GetProcAddress (handle,
						      Get_DLL_Description);
  if (winRDI_Get_DLL_Description == NULL)
    {
      output_error ("Could not get Get_DLL_Description from the Multi-ICE DLL\n");
      return 0;
    }
  
  winRDI_Config = WinRDI_GetProcAddress (handle, Config);
  if (winRDI_Config == NULL)
    {
      output_error ("Could not get Config from the Multi-ICE DLL\n");
      return 0;
    }
  
  winRDI_Initialise = WinRDI_GetProcAddress (handle, Initialise);
  if (winRDI_Initialise == NULL)
    {
      output_error ("Could not get Initialise from the Multi-ICE DLL\n");
      return 0;
    }

  winRDI_GetRDIProcVec = WinRDI_GetProcAddress (handle, GetRDIProcVec);
  if (winRDI_GetRDIProcVec == NULL)
    {
      output_error ("Could not get GetRDIProcVec from the Multi-ICE DLL\n");
      return 0;
    }

  winRDI_Register_Yield_Callback = WinRDI_GetProcAddress (handle, Register_Yield_Callback);
  if (winRDI_Register_Yield_Callback == NULL)
    {
      output_error ("Could not get Register_Yield_Callback from the Multi-ICE DLL\n");
      return 0;
    }

  /* Register a "yield" callback so we can regain control while 'executing' */
  winRDI_Register_Yield_Callback(yield_func, (WinRDI_YieldArg *)&yield_arg);
  
  /*
   * Okay, now we are ready to start the actual initialization process.
   * First we check that the DLL is copasetic, then start to open the
   * connection to the target.
   */
  
  valid = winRDI_Valid_RDI_DLL ();
  
  if (!valid)
    {
      output_error ("RDI DLL says it is not valid\n");
      return 0;
    }
  
  vers = winRDI_GetVersion ();
  if (vers < 150 || vers >= 200)
    {
      output_error ("RDI version mismatch, expected 150 <> 200, got %d\n",
		    vers);
      return 0;
    }
  
  rdi_proc_vec = winRDI_GetRDIProcVec ();
  if (rdi_proc_vec == NULL)
    {
      output_error ("Got null proc vector from GetRDIProcVec\n");
      return 0;
    }
  
  /*
   * Now fill the ToolConf database.  Some of these have to be
   * filled BEFORE you call winRDI_Initialise, or the DLL will
   * crash in random places...  This is supposed to be fixed in
   * the 1.4 version of the DLL.
   */
  
  gdb_config = ToolConf_New (30);
  for (i = 0; cfnArray[i] != NULL; i++)
    {
      ToolConf_Add (gdb_config, cfnArray[i]);
    }
  
  if (target_port != NULL)
    {
      char buffer[256];
      /* This is the Multi-ICE DLL < 1.3 version. */
      sprintf (buffer, "MICESERVER=%s", target_port);
      ToolConf_Update (gdb_config, buffer);
      /* This is the Multi-ICE DLL >= 1.3 version. */
      sprintf (buffer, "Multi-ICE_Server_Location=%s", target_port);
      ToolConf_Update (gdb_config, buffer);
  }

  if (byte_sex != NULL)
    {
      if (*byte_sex == 'b' || *byte_sex == 'B')
	{
	  ToolConf_Update (gdb_config, "BYTESEX=BIG");
          target_byte_order = BIG_ENDIAN;
	}
      else if (*byte_sex == 'l' || *byte_sex == 'L')
	{
	  ToolConf_Update (gdb_config, "BYTESEX=LITTLE");
          target_byte_order = LITTLE_ENDIAN;
	}
    }

  main_win = get_main_window ();
  
  if (main_win == NULL)
    {
      output_error ("The main window handle was null.\n");
      return 0;
    }

  if (query)
    {
      result = winRDI_Config (gdb_config, main_win);
      if (!result)
	{
	  output_error ("Configure failed...\n");
	  return 0;
	}
    }
  
  result = winRDI_Initialise (main_win, gdb_config);
  if (!result)
    {
      output_error ("Initialise failed...\n");
      return 0;
    }
  
  /*
   * Set the open type to cold boot, reset comm, byte sex dont care
   */
  
  open_type = (0 << RDIOpen_BootLevel);
  open_type |= (1 << RDIOpen_CommsReset);
  open_type |= (RDISex_DontCare << RDIOpen_ByteSexShift);
  
  /*
   * Fill the IO structure with the appropriate functions.
   */
  
  gdb_IO_struct.dbgprint = my_IO_dbgprint;
  gdb_IO_struct.dbgpause = my_IO_dbgpause;
  gdb_IO_struct.writec = my_IO_writec;
  gdb_IO_struct.readc = my_IO_readc;
  gdb_IO_struct.write = my_IO_write;
  gdb_IO_struct.gets = my_IO_gets;
  
  result = rdi_proc_vec->openagent (&gdb_agent, open_type, gdb_config, 
				   &gdb_IO_struct, gdb_debug_state);
  
  if (result != RDIError_NoError) {
    output_error ("RDI_OpenAgent failed with error %d.\n", result);
    if (gdb_agent != NULL)
      {
	rdi_proc_vec->closeagent (gdb_agent);
      }
    return 0;
  }

  /*
   * Next, query the agent to find out how many processors are on
   * board, get the info for these, and open them all.  Note that
   * you have to open all of them even though you only intend to
   * talk to one of them.
   */
  
  num_procs = 0;
  result = rdi_proc_vec->info (gdb_agent, RDIInfo_Modules, 
			      (ARMword *) &num_procs,
			      (ARMword *) NULL);
  if (num_procs == 0) 
    {
      output_error ("Found no processors for agent %d\n", gdb_agent);
      return 0;
    }

  proc_desc_array = (RDI_ModuleDesc *) 
    malloc(sizeof(RDI_ModuleDesc) * num_procs);
  result = rdi_proc_vec->info(gdb_agent, RDIInfo_Modules, 
			      (ARMword *) &num_procs,
			      (ARMword *) proc_desc_array);

  for (i = 0; i < num_procs; i++)
    {
      /*
       * Some modules may have their own RDI vectors, use it if
       * it is present.
       */
      
      if (proc_desc_array[i].rdi == NULL)
	{
	  result = rdi_proc_vec->open (proc_desc_array[i].handle, 
				       open_type, gdb_config,
				       &gdb_IO_struct,
				       gdb_debug_state);
	}
      else
	{
	  result = proc_desc_array[i].rdi->open (proc_desc_array[i].handle, 
						 open_type, gdb_config,
						 &gdb_IO_struct,
						 gdb_debug_state);
	}
      
      if (result != RDIError_NoError)
	{
	  int j;
          char msg[256];
	  
	  output_error ("Error #%d opening module module %d\n", result, i);
          rdi_proc_vec->errmess(gdb_agent, msg, sizeof(msg), result);
          output_error("Error: %s\n", msg);
	  for (j = 0; j < i; j++)
	    {
	      if (proc_desc_array[j].rdi == NULL)
		{
		  result = rdi_proc_vec->close (proc_desc_array[j].handle);
		}
	      else
		{
		  result =
		    proc_desc_array[j].rdi->close (proc_desc_array[j].handle);
		}
	    }
	    rdi_proc_vec->closeagent (gdb_agent);
	    return 0;
	}

      /*
       * For now there is no interface to tell us which ARM core
       * you want to debug.  We default to the first one found.
       * At this point we also get its byte order.
       */
      
      if (target_arm_core < 0)
	{
	  if (strcmp (proc_desc_array[i].type, "ARM") == 0)
	    {
	      unsigned32 sex;
	      target_arm_core = i;
	      target_arm_module = proc_desc_array[i].handle;
	      rdi_proc_vec->info(target_arm_module,
				 RDIInfo_ByteSex,
				 &sex,
				 NULL);
              /* The "sex" value is coming back with some
                 bogus value, so if I don't recognize it, 
                 just leave it alone, since the code above
                 sets it to whatever I have requested... */

#if 0
	      if (sex == RDISex_Big) 
		{
		  target_byte_order = BIG_ENDIAN;
		}
	      else if (sex == RDISex_Little)
		{
		  target_byte_order = LITTLE_ENDIAN;
		}	    
#endif
	    }
	}
      
  }

  if (target_arm_core < 0)
    {
      low_close_target();
      output_error ("Didn't find a core of type \"ARM\" on target.\n");
      return 0;
    }

  /*
   * Set the frame pointer to zero.  Sometimes if you power-cycle
   * the board this can be pointing off into never-never land, and
   * that can confuse GDB no end.
   */

  write_mask = ( 1 << 11 );
  write_mask |= (1 << 15);
  rdi_proc_vec->CPUwrite (target_arm_module, RDIMode_Curr,
			  write_mask, null_value);

  /*
   * Finally, write out some information on the processor we
   * are connected to:
   */

  output ("\nConnected to the Multi-Ice target.\n\n");
  output ("Targeted ARM core: %s\n", proc_desc_array[target_arm_core].name);
  output ("\nTarget characteristics:\n");

  rdi_proc_vec->info (target_arm_module, RDIInfo_Target, &flags, &dummy);
  output ("\tHardware?:\t\t");
  if (flags & RDITarget_HW)
    {
      output ("YES\n");
    }
  else
    {
      output ("NO\n");
    }
  
  output ("\tComms Channel?:\t\t");
  if (flags & RDITarget_HasCommsChannel)
    {
      output ("YES\n");
    }
  else
    {
      output ("NO\n");
    }
  
  output ("\tEndian-ness:\t\t");
  if (target_byte_order ==BIG_ENDIAN)
    {
      output ("BIG\n");
    }
  else
    {
      output ("LITTLE\n");
    }

  rdi_proc_vec->info (target_arm_module, RDIInfo_Step, &flags, NULL);

  output ("\tSingle-Step Support:\t");
  if (flags & RDIStep_Single)
    {
      target_step = 1;
      output ("YES\n");
    }
  else
    {
      target_step = 0;
      output ("NO\n");
    }
  
  result = rdi_proc_vec->info (target_arm_module,
			      RDIInfo_GetLoadSize,
			      (ARMword *) &target_load_size,
			      NULL);
  output ("\tLoad Size:\t\t");
  if (result == RDIError_NoError)
    {
      output ("%d\n", target_load_size);
    }
  else
    {
      output ("unknown\n");
      target_load_size = -1;
    }

  result = rdi_proc_vec->info (target_arm_module,
			      RDIInfo_CustomLoad,
			      (ARMword *) &dummy,
			      NULL);
  output ("\tCustom Load?:\t\t");
  if (result == RDIError_NoError)
    {
      output ("YES\n");
    }
  else
    {
      output ("NO\n");
    }

  
  output ("\n");

  if (num_procs > 1)
    {
      output ("Other modules found on debug target:\n");
      for (i = 0; i < num_procs; i++)
	{
	  if (i == target_arm_core)
	    {
	      output ("\t%d\tCurrent Target\n", i);
	    }
	  else
	    {
	      output ("\t%d\t%s\n", i, proc_desc_array[i].name);
	    }
	}
    }
  else
    {
      output ("No other modules found on debug target.\n");
    }

  output ("\n");
  
  return 1;
}

/*
 * low_close_target - Closes all the open modules in the target, and
 * closes the agent connection.
 */

int
low_close_target()
{

  unsigned i;
  int result;
  ARMword dummy1, dummy2;

  /* If we haven't allocated the processor description array yet,
     then we have probably not gotten anywhere yet, so just exit.
  */
  
  if (proc_desc_array == NULL)
    return 1;

  if (currently_running)
    {
      result = rdi_proc_vec->info (gdb_agent, RDISignal_Stop, &dummy1,
				   &dummy2);
     if (result != RDIError_NoError)
       {
	 output_error ("Could not stop the target.\n");
	 
	 /* If we could not stop the target, it does not seem safe
	    to shut down (we often crash when we do...) */
	 return 0;
       }
    }
  
  for (i = 0; i < num_procs; i++) {
    rdi_proc_vec->close(proc_desc_array[i].handle);
  }
  rdi_proc_vec->closeagent(gdb_agent);
  free (proc_desc_array);
  return 1;  
}


/*
 * None of the Thread code works at present, since the RDI Thread
 * queries are not supported.  This will have to be supplemented
 * by calls into the OS on the board.
 */

/* low_set_thread_for_resume:

   This sets the target for step and continue operations.  For now,
   the RDI doesn't support these operations, so this is a no-op.
   If it did work, THREAD_ID would be the thread to target, and -1
   would indicate all threads.

   Returns 1 for success, 0 for error.
   
*/

int
low_set_thread_for_resume (long thread_id)
{
  return 1;
}

/* low_set_thread_for_query:

   This sets the target for all query operations.

   Returns 1 for success, 0 for error.
   
*/

int
low_set_thread_for_query (char *input_buffer)
{
    /* Need to notify target as well */
    return low_thread_op(input_buffer);
}

/* low_is_thread_alive:

   Returns 1 if thread THREAD_ID is alive, 0 otherwise.
   
*/

int
low_is_thread_alive (char *input_buffer)
{
    return low_thread_op(input_buffer);
}

/* low_get_current_thread:

   Sets THREAD_ID to the current thread id.  Returns 1 if it can do
   this, and 0 if for some reason it cannot (like the fact that this
   is not supported in RDI1.5.0.

*/

int
low_get_current_thread (long *thread_id)
{
  return 0;
}

/* low_get_offsets:

   Fills TEXT, DATA and BSS with the program offsets.  Returns 1 for
   success, and 0 if the packet is not supported.  For now, I just
   return 1 since it is not that important to answer this one.

*/

int
low_get_offsets (CORE_ADDR *text, CORE_ADDR *data, CORE_ADDR *bss)
{
  /*
   * The RDI doesn't really know this information, I think,
   * since it has nothing really to do with loading the executible.
   * So I just pretend I don't understand the query.
   */
  
  return 0;
}

/* low_query_last_signal

   Fills SIGNAL with the reason for the last stop.

   Returns 1 on success, and 0 if it cannot return the information.

*/

enum target_signal
low_query_last_signal ()
{
  /* return TARGET_SIGNAL_TRAP; */
  return 0;
}

/*
 * low_update_registers
 *
 * Fetches the register values from the board, and updates the registers
 * array with the current values for all the registers.
 *
 */

int low_update_registers()
{
  int result, rdi_regmask, regno;
  ARMword rawreg, rawregs[32];

  if (registers_up_to_date)
    {
      return 1;
    }
  
  result = rdi_proc_vec->CPUread (target_arm_module,
				  RDIMode_Curr, 0x7fff|RDIReg_CPSR, rawregs);
  if (result)
    {
      output_error ("RDI_CPUread: %s\n", rdi_error_message (result));
      return 0;
    }
  
  for (regno = 0; regno < 15; regno++)
    {
     
      record_register (regno, rawregs[regno]);
    }

/* Note: this is very strange.  It seems that only R15 will return the PS,
 * but RDIReg_CPSR must be used to set it!  Looks like an ARM server bug.
 */  
  record_register (PS_REGNUM, rawregs[15]);

  /*
   * Now get the PC register...
   */
  
  rdi_regmask = RDIReg_PC;

  result = rdi_proc_vec->CPUread (target_arm_module,
				  RDIMode_Curr, rdi_regmask, &rawreg);
  if (result)
    {
      output_error ("RDI_CPUread: %s\n", rdi_error_message (result));
      return 0;
    }
  
  record_register (PC_REGNUM, rawreg);

  registers_up_to_date = 1;
  registers_are_dirty = 0;
  
  return 1;
}
/*
 * low_write_registers
 *
 * Writes the registers in the register array down to the board
 */

int
low_write_registers ()
{
  ARMword rawregs[32], rawreg;
  unsigned int write_mask;
  int regno;

  if (!registers_are_dirty)
    {
      return 1;
    }

  write_mask = 0;
  for (regno = 0; regno < 15; regno++)
    {
      rawregs[regno] = restore_register (regno);
      write_mask |= (1 << regno);
    }
  rawregs[15] = restore_register (PS_REGNUM);
  write_mask |= (1 << 15);

  rdi_proc_vec->CPUwrite (target_arm_module, RDIMode_Curr,
			  write_mask, rawregs);

  write_mask = RDIReg_PC;
  rawreg = restore_register (PC_REGNUM);
  rdi_proc_vec->CPUwrite (target_arm_module, RDIMode_Curr,
			  write_mask, &rawreg);

  rawreg = restore_register (PS_REGNUM);
  write_mask = RDIReg_CPSR;
  rdi_proc_vec->CPUwrite (target_arm_module, RDIMode_Curr,
			  write_mask, &rawreg);

  registers_are_dirty = 0;
  
  return 1;

}

int
low_read_memory_raw (CORE_ADDR start, void *buffer, unsigned int *nbytes)
{
  int result;
  
  result = rdi_proc_vec->read (target_arm_module,
		  (ARMword) start, buffer,
		  nbytes, RDIAccess_Data);
  switch (result)
    {
    case RDIError_BigEndian:    
      output ("Processor has switched endianness - now BIG ENDIAN\n");
      break;
    case RDIError_LittleEndian:
      output ("Processor has switched endianness - now LITTLE ENDIAN\n");
      break;
    case RDIError_NoError:
      break;
    default:
      output_error ("RDI_Read error: %s\n", rdi_error_message (result));
      return 0;
    }
  return 1;
}

int
low_read_memory (char *buffer, CORE_ADDR start, unsigned int nbytes)
{
#define BUFF_LEN 1024
  char rdi_out[BUFF_LEN];
  
  if (nbytes > BUFF_LEN)
    {
      output_error ("Requested too much data: %d\n", nbytes);
      return 0;
    }
  
  if (!low_read_memory_raw (start, rdi_out, &nbytes))
    {
      return 0;
    }
  
  convert_bytes_to_ascii (rdi_out, buffer, nbytes, 0);
  return nbytes;      
		  
}

int
low_write_memory (char *data, CORE_ADDR start_addr, unsigned int nbytes)
{
  int result, i;
  char buff[1024];

  result = rdi_proc_vec->write (target_arm_module, data, start_addr,
		       &nbytes, RDIAccess_Data);

  /* This is stupid, but let's see whether we got back what we
   * put in...
   */
    
  rdi_proc_vec->read (target_arm_module, start_addr, buff, &nbytes,
			RDIAccess_Data);

  for (i = 0; i < nbytes; i++)
    {
      if (data[i] != buff[i])
	{
	  output_error ("Readback did not match original data\n");
	}
    }
  
  switch (result)
    {
    case RDIError_BigEndian:    
      output ("Processor has switched endianness - now BIG ENDIAN\n");
      break;
    case RDIError_LittleEndian:
      output ("Processor has switched endianness - now LITTLE ENDIAN\n");
      break;
    case RDIError_NoError:
      break;
    default:
      output_error ("RDI_Read error: %s\n", rdi_error_message (result));
      return 0;
    }
  return nbytes;
  
}

int
low_set_breakpoint (CORE_ADDR bp_addr, int size)
{
  struct rdi_points *new_point;
  int result, type;

  new_point = (struct rdi_points *) malloc (sizeof(struct rdi_points));

  new_point->addr = bp_addr;
  new_point->handle = RDI_NoPointHandle;

  type = RDIPoint_EQ;
  if (size == 2)
    {
      type |= RDIPoint_16Bit;
    }
  
  if (debug_on)
    {      
      output ("Adding breakpoint at 0x%x, size: %d\n", bp_addr, size);
    }

  result = rdi_proc_vec->setbreak (target_arm_module, bp_addr,
				   type,
				   0, RDI_NoHandle, &new_point->handle);
  switch (result)
    {
    case RDIError_NoMorePoints:
      output ("This breakpoint exhausted the available points.\n");
    case RDIError_NoError:
      break; /* Success branches break here */
    case RDIError_ConflictingPoint:
      output_error ("RDI Error: Conflicting breakpoint.\n");
    case RDIError_CantSetPoint:
      output_error ("RDI Error: No more breakpoint resources left.\n");
    case RDIError_PointInUse:
      output_error ("RDI Error: Breakpoint in use.\n");
    default:
      output_error ("RDI Error: %s.\n", rdi_error_message (result));
      free (new_point);
      return 0; /* All errors fall through to here. */
    }
  
  if (breakpoint_list == NULL)
    {
      breakpoint_list = new_point;
      new_point->next = NULL;
    }
  else
    {
      new_point->next = breakpoint_list;
      breakpoint_list = new_point;
    }

  if (debug_on)
    {
      ARMword arg1, arg2;
      int result;
      struct rdi_points *iter;
      
      for (iter = breakpoint_list; iter != NULL; iter = iter->next)
	{
	  arg1 = (ARMword) iter->handle;
	  result = rdi_proc_vec->info (target_arm_module,
			      RDIPointStatus_Break,
			      &arg1,
			      &arg2);
	  output ("\t-- addr: 0x%x handle 0x%x hw resource: %d type 0x%x result %d\n",
		  iter->addr, iter->handle, arg1, arg2, result);
	}
    }

  return 1;
}

int
low_delete_breakpoint (CORE_ADDR bp_addr, int size)
{
  struct rdi_points *point_ptr, *prev_ptr;
  int result;

  if (debug_on)
    {
      ARMword arg1, arg2;
      int result;
      struct rdi_points *iter;
      
      output ("Before deletion of 0x%x, Current breakpoint list:\n", bp_addr);
      for (iter = breakpoint_list; iter != NULL; iter = iter->next)
	{
	  arg1 = (ARMword) iter->handle;
	  result = rdi_proc_vec->info (target_arm_module,
			      RDIPointStatus_Break,
			      &arg1,
			      &arg2);
	  output ("\t-- addr: 0x%x handle 0x%x hw resource: %d type 0x%x result %d\n",
		  iter->addr, iter->handle, arg1, arg2, result);
	}
    }

  for (point_ptr = breakpoint_list, prev_ptr = NULL;
        point_ptr != NULL; prev_ptr = point_ptr, point_ptr = point_ptr->next)
    {
      if (point_ptr->addr == bp_addr)
	{
	  break;
	}
    }

  if (point_ptr == NULL)
    {
      output_error ("Breakpoint at addr %x not found.\n", bp_addr);
      return 0;
    }

  result = rdi_proc_vec->clearbreak (target_arm_module, point_ptr->handle);

  if (result != RDIError_NoError)
    {
      output_error ("RDI Error deleting breakpoint: %s.\n",
		    rdi_error_message (result));
      return 0;
    }

  if (prev_ptr == NULL)
    {
      breakpoint_list = point_ptr->next;
    }
  else
    {
      prev_ptr->next = point_ptr->next;
    }
  
  free (point_ptr);
  
  if (debug_on)
    {
      struct rdi_points *iter;
      output ("After Deleting 0x%x, Current breakpoint list:\n", bp_addr);
      for (iter = breakpoint_list; iter != NULL; iter = iter->next)
	output ("\t-- 0x%x\n", iter->addr);
    }
  
  return 1;
}

int
low_set_watchpoint (CORE_ADDR watch_addr, int size, enum watch_type mode)
{
  struct rdi_points *new_point;
  int result, type, watch_mode;

  new_point = (struct rdi_points *) malloc (sizeof(struct rdi_points));

  new_point->addr = watch_addr;
  new_point->handle = RDI_NoPointHandle;

  type = RDIPoint_EQ;

  watch_mode = 0;
  if ((mode == WATCHPOINT_WRITE) || (mode == WATCHPOINT_ACCESS)) {
      watch_mode |= size*RDIWatch_ByteWrite;
  }
  if ((mode == WATCHPOINT_READ) || (mode == WATCHPOINT_ACCESS)) {
      watch_mode |= size*RDIWatch_ByteRead;
  }
  
  if (debug_on)
    {      
      output ("Adding watchpoint at 0x%x, size: %d, mode: %x\n", watch_addr, size, watch_mode);
    }

  result = rdi_proc_vec->setwatch (target_arm_module, watch_addr,
				   type, watch_mode, 
				   0, RDI_NoHandle, &new_point->handle);
  switch (result)
    {
    case RDIError_NoMorePoints:
      output ("This watchpoint exhausted the available points.\n");
    case RDIError_NoError:
      break; /* Success branches break here */
    case RDIError_ConflictingPoint:
      output_error ("RDI Error: Conflicting watchpoint.\n");
    case RDIError_CantSetPoint:
      output_error ("RDI Error: No more watchpoint resources left.\n");
    case RDIError_PointInUse:
      output_error ("RDI Error: Watchpoint in use.\n");
    default:
      output_error ("RDI Error: %s.\n", rdi_error_message (result));
      free (new_point);
      return 0; /* All errors fall through to here. */
    }
  
  if (watchpoint_list == NULL)
    {
      watchpoint_list = new_point;
      new_point->next = NULL;
    }
  else
    {
      new_point->next = watchpoint_list;
      watchpoint_list = new_point;
    }

  if (debug_on)
    {
      ARMword arg1, arg2;
      int result;
      struct rdi_points *iter;
      
      for (iter = watchpoint_list; iter != NULL; iter = iter->next)
	{
	  arg1 = (ARMword) iter->handle;
	  result = rdi_proc_vec->info (target_arm_module,
			      RDIPointStatus_Watch,
			      &arg1,
			      &arg2);
	  output ("\t-- addr: 0x%x handle 0x%x hw resource: %d type 0x%x result %d\n",
		  iter->addr, iter->handle, arg1, arg2, result);
	}
    }

  return 1;
}

int
low_delete_watchpoint (CORE_ADDR watch_addr, int size, enum watch_type type)
{
  struct rdi_points *point_ptr, *prev_ptr;
  int result;

  if (debug_on)
    {
      ARMword arg1, arg2;
      int result;
      struct rdi_points *iter;
      
      output ("Before deletion of 0x%x, Current watchpoint list:\n", watch_addr);
      for (iter = watchpoint_list; iter != NULL; iter = iter->next)
	{
	  arg1 = (ARMword) iter->handle;
	  result = rdi_proc_vec->info (target_arm_module,
			      RDIPointStatus_Watch,
			      &arg1,
			      &arg2);
	  output ("\t-- addr: 0x%x handle 0x%x hw resource: %d type 0x%x result %d\n",
		  iter->addr, iter->handle, arg1, arg2, result);
	}
    }

  for (point_ptr = watchpoint_list, prev_ptr = NULL;
        point_ptr != NULL; prev_ptr = point_ptr, point_ptr = point_ptr->next)
    {
      if (point_ptr->addr == watch_addr)
	{
	  break;
	}
    }

  if (point_ptr == NULL)
    {
      output_error ("Watchpoint at addr %x not found.\n", watch_addr);
      return 0;
    }

  result = rdi_proc_vec->clearwatch (target_arm_module, point_ptr->handle);

  if (result != RDIError_NoError)
    {
      output_error ("RDI Error deleting watchpoint: %s.\n",
		    rdi_error_message (result));
      return 0;
    }

  if (prev_ptr == NULL)
    {
      watchpoint_list = point_ptr->next;
    }
  else
    {
      prev_ptr->next = point_ptr->next;
    }
  
  free (point_ptr);
  
  if (debug_on)
    {
      struct rdi_points *iter;
      output ("After Deleting 0x%x, Current watchpoint list:\n", watch_addr);
      for (iter = watchpoint_list; iter != NULL; iter = iter->next)
	output ("\t-- 0x%x\n", iter->addr);
    }
  
  return 1;
}

int
low_resume (enum resume_mode mode, int signo)
{
  RDI_PointHandle ret_point;
  RDI_ModuleHandle target_module = target_arm_module;
  int result;

  /* Make sure any changes to the registers are flushed before
     proceeding */
  
  low_write_registers();

  
  switch (mode)
    {
    case RESUME_STEP:
      if (target_step)
	{
	  RDI_ModuleHandle mh_ptr;
	  RDI_PointHandle out_handle;
	  mh_ptr = target_arm_module;
	  currently_running = 1;
	  result = rdi_proc_vec->step (gdb_agent, &mh_ptr, 1, 1, &out_handle);
	  currently_running = 0;
	}
      else
	{
	  CORE_ADDR next_pc, current_pc;
	  int dont_delete, type;
	  unsigned short is_thumb;
	  RDI_PointHandle temp_point;

	  /* We just wrote out the registers above, so they should be good. */
	  current_pc = restore_register(PC_REGNUM);

	  /* Now get the next pc.  We need to know whether the next
	     PC is in arm or thumb code, so that we can set the right
	     kind of breakpoint there. */
	  
	  next_pc = server_arm_get_next_pc (current_pc, &is_thumb);

	  if (debug_on)
	    {
	      output ("Single stepping from 0x%x to 0x%x.\n", current_pc,
		      next_pc);
	    }
	  
	  temp_point = RDI_NoPointHandle;
	  type = RDIPoint_EQ;
	  
	  if (is_thumb)
	    {
	      type |= RDIPoint_16Bit;
	    }
	  result = rdi_proc_vec->setbreak (target_arm_module, next_pc,
					   type, 0, RDI_NoHandle,
					   &temp_point);
	  /*
	   * If there is already a breakpoint here, then don't remove
	   * it.  The RDI only allows one Breakpoint at an address.
	   */
	  
	  dont_delete = 0;
	  if (result == RDIError_PointInUse)
	    {
	      dont_delete = 1;
	      output ("Temp breakpoint fell at used breakpoint at 0x%x.\n",
		      next_pc);
	    }
	  else if (result != RDIError_NoError)
	    {
	      output_error ("BP Error in single stepping: %s.\n",
			    rdi_error_message(result));
	      break;
	    }

	  /* Always stop the other processors when we are single
	     stepping, since we would rather now hit one of their
	     breakpoints.
	  */
	  
	  currently_running = 1;
	  result = rdi_proc_vec->execute (gdb_agent,
					  &target_module,
					  1,
					  &ret_point);
	  currently_running = 0
	    ;
	  if (!dont_delete)
	    {
	      rdi_proc_vec->clearbreak (target_arm_module, temp_point);
	    }

	  if (debug_on)
	    {
	      if (result == RDIError_BreakpointReached)
		{
		  if (ret_point != temp_point)
		    {
		      output ("Stopped at a different BP from the temp");
		      output (" one I set for stepping.\n");
		    }
		  else
		    {
		      output ("Stopped at the step breakpoint.\n");
		    }
		}
	      else
		{
		  output ("Stopped unexpectedly with return value: %s.\n",
			  rdi_error_message (result));
		}
	    }
	      
	  break;
	}
    case RESUME_CONTINUE:
      currently_running = 1;
      result = rdi_proc_vec->execute (gdb_agent, 
			     &target_module,
			     target_stop_others,
			     &ret_point);
      currently_running = 0;
      
      if (debug_on)
	{
	  if (result == RDIError_BreakpointReached)
	    {
	      struct rdi_points *iter_ptr;
	      
	      for (iter_ptr = breakpoint_list; iter_ptr != NULL;
		   iter_ptr = iter_ptr->next)
		{
		  if (iter_ptr->handle == ret_point)
		    {
		      output ("Stopped at breakpoint at 0x%x, handle 0x%x.\n",
			      iter_ptr->addr, iter_ptr->handle);
		      break;
		    }
		}
	      if (iter_ptr == NULL)
		{
		  output ("Stopped at unknown breakpoint.\n");
		}
	    }
	  else
	    {
	      output ("Stopped unexpectedly with return %s: %d.\n",
		      rdi_error_message (result));
	    }
	}
    }

  /* FIXME - we should probably continue again till WE cause the
     stop... But I am not sure whether this would steal control
     from the debug controller watching the other debug target.
     At least figure out a signal that will keep gdb from getting
     all confused about this.
  */
  
  if (target_module != target_arm_module)
    {
      output_error ("Oops, somebody else caused the stop while single stepping.\n");
    }

  currently_running = 0;
  return rdi_to_gdb_signal (result);

}

/*
 * rdi_to_gdb_signal
 *
 * Translate an rdi ending signal to a signal you can send back to gdb.
 * Comes from remote-rdi.c.
 */

static int
rdi_to_gdb_signal (int rdi_error)
{
  switch (rdi_error)
    {
    case RDIError_NoError:
    case RDIError_TargetStopped:
      return TARGET_SIGNAL_TERM;
    case RDIError_Reset:
      return TARGET_SIGNAL_TERM; /* ??? */
    case RDIError_UndefinedInstruction:
      return TARGET_SIGNAL_ILL;
    case RDIError_SoftwareInterrupt:
    case RDIError_PrefetchAbort:
    case RDIError_DataAbort:
      return TARGET_SIGNAL_TRAP;
    case RDIError_AddressException:
      return TARGET_SIGNAL_SEGV;
    case RDIError_IRQ:
    case RDIError_FIQ:
      return TARGET_SIGNAL_TRAP;
    case RDIError_Error:
      return TARGET_SIGNAL_TERM;
    case RDIError_BranchThrough0:
      return TARGET_SIGNAL_TRAP;
    case RDIError_NotInitialised:
    case RDIError_UnableToInitialise:
    case RDIError_WrongByteSex:
    case RDIError_UnableToTerminate:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_BadInstruction:
    case RDIError_IllegalInstruction:
      return TARGET_SIGNAL_ILL;
    case RDIError_BadCPUStateSetting:
    case RDIError_UnknownCoPro:
    case RDIError_UnknownCoProState:
    case RDIError_BadCoProState:
    case RDIError_BadPointType:
    case RDIError_UnimplementedType:
    case RDIError_BadPointSize:
    case RDIError_UnimplementedSize:
    case RDIError_NoMorePoints:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_BreakpointReached:
    case RDIError_WatchpointAccessed:
      return TARGET_SIGNAL_TRAP;
    case RDIError_NoSuchPoint:
    case RDIError_ProgramFinishedInStep:
      return TARGET_SIGNAL_UNKNOWN;
    case RDIError_UserInterrupt:
      return TARGET_SIGNAL_INT;
    case RDIError_IncompatibleRDILevels:
    case RDIError_LittleEndian:
    case RDIError_BigEndian:
    case RDIError_SoftInitialiseError:
    case RDIError_InsufficientPrivilege:
    case RDIError_UnimplementedMessage:
    case RDIError_UndefinedMessage:
    default:
      return TARGET_SIGNAL_UNKNOWN; 
    }  
}

/*
 * This next set of routines are the IO channel for the Target.  I haven't
 * done anything with this yet.  Currently they are just stubs.
 */

void 
my_IO_dbgprint (RDI_Hif_DbgArg *arg, const char *format, va_list ap)
{
	
}

void 
my_IO_dbgpause(RDI_Hif_DbgArg *arg) 
{
	
}

void 
my_IO_writec(RDI_Hif_HostosArg *arg, int c)
{  

}

int 
my_IO_readc(RDI_Hif_HostosArg *arg)
{
    return 0;
}

int 
my_IO_write(RDI_Hif_HostosArg *arg, char const *buffer, int len)
{
    return 0;
}

char *
my_IO_gets(RDI_Hif_HostosArg *arg, char *buffer, int len)
{
	return buffer;
}



/*
 * rdi_error_message
 *
 * Translate an rdi error number into a string, and return it.
 */

char *
rdi_error_message (int err)
{
    static char msg[256];
    rdi_proc_vec->errmess(gdb_agent, msg, sizeof(msg), err);
    return msg;
}

/*
 * record_register
 *
 * This copies the data that comes from the RDI CPUread function
 * over into the registers array.
 * Make sure that it stays in target byte order.
 */

void
record_register (int regno, ARMword val)
{
  unsigned char cookedreg[12];

  store_unsigned_integer (cookedreg, REGISTER_RAW_SIZE (regno), val);
 
  memcpy (&registers[REGISTER_BYTE (regno)], (char *) cookedreg,
	  REGISTER_RAW_SIZE (regno));
}

ARMword
restore_register (int regno)
{
  
  PTR addr;
  int len;
  ULONGEST retval;

  addr = &registers[REGISTER_BYTE (regno)];
  len = REGISTER_RAW_SIZE (regno);

  retval = 0;
  retval = extract_unsigned_integer (addr, len);

  return retval;
}

int
low_test (char *buffer)
{

}

int
low_stop(void)
{
    need_to_abort++;
}

#define ICE_THREAD_VECTOR 0x50
#define ICE_THREAD_VECTOR_KEY0  0x4D494345  /* 'MICE' */
#define ICE_THREAD_VECTOR_KEY1  0x47444220  /* 'GDB ' */

#define ICE_THREAD_HANDLER_KEY0 0xDEAD0001
#define ICE_THREAD_HANDLER_KEY1 0xDEAD0002

struct ice_thread_vector {
    unsigned long _key0;
    unsigned long handler;
    unsigned long eCosRunning;
    unsigned long _key1;
};

struct ice_thread_handler {
    unsigned long _key0;
    unsigned char *inbuf;
    long          inbuf_size;
    unsigned char *outbuf;
    long          outbuf_size;
    unsigned char *stack;
    long          stack_size;
    unsigned long fun;
    unsigned long _key1;
};

/* This routine is the fallback for thread oriented functions when there is
 * no thread support available at the target.
 */
int
no_thread_op(char *input_buffer)
{
    switch (input_buffer[0]) {
    case 'g':
        return handle_read_registers(input_buffer+1);
    default:
        putpkt("ENN");
        return 0;  /* Can't handle it! */
    }
}

/* This routine tries to call into the target for thread oriented functions. */
int
low_thread_op(char *input_buffer)
{
    struct ice_thread_handler handler;
    struct ice_thread_vector vector;
    int nbytes, res;
    static char result[2048], *cp;

/*    output("low_thread_query\n"); */
    nbytes = sizeof(vector);
    if (!low_read_memory_raw(ICE_THREAD_VECTOR, (char *)&vector, &nbytes)) {
        return no_thread_op(input_buffer);
    }
/*    output("key: %x.%x, handler: %x\n", vector._key0, vector._key1, vector.handler); */
    if ((vector._key0 != ICE_THREAD_VECTOR_KEY0) || 
        (vector._key1 != ICE_THREAD_VECTOR_KEY1) ||
        (vector.eCosRunning == 0)) {
        output("Not a good vector or eCos not running!\n");
        return no_thread_op(input_buffer);
    }
    nbytes = sizeof(handler);
    if (!low_read_memory_raw(vector.handler, (char *)&handler, &nbytes)) {
        return no_thread_op(input_buffer);
    }
/*    output("key: %x.%x, buf: %x\n", handler._key0, handler._key1, handler.inbuf); */
    if ((handler._key0 != ICE_THREAD_HANDLER_KEY0) || 
        (handler._key1 != ICE_THREAD_HANDLER_KEY1)) {
        output("Not a good handler!\n");
        return no_thread_op(input_buffer);
    }
    /* Copy input buffer so board can see it */
    low_write_memory(input_buffer, (int)handler.inbuf, strlen(input_buffer)+1);
    /* Make sure output buffer is initialized */
    result[0] = '\0';
    low_write_memory(result, (int)handler.outbuf, 1);
    /* Save current registers */
    low_update_registers();
    memcpy(hold_registers, registers, sizeof(registers));
    /* Set up to call the thread support function */
    record_register(SP_REGNUM, (int)handler.stack+handler.stack_size);
    record_register(LR_REGNUM, ICE_THREAD_VECTOR);
    low_set_breakpoint(ICE_THREAD_VECTOR, 4);  
    record_register(PC_REGNUM, handler.fun);
    record_register(PS_REGNUM, 0xD3);
    registers_are_dirty = 1;
    res = low_resume(RESUME_CONTINUE, 0);  /* Go and execute the code */
    /* Restore registers to original state */
    registers_are_dirty = 1;
    registers_up_to_date = 1;
    memcpy(registers, hold_registers, sizeof(registers));
    low_delete_breakpoint(ICE_THREAD_VECTOR, 4);  
    /* Return results */
    if (res == TARGET_SIGNAL_TRAP) {
        nbytes = sizeof(result);
        if (!low_read_memory_raw((int)handler.outbuf, result, &nbytes)) {
            output("Error reading result\n");
            strcpy(result, "ENN");
        }
    } else {
        output("Target didn't execute/stop properly\n");
        strcpy(result, "ENN");
    }
    putpkt(result);
    return 1;
}
