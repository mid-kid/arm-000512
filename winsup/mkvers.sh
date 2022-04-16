#!/bin/sh
# mkvers.sh - Make version information for cygwin DLL
#
#   Copyright 1998, 1999 Cygnus Solutions.
#
# This file is part of Cygwin.
#
# This software is a copyrighted work licensed under the terms of the
# Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
# details.

#
# Arg 1 is the name of the version include file
#
incfile=$1
#
# Load the current date so we can work on individual fields
#
build_date=`date`
set -$- $build_date
#
# Translate the month into a number
#
case "$2" in
    Jan) m=1 ;;
    Feb) m=2 ;;
    Mar) m=3 ;;
    Apr) m=4 ;;
    May) m=5 ;;
    Jun) m=6 ;;
    Jul) m=7 ;;
    Aug) m=8 ;;
    Sep) m=9 ;;
    Oct) m=10 ;;
    Nov) m=11 ;;
    Dec) m=12 ;;
esac

#
# Set date into YYYY-MM-DD HH:MM:SS format
#
builddate="${6-$5}-$m-$3 $4"

set -$- ''

#
# Output the initial part of version.cc
#
cat <<EOF
#include <winsup.h>

#define strval(x) #x
#define str(x) strval(x)
#define shared_data_version str(CYGWIN_VERSION_SHARED_DATA)

const char *cygwin_version_strings =
  "BEGIN_CYGWIN_VERSION_INFO\n"
EOF

#
# Split version file into dir and filename components
#
dir=`dirname $incfile`
fn=`basename $incfile`

#
# Look in the include file CVS directory for a CVS Tag file.  This file,
# if it exists, will contain the name of the sticky tag associated with
# the current build.  Save that for output later.
#
cvs_tag="`sed 's%^.\(.*\)%\1%' $dir/CVS/Tag 2>/dev/null`"

[ -n "$cvs_tag" ] && cvs_tag=" CVS tag"'
'"$cvs_tag"

#
# Look in the source directory containing the include/cygwin/version.h
# file for a ".snapshot-date" file.  If one is found then this information
# will be saved for output to the DLL.
#
dir=`echo $dir | sed 's%/include/cygwin.*$%%'`
if [ -r "$dir/.snapshot-date" ]; then
    read snapshot < "$dir/.snapshot-date"
    snapshot="snapshot date
$snapshot"
fi

#
# Scan the version.h file for strings that begin with CYGWIN_INFO or
# CYGWIN_VERSION.  Perform crude parsing on the lines to get the values
# associated with these values and then pipe it into a while loop which
# outputs these values in C palatable format for inclusion in the DLL
# with a '%%% ' identifier that will introduce "interesting strings.
# These strings are strictly for use by a user to scan the DLL for
# interesting information.
#
(sed -n -e 's%#define CYGWIN_\(INFO\|VERSION\)_\([A-Z_]*\)[ 	][ 	]*\([a-zA-Z0-9"][^/]*\).*%_\2\
\3%p' $incfile | sed -e 's/["\\]//g'  -e '/^_/y/ABCDEFGHIJKLMNOPQRSTUVWXYZ_/abcdefghijklmnopqrstuvwxyz /';
echo ' build date'; echo $build_date; [ -n "$cvs_tag" ] && echo "$cvs_tag";\
[ -n "$snapshot" ] && echo "$snapshot"
) | while read var; do
    read val
cat <<EOF
  "%%% Cygwin $var: $val\n"
EOF
done

#
# Finally, output the shared ID and set up the cygwin_version structure
# for use by Cygwin itself.
#
cat <<EOF
#ifdef DEBUGGING
  "%%% Cygwin shared id: " CYGWIN_VERSION_DLL_IDENTIFIER "S" shared_data_version "-$builddate\n"
#else
  "%%% Cygwin shared id: " CYGWIN_VERSION_DLL_IDENTIFIER "S" shared_data_version "\n"
#endif
  "END_CYGWIN_VERSION_INFO\n\0";

cygwin_version_info cygwin_version =
{
  CYGWIN_VERSION_API_MAJOR, CYGWIN_VERSION_API_MINOR,
  CYGWIN_VERSION_DLL_MAJOR, CYGWIN_VERSION_DLL_MINOR,
  CYGWIN_VERSION_SHARED_DATA,
  CYGWIN_VERSION_MOUNT_REGISTRY,
  "$builddate",
#ifdef DEBUGGING
  CYGWIN_VERSION_DLL_IDENTIFIER "S" shared_data_version "-$builddate"
#else
  CYGWIN_VERSION_DLL_IDENTIFIER "S" shared_data_version
#endif
};
EOF
