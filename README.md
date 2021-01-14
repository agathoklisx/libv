This is a Virtual Terminal Environment that aims for minimalism and targets UNIX like systems.
  
Components:
  
  - libvwm:  
    This is an implementation of a tiny virtual terminal window manager library (included also a sample
    application that initialize the library). See at src/libvwm for details.  
  
  - libvtach:  
    This is a library that extends libvwm by linking against, with attaching|detaching
    capabilities (included also a sample application). See at src/libvtach for details.  
  
  - libvwmed:  
    This is a library that extends libvwm with readline and editor capabilities.
    It links against libved which is available in this repository as submodule.
    It includes also an application that initialize the library). See at src/libvwmed for details.  
  
  - libv:  
    This is a library that links against the above libraries. It provides
    also the v utility that use the Environment.  

    
  
Usage:
 
```sh
# clone the repository and the submodule

git clone --recurse-submodules https://github.com/agathoklisx/libv

# if you've already cloned the sources, issue to get the submodule:

git submodule update --init
```

In every library directory there is a specific Makefile to build the specific target.  
Note that those Makefiles don't handle dependencies.  
  
From any of those directories issue:
```sh
# to build the shared library

make shared-lib

# to build the static library

make static-lib

# to build the sample applications

make app-shared
make app-static
```

In the src directory there is a generic Makefile that builds all the targets.  
Note that is the recommended way to build any desired target, as this method it  
also handles dependencies.  
```sh
# basic instructions

# this builds all the shared libraries and the v utility

make v

# likewise, but this builds the static targets

make v-static
```
Refer to src/README.md or to src/Makefile for details.

Note that in every subdirectory there is also a specific README.md.  

The v utility.
```sh
# for a short help issue:

v --help
```

Default keybindings:  

By default the `MODKEY' key is CTRL-\.

This it can be set with:  
  
  Vwm.set.mode_key (vwm_t *, char);  

<pre>
  MODKEY-q           : quit the application  
  MODKEY-K           : kill the current procedure in the current frame  
  MODKEY-!           : open the default shell (by default zsh)  
  MODKEY-c           : open the default application (by default zsh)  
  MODKEY-[up|down|w] : switch to the upper|lower frame respectively  
  MODKEY-[j|k]       : likewise  
  MODKEY-[left|right]: switch to the prev|next window respectively  
  MODKEY-[h|l]       : likewise  
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
  MODKEY-CTRL(l)     : redraw current window  
  MODKEY-MODE_KEY    : return the MODE_KEY to the application  
  MODKEY-ESCAPE_KEY  : return with no action  
  MODKEY-CTRL(d)     : detach application  
  MODKEY-TAB         : command completion with the default parameters  
  MODKEY-:           : command completion and readline  
</pre>
Status:  
The environment is complex enough and the code is at early stage (was
initialized at the mid days of the September of 2020). So naturally is not stable.  

Compiles without warnings while turning on -Wall and -Wextra, on Linux by gcc and clang C compilers.
