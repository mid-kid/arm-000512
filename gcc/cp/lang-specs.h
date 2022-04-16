/* Definitions for specs for C++.
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

/* This is the contribution to the `default_compilers' array in gcc.c for
   g++.  */

  {".cc", {"@c++"}},
  {".cp", {"@c++"}},
  {".cxx", {"@c++"}},
  {".cpp", {"@c++"}},
  {".c++", {"@c++"}},
  {".C", {"@c++"}},
  {"@c++",
#if USE_CPPLIB
/* CYGNUS LOCAL Embedded C++ */
   {
     "%{E|M|MM:cpp -lang-c++ %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C++ does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
	-undef -D__GNUC__=%v1 -D__GNUG__=%v1 -D__cplusplus -D__GNUC_MINOR__=%v2\
	%{ansi:-trigraphs -D__STRICT_ANSI__} %{!undef:%{!ansi:%p} %P}\
	%{!fno-exceptions:-D__EXCEPTIONS}\
	%{fembedded-cxx:-D__EMBEDDED_CXX__} \
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}} %{trigraphs}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n}\
      %{!E:%{!M:%{!MM:cc1plus %i %1 %2\
                            -lang-c++ %{nostdinc*} %{C} %{A*} %{I*} %{P} %I\
                            %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
                            -undef -D__GNUC__=%v1 -D__GNUG__=%v1 -D__cplusplus\
                            -D__GNUC_MINOR__=%v2\
                            %{ansi:-trigraphs -D__STRICT_ANSI__} %{!undef:%{!ansi:%p} %P}\
                            %{!fno-exceptions:-D__EXCEPTIONS}\
                            %{fembedded-cxx:-D__EMBEDDED_CXX__} \
                            %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}}\
                            %{trigraphs}\
			    %{!Q:-quiet} -dumpbase %b.cc %{d*} %{m*} %{a}\
			    %{g*} %{O*} %{W*} %{w} %{pedantic*} %{ansi}\
                            %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
			    %{v:-version} %{pg:-p} %{p}\
			    %{f*} %{+e*} %{aux-info*}\
			    %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
			    %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}}|\n\
              %{!S:as %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
                      %{!pipe:%g.s} %A\n }}}}"}},
/* END CYGNUS LOCAL Embedded C++ */
#else /* ! USE_CPPLIB */
/* CYGNUS LOCAL Embedded C++ */
   {"cpp -lang-c++ %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C++ does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
	-undef -D__GNUC__=%v1 -D__GNUG__=%v1 -D__cplusplus -D__GNUC_MINOR__=%v2\
	%{ansi:-trigraphs -D__STRICT_ANSI__} %{!undef:%{!ansi:%p} %P}\
	%{!fno-exceptions:-D__EXCEPTIONS}\
	%{fembedded-cxx:-D__EMBEDDED_CXX__} \
        %c %{Os:-D__OPTIMIZE_SIZE__} %{O*:%{!O0:-D__OPTIMIZE__}} %{trigraphs}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*} %Z\
        %i %{!M:%{!MM:%{!E:%{!pipe:%g.ii}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",
/* END CYGNUS LOCAL Embedded C++ */
     "%{!M:%{!MM:%{!E:cc1plus %{!pipe:%g.ii} %1 %2\
			    %{!Q:-quiet} -dumpbase %b.cc %{d*} %{m*} %{a}\
			    %{g*} %{O*} %{W*} %{w} %{pedantic*} %{ansi}\
			    %{v:-version} %{pg:-p} %{p}\
			    %{f*} %{+e*} %{aux-info*}\
			    %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
			    %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}}|\n\
              %{!S:as %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
                      %{!pipe:%g.s} %A\n }}}}"}},
#endif /* ! USE_CPPLIB */
  {".ii", {"@c++-cpp-output"}},
  {"@c++-cpp-output",
   {"%{!M:%{!MM:%{!E:cc1plus %i %1 %2 %{!Q:-quiet} %{d*} %{m*} %{a}\
			    %{g*} %{O*} %{W*} %{w} %{pedantic*} %{ansi}\
			    %{v:-version} %{pg:-p} %{p}\
			    %{f*} %{+e*} %{aux-info*}\
			    %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
			    %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
	            %{!S:as %a %Y\
			    %{c:%W{o*}%{!o*:-o %w%b%O}}%{!c:-o %d%w%u%O}\
			    %{!pipe:%g.s} %A\n }}}}"}},
