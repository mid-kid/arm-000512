/* dll_init.cc

   Copyright 1998, 1999 Cygnus Solutions.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include <stdlib.h>
#include "winsup.h"
#include "exceptions.h"
#include "dll_init.h"

extern void check_sanity_and_sync (per_process *);

/* WARNING: debug can't be called before init !!!! */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// the private structure

typedef enum   { NONE, LINK, LOAD } dllType;

struct dll
{
  per_process *p;
  HMODULE handle;
  dllType type;
};

//-----------------------------------------------------------------------------

#define MAX_DLL_BEFORE_INIT	100 // FIXME: enough ???
static dll _list_before_init[MAX_DLL_BEFORE_INIT];

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// local variables

static DllList _the;
static int _last = 0;
static int _max = MAX_DLL_BEFORE_INIT;
static dll *_list = _list_before_init;
static int _initCalled = 0;
static int _numberOfOpenedDlls = 0;
static int _forkeeMustReloadDlls = 0;
static int _in_forkee = 0;
static const char *_dlopenedLib = 0;
static int _dlopenIndex = -1;

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

static int __dll_global_dtors_recorded = 0;

static void
__dll_global_dtors()
{
  _the.doGlobalDestructorsOfDlls();
}

static void
doGlobalCTORS (per_process *p)
{
  void (**pfunc)() = p->ctors;

  /* Run ctors backwards, so skip the first entry and find how many
    there are, then run them.  */

  if (pfunc)
    {
      int i;
      for (i = 1; pfunc[i]; i++);

      for (int j = i - 1; j > 0; j-- )
	(pfunc[j]) ();
    }
}

static void
doGlobalDTORS (per_process *p)
{
  if (!p)
    return;
  void (**pfunc)() = p->dtors;
  for (int i = 1; pfunc[i]; i++)
    (pfunc[i]) ();
}

#define INC 500

static int
add (HMODULE h, per_process *p, dllType type)
{
  int ret = -1;

  if (p)
    check_sanity_and_sync (p);

  if (_last == _max)
    {
      if (!_initCalled) // we try to load more than MAX_DLL_BEFORE_INIT
	{
	  small_printf ("try to load more dll than max allowed=%d\n",
		       MAX_DLL_BEFORE_INIT);
	  ExitProcess (1);
	}

      dll* newArray = new dll[_max+INC];
      if (_list)
	{
	  memcpy (newArray, _list, _max * sizeof (dll));
	  if (_list != _list_before_init)
	    delete []_list;
	}
      _list = newArray;
      _max += INC;
    }

  _list[_last].handle = h;
  _list[_last].p = p;
  _list[_last].type = type;

  ret = _last++;
  return ret;
}

static int
initOneDll (per_process *p)
{
  /* global variable user_data must be initialized */
  if (user_data == NULL)
    {
      small_printf ("WARNING: process not inited while trying to init a DLL!\n");
      return 0;
    }

  /* init impure_ptr */
  *(p->impure_ptr_ptr) = *(user_data->impure_ptr_ptr);

  /* FIXME: init environment (useful?) */
  *(p->envptr) = *(user_data->envptr);

  /* FIXME: need other initializations? */

  int ret = 1;
  if (!_in_forkee)
    {
      /* global contructors */
      doGlobalCTORS (p);

      /* entry point of dll (use main of per_process with null args...) */
      if (p->main)
	ret = (*(p->main)) (0, 0, 0);
    }

  return ret;
}

DllList& DllList::the ()
{
  return _the;
}

void
DllList::currentDlOpenedLib (const char *name)
{
  if (_dlopenedLib != 0)
    small_printf ("WARNING: previous dlopen of %s wasn't correctly performed\n", _dlopenedLib);
  _dlopenedLib = name;
  _dlopenIndex = -1;
}

int
DllList::recordDll (HMODULE h, per_process *p)
{
  int ret = -1;

  /* debug_printf ("Record a dll p=%p\n", p); see WARNING */
  dllType type = LINK;
  if (_initCalled)
    {
      type = LOAD;
      _numberOfOpenedDlls++;
      forkeeMustReloadDlls (1);
    }

  if (type == LOAD && _dlopenedLib !=0)
    {
    char buf[MAX_PATH];
    GetModuleFileName (h, buf, MAX_PATH);
    // it is not the current dlopened lib
    // so we insert one empty lib to preserve place for current dlopened lib
    if (!strcasematch (_dlopenedLib, buf))
      {
      if (_dlopenIndex == -1)
	_dlopenIndex = add (0, 0, NONE);
      ret = add (h, p, type);
      }
    else // it is the current dlopened lib
      {
	if (_dlopenIndex != -1)
	  {
	    _list[_dlopenIndex].handle = h;
	    _list[_dlopenIndex].p = p;
	    _list[_dlopenIndex].type = type;
	    ret = _dlopenIndex;
	    _dlopenIndex = -1;
	  }
	else // it this case the dlopened lib doesn't need other lib
	  ret = add (h, p, type);
	_dlopenedLib = 0;
      }
    }
  else
    ret = add (h, p, type);

  if (_initCalled) // main module is already initialized
    {
      if (!initOneDll (p))
	ret = -1;
    }
  return ret;
}

void
DllList::detachDll (int dll_index)
{
  if (dll_index != -1)
    {
      dll *aDll = &(_list[dll_index]);
      doGlobalDTORS (aDll->p);
      if (aDll->type == LOAD)
	_numberOfOpenedDlls--;
      aDll->type = NONE;
    }
  else
    small_printf ("WARNING: try to detach an already detached dll ...\n");
}

void
DllList::initAll ()
{
  // init for destructors
  // because initAll isn't called in forked process, this exit function will
  // be recorded only once
  if (!__dll_global_dtors_recorded)
    {
      atexit (__dll_global_dtors);
      __dll_global_dtors_recorded = 1;
    }

  if (!_initCalled)
    {
      debug_printf ("call to DllList::initAll");
      for (int i = 0; i < _last; i++)
	{
	  per_process *p = _list[i].p;
	  if (p)
	    initOneDll (p);
	}
      _initCalled = 1;
    }
}

void
DllList::doGlobalDestructorsOfDlls ()
{
  // global destructors in reverse order
  for (int i = _last - 1; i >= 0; i--)
    {
      if (_list[i].type != NONE)
	{
	  per_process *p = _list[i].p;
	  if (p)
	    doGlobalDTORS (p);
	}
    }
}

int
DllList::numberOfOpenedDlls ()
{
  return _numberOfOpenedDlls;
}

int
DllList::forkeeMustReloadDlls ()
{
  return _forkeeMustReloadDlls;
}

void
DllList::forkeeMustReloadDlls (int i)
{
  _forkeeMustReloadDlls = i;
}

void
DllList::forkeeStartLoadedDlls ()
{
  _initCalled = 1;
  _in_forkee = 1;
}

void
DllList::forkeeEndLoadedDlls ()
{
  _in_forkee = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// iterators

DllListIterator::DllListIterator (int type) : _type (type), _index (-1)
{
  operator++ ();
}

DllListIterator::~DllListIterator ()
{
}

DllListIterator::operator per_process* ()
{
  return _list[index ()].p;
}

void
DllListIterator::operator++ ()
{
  _index++;
  while (_index < _last && (int) (_list[_index].type) != _type)
    _index++;
  if (_index == _last)
    _index = -1;
}

DllNameIterator::DllNameIterator (int type) : DllListIterator (type)
{
}

DllNameIterator::~DllNameIterator ()
{
}

static char buffer[MAX_PATH];

DllNameIterator::operator const char* ()
{
  char *ret = 0;
  if (GetModuleFileName (_list[index ()].handle, buffer, MAX_PATH))
    ret = buffer;
  else
    small_printf ("WARNING: can't get name of loaded module, %E\n");
  return ret;
}

LinkedDllIterator::LinkedDllIterator () : DllListIterator ((int) LINK)
{
}

LinkedDllIterator::~LinkedDllIterator ()
{
}

LoadedDllIterator::LoadedDllIterator () : DllListIterator ((int) LOAD)
{
}

LoadedDllIterator::~LoadedDllIterator ()
{
}

LinkedDllNameIterator::LinkedDllNameIterator () : DllNameIterator ((int) LINK)
{
}

LinkedDllNameIterator::~LinkedDllNameIterator ()
{
}

LoadedDllNameIterator::LoadedDllNameIterator () : DllNameIterator ((int) LOAD)
{
}

LoadedDllNameIterator::~LoadedDllNameIterator ()
{
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// the extern symbols

extern "C"
{
  /* This is an exported copy of environ which can be used by DLLs
     which use cygwin.dll.  */
  extern struct _reent reent_data;
};

/* Initialize Cygwin DLL. This is only done if this is the first cygwin
   DLL to be loaded and the main app is not Cygwin. */
/* FIXME: This function duplicates too much code from dll_crt0_1 in
   dcrt0.cc.  Need to consolidate this and remove the duplication. */
static void
dll_dllcrt0_1 (per_process *uptr)
{
  /* According to onno@stack.urc.tue.nl, the exception handler record must
     be on the stack.  */
  /* FIXME: Verify forked children get their exception handler set up ok. */
  exception_list cygwin_except_entry;

  check_sanity_and_sync (uptr);

  /* Set the local copy of the pointer into the user space. */
  user_data = uptr;

  /* Nasty static stuff needed by newlib -- point to a local copy of
     the reent stuff.
     Note: this MUST be done here (before the forkee code) as the
     fork copy code doesn't copy the data in libccrt0.cc (that's why we
     pass in the per_process struct into the .dll from libccrt0). */

  *(user_data->impure_ptr_ptr) = &reent_data;
  _impure_ptr = &reent_data;

  /* Initialize the cygwin32 subsystem if this is the first process,
     or attach to the shared data structure if it's already running. */
  shared_init ();

  /* Initialize events. */
  events_init ();

  (void) SetErrorMode (SEM_FAILCRITICALERRORS);

  /* Initialize the heap. */
  heap_init ();

  /* Initialize SIGSEGV handling, etc...  Because the exception handler
     references data in the shared area, this must be done after
     shared_init. */
  init_exceptions (&cygwin_except_entry);

  pinfo_init (NULL);		/* Initialize our process table entry. */

  /* Nasty static stuff needed by newlib - initialize it.
     Note that impure_ptr has already been set up to point to this above
     NB. This *MUST* be done here, just after the forkee code as some
     of the calls below (eg. uinfo_init) do stdio calls - this area must
     be set to zero before then. */

  memset (&reent_data, 0, sizeof (reent_data));
  reent_data._errno = 0;
  reent_data._stdin =  reent_data.__sf + 0;
  reent_data._stdout = reent_data.__sf + 1;
  reent_data._stderr = reent_data.__sf + 2;

  /* Allocate dtable */
  dtable_init ();

  /* Connect to tty. */
  tty_init ();

  /* Set up standard fds in file descriptor table. */
  hinfo_init ();

  /* Call init of loaded dlls. */
  DllList::the().initAll();

  set_errno (0);
}

extern "C"
int
dll_dllcrt0 (HMODULE h, per_process *p)
{
  return _the.recordDll (h, p);
}

extern "C"
int
dll_noncygwin_dllcrt0 (HMODULE h, per_process *p)
{
  /* Partially initialize Cygwin guts for non-cygwin apps. */
  if (! user_data)
    {
      dll_dllcrt0_1 (p);
    }
  return dll_dllcrt0 (h, p);
}

extern "C"
void
cygwin_detach_dll (int dll_index)
{
  _the.detachDll (dll_index);
}

extern "C"
void
dlfork (int val)
{
  _the.forkeeMustReloadDlls (val);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
