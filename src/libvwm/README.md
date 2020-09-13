This is a library that implements a tiny window manager for a virtual terminal environment, without any prerequisite other than a standard libc.

Usage:
```sh
  make

  # This builds and installs, the libvwm library, the vwm executable and the required
  # header.
  # The default installed hierarchy (lib,include,bin), is the sys directory one level
  # lower to the cloned distribution. To change that, use the SYSDIR variable when
  # invoking make, or modify the Makefile. There is no specific install target.

```

This code has been tested and can be compiled with gcc, clang and the tcc C compilers,
without a warning and while turning the DEBUG flags on.

Tested with valgrind for memory leaks.

This library exposes a vwm_t * structure, which holds all the required information,
and it can be initialized with:

  vwm_t *this = __init_vwm__ ();

and can deinitialized with:

  __deinit_vwm__ (&this);

All the operations are controlled with the keyboard. There is no code to handle
mouse events, but the original code includes this functionality.

The vwm.c unit, it serves as an example using this library from C.

You might want to invoke this executable by preceding the LD_LIBRARY_PATH, like:

  LD_LIBRARY_PATH=`path to libdir` vwm [argv]

The key bindings are described in the vwm_process_input() function.

By default the `mode' key is CTRL-\.

Default Key bindings:

MODE_KEY-q: quit the application  
MODE_KEY-k: kill the current procedure in the current frame  
MODE_KEY-c: open the default application (zsh)  
MODE_KEY-[left|right]: switch to the prev|next window respectively  
MODE_KEY-[up|down|w]: switch to the upper|lower frame respectively  
MODE_KEY-MODE_KEY: return the MODE_KEY to the application  

BUGS and missing functionality:

 - vim and htop work both in monochrome mode, plus htop output has a couple of artifacts

 - editing the scrollback buffer hasn't been implemented yet


The initial code was derived by splitvt by Sam Lantinga, which is licensed with GPL2
and it is included within this directory.

This same license applies to this project.