This Makefile offers a convenient way to build all the targets.

```sh
# to build all the libraries and the v utility, that includes all
# the functionality, issue:

make v

# to build the libvwm library, issue:

make libvwm

# to build the vwm executable, issue:

make vwm

# to build the libvtach library, issue:

make libvtach

# to build the vtach executable, issue:

make vtach

# to build the libvwmed library, issue:

make libvwmed

# to build the vwmed executable, issue:

make vwmed

# to build the libv library, issue:

make libv

# to build the v executable, issue:

make v

# all these targets have a clean up target, e.g.,

make clean_libvwm
make clean_vwm

make clean_libvtach
make clean_vtach

make clean_libvwmed
make clean_vwmed

make clean_libv
make clean_v

# the zsh shell features auto-completion for Makefiles
```

Make Options:  

EDITOR := editor (default vi)  
SHELL  := shell  (default zsh)  
DEFAULT_APP := default application (default zsh)  
SYSDIR := system directory (default, one level up to this repository)  
DEBUG  := turning on/off debuging (default 0)  
CC     := C compiler (default gcc)  
