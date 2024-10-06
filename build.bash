#!/bin/bash
# This build file is far from perfect from a dependancy standpoint,
# but it is useful when make is not present.

#set -o xtrace
#set -o noexec

NAME=vis

CXX=g++
INCS=
DEBUG= #-g
OS=LINUX
DEFINES= #-DUSE_REGEX
CXXFLAGS="-c -O ${DEBUG} -D${OS} ${DEFINES}"
LIBS=-lm
LIB_PATHS=

DOT_O_DIR=OBJS/$OS
DEPS_DIR=DEPS/$OS
PP_DIR=PP/$OS

CLASS_DIR=classes_fx

FILES='ChangeHist Console_Unix Cover_Array Diff FileBuf
       Highlight_Base Highlight_Bash Highlight_BufferEditor
       Highlight_CPP Highlight_Code Highlight_Dir Highlight_Go
       Highlight_HTML Highlight_IDL Highlight_JS Highlight_Java
       Highlight_Make Highlight_MIB Highlight_CMake Highlight_ODB
       Highlight_Python Highlight_SQL Highlight_STL Highlight_Swift
       Highlight_TCL Highlight_Text Highlight_XML Key Line LineView
       MemCheck MemLog Shell String Types Utilities View Vis'

DOT_O_FILES=

function populate_DOT_O_FILES
{
  for file in $FILES; do
    DOT_O_FILES+="$DOT_O_DIR/$file.o "
  done
}

# Running without arguments will make without cleaning
clean=false
make=true
preproc=false

function usage
{
  echo "usage: $0 [clean] [make] [preproc]"

  make=false
}

function run_cmd
{
  echo "$*"
  $*
  es=$?
}

function do_clean
{
  run_cmd \rm -rf $DOT_O_DIR
  run_cmd \rm -rf $DEPS_DIR
  run_cmd \rm -rf $PP_DIR
  run_cmd \rm $NAME
}

function do_make
{
  if [ ! -d $DOT_O_DIR ]; then
    run_cmd mkdir -p $DOT_O_DIR
    if [ $es -ne 0 ]; then
      return
    fi
  fi

  recompiled_file=false

  for file in $FILES; do
    if [ ! -e $DOT_O_DIR/$file.o ] ||
       [ $file.cc -nt $DOT_O_DIR/$file.o ] ||
       [ $file.hh -nt $DOT_O_DIR/$file.o ]
    then
      run_cmd $CXX $INCS $CXXFLAGS $file.cc -o $DOT_O_DIR/$file.o
      if [ $es -ne 0 ]; then
        return
      fi
      recompiled_file=true
    fi
  done

  populate_DOT_O_FILES

  if [ ! -e $NAME ] || [ $recompiled_file = true ]
  then
    run_cmd $CXX -o $NAME $DOT_O_FILES $LIBS $LIB_PATHS
    if [ $es -ne 0 ]; then
      return
    fi
  fi

  echo "Done making $NAME"
}

function do_preproc
{
  if [ ! -d $PP_DIR ]; then
    run_cmd mkdir -p $PP_DIR
    if [ $es -ne 0 ]; then
      return
    fi
  fi

  recompiled_file=false

  for file in $FILES; do
    if [ ! -e $PP_DIR/$file.pp.cc ] ||
       [ $file.cc -nt $PP_DIR/$file.pp.cc ] ||
       [ $file.hh -nt $PP_DIR/$file.pp.cc ]
    then
      run_cmd $CXX -E $INCS $CXXFLAGS $file.cc -o $PP_DIR/$file.pp.cc
      if [ $es -ne 0 ]; then
        return
      fi
      recompiled_file=true
    fi
  done
}

if [ $# -gt 2 ]; then
  # Too many arguments
  usage
elif [ $# -eq 1 ]; then
  # Checkout for clean, make or preproc
  if [ $1 = clean ]; then
    clean=true
    make=false
  elif [ $1 = make ]; then
    make=true
  elif [ $1 = preproc ]; then
    make=false
    preproc=true
  else
    usage
  fi
elif [ $# -eq 2 ]; then
  if [ $1 = clean ] && [ $2 = make ]; then
    clean=true
    make=true
  elif [ $1 = make ] && [ $2 = clean ]; then
    clean=true
    make=true
  else
    usage
  fi
fi

if [ $clean = true ]; then
  do_clean
fi

if [ $make = true ]; then
  do_make
fi

if [ $preproc = true ]; then
  do_preproc
fi

