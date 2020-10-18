This is a library that links against libved at:
https://github.com/agathoklisx/libved
  
It extends the libvwm library with readline and editor capabilities.
  
While this is by itself enough, the big idea is that we get also for
a quite rich functionality (like a tiny interpeter, but also if
desirable (while building libved+), a more rich but also very small
and simple programming language, a C (tcc) compiler, a json parser,  math expressions evaluation, an argument parser, ...).
  
Usage:
```sh
  make
  
  # This builds and installs, the libvwmed library, the vwmed executable and the required
  # header.
  # The default installed hierarchy (lib,include,bin), is the sys directory one level
  # lower to the cloned distribution. To change that, use the SYSDIR variable when
  # invoking make, or modify the Makefile. There is no specific install target.
  
```
  
This code has been tested and can be compiled with gcc, and clang C compilers,
without a warning and while turning the DEBUG flags on.  
  
Tested with valgrind for memory leaks.  
  
You might want to invoke the executable by preceding the LD_LIBRARY_PATH, like:  
  
  LD_LIBRARY_PATH=`path to libdir` vwmed [argv]  
  
Invocation:  
  
  vwmed [command] [command arguments]  
  
This library adds two more key binding to the existing ones.
  
MODKEY-TAB        : tab completion for the declared commands  
MODKEY-:          : a readline interface (at the invocation the behavior is like the tab completion)  
  
By default the `mode' key is CTRL-\.
  
Selection menu behavior (note that this a copy from libved sources):  
  
   Navigation keys:  
    - left and right (for left and right motions)  
      the left key should move the focus to the previous item on line, unless the  
      focus is on the first item in line, which in that case should focus to the  
      previous item (the last one on the previous line, unless is on the first line  
      which in that case should jump to the last item (the last item to the last  
      line))  
  
    - the right key should jump to the next item, unless the focus is on the last  
      item, which in that case should focus to the next item (the first one on the  
      next line, unless is on the last line, which in that case should jump to the  
      first item (the first item to the first line))  
  
    - page down/up keys for page down|up motions  
  
    - tab key is like the right key  
  
    Decision keys:  
    - Enter accepts selection; the function should return the focused item to the  
      caller  
  
    - Spacebar can also accept selection if it is enabled by the caller. That is 
      because a space can change the initial pattern|seed which calculates the 
      displayed results. But using the spacebar speeds a lot of those operations, 
      so in most cases is enabled, even in cases like CTRL('n') in insert mode. 
  
    - Escape key aborts the operation  
  
  
Command line mode (again this a copy from libved sources):  
 |   key[s]          |  Semantics                     |  
 |___________________|________________________________|  
 | carriage return   | accepts                        |  
 | escape            | aborts                         |  
 | ARROW[UP|DOWN]    | search item on the history list|  
 | ARROW[LEFT|RIGHT] | left|right cursor              |  
 | CTRL-a|HOME       | cursor to the beginning        |  
 | CTRL-e|END        | cursor to the end              |  
 | DELETE|BACKSPACE  | delete next|previous char      |  
 | CTRL-r            | insert register contents (charwise only)|  
 | CTRL-l            | clear line                     |  
 | CTRL-/ |CTRL-_    | insert last component of previous command|  
 |   can be repeated for: RLINE_LAST_COMPONENT_NUM_ENTRIES (default: 10)|  
 | TAB               | trigger completion[s]          |  
  
  
Application Interface:  
```C
  // initialize the structure
  vwmed_t *vwmed = __init_vwmed__ (NULL);
  // get the vwm structure and use it to create the environment
  vwm_t *this = Vwmed.get.vwm (vwmed);

  // init terminal
  int rows, cols;
  vwm_term *term = Vwmed.init.term (vwmed, &rows, &cols);

  // init libved
  Vwmed.init.ved (vwmed);

  // create a new window
  vwm_win *win = Vwm.new.win (this, "main", WinNewOpts (
    .rows = rows,
    .cols = cols,
    .num_frames = 1,
    .max_frames = 3));

  // get the frame ar 0 index
  vwm_frame *frame = Vwin.get.frame_at (win, 0);

  // set the argument list if it has been set
  if (argc > 1)
    Vframe.set.argv (frame, argc-1, argv + 1);

  // the a log file if desired
  Vframe.set.log (frame, NULL, 1);

  // save terminal state
  Vterm.screen.save (term);
  Vterm.screen.clear (term);

  // loop
  int retval = Vwm.main (this);

  // restore terminal state
  Vterm.screen.restore (term);

  // de-initialize the structures
  __deinit_vwmed__ (&vwmed);
  __deinit_vwm__ (&this);

  // the vwmed.c unit, it serves as an example using this library from C
```

BUGS:  
Tested only in Linux.  
