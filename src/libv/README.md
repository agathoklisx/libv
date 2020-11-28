This is a library that links against libvwm, libvwmed and libvtach. 
and abstracts the details, to create the environment.
  
It provides the v utility.  
  
Issue: v -h,--help for a short message.  
  
The key bindings is the summary of all the bindings of all the libraries.
  
Default keybindings of the v utility:  

By default the `MODKEY' key is CTRL-\.  

This it can be set with:  
  
  Vwm.set.mode_key (vwm_t *, char);  

  MODKEY-q           : quit the application  
  MODKEY-k           : kill the current procedure in the current frame  
  MODKEY-!           : open the default shell (by default zsh)  
  MODKEY-c           : open the default application (by default zsh)  
  MODKEY-[up|down|w] : switch to the upper|lower frame respectively  
  MODKEY-[left|right]: switch to the prev|next window respectively  
  MODKEY-`           : switch to the previously focused window  
  MODKEY-F[1-12]     : switch to `nth' window indicated by the digit of the Function Key    
  MODKEY-[param]+    : increase the size of the current frame (default count 1)  
  MODKEY-[param]-    : decrease the size of the current frame (default count 1)  
  MODKEY-[param]=    : set the lines (param) of the current frame  
  MODKEY-[param]n    : create and switch to a new window with `count' frames (default 1)    
  MODKEY-E|PageUp    : edit the log file (if it is has been set)  
  MODKEY-s           : split the window and add a new frame  
  MODKEY-S[!ec]      : likewise, but also fork with a shell or an editor or the default application respectively (without a param is like MODE_KEY-s)  
  MODKEY-d           : delete current frame  
  MODKEY-MODE_KEY    : return the MODE_KEY to the application  
  MODKEY-ESCAPE_KEY  : return with no action  
  MODKEY-CTRL(d)     : detach application  

Issue MODKEY-: for a readline interface.

This offers command, argument and filename completion.  
  
Editing line mode keybindings:  
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


 - command completion is triggered when the cursor is at the first word token.  
  (note: and also by default, and when the first char is one of the "`~@" set).  
  
 - arg completion is triggered when the first char word token is an '-'.
   (otherwise a filename completion is performed).  

Commands should be self explanatory and should be execute (albeit with  
optional arguments) the basic commands.  
  
Setting and restoring an image of the current layout.  
The initial setting is done through the set command:  

  :set --image-name=name --save-image=1  

Though there are other choises, this is the supported development intention,  
as it makes more practical sense, since first is controlled through an inner  
mechanism, and then it allows restoring the saved image, in a permanent way    
at the shell invovation level, easily with:  
  
  v --loadfile=name  
  
This should restore the saved image.  
  
But and for testing mostly purposes, it can work also that way:  
  :set --image-file=/some/file --save-image=1  
  :@save_image 
  
or simply  
  :@save_image --as=/some/file  
  
But the image is saved automatically at the application deinitialization,  
and if the --save-image=1 argument has been set before.  
Using:
 
  :set --save-image=0  

unsets the option.  


Application Interface.
```C
  // set the options
  v_init_opts opts = V_INIT_OPTS(
    .argc = argc,
    .argv = argv
  );

  // initialize the structure
  // it returns a v_t *object or NULL on error
  v_t *v = __init_v__ (NULL, &opts);

  // call the main function
  int retval = V.main (v);

  // deinitialize the structure
  __deinit_v__ (&v);
```
  
Invocation and Options:
```sh
  v [options] [command] [command arguments]
  
  Options:
     -s, --sockname=     set the socket name [required if --as= missing]
         --as=           create the socket name in an inner environment [required if -s is missing]
     -a, --attach        attach to the specified socket
     -f, --force         connect to socket, even when socket exists
         --send          send data to the specified socket from standard input and then exit
         --exit          create the socket, fork and then exit
         --remove-socket remove socket if exists and can not be connected
         --loadfile=     load file for evaluation
```
  
Refer to the sources for inner details and probably a more updated help message  
or|and api updates.
