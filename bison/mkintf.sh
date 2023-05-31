#! /bin/sh
# Makes internalized versions of bison.simple and bison.hairy

cat <<!
/* File created from bison.simple via mkintf.sh */

const char *internal_bison_simple[] = {
!

sed 's/\\/&&/g' $1 | sed 's/"/\\"/g' | sed 's/.*/  "&\\n",/'

cat <<!
  0
};

const char *internal_bison_hairy[] = {
!

sed 's/\\/&&/g' $2 | sed 's/"/\\"/g' | sed 's/.*/  "&\\n",/'

cat <<!
  0
};
!
