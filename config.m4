dnl
dnl $Id:$
dnl vim:ts=2:sw=2:expandtab

AC_DEFUN([XCACHE_OPTION], [
  PHP_ARG_ENABLE(xcache-$1, for XCACHE $1,
  [  --enable-xcache-$2    XCACHE: $4], no, no)
  if test "$PHP_$3" = "yes"; then
    xcache_sources="$xcache_sources $1.c"
    HAVE_$3=1
    AC_DEFINE([HAVE_$3], 1, [Define for XCACHE: $4])
  else
    HAVE_$3=
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

  dnl $ac_srcdir etc require PHP_NEW_EXTENSION
  XCACHE_PROC_SOURCES=`ls $ac_srcdir/processor/*.m4`
  PHP_SUBST([XCACHE_PROC_SOURCES])

  AC_MSG_CHECKING(if you have opcode_spec_def.h for xcache)
  if test -e "$ac_srcdir/opcode_spec_def.h" ; then
    AC_DEFINE([HAVE_XCACHE_OPCODE_SPEC_DEF], 1, [Define if you have opcode_spec_def.h for xcache])
    AC_MSG_RESULT(yes)
  else
    dnl check for features depend on opcode_spec_def.h
    AC_MSG_RESULT(no)
    define([ERROR], [
      AC_MSG_ERROR([cannot build with $1, $ac_srcdir/opcode_spec_def.h required])
    ])
    if test "$HAVE_XCACHE_DISASSEMBLER" = "1" ; then
      ERROR(disassembler)
    fi
    undefine([ERROR])
  fi
fi
