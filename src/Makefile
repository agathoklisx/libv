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

MARGS := DEBUG=$(DEBUG) SYSDIR=$(SYSDIR) API=$(API) REV=$(REV)
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
libvwmed: Env libvwm
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) shared-lib

vwmed: libvwmed
	@cd $(VWMED_DIR) && $(MAKE) $(MARGS) app-shared

libvwmed-static: Env
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

v-static:
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

debug:
	@echo $(SYSDIR)
	@echo $(VWM_DIR)
	@echo $(VTACH_DIR)
	@echo $(MARGS)

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
