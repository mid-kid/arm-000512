/*
 * ------------------------------------------------------------------------
 *      PACKAGE:  [incr Tcl]
 *  DESCRIPTION:  Object-Oriented Extensions to Tcl
 *
 *  [incr Tcl] provides object-oriented extensions to Tcl, much as
 *  C++ provides object-oriented extensions to C.  It provides a means
 *  of encapsulating related procedures together with their shared data
 *  in a local namespace that is hidden from the outside world.  It
 *  promotes code re-use through inheritance.  More than anything else,
 *  it encourages better organization of Tcl applications through the
 *  object-oriented paradigm, leading to code that is easier to
 *  understand and maintain.
 *
 *  This file defines information that tracks classes and objects
 *  at a global level for a given interpreter.
 *
 * ========================================================================
 *  AUTHOR:  Michael J. McLennan
 *           Bell Labs Innovations for Lucent Technologies
 *           mmclennan@lucent.com
 *           http://www.tcltk.com/itcl
 *
 *     RCS:  $Id: itcl_cmds.c,v 1.2 1999/01/27 18:56:05 jingham Exp $
 * ========================================================================
 *           Copyright (c) 1993-1998  Lucent Technologies, Inc.
 * ------------------------------------------------------------------------
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */
#include "itclInt.h"

/*
 *  FORWARD DECLARATIONS
 */
static void ItclDelObjectInfo _ANSI_ARGS_((char* cdata));
static int Initialize _ANSI_ARGS_((Tcl_Interp *interp));
static int ItclHandleStubCmd _ANSI_ARGS_((ClientData clientData,
    Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]));
static void ItclDeleteStub _ANSI_ARGS_((ClientData cdata));

/*
 * The following string is the startup script executed in new
 * interpreters.  It locates the Tcl code in the [incr Tcl] library
 * directory and loads it in.
 */

static char initScript[] = "\n\
namespace eval ::itcl {\n\
    proc _find_init {} {\n\
        global env tcl_library\n\
        variable library\n\
        variable version\n\
        rename _find_init {}\n\
        if {[catch {uplevel #0 source -rsrc itcl}] == 0} {\n\
            return\n\
        }\n\
        tcl_findLibrary itcl 3.0 {} itcl.tcl ITCL_LIBRARY ::itcl::library {} {} itcl
           
   }\n\
    _find_init\n\
}";

/*
 * The following script is used to initialize Itcl in a safe interpreter.
 */

static char safeInitScript[] =
"proc ::itcl::local {class name args} {\n\
    set ptr [uplevel eval [list $class $name] $args]\n\
    uplevel [list set itcl-local-$ptr $ptr]\n\
    set cmd [uplevel namespace which -command $ptr]\n\
    uplevel [list trace variable itcl-local-$ptr u \"::itcl::delete object $cmd; list\"]\n\
    return $ptr\n\
}";


/*
 * ------------------------------------------------------------------------
 *  Initialize()
 *
 *  Invoked whenever a new interpeter is created to install the
 *  [incr Tcl] package.  Usually invoked within Tcl_AppInit() at
 *  the start of execution.
 *
 *  Creates the "::itcl" namespace and installs access commands for
 *  creating classes and querying info.
 *
 *  Returns TCL_OK on success, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
static int
Initialize(interp)
    Tcl_Interp *interp;  /* interpreter to be updated */
{
    Tcl_CmdInfo cmdInfo;
    Tcl_Namespace *itclNs;
    ItclObjectInfo *info;

    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 1) == NULL) {
	return TCL_ERROR;
    }

    /*
     *  See if [incr Tcl] is already installed.
     */
    if (Tcl_GetCommandInfo(interp, "::itcl::class", &cmdInfo)) {
        Tcl_SetResult(interp, "already installed: [incr Tcl]", TCL_STATIC);
        return TCL_ERROR;
    }

    /*
     *  Initialize the ensemble package first, since we need this
     *  for other parts of [incr Tcl].
     */
    if (Itcl_EnsembleInit(interp) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Create the top-level data structure for tracking objects.
     *  Store this as "associated data" for easy access, but link
     *  it to the itcl namespace for ownership.
     */
    info = (ItclObjectInfo*)ckalloc(sizeof(ItclObjectInfo));
    info->interp = interp;
    Tcl_InitHashTable(&info->objects, TCL_ONE_WORD_KEYS);
    Itcl_InitStack(&info->transparentFrames);
    Tcl_InitHashTable(&info->contextFrames, TCL_ONE_WORD_KEYS);
    info->protection = ITCL_DEFAULT_PROTECT;
    Itcl_InitStack(&info->cdefnStack);

    Tcl_SetAssocData(interp, ITCL_INTERP_DATA,
        (Tcl_InterpDeleteProc*)NULL, (ClientData)info);

    /*
     *  Install commands into the "::itcl" namespace.
     */
    Tcl_CreateObjCommand(interp, "::itcl::class", Itcl_ClassCmd,
        (ClientData)info, Itcl_ReleaseData);
    Itcl_PreserveData((ClientData)info);

    Tcl_CreateObjCommand(interp, "::itcl::body", Itcl_BodyCmd,
        (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::configbody", Itcl_ConfigBodyCmd,
        (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    Itcl_EventuallyFree((ClientData)info, ItclDelObjectInfo);

    /*
     *  Create the "itcl::find" command for high-level queries.
     */
    if (Itcl_CreateEnsemble(interp, "::itcl::find") != TCL_OK) {
        return TCL_ERROR;
    }
    if (Itcl_AddEnsemblePart(interp, "::itcl::find",
            "classes", "?pattern?",
            Itcl_FindClassesCmd,
            (ClientData)info, Itcl_ReleaseData) != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    if (Itcl_AddEnsemblePart(interp, "::itcl::find",
            "objects", "?-class className? ?-isa className? ?pattern?",
            Itcl_FindObjectsCmd,
            (ClientData)info, Itcl_ReleaseData) != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);


    /*
     *  Create the "itcl::delete" command to delete objects
     *  and classes.
     */
    if (Itcl_CreateEnsemble(interp, "::itcl::delete") != TCL_OK) {
        return TCL_ERROR;
    }
    if (Itcl_AddEnsemblePart(interp, "::itcl::delete",
            "class", "name ?name...?",
            Itcl_DelClassCmd,
            (ClientData)info, Itcl_ReleaseData) != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    if (Itcl_AddEnsemblePart(interp, "::itcl::delete",
            "object", "name ?name...?",
            Itcl_DelObjectCmd,
            (ClientData)info, Itcl_ReleaseData) != TCL_OK) {
        return TCL_ERROR;
    }
    Itcl_PreserveData((ClientData)info);

    /*
     *  Add "code" and "scope" commands for handling scoped values.
     */
    Tcl_CreateObjCommand(interp, "::itcl::code", Itcl_CodeCmd,
        (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    Tcl_CreateObjCommand(interp, "::itcl::scope", Itcl_ScopeCmd,
        (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL);

    /*
     *  Add commands for handling import stubs at the Tcl level.
     */
    if (Itcl_CreateEnsemble(interp, "::itcl::import::stub") != TCL_OK) {
        return TCL_ERROR;
    }
    if (Itcl_AddEnsemblePart(interp, "::itcl::import::stub",
            "create", "name", Itcl_StubCreateCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Itcl_AddEnsemblePart(interp, "::itcl::import::stub",
            "exists", "name", Itcl_StubExistsCmd,
            (ClientData)NULL, (Tcl_CmdDeleteProc*)NULL) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Install a variable resolution procedure to handle scoped
     *  values everywhere within the interpreter.
     */
    Tcl_AddInterpResolvers(interp, "itcl", (Tcl_ResolveCmdProc*)NULL,
        Itcl_ScopedVarResolver, (Tcl_ResolveCompiledVarProc*)NULL);

    /*
     *  Install the "itcl::parser" namespace used to parse the
     *  class definitions.
     */
    if (Itcl_ParseInit(interp, info) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Create "itcl::builtin" namespace for commands that
     *  are automatically built into class definitions.
     */
    if (Itcl_BiInit(interp) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Install stuff needed for backward compatibility with previous
     *  version of [incr Tcl].
     */
    if (Itcl_OldInit(interp, info) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Export all commands in the "itcl" namespace so that they
     *  can be imported with something like "namespace import itcl::*"
     */
    itclNs = Tcl_FindNamespace(interp, "::itcl", (Tcl_Namespace*)NULL,
        TCL_LEAVE_ERR_MSG);

    if (!itclNs ||
        Tcl_Export(interp, itclNs, "*", /* resetListFirst */ 1) != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     *  Set up the variables containing version info.
     */

    Tcl_SetVar(interp, "::itcl::patchLevel", ITCL_PATCH_LEVEL,
        TCL_NAMESPACE_ONLY);

    Tcl_SetVar(interp, "::itcl::version", ITCL_VERSION,
        TCL_NAMESPACE_ONLY);

    /*
     *  Package is now loaded.
     */
    if (Tcl_PkgProvide(interp, "Itcl", ITCL_VERSION) != TCL_OK) {
	return TCL_ERROR;
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_Init()
 *
 *  Invoked whenever a new INTERPRETER is created to install the
 *  [incr Tcl] package.  Usually invoked within Tcl_AppInit() at
 *  the start of execution.
 *
 *  Creates the "::itcl" namespace and installs access commands for
 *  creating classes and querying info.
 *
 *  Returns TCL_OK on success, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
int
Itcl_Init(interp)
    Tcl_Interp *interp;  /* interpreter to be updated */
{
    if (Initialize(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tcl_Eval(interp, initScript);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_SafeInit()
 *
 *  Invoked whenever a new SAFE INTERPRETER is created to install
 *  the [incr Tcl] package.
 *
 *  Creates the "::itcl" namespace and installs access commands for
 *  creating classes and querying info.
 *
 *  Returns TCL_OK on success, or TCL_ERROR (along with an error
 *  message in the interpreter) if anything goes wrong.
 * ------------------------------------------------------------------------
 */
int
Itcl_SafeInit(interp)
    Tcl_Interp *interp;  /* interpreter to be updated */
{
    if (Initialize(interp) != TCL_OK) {
	return TCL_ERROR;
    }
    return Tcl_Eval(interp, safeInitScript);
}


/*
 * ------------------------------------------------------------------------
 *  ItclDelObjectInfo()
 *
 *  Invoked when the management info for [incr Tcl] is no longer being
 *  used in an interpreter.  This will only occur when all class
 *  manipulation commands are removed from the interpreter.
 * ------------------------------------------------------------------------
 */
static void
ItclDelObjectInfo(cdata)
    char* cdata;    /* client data for class command */
{
    ItclObjectInfo *info = (ItclObjectInfo*)cdata;

    ItclObject *contextObj;
    Tcl_HashSearch place;
    Tcl_HashEntry *entry;

    /*
     *  Destroy all known objects by deleting their access
     *  commands.
     */
    entry = Tcl_FirstHashEntry(&info->objects, &place);
    while (entry) {
        contextObj = (ItclObject*)Tcl_GetHashValue(entry);
        Tcl_DeleteCommandFromToken(info->interp, contextObj->accessCmd);
        entry = Tcl_NextHashEntry(&place);
    }
    Tcl_DeleteHashTable(&info->objects);

    /*
     *  Discard all known object contexts.
     */
    entry = Tcl_FirstHashEntry(&info->contextFrames, &place);
    while (entry) {
        Itcl_ReleaseData( Tcl_GetHashValue(entry) );
        entry = Tcl_NextHashEntry(&place);
    }
    Tcl_DeleteHashTable(&info->contextFrames);

    Itcl_DeleteStack(&info->transparentFrames);
    Itcl_DeleteStack(&info->cdefnStack);
    ckfree((char*)info);
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_FindClassesCmd()
 *
 *  Part of the "::info" ensemble.  Invoked by Tcl whenever the user
 *  issues an "info classes" command to query the list of classes
 *  in the current namespace.  Handles the following syntax:
 *
 *    info classes ?<pattern>?
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itcl_FindClassesCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* class/object info */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    Tcl_Namespace *activeNs = Tcl_GetCurrentNamespace(interp);
    Tcl_Namespace *globalNs = Tcl_GetGlobalNamespace(interp);
    int forceFullNames = 0;

    char *pattern;
    char *name;
    int i, nsearch, newEntry;
    Tcl_HashTable unique;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Namespace *search[2];
    Tcl_Command cmd, originalCmd;
    Namespace *nsPtr;
    Tcl_Obj *listPtr, *objPtr;

    if (objc > 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "?pattern?");
        return TCL_ERROR;
    }

    if (objc == 2) {
        pattern = Tcl_GetStringFromObj(objv[1], (int*)NULL);
        forceFullNames = (strstr(pattern, "::") != NULL);
    } else {
        pattern = NULL;
    }

    /*
     *  Search through all commands in the current namespace and
     *  in the global namespace.  If we find any commands that
     *  represent classes, report them.
     */
    listPtr = Tcl_NewListObj(0, (Tcl_Obj* CONST*)NULL);

    nsearch = 0;
    search[nsearch++] = activeNs;
    if (activeNs != globalNs) {
        search[nsearch++] = globalNs;
    }

    Tcl_InitHashTable(&unique, TCL_ONE_WORD_KEYS);

    for (i=0; i < nsearch; i++) {
        nsPtr = (Namespace*)search[i];

        entry = Tcl_FirstHashEntry(&nsPtr->cmdTable, &place);
        while (entry) {
            cmd = (Tcl_Command)Tcl_GetHashValue(entry);
            if (Itcl_IsClass(cmd)) {
                originalCmd = TclGetOriginalCommand(cmd);

                /*
                 *  Report full names if:
                 *  - the pattern has namespace qualifiers
                 *  - the class namespace is not in the current namespace
                 *  - the class's object creation command is imported from
                 *      another namespace.
                 *
                 *  Otherwise, report short names.
                 */
                if (forceFullNames || nsPtr != (Namespace*)activeNs ||
                    originalCmd != NULL) {

                    objPtr = Tcl_NewStringObj((char*)NULL, 0);
                    Tcl_GetCommandFullName(interp, cmd, objPtr);
                    name = Tcl_GetStringFromObj(objPtr, (int*)NULL);
                } else {
                    name = Tcl_GetCommandName(interp, cmd);
                    objPtr = Tcl_NewStringObj(name, -1);
                }

                if (originalCmd) {
                    cmd = originalCmd;
                }
                Tcl_CreateHashEntry(&unique, (char*)cmd, &newEntry);

                if (newEntry && (!pattern || Tcl_StringMatch(name, pattern))) {
                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL,
                        listPtr, objPtr);
                }
            }
            entry = Tcl_NextHashEntry(&place);
        }
    }
    Tcl_DeleteHashTable(&unique);

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_FindObjectsCmd()
 *
 *  Part of the "::info" ensemble.  Invoked by Tcl whenever the user
 *  issues an "info objects" command to query the list of known objects.
 *  Handles the following syntax:
 *
 *    info objects ?-class <className>? ?-isa <className>? ?<pattern>?
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
int
Itcl_FindObjectsCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* class/object info */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    Tcl_Namespace *activeNs = Tcl_GetCurrentNamespace(interp);
    Tcl_Namespace *globalNs = Tcl_GetGlobalNamespace(interp);
    int forceFullNames = 0;

    char *pattern = NULL;
    ItclClass *classDefn = NULL;
    ItclClass *isaDefn = NULL;

    char *name, *token;
    int i, pos, nsearch, newEntry, match;
    ItclObject *contextObj;
    Tcl_HashTable unique;
    Tcl_HashEntry *entry;
    Tcl_HashSearch place;
    Tcl_Namespace *search[2];
    Tcl_Command cmd, originalCmd;
    Namespace *nsPtr;
    Command *cmdPtr;
    Tcl_Obj *listPtr, *objPtr;

    /*
     *  Parse arguments:
     *  objects ?-class <className>? ?-isa <className>? ?<pattern>?
     */
    pos = 0;
    while (++pos < objc) {
        token = Tcl_GetStringFromObj(objv[pos], (int*)NULL);
        if (*token != '-') {
            if (!pattern) {
                pattern = token;
                forceFullNames = (strstr(pattern, "::") != NULL);
            } else {
                break;
            }
        }
        else if ((pos+1 < objc) && (strcmp(token,"-class") == 0)) {
            name = Tcl_GetStringFromObj(objv[pos+1], (int*)NULL);
            classDefn = Itcl_FindClass(interp, name, /* autoload */ 1);
            if (classDefn == NULL) {
                return TCL_ERROR;
            }
            pos++;
        }
        else if ((pos+1 < objc) && (strcmp(token,"-isa") == 0)) {
            name = Tcl_GetStringFromObj(objv[pos+1], (int*)NULL);
            isaDefn = Itcl_FindClass(interp, name, /* autoload */ 1);
            if (isaDefn == NULL) {
                return TCL_ERROR;
            }
            pos++;
        }
        else {
            break;
        }
    }

    if (pos < objc) {
        Tcl_WrongNumArgs(interp, 1, objv,
            "?-class className? ?-isa className? ?pattern?");
        return TCL_ERROR;
    }

    /*
     *  Search through all commands in the current namespace and
     *  in the global namespace.  If we find any commands that
     *  represent objects, report them.
     */
    listPtr = Tcl_NewListObj(0, (Tcl_Obj* CONST*)NULL);

    nsearch = 0;
    search[nsearch++] = activeNs;
    if (activeNs != globalNs) {
        search[nsearch++] = globalNs;
    }

    Tcl_InitHashTable(&unique, TCL_ONE_WORD_KEYS);

    for (i=0; i < nsearch; i++) {
        nsPtr = (Namespace*)search[i];

        entry = Tcl_FirstHashEntry(&nsPtr->cmdTable, &place);
        while (entry) {
            cmd = (Tcl_Command)Tcl_GetHashValue(entry);
            if (Itcl_IsObject(cmd)) {
                originalCmd = TclGetOriginalCommand(cmd);
                if (originalCmd) {
                    cmd = originalCmd;
                }
                cmdPtr = (Command*)cmd;
                contextObj = (ItclObject*)cmdPtr->objClientData;

                /*
                 *  Report full names if:
                 *  - the pattern has namespace qualifiers
                 *  - the class namespace is not in the current namespace
                 *  - the class's object creation command is imported from
                 *      another namespace.
                 *
                 *  Otherwise, report short names.
                 */
                if (forceFullNames || nsPtr != (Namespace*)activeNs ||
                    originalCmd != NULL) {

                    objPtr = Tcl_NewStringObj((char*)NULL, 0);
                    Tcl_GetCommandFullName(interp, cmd, objPtr);
                    name = Tcl_GetStringFromObj(objPtr, (int*)NULL);
                } else {
                    name = Tcl_GetCommandName(interp, cmd);
                    objPtr = Tcl_NewStringObj(name, -1);
                }

                Tcl_CreateHashEntry(&unique, (char*)cmd, &newEntry);

                match = 0;
                if (newEntry && (!pattern || Tcl_StringMatch(name, pattern))) {
                    if (!classDefn || (contextObj->classDefn == classDefn)) {
                        if (!isaDefn) {
                            match = 1;
                        } else {
                            entry = Tcl_FindHashEntry(
                                &contextObj->classDefn->heritage,
                                (char*)isaDefn);

                            if (entry) {
                                match = 1;
                            }
                        }
                    }
                }

                if (match) {
                    Tcl_ListObjAppendElement((Tcl_Interp*)NULL,
                        listPtr, objPtr);
                } else {
                    Tcl_IncrRefCount(objPtr);  /* throw away the name */
                    Tcl_DecrRefCount(objPtr);
                }
            }
            entry = Tcl_NextHashEntry(&place);
        }
    }
    Tcl_DeleteHashTable(&unique);

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_ProtectionCmd()
 *
 *  Invoked by Tcl whenever the user issues a protection setting
 *  command like "public" or "private".  Creates commands and
 *  variables, and assigns a protection level to them.  Protection
 *  levels are defined as follows:
 *
 *    public    => accessible from any namespace
 *    protected => accessible from selected namespaces
 *    private   => accessible only in the namespace where it was defined
 *
 *  Handles the following syntax:
 *
 *    public <command> ?<arg> <arg>...?
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
int
Itcl_ProtectionCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* protection level (public/protected/private) */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    int pLevel = (int)clientData;

    int result;
    int oldLevel;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ?arg arg...?");
        return TCL_ERROR;
    }

    oldLevel = Itcl_Protection(interp, pLevel);

    if (objc == 2) {
        result = Tcl_EvalObj(interp, objv[1]);
    } else {
        result = Itcl_EvalArgs(interp, objc-1, objv+1);
    }

    if (result == TCL_BREAK) {
        Tcl_SetResult(interp, "invoked \"break\" outside of a loop",
            TCL_STATIC);
        result = TCL_ERROR;
    }
    else if (result == TCL_CONTINUE) {
        Tcl_SetResult(interp, "invoked \"continue\" outside of a loop",
            TCL_STATIC);
        result = TCL_ERROR;
    }
    else if (result != TCL_OK) {
        char mesg[256], *name;
        name = Tcl_GetStringFromObj(objv[0], (int*)NULL);
        sprintf(mesg, "\n    (%.100s body line %d)",
            name, interp->errorLine);
        Tcl_AddErrorInfo(interp, mesg);
    }

    Itcl_Protection(interp, oldLevel);
    return result;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_DelClassCmd()
 *
 *  Part of the "delete" ensemble.  Invoked by Tcl whenever the
 *  user issues a "delete class" command to delete classes.
 *  Handles the following syntax:
 *
 *    delete class <name> ?<name>...?
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itcl_DelClassCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* unused */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    int i;
    char *name;
    ItclClass *cdefn;

    /*
     *  Since destroying a base class will destroy all derived
     *  classes, calls like "destroy class Base Derived" could
     *  fail.  Break this into two passes:  first check to make
     *  sure that all classes on the command line are valid,
     *  then delete them.
     */
    for (i=1; i < objc; i++) {
        name = Tcl_GetStringFromObj(objv[i], (int*)NULL);
        cdefn = Itcl_FindClass(interp, name, /* autoload */ 1);
        if (cdefn == NULL) {
            return TCL_ERROR;
        }
    }

    for (i=1; i < objc; i++) {
        name = Tcl_GetStringFromObj(objv[i], (int*)NULL);
        cdefn = Itcl_FindClass(interp, name, /* autoload */ 0);

        if (cdefn) {
            Tcl_ResetResult(interp);
            if (Itcl_DeleteClass(interp, cdefn) != TCL_OK) {
                return TCL_ERROR;
            }
        }
    }
    Tcl_ResetResult(interp);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_DelObjectCmd()
 *
 *  Part of the "delete" ensemble.  Invoked by Tcl whenever the user
 *  issues a "delete object" command to delete [incr Tcl] objects.
 *  Handles the following syntax:
 *
 *    delete object <name> ?<name>...?
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
int
Itcl_DelObjectCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* object management info */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    int i;
    char *name;
    ItclObject *contextObj;

    /*
     *  Scan through the list of objects and attempt to delete them.
     *  If anything goes wrong (i.e., destructors fail), then
     *  abort with an error.
     */
    for (i=1; i < objc; i++) {
        name = Tcl_GetStringFromObj(objv[i], (int*)NULL);
        if (Itcl_FindObject(interp, name, &contextObj) != TCL_OK) {
            return TCL_ERROR;
        }

        if (contextObj == NULL) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "object \"", name, "\" not found",
                (char*)NULL);
            return TCL_ERROR;
        }

        if (Itcl_DeleteObject(interp, contextObj) != TCL_OK) {
            return TCL_ERROR;
        }
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_ScopeCmd()
 *
 *  Invoked by Tcl whenever the user issues a "scope" command to
 *  create a fully qualified variable name.  Handles the following
 *  syntax:
 *
 *    scope <variable>
 *
 *  If the input string is already fully qualified (starts with "::"),
 *  then this procedure does nothing.  Otherwise, it looks for a
 *  data member called <variable> and returns its fully qualified
 *  name.  If the <variable> is a common data member, this procedure
 *  returns a name of the form:
 *
 *    ::namesp::namesp::class::variable
 *
 *  If the <variable> is an instance variable, this procedure returns
 *  a name of the form:
 *
 *    @itcl ::namesp::namesp::object variable
 *
 *  This kind of scoped value is recognized by the Itcl_ScopedVarResolver
 *  proc, which handles variable resolution for the entire interpreter.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itcl_ScopeCmd(dummy, interp, objc, objv)
    ClientData dummy;        /* unused */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    int result = TCL_OK;
    Tcl_Namespace *contextNs = Tcl_GetCurrentNamespace(interp);
    char *openParen = NULL;

    register char *p;
    char *token;
    ItclClass *contextClass;
    ItclObject *contextObj;
    ItclObjectInfo *info;
    Tcl_CallFrame *framePtr;
    Tcl_HashEntry *entry;
    ItclVarLookup *vlookup;
    Tcl_Obj *objPtr;
    Tcl_Var var;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "varname");
        return TCL_ERROR;
    }

    /*
     *  If this looks like a fully qualified name already,
     *  then return it as is.
     */
    token = Tcl_GetStringFromObj(objv[1], (int*)NULL);
    if (*token == ':' && *(token+1) == ':') {
        Tcl_SetObjResult(interp, objv[1]);
        return TCL_OK;
    }

    /*
     *  If the variable name is an array reference, pick out
     *  the array name and use that for the lookup operations
     *  below.
     */
    for (p=token; *p != '\0'; p++) {
        if (*p == '(') {
            openParen = p;
        }
        else if (*p == ')' && openParen) {
            *openParen = '\0';
            break;
        }
    }

    /*
     *  Figure out what context we're in.  If this is a class,
     *  then look up the variable in the class definition.
     *  If this is a namespace, then look up the variable in its
     *  varTable.  Note that the normal Itcl_GetContext function
     *  returns an error if we're not in a class context, so we
     *  perform a similar function here, the hard way.
     *
     *  TRICKY NOTE:  If this is an array reference, we'll get
     *    the array variable as the variable name.  We must be
     *    careful to add the index (everything from openParen
     *    onward) as well.
     */
    if (Itcl_IsClassNamespace(contextNs)) {
        contextClass = (ItclClass*)contextNs->clientData;

        entry = Tcl_FindHashEntry(&contextClass->resolveVars, token);
        if (!entry) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "variable \"", token, "\" not found in class \"",
                contextClass->fullname, "\"",
                (char*)NULL);
            result = TCL_ERROR;
            goto scopeCmdDone;
        }
        vlookup = (ItclVarLookup*)Tcl_GetHashValue(entry);

        if (vlookup->vdefn->member->flags & ITCL_COMMON) {
            Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
            Tcl_AppendToObj(resultPtr, vlookup->vdefn->member->fullname, -1);
            if (openParen) {
                *openParen = '(';
                Tcl_AppendToObj(resultPtr, openParen, -1);
                openParen = NULL;
            }
            result = TCL_OK;
            goto scopeCmdDone;
        }

        /*
         *  If this is not a common variable, then we better have
         *  an object context.  Return the name "@itcl object variable".
         */
        framePtr = _Tcl_GetCallFrame(interp, 0);
        info = contextClass->info;

        entry = Tcl_FindHashEntry(&info->contextFrames, (char*)framePtr);
        if (!entry) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "can't scope variable \"", token,
                "\": missing object context\"",
                (char*)NULL);
            result = TCL_ERROR;
            goto scopeCmdDone;
        }
        contextObj = (ItclObject*)Tcl_GetHashValue(entry);

        Tcl_AppendElement(interp, "@itcl");

        objPtr = Tcl_NewStringObj((char*)NULL, 0);
        Tcl_IncrRefCount(objPtr);
        Tcl_GetCommandFullName(interp, contextObj->accessCmd, objPtr);
        Tcl_AppendElement(interp, Tcl_GetStringFromObj(objPtr, (int*)NULL));
        Tcl_DecrRefCount(objPtr);

        objPtr = Tcl_NewStringObj((char*)NULL, 0);
        Tcl_IncrRefCount(objPtr);
        Tcl_AppendToObj(objPtr, vlookup->vdefn->member->fullname, -1);

        if (openParen) {
            *openParen = '(';
            Tcl_AppendToObj(objPtr, openParen, -1);
            openParen = NULL;
        }
        Tcl_AppendElement(interp, Tcl_GetStringFromObj(objPtr, (int*)NULL));
        Tcl_DecrRefCount(objPtr);
    }

    /*
     *  We must be in an ordinary namespace context.  Resolve
     *  the variable using Tcl_FindNamespaceVar.
     *
     *  TRICKY NOTE:  If this is an array reference, we'll get
     *    the array variable as the variable name.  We must be
     *    careful to add the index (everything from openParen
     *    onward) as well.
     */
    else {
        Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);

        var = Tcl_FindNamespaceVar(interp, token, contextNs,
            TCL_NAMESPACE_ONLY);

        if (!var) {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "variable \"", token, "\" not found in namespace \"",
                contextNs->fullName, "\"",
                (char*)NULL);
            result = TCL_ERROR;
            goto scopeCmdDone;
        }

        Tcl_GetVariableFullName(interp, var, resultPtr);
        if (openParen) {
            *openParen = '(';
            Tcl_AppendToObj(resultPtr, openParen, -1);
            openParen = NULL;
        }
    }

scopeCmdDone:
    if (openParen) {
        *openParen = '(';
    }
    return result;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_CodeCmd()
 *
 *  Invoked by Tcl whenever the user issues a "code" command to
 *  create a scoped command string.  Handles the following syntax:
 *
 *    code ?-namespace foo? arg ?arg arg ...?
 *
 *  Unlike the scope command, the code command DOES NOT look for
 *  scoping information at the beginning of the command.  So scopes
 *  will nest in the code command.
 *
 *  The code command is similar to the "namespace code" command in
 *  Tcl, but it preserves the list structure of the input arguments,
 *  so it is a lot more useful.
 *
 *  Returns TCL_OK/TCL_ERROR to indicate success/failure.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
int
Itcl_CodeCmd(dummy, interp, objc, objv)
    ClientData dummy;        /* unused */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    Tcl_Namespace *contextNs = Tcl_GetCurrentNamespace(interp);

    int pos;
    char *token;
    Tcl_Obj *listPtr, *objPtr;

    /*
     *  Handle flags like "-namespace"...
     */
    for (pos=1; pos < objc; pos++) {
        token = Tcl_GetStringFromObj(objv[pos], (int*)NULL);
        if (*token != '-') {
            break;
        }

        if (strcmp(token, "-namespace") == 0) {
            if (objc == 2) {
                Tcl_WrongNumArgs(interp, 1, objv,
                    "?-namespace name? command ?arg arg...?");
                return TCL_ERROR;
            } else {
                token = Tcl_GetStringFromObj(objv[pos+1], (int*)NULL);
                contextNs = Tcl_FindNamespace(interp, token,
                    (Tcl_Namespace*)NULL, TCL_LEAVE_ERR_MSG);

                if (!contextNs) {
                    return TCL_ERROR;
                }
                pos++;
            }
        }
        else if (strcmp(token, "--") == 0) {
            pos++;
            break;
        }
        else {
            Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
                "bad option \"", token, "\": should be -namespace or --",
                (char*)NULL);
            return TCL_ERROR;
        }
    }

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv,
            "?-namespace name? command ?arg arg...?");
        return TCL_ERROR;
    }

    /*
     *  Now construct a scoped command by integrating the
     *  current namespace context, and appending the remaining
     *  arguments AS A LIST...
     */
    listPtr = Tcl_NewListObj(0, (Tcl_Obj**)NULL);

    Tcl_ListObjAppendElement(interp, listPtr,
        Tcl_NewStringObj("namespace", -1));
    Tcl_ListObjAppendElement(interp, listPtr,
        Tcl_NewStringObj("inscope", -1));

    if (contextNs == Tcl_GetGlobalNamespace(interp)) {
        objPtr = Tcl_NewStringObj("::", -1);
    } else {
        objPtr = Tcl_NewStringObj(contextNs->fullName, -1);
    }
    Tcl_ListObjAppendElement(interp, listPtr, objPtr);

    if (objc-pos == 1) {
        objPtr = objv[pos];
    } else {
        objPtr = Tcl_NewListObj(objc-pos, &objv[pos]);
    }
    Tcl_ListObjAppendElement(interp, listPtr, objPtr);

    Tcl_SetObjResult(interp, listPtr);
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_StubCreateCmd()
 *
 *  Invoked by Tcl whenever the user issues a "stub create" command to
 *  create an autoloading stub for imported commands.  Handles the
 *  following syntax:
 *
 *    stub create <name>
 *
 *  Creates a command called <name>.  Executing this command will cause
 *  the real command <name> to be autoloaded.
 * ------------------------------------------------------------------------
 */
int
Itcl_StubCreateCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* not used */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    char *cmdName;
    Command *cmdPtr;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "name");
        return TCL_ERROR;
    }
    cmdName = Tcl_GetStringFromObj(objv[1], (int*)NULL);

    /*
     *  Create a stub command with the characteristic ItclDeleteStub
     *  procedure.  That way, we can recognize this command later
     *  on as a stub.  Save the cmd token as client data, so we can
     *  get the full name of this command later on.
     */
    cmdPtr = (Command *) Tcl_CreateObjCommand(interp, cmdName,
        ItclHandleStubCmd, (ClientData)NULL,
        (Tcl_CmdDeleteProc*)ItclDeleteStub);

    cmdPtr->objClientData = (ClientData) cmdPtr;

    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_StubExistsCmd()
 *
 *  Invoked by Tcl whenever the user issues a "stub exists" command to
 *  see if an existing command is an autoloading stub.  Handles the
 *  following syntax:
 *
 *    stub exists <name>
 *
 *  Looks for a command called <name> and checks to see if it is an
 *  autoloading stub.  Returns a boolean result.
 * ------------------------------------------------------------------------
 */
int
Itcl_StubExistsCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* not used */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    char *cmdName;
    Tcl_Command cmd;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "name");
        return TCL_ERROR;
    }
    cmdName = Tcl_GetStringFromObj(objv[1], (int*)NULL);

    cmd = Tcl_FindCommand(interp, cmdName, (Tcl_Namespace*)NULL, 0);

    if (cmd != NULL && Itcl_IsStub(cmd)) {
        Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
    } else {
        Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
    }
    return TCL_OK;
}


/*
 * ------------------------------------------------------------------------
 *  Itcl_IsStub()
 *
 *  Checks the given Tcl command to see if it represents an autoloading
 *  stub created by the "stub create" command.  Returns non-zero if
 *  the command is indeed a stub.
 * ------------------------------------------------------------------------
 */
int
Itcl_IsStub(cmd)
    Tcl_Command cmd;         /* command being tested */
{
    Command *cmdPtr = (Command*)cmd;

    /*
     *  This may be an imported command, but don't try to get the
     *  original.  Just check to see if this particular command
     *  is a stub.  If we really want the original command, we'll
     *  find it at a higher level.
     */
    if (cmdPtr->deleteProc == ItclDeleteStub) {
        return 1;
    }
    return 0;
}


/*
 * ------------------------------------------------------------------------
 *  ItclHandleStubCmd()
 *
 *  Invoked by Tcl to handle commands created by "stub create".
 *  Calls "auto_load" with the full name of the current command to
 *  trigger autoloading of the real implementation.  Then, calls the
 *  command to handle its function.  If successful, this command
 *  returns TCL_OK along with the result from the real implementation
 *  of this command.  Otherwise, it returns TCL_ERROR, along with an
 *  error message in the interpreter.
 * ------------------------------------------------------------------------
 */
static int
ItclHandleStubCmd(clientData, interp, objc, objv)
    ClientData clientData;   /* command token for this stub */
    Tcl_Interp *interp;      /* current interpreter */
    int objc;                /* number of arguments */
    Tcl_Obj *CONST objv[];   /* argument objects */
{
    Tcl_Command cmd = (Tcl_Command) clientData;

    int result, loaded;
    char *cmdName;
    int cmdlinec;
    Tcl_Obj **cmdlinev;
    Tcl_Obj *objAutoLoad[2], *objPtr, *cmdNamePtr, *cmdlinePtr;

    cmdNamePtr = Tcl_NewStringObj((char*)NULL, 0);
    Tcl_GetCommandFullName(interp, cmd, cmdNamePtr);
    Tcl_IncrRefCount(cmdNamePtr);
    cmdName = Tcl_GetStringFromObj(cmdNamePtr, (int*)NULL);

    /*
     *  Try to autoload the real command for this stub.
     */
    objAutoLoad[0] = Tcl_NewStringObj("::auto_load", -1);
    Tcl_IncrRefCount(objAutoLoad[0]);
    objAutoLoad[1] = cmdNamePtr;
    Tcl_IncrRefCount(objAutoLoad[1]);

    result = Itcl_EvalArgs(interp, 2, objAutoLoad);

    Tcl_DecrRefCount(objAutoLoad[0]);
    Tcl_DecrRefCount(objAutoLoad[1]);

    if (result != TCL_OK) {
        Tcl_DecrRefCount(cmdNamePtr);
        return TCL_ERROR;
    }

    objPtr = Tcl_GetObjResult(interp);
    result = Tcl_GetIntFromObj(interp, objPtr, &loaded);
    if (result != TCL_OK || !loaded) {
        Tcl_ResetResult(interp);
        Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
            "can't autoload \"", cmdName, "\"", (char*)NULL);
        Tcl_DecrRefCount(cmdNamePtr);
        return TCL_ERROR;
    }

    /*
     *  At this point, the real implementation has been loaded.
     *  Invoke the command again with the arguments passed in.
     */
    cmdlinePtr = Itcl_CreateArgs(interp, cmdName, objc-1, objv+1);

    (void) Tcl_ListObjGetElements((Tcl_Interp*)NULL, cmdlinePtr,
        &cmdlinec, &cmdlinev);

    Tcl_ResetResult(interp);
    result = Itcl_EvalArgs(interp, cmdlinec, cmdlinev);
    Tcl_DecrRefCount(cmdlinePtr);

    return result;
}


/*
 * ------------------------------------------------------------------------
 *  ItclDeleteStub()
 *
 *  Invoked by Tcl whenever a stub command is deleted.  This procedure
 *  does nothing, but its presence identifies a command as a stub.
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
ItclDeleteStub(cdata)
    ClientData cdata;      /* not used */
{
    /* do nothing */
}
