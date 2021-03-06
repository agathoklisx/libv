THIS_LIBNAME := vwm
NAME         := vwm

API := 0
REV := 2

# macOS doesn't have realpath by default (available through coreutils)
# THISDIR := $(shell realpath .)
THISDIR := $(shell (cd . && pwd))

THIS_LIBVERS  :=  $(API).$(REV)

THIS_HEADER     = lib$(THIS_LIBNAME).h
C_HEADER        = libvci.h
THIS_HEADER_EXT = lib$(THIS_LIBNAME)+.h
THIS_LIBSRC     = lib$(THIS_LIBNAME).c
LIBNAME         = $(THIS_LIBNAME)-$(THIS_LIBVERS)
THIS_LIB        = lib$(LIBNAME)

CC            := gcc
CC_STD        := -std=c11

SYSKERNEL     := $(shell uname -s)
SYSARCH       := $(shell uname -m)
SYS           := $(SYSKERNEL)-$(SYSARCH)

DATADIR    =  $(THISDIR)/data

SYSDIR    := $(shell realpath $(THISDIR)/../../../sys)
SYSLIBDIR  = $(SYSDIR)/lib
SYSBINDIR  = $(SYSDIR)/bin
SYSINCDIR  = $(SYSDIR)/include
SYSVINCDIR = $(SYSINCDIR)/libv
SYSDATADIR = $(SYSDIR)/data
SYSTMPDIR  = $(SYSDIR)/tmp

THIS_SHAREDLIB := $(SYSLIBDIR)/$(THIS_LIB).so
THIS_STATICLIB := $(SYSLIBDIR)/lib$(THIS_LIBNAME).a

BASE_FLAGS  := $(CC_STD) -O2 -march=native -fvisibility=hidden -fdollars-in-identifiers

DEBUG_FLAGS := -g -Wextra -Wshadow -Wall -Wunused-function -Wunused-macros -Wno-override-init

# -Wno-override-init: is for designated initializers and both compilers complain.
# it is included in extra in gcc, but clang needs this unconditionally

DEBUG := 0

ifneq ($(DEBUG), 1)
  ifeq (-$(CC), -clang)
    BASE_FLAGS  +=  -Wno-override-init
  endif
endif

ifneq (-$(CC), -tcc)
  DEBUG_FLAGS += -Werror
  # while compiling tinyexpr tcc warns with: assigment discards qualifiers
  # from pointer target type
  # -Wno-error=discarded-qualifiers doesn't have effect
endif

# DEBUG_FLAGS += -fsanitize=undefined
# if turn on -Wpedantic gcc gives: ISO C forbids braced-groups within expressions
# (this is for statement expressions)

SHARED_CFLAGS := -shared -fPIC
STATIC_CFLAGS := -static

FLAGS         := $(BASE_FLAGS)

CHECK_PTR_BOUNDS_FLAGS := -mmpx -fcheck-pointer-bounds

CHECK_PTR_BOUNDS := 0

ifneq ($(CHECK_PTR_BOUNDS), 0)
  FLAGS += $(CHECK_PTR_BOUNDS_FLAGS)
endif

ifeq (-$(CC), -gcc)
  FLAGS += -Wno-implicit-fallthrough
  # for gcc < 7 version
  #  FLAGS += -Wimplicit-fallthrough=0
endif

ifneq ($(DEBUG), 0)
  FLAGS += $(DEBUG_FLAGS) -DDEBUG
endif

#----------------------------------------------------------#
LIBFLAGS := -I. -I$(SYSINCDIR) $(FLAGS) -lutil

EDITOR := vim
SHELL  := zsh
DEFAULT_APP := $(SHELL)

LIBFLAGS += -DEDITOR='"$(EDITOR)"'
LIBFLAGS += -DSHELL='"$(SHELL)"'
LIBFLAGS += -DTMPDIR='"$(SYSTMPDIR)"'
LIBFLAGS += -DDEFAULT_APP='"$(DEFAULT_APP)"'

all: shared-lib app-shared

prereq: Env

shared-lib: prereq headers $(THIS_SHAREDLIB)
$(THIS_SHAREDLIB):
	$(CC) -x c $(THIS_LIBSRC) $(LIBFLAGS) $(SHARED_CFLAGS) -o $(THIS_SHAREDLIB)
	@$(LN_S) -vf $(THIS_SHAREDLIB) $(SYSLIBDIR)/lib$(THIS_LIBNAME).so

static-lib: prereq headers $(THIS_STATICLIB)
$(THIS_STATICLIB):
	$(CC) -x c $(THIS_LIBSRC) $(LIBFLAGS) $(STATIC_CFLAGS) -c -o $(THIS_LIB).o
	@$(AR) $(THIS_STATICLIB) $(THIS_LIB).o
	@$(RM) $(THIS_LIB).o

SYSAPPSHARED = $(SYSBINDIR)/$(NAME)
SYSAPPSTATIC = $(SYSBINDIR)/$(NAME)_static

APPFLAGS += -I$(SYSINCDIR) -L$(SYSLIBDIR) -l$(THIS_LIBNAME)
APPFLAGS += $(FLAGS)

THIS_APPSRC  := $(NAME).c

app: app-shared app-static

app-shared: shared-lib $(SYSAPPSHARED)
$(SYSAPPSHARED):
	$(CC) -x c $(THIS_APPSRC) $(APPOPTS) $(APPFLAGS) $(SHARED_APP_FLAGS) -o $(NAME)
	@$(INSTALL) -v $(NAME) $(SYSBINDIR)
	@$(RM) $(NAME)

app-static: static-lib $(SYSAPPSTATIC)
$(SYSAPPSTATIC):
	$(CC) -x c $(THIS_APPSRC) $(APPOPTS) $(APPFLAGS) $(STATIC_CFLAGS) -lutil -o $(NAME)_static
	@$(INSTALL) -v $(NAME)_static $(SYSBINDIR)
	@$(RM) $(NAME)_static

headers: header cheader

header: clean_header $(SYSVINCDIR)/$(THIS_HEADER)

$(SYSVINCDIR)/$(THIS_HEADER):
	@$(CP) $(THIS_HEADER) $(SYSVINCDIR)

cheader: clean_cheader $(SYSVINCDIR)/$(C_HEADER)

$(SYSVINCDIR)/$(C_HEADER):
	@$(CP) $(C_HEADER) $(SYSVINCDIR)

clean: clean_shared clean_static clean_headers clean_app
clean_shared: clean_header
	@$(TEST) ! -f $(THIS_SHAREDLIB) || $(RM) $(THIS_SHAREDLIB)
clean_static: clean_header
	@$(TEST) ! -f $(THIS_STATICLIB) || $(RM) $(THIS_STATICLIB)

clean_headers: clean_header clean_cheader
clean_header:
	@$(TEST) ! -f $(SYSVINCDIR)/$(THIS_HEADER) || $(RM) $(SYSVINCDIR)/$(THIS_HEADER)

clean_cheader:
	@$(TEST) ! -f $(SYSVINCDIR)/$(C_HEADER) || $(RM) $(SYSVINCDIR)/$(C_HEADER)

clean_app: clean_app_static clean_app_shared
clean_app_shared:
	@$(TEST) ! -f $(SYSAPPSHARED) || $(RM) $(SYSAPPSHARED)
clean_app_static:
	@$(TEST) ! -f $(SYSAPPSTATIC) || $(RM) $(SYSAPPSTATIC)

Env: makeenv checkenv
makeenv:
	@$(TEST) -d $(SYSDIR)     || $(MKDIR_P) $(SYSDIR)
	@$(TEST) -d $(SYSBINDIR)  || $(MKDIR)   $(SYSBINDIR)
	@$(TEST) -d $(SYSTMPDIR)  || $(MKDIR)   $(SYSTMPDIR)
	@$(TEST) -d $(SYSLIBDIR)  || $(MKDIR)   $(SYSLIBDIR)
	@$(TEST) -d $(SYSVINCDIR) || $(MKDIR_P) $(SYSVINCDIR)

checkenv:
	@$(TEST) -w $(SYSDIR)     || exit 1
	@$(TEST) -w $(SYSBINDIR)  || exit 1
	@$(TEST) -w $(SYSTMPDIR)  || exit 1
	@$(TEST) -w $(SYSLIBDIR)  || exit 1
	@$(TEST) -w $(SYSVINCDIR) || exit 1

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
