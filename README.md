This is a Virtual Terminal environment that aims for minimalism and targets UNIX like systems.
  
Components:
  
  - libvwm:  
    This is an implementation of a tiny virtual terminal window manager library (included also a sample    
    application that initialize the library). See at src/libvwm for details.  
  
  - libvtach:  
    This is a library that extends libvwm by linking against, with attaching|detaching   
    capabilities (included also a sample application). See at src/libvtach for details.  
  
  - libvwmed:  
    This is a library that extends libvwm with readline and editor capabilities.  
    It links against libved which is available in this repository.  
    It includes also an application that initialize the library). See at src/libvwmed for details.  
  
  - libv:  
    This is a library that link against the above libraries. It provides  
    the v utility, that accepts the same arguments with vtach.  
  
Usage:
  
In every library directory there is a specific Makefile to build the targets.  
This Makefile doesn't handle the dependencies.  
  
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
  
In the src directory there is a generic Makefile that builds all the targers.  
Refer to src/README.md for details.
