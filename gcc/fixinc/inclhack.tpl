[= autogen template -*- Mode: ksh -*-
sh
#
#  $Id: inclhack.tpl,v 1.2 1999/01/13 22:56:22 law Exp $
#
=]
#!/bin/sh
#
[= _EVAL "## " _DNE =]
#
# Install modified versions of certain ANSI-incompatible system header
# files which are fixed to work correctly with ANSI C and placed in a
# directory that GNU C will search.
#
# This script contains [=_eval fix _count =] fixup scripts.
#
# See README-fixinc for more information.
#
[=_eval fixincludes "## " _gpl=]
#
# # # # # # # # # # # # # # # # # # # # #
#
# Directory in which to store the results.
# Fail if no arg to specify a directory for the output.
if [ "x$1" = "x" ]
then echo fixincludes: no output directory specified
exit 1
fi

LIB=${1}
shift

# Make sure it exists.
if [ ! -d $LIB ]; then
  mkdir $LIB || {
    echo fixincludes:  output dir '`'$LIB"' cannot be created"
    exit 1
  }
else
  ( \cd $LIB && touch DONE && rm DONE ) || {
    echo fixincludes:  output dir '`'$LIB"' is an invalid directory"
    exit 1
  }
fi

# Define what target system we're fixing.
#
if test -r ./Makefile; then
  target_canonical="`sed -n -e 's,^target[ 	]*=[ 	]*\(.*\)$,\1,p' < Makefile`"
fi

# If not from the Makefile, then try config.guess
#
if test -z "${target_canonical}" ; then
  if test -x ./config.guess ; then
    target_canonical="`config.guess`" ; fi
  test -z "${target_canonical}" && target_canonical=unknown
fi
export target_canonical

# # # # # # # # # # # # # # # # # # # # #
#
# Define PWDCMD as a command to use to get the working dir
# in the form that we want.
PWDCMD=pwd

case "`$PWDCMD`" in
//*)
    # On an Apollo, discard everything before `/usr'.
    PWDCMD="eval pwd | sed -e 's,.*/usr/,/usr/,'"
    ;;
esac

# Original directory.
ORIGDIR=`${PWDCMD}`[=
_IF PROGRAM _env =]
FIXINCL=${ORIGDIR}/fixincl
export FIXINCL[=
_ENDIF=]

# Make LIB absolute only if needed to avoid problems with the amd.
case $LIB in
/*)
    ;;
*)
    cd $LIB; LIB=`${PWDCMD}`
    ;;
esac

echo Fixing headers into ${LIB} for ${target_canonical} target
 
# Determine whether this system has symbolic links.
if ln -s X $LIB/ShouldNotExist 2>/dev/null; then
  rm -f $LIB/ShouldNotExist
  LINKS=true
elif ln -s X /tmp/ShouldNotExist 2>/dev/null; then
  rm -f /tmp/ShouldNotExist
  LINKS=true
else
  LINKS=false
fi
 
# # # # # # # # # # # # # # # # # # # # #
#
#  Search each input directory for broken header files.
#  This loop ends near the end of the file.
#
if test $# -eq 0
then
    INPUTLIST="/usr/include"
else
    INPUTLIST="$@"
fi

for INPUT in ${INPUTLIST} ; do

cd ${ORIGDIR}

cd ${INPUT} || {
  echo 'fixincludes:  input dir `'$INPUT"' is an invalid directory"
  exit 1
}

#
# # # # # # # # # # # # # # # # # # # # #
#
echo Finding directories and links to directories

# Find all directories and all symlinks that point to directories.
# Put the list in $files.
# Each time we find a symlink, add it to newdirs
# so that we do another find within the dir the link points to.
# Note that $files may have duplicates in it;
# later parts of this file are supposed to ignore them.
dirs="."
levels=2
while [ -n "$dirs" ] && [ $levels -gt 0 ]
do
    levels=`expr $levels - 1`
    newdirs=
    for d in $dirs
    do
    echo " Searching $INPUT/$d"
 
    # Find all directories under $d, relative to $d, excluding $d itself.
    # (The /. is needed after $d in case $d is a symlink.)
    files="$files `find $d/. -type d -print | \
               sed -e '/\/\.$/d' -e 's@/./@/@g'`"
    # Find all links to directories.
    # Using `-exec test -d' in find fails on some systems,
    # and trying to run test via sh fails on others,
    # so this is the simplest alternative left.
    # First find all the links, then test each one.
    theselinks=
    $LINKS && \
      theselinks=`find $d/. -type l -print | sed -e 's@/./@/@g'`
    for d1 in $theselinks --dummy--
    do
        # If the link points to a directory,
        # add that dir to $newdirs
        if [ -d $d1 ]
        then
        files="$files $d1"
        if [ "`ls -ld $d1 | sed -n 's/.*-> //p'`" != "." ]
        then
            newdirs="$newdirs $d1"
        fi
        fi
    done
    done
 
    dirs="$newdirs"
done
 
# # # # # # # # # # # # # # # # # # # # #
#
dirs=
echo "All directories (including links to directories):"
echo $files
 
for file in $files; do
  rm -rf $LIB/$file
  if [ ! -d $LIB/$file ]
  then mkdir $LIB/$file
  fi
done
mkdir $LIB/root
 
# # # # # # # # # # # # # # # # # # # # #
#
# treetops gets an alternating list
# of old directories to copy
# and the new directories to copy to.
treetops="${INPUT} ${LIB}"
 
if $LINKS; then
  echo 'Making symbolic directory links'
  for file in $files; do
    dest=`ls -ld $file | sed -n 's/.*-> //p'`
    if [ "$dest" ]; then
      cwd=`${PWDCMD}`
      # In case $dest is relative, get to $file's dir first.
      cd ${INPUT}
      cd `echo ./$file | sed -n 's&[^/]*$&&p'`
      # Check that the target directory exists.
      # Redirections changed to avoid bug in sh on Ultrix.
      (cd $dest) > /dev/null 2>&1
      if [ $? = 0 ]; then
    cd $dest
    # X gets the dir that the link actually leads to.
    x=`${PWDCMD}`
    # Canonicalize ${INPUT} now to minimize the time an
    # automounter has to change the result of ${PWDCMD}.
    cinput=`cd ${INPUT}; ${PWDCMD}`
    # If a link points to ., make a similar link to .
    if [ $x = ${cinput} ]; then
      echo $file '->' . ': Making link'
      rm -fr ${LIB}/$file > /dev/null 2>&1
      ln -s . ${LIB}/$file > /dev/null 2>&1
    # If link leads back into ${INPUT},
    # make a similar link here.
    elif expr $x : "${cinput}/.*" > /dev/null; then
      # Y gets the actual target dir name, relative to ${INPUT}.
      y=`echo $x | sed -n "s&${cinput}/&&p"`
      # DOTS is the relative path from ${LIB}/$file's dir back to ${LIB}.
      dots=`echo "$file" |
        sed -e 's@^./@@' -e 's@/./@/@g' -e 's@[^/][^/]*@..@g' -e 's@..$@@'`
      echo $file '->' $dots$y ': Making link'
      rm -fr ${LIB}/$file > /dev/null 2>&1
      ln -s $dots$y ${LIB}/$file > /dev/null 2>&1
    else
      # If the link is to a dir $target outside ${INPUT},
      # repoint the link at ${INPUT}/root$target
      # and process $target into ${INPUT}/root$target
      # treat this directory as if it actually contained the files.
      echo $file '->' root$x ': Making link'
      if [ -d $LIB/root$x ]
      then true
      else
        dirname=root$x/
        dirmade=.
        cd $LIB
        while [ x$dirname != x ]; do
          component=`echo $dirname | sed -e 's|/.*$||'`
          mkdir $component >/dev/null 2>&1
          cd $component
          dirmade=$dirmade/$component
          dirname=`echo $dirname | sed -e 's|[^/]*/||'`
        done
      fi
      # Duplicate directory structure created in ${LIB}/$file in new
      # root area.
      for file2 in $files; do
        case $file2 in
          $file/*)
        dupdir=${LIB}/root$x/`echo $file2 | sed -n "s|^${file}/||p"`
        echo "Duplicating ${file}'s ${dupdir}"
        if [ -d ${dupdir} ]
        then true
        else
          mkdir ${dupdir}
        fi
        ;;
          *)
        ;;
        esac
          done
      # Get the path from ${LIB} to $file, accounting for symlinks.
      parent=`echo "$file" | sed -e 's@/[^/]*$@@'`
      libabs=`cd ${LIB}; ${PWDCMD}`
      file2=`cd ${LIB}; cd $parent; ${PWDCMD} | sed -e "s@^${libabs}@@"`
      # DOTS is the relative path from ${LIB}/$file's dir back to ${LIB}.
      dots=`echo "$file2" | sed -e 's@/[^/]*@../@g'`
      rm -fr ${LIB}/$file > /dev/null 2>&1
      ln -s ${dots}root$x ${LIB}/$file > /dev/null 2>&1
      treetops="$treetops $x ${LIB}/root$x"
    fi
      fi
      cd $cwd
    fi
  done
fi
 
# # # # # # # # # # # # # # # # # # # # #
#
required=
set x $treetops
shift
while [ $# != 0 ]; do
  # $1 is an old directory to copy, and $2 is the new directory to copy to.
  #
  SRCDIR=`cd ${INPUT} ; cd $1 ; ${PWDCMD}`
  export SRCDIR
  shift

  DESTDIR=`cd $1;${PWDCMD}`
  export DESTDIR
  shift

  # The same dir can appear more than once in treetops.
  # There's no need to scan it more than once.
  #
  if [ -f ${DESTDIR}/DONE ]
  then continue ; fi

  touch ${DESTDIR}/DONE
  echo Fixing directory ${SRCDIR} into ${DESTDIR}

  # Check .h files which are symlinks as well as those which are files.
  # A link to a header file will not be processed by anything but this.
  #
  cd ${SRCDIR}
[=_IF PROGRAM _env ! =][=

    _include hackshell =][=

  _ELSE

=]
  required="$required `if $LINKS; then
    find . -name '*.h' \( -type f -o -type l \) -print
  else
    find . -name '*.h' -type f -print
  fi | ${FIXINCL}`"[=


  _ENDIF =]
done

## Make sure that any include files referenced using double quotes
## exist in the fixed directory.  This comes last since otherwise
## we might end up deleting some of these files "because they don't
## need any change."
set x `echo $required`
shift
while [ $# != 0 ]; do
  newreq=
  while [ $# != 0 ]; do
    # $1 is the directory to copy from,
    # $2 is the unfixed file,
    # $3 is the fixed file name.
    #
    cd ${INPUT}
    cd $1
    if [ -r $2 ] && [ ! -r $3 ]; then
      cp $2 $3 >/dev/null 2>&1 || echo "Can't copy $2" >&2
      chmod +w $3 2>/dev/null
      chmod a+r $3 2>/dev/null
      echo Copied $2
      for include in `egrep '^[ 	]*#[ 	]*include[ 	]*"[^/]' $3 |
             sed -e 's/^[ 	]*#[ 	]*include[ 	]*"\([^"]*\)".*$/\1/'`
      do
	dir=`echo $2 | sed -e s'|/[^/]*$||'`
	dir2=`echo $3 | sed -e s'|/[^/]*$||'`
	newreq="$newreq $1 $dir/$include $dir2/$include"
      done
    fi
    shift; shift; shift
  done
  set x $newreq
  shift
done

echo 'Cleaning up DONE files.'
cd $LIB
find . -name DONE -exec rm -f '{}' ';'

echo 'Removing unneeded directories:'
cd $LIB
files=`find . -type d -print | sort -r`
for file in $files; do
  rmdir $LIB/$file > /dev/null 2>&1 | :
done

# # # # # # # # # # # # # # # # # # # # #
#
# End of for INPUT directories
#
done
#
# # # # # # # # # # # # # # # # # # # # #

cd $ORIGDIR
rm -f include/assert.h
cp ${EGCS_SRCDIR}/assert.h include/assert.h
chmod a+r include/assert.h
[=

#  Make the output file executable
# =][=
_eval _outfile "chmod +x %s" _printf _shell=]
