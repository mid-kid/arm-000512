#!/bin/sh
### quick sanity test for patch.
###
### This file was written and is maintained by K. Richard Pixley,
### rich@cygnus.com.

### fail on errors
set -e

### first arg is directory in which binaries to be tested reside.
case "$1" in
"") BIN=. ;;
*)  BIN="$1" ;;
esac

### -u files.

${BIN}/patch > /dev/null 2>&1 <<'EOF'
--- /dev/null	Fri May  1 16:40:23 1992
+++ testfile	Fri May  1 16:43:33 1992
@@ -0,0 +1,5 @@
+This
+is
+a
+small
+testfile.
--- testfile.~1~	Fri May  1 16:43:33 1992
+++ testfile	Fri May  1 16:46:54 1992
@@ -1,3 +1,5 @@
+addition at
+head of:
 This
 is
 a
--- testfile.old	Fri May  1 16:54:25 1992
+++ testfile	Fri May  1 16:53:53 1992
@@ -5,3 +5,5 @@
 a
 small
 testfile.
+addition at tail
+of file.
--- testfile.old	Fri May  1 16:55:28 1992
+++ testfile	Fri May  1 16:55:39 1992
@@ -3,6 +3,7 @@
 This
 is
 a
+addition in middle of file.
 small
 testfile.
 addition at tail
--- testfile.old	Fri May  1 16:56:19 1992
+++ testfile	Fri May  1 16:56:34 1992
@@ -1,6 +1,3 @@
-addition at
-head of:
-This
 is
 a
 addition in middle of file.
--- testfile.old	Fri May  1 16:57:22 1992
+++ testfile	Fri May  1 16:57:30 1992
@@ -1,7 +0,0 @@
-is
-a
-addition in middle of file.
-small
-testfile.
-addition at tail
-of file.
EOF

test ! -s testfile
rm testfile

### -c files

${BIN}/patch > /dev/null 2>&1 <<'EOF'
*** /dev/null	Fri May  1 16:43:57 1992
--- testfile	Fri May  1 16:43:33 1992
***************
*** 0 ****
--- 1,5 ----
+ This
+ is
+ a
+ small
+ testfile.
*** testfile.~1~	Fri May  1 16:43:33 1992
--- testfile	Fri May  1 16:46:54 1992
***************
*** 1,3 ****
--- 1,5 ----
+ addition at
+ head of:
  This
  is
  a
*** testfile.old	Fri May  1 16:54:25 1992
--- testfile	Fri May  1 16:53:53 1992
***************
*** 5,7 ****
--- 5,9 ----
  a
  small
  testfile.
+ addition at tail
+ of file.
*** testfile.old	Fri May  1 16:55:28 1992
--- testfile	Fri May  1 16:55:39 1992
***************
*** 3,8 ****
--- 3,9 ----
  This
  is
  a
+ addition in middle of file.
  small
  testfile.
  addition at tail
*** testfile.old	Fri May  1 16:56:19 1992
--- testfile	Fri May  1 16:56:34 1992
***************
*** 1,6 ****
- addition at
- head of:
- This
  is
  a
  addition in middle of file.
--- 1,3 ----
*** testfile.old	Fri May  1 16:57:22 1992
--- testfile	Fri May  1 16:57:30 1992
***************
*** 1,7 ****
- is
- a
- addition in middle of file.
- small
- testfile.
- addition at tail
- of file.
--- 0 ----
EOF

test ! -s testfile

### old style diffs.
${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
0a1,5
> This
> is
> a
> small
> testfile.
EOF

${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
0a1,2
> addition at
> head of:
EOF

${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
7a8,9
> addition at tail
> of file.
EOF

${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
5a6
> addition in middle of file.
EOF

${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
1,3d0
< addition at
< head of:
< This
EOF

${BIN}/patch testfile > /dev/null 2>&1 <<'EOF'
1,7d0
< is
< a
< addition in middle of file.
< small
< testfile.
< addition at tail
< of file.
EOF

test ! -s testfile
rm testfile

exit 0
