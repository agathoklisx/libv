V     := v
VWM   := vwm
VWMED := vwmed
VTACH := vtach

API := 0
REV := 3

# macOS doesn't have realpath by default (available through coreutils)
# THISDIR := $(shell realpath .)
THISDIR := $(shell (cd . && pwd))

VERSION :=  $(API).$(REV)

CC            := gcc
CC_STD        := -std=c11

SYSKERNEL     := $(shell uname -s)
SYSARCH       := $(shell uname -m)
SYS           := $(SYSKERNEL)-$(SYSARCH)

SYSDIR     := $(shell realpath $(THISDIR)/../../sys)
SYSLIBDIR   = $(SYSDIR)/lib
SYSBINDIR   = $(SYSDIR)/bin
SYSINCDIR   = $(SYSDIR)/include
SYSDATADIR  = $(SYSDIR)/data
SYSTMPDIR   = $(SYSDIR)/tmp

V_DIR      := lib$(V)
VWM_DIR    := lib$(VWM)
VTACH_DIR  := lib$(VTACH)
VWMED_DIR  := lib$(VWMED)

EDITOR := vi
SHELL  := zsh
DEFAULT_APP := $(SHELL)

DEBUG := 0

MARGS := DEBUG=$(DEBUG) SYSDIR=$(SYSDIR) API=$(API) REV=$(REV) SYSDATADIR=$(SYSDATADIR) $(SYSTMPDIR)=$(SYSTMPDIR)
VWM_MARGS += EDITOR=$(EDITOR) SHELL=$(SHELL) DEFAULT_APP=$(DEFAULT_APP)
#----------------------------------------------------------#
libvwm: Env
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) $(VWM_MARGS) shared-lib

vwm: libvwm
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) app-shared

libvwm-static: Env
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) $(VWM_MARGS) static-lib

vwm-static: libvwm-static
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) app-static

clean_libvwm:
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) clean_shared

clean_vwm:
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) clean_app_shared

clean_libvwm_static:
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) clean_static

clean_vwm_static:
	@cd $(VWM_DIR) && $(MAKE) $(MARGS) clean_app_static

clean_libvwm_shared_all: clean_libvwm clean_vwm
clean_libvwm_static_all: clean_libvwm_static clean_vwm_static
clean_libvwm_all: clean_libvwm_shared_all clean_libvwm_static_all
#----------------------------------------------------------#
libvtach: Env libvwm
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) shared-lib

vtach: libvtach
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) app-shared

libvtach-static: Env libvwm-static
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) static-lib

vtach-static: libvtach-static
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) app-static

clean_libvtach:
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) clean_shared

clean_vtach:
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) clean_app_shared

clean_libvtach_static:
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) clean_static

clean_vtach_static:
	@cd $(VTACH_DIR) && $(MAKE) $(MARGS) clean_app_static

clean_libvtach_shared_all: clean_libvtach clean_vtach
clean_libvtach_static_all: clean_libvtach_static clean_vtach_static
clean_libvtach_all:        clean_libvtach_shared_all clean_libvtach_static_all
#----------------------------------------------------------#
libvwmed: Env libved libved-ext libvwm
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) shared-lib

vwmed: libvwmed
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) app-shared

libvwmed-static: Env libved-static libved-ext-static
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) static-lib

vwmed-static: libvwmed-static
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) app-static

clean_libvwmed:
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) clean_shared

clean_vwmed:
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) clean_app_shared

clean_libvwmed_static:
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) clean_static

clean_vwmed_static:
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) clean_app_static

clean_libvwmed_shared_all: clean_libvwmed clean_vwmed
clean_libvwmed_static_all: clean_libvwmed_static clean_vwmed_static
clean_libvwmed_all:        clean_libvwmed_shared_all clean_libvwmed_static_all
#----------------------------------------------------------#
libv: Env libvwm libvtach libvwmed
	@cd $(V_DIR) && $(MAKE) $(MARGS) shared-lib

v: libv
	@cd $(V_DIR) && $(MAKE) $(MARGS) app-shared

libv-static: Env libvwm-static libvtach-static libvwmed-static
	@cd $(V_DIR) && $(MAKE) $(MARGS) static-lib

v-static: libv-static
	@cd $(V_DIR) && $(MAKE) $(MARGS) app-static

clean_libv:
	@cd $(V_DIR) && $(MAKE) $(MARGS) clean_shared

clean_v:
	@cd $(V_DIR) && $(MAKE) $(MARGS) clean_app_shared

clean_libv_static:
	@cd $(V_DIR) && $(MAKE) $(MARGS) clean_static

clean_v_static:
	@cd $(V_DIR) && $(MAKE) $(MARGS) clean_app_static

clean_libv_shared_all: clean_libv clean_v
clean_libv_static_all: clean_libv_static clean_v_static
clean_libv_all:        clean_libv_shared_all clean_libv_static_all
#----------------------------------------------------------#
all: vwm vtach vwmed v

all-libs: libvwm libvtach libvwmed libv

clean_all: clean_libvwm_all clean_libvtach_all clean_libvwmed_all clean_libv_all
clean_shared_all: clean_libvwm_shared_all clean_libvtach_shared_all clean_libvwmed_shared_all clean_libv_shared_all
clean_static_all: clean_libvwm_static_all clean_libvtach_static_all clean_libvwmed_static_all clean_libv_static_all
#----------------------------------------------------------#

TABWIDTH                                      := 8
UNDO_NUM_ENTRIES                              := 100
RLINE_HISTORY_NUM_ENTRIES                     := 20
CLEAR_BLANKLINES                              := 1
TAB_ON_INSERT_MODE_INDENTS                    := 0
CARRIAGE_RETURN_ON_NORMAL_IS_LIKE_INSERT_MODE := 1
SPACE_ON_NORMAL_IS_LIKE_INSERT_MODE           := 1
SMALL_E_ON_NORMAL_GOES_INSERT_MODE            := 1
BACKSPACE_ON_FIRST_IDX_REMOVE_TRAILING_SPACES := 0
BACKSPACE_ON_NORMAL_IS_LIKE_INSERT_MODE       := 1
BACKSPACE_ON_NORMAL_GOES_UP                   := 0
BACKSPACE_ON_INSERT_GOES_UP_AND_JOIN          := 0

LIBVED_OPTS :=

LIBVED_OPTS += TABWIDTH=$(TABWIDTH)
LIBVED_OPTS += UNDO_NUM_ENTRIES=$(UNDO_NUM_ENTRIES)
LIBVED_OPTS += RLINE_HISTORY_NUM_ENTRIES=$(RLINE_HISTORY_NUM_ENTRIES)
LIBVED_OPTS += CLEAR_BLANKLINES=$(CLEAR_BLANKLINES)
LIBVED_OPTS += TAB_ON_INSERT_MODE_INDENTS=$(TAB_ON_INSERT_MODE_INDENTS)
LIBVED_OPTS += CARRIAGE_RETURN_ON_NORMAL_IS_LIKE_INSERT_MODE=$(CARRIAGE_RETURN_ON_NORMAL_IS_LIKE_INSERT_MODE)
LIBVED_OPTS += SPACE_ON_NORMAL_IS_LIKE_INSERT_MODE=$(SPACE_ON_NORMAL_IS_LIKE_INSERT_MODE)
LIBVED_OPTS += SMALL_E_ON_NORMAL_GOES_INSERT_MODE=$(SMALL_E_ON_NORMAL_GOES_INSERT_MODE)
LIBVED_OPTS += BACKSPACE_ON_FIRST_IDX_REMOVE_TRAILING_SPACES=$(BACKSPACE_ON_FIRST_IDX_REMOVE_TRAILING_SPACES)
LIBVED_OPTS += BACKSPACE_ON_NORMAL_IS_LIKE_INSERT_MODE=$(BACKSPACE_ON_NORMAL_IS_LIKE_INSERT_MODE)
LIBVED_OPTS += BACKSPACE_ON_NORMAL_GOES_UP=$(BACKSPACE_ON_NORMAL_GOES_UP)
LIBVED_OPTS += BACKSPACE_ON_INSERT_GOES_UP_AND_JOIN=$(BACKSPACE_ON_INSERT_GOES_UP_AND_JOIN)

HAS_PROGRAMMING_LANGUAGE := 1
HAS_USER_EXTENSIONS      := 1
HAS_SPELL                := 1
HAS_JSON                 := 0
HAS_TCC                  := 0
HAS_CURL                 := 0
HAS_LOCAL_EXTENSIONS     := 0

LIBVED_EXT_OPTS :=

LIBVED_EXT_OPTS += HAS_PROGRAMMING_LANGUAGE=$(HAS_PROGRAMMING_LANGUAGE)
LIBVED_EXT_OPTS += HAS_USER_EXTENSIONS=$(HAS_USER_EXTENSIONS)
LIBVED_EXT_OPTS += HAS_SPELL=$(HAS_SPELL)
LIBVED_EXT_OPTS += HAS_JSON=$(HAS_JSON)
LIBVED_EXT_OPTS += HAS_TCC=$(HAS_TCC)
LIBVED_EXT_OPTS += HAS_CURL=$(HAS_CURL)
LIBVED_EXT_OPTS += HAS_LOCAL_EXTENSIONS=$(HAS_LOCAL_EXTENSIONS)

PROGRAMMING_LANGUAGE_SHARED_FLAGS :=
PROGRAMMING_LANGUAGE_STATIC_FLAGS := "ENABLE_HTTP=0"

ifneq ($(HAS_PROGRAMMING_LANGUAGE), 0)
  ifeq ($(HAS_CURL), 0)
    PROGRAMMING_LANGUAGE_SHARED_FLAGS += "ENABLE_HTTP=0"
  else
    PROGRAMMING_LANGUAGE_SHARED_FLAGS += "ENABLE_HTTP=1"
  endif
endif

libved:
	@cd modules/libved/src && $(MAKE) $(MARGS) $(LIBVED_OPTS) shared

libved-static:
	@cd modules/libved/src && $(MAKE) $(MARGS) $(LIBVED_OPTS) static

libved-ext:
	@cd modules/libved/src && $(MAKE) $(MARGS) $(LIBVED_EXT_OPTS) shared-libext

libved-ext-static:
	@cd modules/libved/src && $(MAKE) $(MARGS) $(LIBVED_EXT_OPTS) static-libext

pl-shared-library:
	@cd modul_s/libved/src && $(MAKE) $(MARGS) $(PROGRAMMING_LANGUAGE_SHARED_FLAGS) pl-shared-library

clean_libved:
	@cd modules/libved/src && $(MAKE) $(MARGS) clean_shared

clean_libved-ext:
	@cd modules/libved/src && $(MAKE) $(MARGS) clean_libext

clean_pl:
	@cd modules/libved/src && $(MAKE) $(MARGS) pl_clean

clean_pl_shared:
	@cd modules/libved/src && $(MAKE) $(MARGS) pl_clean_shared

clean_pl_static:
	@cd modules/libved/src && $(MAKE) $(MARGS) pl_clean_static

debug:
	@echo $(SYSDIR)
	@echo $(VWM_DIR)
	@echo $(VTACH_DIR)
	@echo $(MARGS)
	@echo $(LIBVED_OPTS)

Env: makeenv checkenv
makeenv:
	@$(TEST) -d $(SYSDIR)     || $(MKDIR_P) $(SYSDIR)
	@$(TEST) -d $(SYSLIBDIR)  || $(MKDIR)   $(SYSLIBDIR)
	@$(TEST) -d $(SYSINCDIR)  || $(MKDIR)   $(SYSINCDIR)
	@$(TEST) -d $(SYSBINDIR)  || $(MKDIR)   $(SYSBINDIR)
	@$(TEST) -d $(SYSTMPDIR)  || $(MKDIR)   $(SYSTMPDIR)

checkenv:
	@$(TEST) -w $(SYSDIR)     || exit 1
	@$(TEST) -w $(SYSLIBDIR)  || exit 1
	@$(TEST) -w $(SYSINCDIR)  || exit 1
	@$(TEST) -w $(SYSBINDIR)  || exit 1
	@$(TEST) -w $(SYSTMPDIR)  || exit 1

INSTALL = install
RM = rm -f
CP = cp
AR = ar rs
CP_R = $(CP) -r
TEST = test
LN = ln
LN_S = $(LN) -s
MKDIR = mkdir
MKDIR_P = $(MKDIR) -p
