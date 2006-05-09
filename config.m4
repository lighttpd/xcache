dnl
dnl $Id:$
dnl vim:ts=2:sw=2:expandtab

AC_DEFUN([XCACHE_OPTION], [
  PHP_ARG_ENABLE(xcache-$1, for XCACHE $1,
  [  --enable-xcache-$2    XCACHE: $4], no, no)
  if test "$PHP_$3" = "yes"; then
    xcache_sources="$xcache_sources $1.c"
    AC_DEFINE([HAVE_$3], 1, [Define for XCACHE: $4])
  fi
])dnl

PHP_ARG_ENABLE(xcache, for XCACHE support,
[  --enable-xcache         Include XCACHE support.])

if test "$PHP_XCACHE" != "no"; then
  xcache_sources="processor.c \
                  xcache.c \
                  mmap.c \
                  mem.c \
                  const_string.c \
                  opcode_spec.c \
                  stack.c \
                  utils.c \
                  lock.c \
                  "
  XCACHE_OPTION([optimizer],    [optimizer   ], [XCACHE_OPTIMIZER],    [(N/A)])
  XCACHE_OPTION([coverage],     [coverage    ], [XCACHE_COVERAGE],     [Enable code coverage dumper, NOT for production server])
  XCACHE_OPTION([assembler],    [assembler   ], [XCACHE_ASSEMBLER],    [(N/A)])
  XCACHE_OPTION([disassembler], [disassembler], [XCACHE_DISASSEMBLER], [Enable opcode to php variable dumper, NOT for production server])
  XCACHE_OPTION([encoder],      [encoder     ], [XCACHE_ENCODER],      [(N/A)])
  XCACHE_OPTION([decoder],      [decoder     ], [XCACHE_DECODER],      [(N/A)])

  PHP_NEW_EXTENSION(xcache, $xcache_sources, $ext_shared)
  PHP_ADD_MAKEFILE_FRAGMENT()

  PHP_ARG_ENABLE(xcache-test, for XCACHE self test,
  [  --enable-xcache-test            XCACHE: Enable self test - FOR DEVELOPERS ONLY!!], no, no)
  if test "$PHP_XCACHE_TEST" = "yes"; then
    XCACHE_ENABLE_TEST=-DXCACHE_ENABLE_TEST
    AC_DEFINE([HAVE_XCACHE_TEST], 1, [Define to enable XCACHE self test])
  else
    XCACHE_ENABLE_TEST=
  fi
  PHP_SUBST([XCACHE_ENABLE_TEST])

  AC_PATH_PROGS(INDENT, [indent cat])
  case $INDENT in
  */indent[)]
    XCACHE_INDENT="$INDENT -kr --use-tabs --tab-size 4 -sob -nce"
    ;;
  *[)]
    XCACHE_INDENT=cat
    ;;
  esac
  PHP_SUBST([XCACHE_INDENT])

  XCACHE_PROC_SOURCES=`ls $ac_srcdir/processor/*.m4`
  PHP_SUBST([XCACHE_PROC_SOURCES])
fi
