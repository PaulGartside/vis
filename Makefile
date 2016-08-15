
# Select your operating system:
OS = OSX
#OS = LINUX
#OS = WIN32
#OS = SUNOS

NAME      = vis
DEFINES   =
CXX       = g++
DEBUG     = #-g
CXXFLAGS  = -c -O $(DEBUG) -D$(OS) $(DEFINES)
LIBS      = -lm
LIB_PATHS =
INCS      =
DOT_O_DIR = OBJS/$(OS)
DEPS_DIR  = DEPS/$(OS)
PP_DIR    = PP/$(OS)

SOURCES = ChangeHist \
          Colon \
          Console \
          Cover_Array \
          Diff \
          FileBuf \
          Highlight_Base \
          Highlight_Bash \
          Highlight_BufferEditor \
          Highlight_CPP \
          Highlight_Code \
          Highlight_Dir \
          Highlight_HTML \
          Highlight_IDL \
          Highlight_JS \
          Highlight_Java \
          Highlight_Make \
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

SOURCE_CC_FILES = $(addsuffix .cc,$(SOURCES))
SOURCE_HH_FILES = $(addsuffix .hh,$(SOURCES))
DOT_O_FILES   = $(addprefix $(DOT_O_DIR)/,$(addsuffix .o,$(SOURCES)))
DOT_DEP_FILES = $(addprefix $(DEPS_DIR)/,$(addsuffix .dep,$(SOURCES)))
PREPROC_FILES = $(addprefix $(PP_DIR)/,$(addsuffix .pp.cc,$(SOURCES)))

.PHONY: all clean install preproc tar

all: $(NAME)

clean:
	-rm -r $(DOT_O_DIR)
	-rm -r $(DEPS_DIR)
	-rm -r $(PP_DIR)

install:
	cp ./$(NAME) `which $(NAME)`

preproc: $(PREPROC_FILES)

tar:
	tar cf vis.tar Makefile \
                Array_t.hh \
                Help.hh \
                gArray_t.hh \
                Console_Unix.cc \
                Console_Win32.cc \
                $(SOURCE_CC_FILES) \
                $(SOURCE_HH_FILES)
	gzip -f vis.tar

$(NAME): $(DOT_O_DIR) $(DOT_O_FILES)
	$(CXX) -o $@ $(DOT_O_FILES) $(LIBS) $(LIB_PATHS)
	echo Done making $(NAME)

$(DOT_O_DIR):; mkdir -p $(DOT_O_DIR)
$(DEPS_DIR) :; mkdir -p $(DEPS_DIR)
$(PP_DIR)   :; mkdir -p $(PP_DIR)

$(DOT_O_FILES): $(DOT_O_DIR)/%.o: %.cc
	$(CXX) $(INCS) $(CXXFLAGS) $< -o $@

$(DOT_DEP_FILES): $(DEPS_DIR)/%.dep: %.cc $(DEPS_DIR)
	$(CXX) -MM $< $(INCS) $(CXXFLAGS) | sed '1 s|^|$(DOT_O_DIR)/$*\.dep $(DOT_O_DIR)/|' > $@

$(PP_DIR)/%.pp.cc : %.cc $(PP_DIR)
	$(CXX) -E $(INCS) $(CXXFLAGS) $< > $@

-include $(DOT_DEP_FILES)

