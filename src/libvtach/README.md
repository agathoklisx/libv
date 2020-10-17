This is a library that extends libvwm with attaching|detaching capabilities.

The algorithms derived from the dtach utility.  

dtach is a simple program that emulates the detach feature of screen.  
dtach is (C)Copyright 2004-2016 Ned T. Crigler, and is under the GNU General
Public License.  

This license should be included in this directory.  
This same license applies to this library.  

Usage:
```sh
  make

  # This builds and installs, the libvtach library, the vtach executable and the required
  # header.
  # The default installed hierarchy (lib,include,bin), is the sys directory one level
  # lower to the cloned distribution. To change that, use the SYSDIR variable when
  # invoking make, or modify the Makefile. There is no specific install target.

```

This code has been tested and can be compiled with gcc, and clang C compilers,
without a warning and while turning the DEBUG flags on.  

Tested with valgrind for memory leaks.  

You might want to invoke the executable by preceding the LD_LIBRARY_PATH, like:  

  LD_LIBRARY_PATH=`path to libdir` vtach [options] [argv]  

Invocation:  

  vtach [options] [command] [command arguments]  

  Options:  
      -s, --sockname=     set the socket name [required]  
      -a, --attach        attach to the specified socket  

This library adds one more key binding to the existing ones.

MODKEY-CTRL(d)           : detach application   

By default the `mode' key is CTRL-\.

Application Interface:
```C
  // initialize the structure
  vtach_t *vtach = __init_vtach__ (NULL);
  // get the libvwm structure
  vwm_t *this = Vtach.get.vwm (vtach);

  // initialize the properties given the required socket name
  // it returns -1 if socket name exceeds sizeof (sun_path) and the
  // structures should be released, otherwise returns 0
  (int) retval = Vtach.init.pty (vtach, sockname);

  // if it is the master process, then create the required socket, daemonize
  // and create the pty process with whatever is in the argument list 
  // it returns -1 if it can not create the socket or if fork fails and the
  // structures should be released, otherwise returns 0
  (int) retval = Vtach.pty.main (vtach, argc, argv);

  // at the end attach to the socket
  // it returns -1 if it can not be connected to the socket or because
  // of a read() or a select() error, otherwise it returns 0 either if it was
  // exited normally (when the application exits) or if it was detached
  // by the user
  (int) retval = Vtach.tty.main (vtach);

  // de-initialize the structures
  __deinit_vtach__ (&vtach);
  __deinit_vwm__ (&this);

  // that's it
  // the vtach.c unit, it serves as an example using this library from C
```

BUGS:  
Tested only in Linux.  

Many Thanks to the dtach project.
