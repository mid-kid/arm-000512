/* Definitions for switches for C++.
   Copyright (C) 1995, 96-97, 1998 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

DEFINE_LANG_NAME ("C++")
     
/* This is the contribution to the `lang_options' array in gcc.c for
   g++.  */

  { "-faccess-control", "" },
  { "-fno-access-control", "Do not obey access control semantics" },
  { "-fall-virtual", "Make all member functions virtual" },
  { "-fno-all-virtual", "" },
  { "-falt-external-templates", "Change when template instances are emitted" },
  { "-fno-alt-external-templates", "" },
  { "-fansi-overloading", "" },
  { "-fno-ansi-overloading", "" },
  { "-fcheck-new", "Check the return value of new" },
  { "-fno-check-new", "" },
  { "-fconserve-space", "Reduce size of object files" },
  { "-fno-conserve-space", "" },
  { "-fconst-strings", "" },
  { "-fno-const-strings", "Make string literals `char[]' instead of `const char[]'" },
  { "-fdefault-inline", "" },
  { "-fno-default-inline", "Do not inline member functions by default"},
  { "-frtti", "" },
  { "-fno-rtti", "Do not generate run time type descriptor information" },
  { "-felide-constructors", "" },
  { "-fno-elide-constructors", "" },
/* CYGNUS LOCAL Embedded C++ */
  { "-fembedded-cxx", "Implement Embedded C++ specification" },
  { "-fno-embedded-cxx", "" },
/* END CYGNUS LOCAL Embedded C++ */
  { "-fenum-int-equiv", "" },
  { "-fno-enum-int-equiv", "" },
  { "-fexternal-templates", "" },
  { "-fno-external-templates", "" },
  { "-ffor-scope", "" },
  { "-fno-for-scope", "Scope of for-init-statement vars extends outside" },
  { "-fguiding-decls", "Implement guiding declarations" },
  { "-fno-guiding-decls", "" },
  { "-fgnu-keywords", "" },
  { "-fno-gnu-keywords", "Do not recognise GNU defined keywords" },
  { "-fhandle-exceptions", "" },
  { "-fno-handle-exceptions", "" },
  { "-fhandle-signatures", "Handle signature language constructs" },
  { "-fno-handle-signatures", "" },
  { "-fhonor-std", "Treat the namespace `std' as a normal namespace" },
  { "-fno-honor-std", "" },
  { "-fhuge-objects", "Enable support for huge objects" },
  { "-fno-huge-objects", "" },
  { "-fimplement-inlines", "" },
  { "-fno-implement-inlines", "Export functions even if they can be inlined" },
  { "-fimplicit-templates", "" },
  { "-fno-implicit-templates", "Only emit explicit template instatiations" },
  { "-fimplicit-inline-templates", "" },
  { "-fno-implicit-inline-templates", "Only emit explicit instatiations of inline templates" },
  { "-finit-priority", "Handle the init_priority attribute" },
  { "-fno-init-priority", "" },
  { "-flabels-ok", "Labels can be used as first class objects" },
  { "-fno-labels-ok", "" },
  { "-fmemoize-lookups", "" },
  { "-fno-memoize-lookups", "" },
  { "-fname-mangling-version-", "" },
  { "-fnew-abi", "Enable experimental ABI changes" },
  { "-fno-new-abi", "" },
  { "-fnonnull-objects", "" },
  { "-fno-nonnull-objects", "Do not assume that a reference is always valid" },
  { "-foperator-names", "Recognise and/bitand/bitor/compl/not/or/xor" },
  { "-fno-operator-names", "" },
  { "-foptional-diags", "" },
  { "-fno-optional-diags", "Disable optional diagnostics" },
  { "-fpermissive", "Downgrade conformance errors to warnings" },
  { "-fno-permissive", "" },
  { "-frepo", "Enable automatic template instantiation" },
  { "-fno-repo", "" },
  { "-fsave-memoized", "" },
  { "-fno-save-memoized", "" },
  { "-fsquangle", "Enable squashed name mangling" },
  { "-fno-squangle", "" },
  { "-fstats", "Display statistics accumulated during compilation" },
  { "-fno-stats", "" },
  { "-fstrict-prototype", "" },
  { "-fno-strict-prototype", "Do not assume that empty prototype means no args" },
  { "-ftemplate-depth-", "Specify maximum template instantiation depth"},
  { "-fthis-is-variable", "Make 'this' not be type '* const'"  },
  { "-fno-this-is-variable", "" },
  { "-fvtable-gc", "Discard unused virtual functions" },
  { "-fno-vtable-gc", "" },
  { "-fvtable-thunks", "Implement vtables using thunks" },
  { "-fno-vtable-thunks", "" },
  { "-fweak", "Emit common-like symbols as weak symbols" },
  { "-fno-weak", "" },
  { "-fxref", "Emit cross referencing information" },
  { "-fno-xref", "" },

  { "-Wreturn-type", "Warn about inconsistent return types" },
  { "-Wno-return-type", "" },
  { "-Woverloaded-virtual", "Warn about overloaded virtual function names" },
  { "-Wno-overloaded-virtual", "" },
  { "-Wctor-dtor-privacy", "" },
  { "-Wno-ctor-dtor-privacy", "Don't warn when all ctors/dtors are private" },
  { "-Wnon-virtual-dtor", "Warn about non virtual destructors" },
  { "-Wno-non-virtual-dtor", "" },
  { "-Wextern-inline", "Warn when a function is declared extern, then inline" },
  { "-Wno-extern-inline", "" },
  { "-Wreorder", "Warn when the compiler reorders code" },
  { "-Wno-reorder", "" },
  { "-Wsynth", "Warn when synthesis behaviour differs from Cfront" },
  { "-Wno-synth", "" },
  { "-Wpmf-conversions", "" },
  { "-Wno-pmf-conversions", "Don't warn when type converting pointers to member functions" },
  { "-Weffc++", "Warn about violations of Effective C++ style rules" },
  { "-Wno-effc++", "" },
  { "-Wsign-promo", "Warn when overload promotes from unsigned to signed" },
  { "-Wno-sign-promo", "" },
  { "-Wold-style-cast", "Warn if a C style cast is used in a program" },
  { "-Wno-old-style-cast", "" },
  { "-Wnon-template-friend", "" }, 
  { "-Wno-non-template-friend", "Don't warn when non-templatized friend functions are declared within a template" },

