
DEFINES =
CCX  = g++
CXXFLAGS = -c -O -g -DOSX $(DEFINES)
#CXXFLAGS = -c -O -g -DLINUX $(DEFINES)
#CXXFLAGS = -c -O -g -DWIN32 $(DEFINES)
#CXXFLAGS = -c -O -g -DSUNOS $(DEFINES)

LIBS = -lm
LIB_PATHS =
INCS =
NAME = vis
DOT_O_DIR = OBJS/osx
DEPS_DIR  = DEPS/osx
PP_DIR    = PP/osx

#DOT_O_DIR = OBJS/linux
#DEPS_DIR  = DEPS/linux
#PP_DIR    = PP/linux

#DOT_O_DIR = OBJS/win32
#DEPS_DIR  = DEPS/win32
#PP_DIR    = PP/win32

#DOT_O_DIR = OBJS/sunos
#DEPS_DIR  = DEPS/sunos
#PP_DIR    = PP/sunos

SOURCES = ChangeHist \
          Colon \
          Console \
          Cover_Array \
          Diff \
          FileBuf \
          Highlight_Base \
          Highlight_Bash \
          Highlight_Code \
          Highlight_CPP \
          Highlight_HTML \
          Highlight_IDL \
          Highlight_Java \
          Highlight_JS \
          Highlight_ODB \
          Highlight_SQL \
          Highlight_STL \
          Highlight_Swift \
          Highlight_TCL \
          Highlight_Text \
          Highlight_XML \
          Key \
          Line \
          MemCheck \
          MemLog \
          String \
          Types \
          Utilities \
          View \
          Vis

DOT_O_FILES   = $(addprefix $(DOT_O_DIR)/,$(addsuffix .o,$(SOURCES)))
DOT_DEP_FILES = $(addprefix $(DEPS_DIR)/,$(addsuffix .dep,$(SOURCES)))
PREPROC_FILES = $(addprefix $(PP_DIR)/,$(addsuffix .pp.cc,$(SOURCES)))

all: $(NAME)

.PHONY: clean install tar
clean:
	-rm -r $(DOT_O_DIR)
	-rm -r $(DEPS_DIR)
	-rm -r $(PP_DIR)

$(NAME): $(DOT_O_DIR) $(DOT_O_FILES)
	$(CCX) -o $(NAME) $(DOT_O_FILES) $(LIBS) $(LIB_PATHS)
	echo Done making $(NAME)

install:
	@echo "Add your own install command to the Makefile"

preproc: $(PREPROC_FILES)

$(DOT_O_DIR):; mkdir -p $(DOT_O_DIR)
$(DEPS_DIR) :; mkdir -p $(DEPS_DIR)
$(PP_DIR)   :; mkdir -p $(PP_DIR)

$(DOT_O_FILES): $(DOT_O_DIR)/%.o: %.cc
	$(CCX) $(INCS) $(CXXFLAGS) $< -o $@

$(DOT_DEP_FILES): $(DEPS_DIR)/%.dep: %.cc $(DEPS_DIR)
	$(CCX) -MM $< $(INCS) $(CXXFLAGS) | sed '1 s|^|$(DOT_O_DIR)/|' > $@

$(PP_DIR)/%.pp.cc : %.cc $(PP_DIR)
	$(CCX) -E $(INCS) $(CXXFLAGS) $< > $@

-include $(DOT_DEP_FILES)

tar:
	tar cf vis.tar Makefile \
                Array_t.hh \
                ChangeHist.cc \
                ChangeHist.hh \
                Colon.cc \
                Colon.hh \
                Console.cc \
                Console.hh \
                Console_Unix.cc \
                Console_Win32.cc \
                Cover_Array.cc \
                Cover_Array.hh \
                Diff.cc \
                Diff.hh \
                FileBuf.cc \
                FileBuf.hh \
                Help.hh \
                Highlight_Base.cc \
                Highlight_Base.hh \
                Highlight_Bash.cc \
                Highlight_Bash.hh \
                Highlight_CPP.cc \
                Highlight_CPP.hh \
                Highlight_Code.cc \
                Highlight_Code.hh \
                Highlight_HTML.cc \
                Highlight_HTML.hh \
                Highlight_IDL.cc \
                Highlight_IDL.hh \
                Highlight_JS.cc \
                Highlight_JS.hh \
                Highlight_Java.cc \
                Highlight_Java.hh \
                Highlight_ODB.cc \
                Highlight_ODB.hh \
                Highlight_SQL.hh \
                Highlight_SQL.cc \
                Highlight_STL.cc \
                Highlight_STL.hh \
                Highlight_Swift.cc \
                Highlight_Swift.hh \
                Highlight_TCL.cc \
                Highlight_TCL.hh \
                Highlight_Text.cc \
                Highlight_Text.hh \
                Highlight_XML.cc \
                Highlight_XML.hh \
                Key.cc \
                Key.hh \
                Line.cc \
                Line.hh \
                MemCheck.cc \
                MemCheck.hh \
                MemLog.cc \
                MemLog.hh \
                String.cc \
                String.hh \
                Types.cc \
                Types.hh \
                Utilities.cc \
                Utilities.hh \
                View.cc \
                View.hh \
                Vis.cc \
                Vis.hh \
                gArray_t.hh
	gzip -f vis.tar

