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

This code has been tested and can be compiled with gcc and clang C compilers,
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

MODKEY-q           : quit the application  
MODKEY-K           : kill the current procedure in the current frame  
MODKEY-!           : open the default shell (by default zsh)  
MODKEY-c           : open the default application (by default zsh)  
MODKEY-[up|down|w] : switch to the upper|lower frame respectively  
MODKEY-[j|k]       : likewise  
MODKEY-[left|right]: switch to the prev|next window respectively  
MODKEY-[h|l]       : likewise  
MODKEY-`           : switch to the previously focused window  
MODKEY-[param]+    : increase the size of the current frame (default count 1)  
MODKEY-[param]-    : decrease the size of the current frame (default count 1)  
MODKEY-[param]=    : set the lines (param) of the current frame  
MODKEY-[param]n    : create and switch to a new window with `count' frames (default 1)    
MODKEY-E|PageUp    : edit the log file (if it is has been set)  
MODKEY-s           : split the window and add a new frame  
MODKEY-S[!ec]      : likewise, but also fork with a shell or an editor or the default application respectively (without a param is like MODE_KEY-s)  
MODKEY-d           : delete current frame  
MODKEY-CTRL(l)     : redraw current window  
MODKEY-MODE_KEY    : return the MODE_KEY to the application  
MODKEY-ESCAPE_KEY  : return with no action  

BUGS and missing functionality:  

 - vim and htop works both in monochrome mode, plus htop output has a couple of artifacts  

 - mostly un-handled the condition, when the available terminal lines, are less than the required to function properly  
   (it was developed and is being used, under a fullscreen window environment)  


Application Interface.
```C
  // initialize the structure
  vwm_t *this = __init_vwm__ ();

  // get the terminal instance
  vwm_term *term =  Vwm.get.term (this);

  // and set it to raw mode
  Vterm.raw_mode (term);

  // init the LINES and COLUMNS based on the size of the current terminal
  int rows, cols;
  Vterm.init_size (term, &rows, &cols);

  // and set the size
  Vwm.set.size (this, rows, cols, 1);

  // create a new window
  vwm_win *win = Vwm.new.win (this, NULL, WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 2,
    .max_frames = 3));

  // get the frame at the idx 0
  vwm_frame *frame = Vwin.get.frame_at (win, 0);
  // and set the argument list if it has been set
  // if not then the frame will use the default application to fork
  if (argc > 1)
    Vframe.set.argv (frame, argc-1, argv + 1);

  // set a log file if it is desired (this can be used as a scrollback buffer)
  Vframe.set.log (frame, NULL, 1);

  // fork (this can be omitted, as in this case forking in the main function)
  Vframe.fork (frame);

  // set also the second frame
  frame = Vwin.get.frame_at (win, 1);
  // another way to set an argument list to fork
  Vframe.set.command (frame, "bash");

  // create other windows if it is desired

  // save screen state
  Vterm.screen.save (term);
  Vterm.screen.clear (term);

  // and finally call main
  int retval = Vwm.main (this);

  // restore screen state
  Vterm.screen.restore (term);

  // de-initialize
  __deinit_vwm__ (&this);
```

The initial code was derived from splitvt by Sam Lantinga, which is licensed with GPL2
and it is included within this directory.

This same license applies to this project.
