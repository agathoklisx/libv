This is a library that links against libvwm, libvwmed and libvtach. 
and abstracts the details, to create the environment.
  
It provides the v utility.  
  
Issue: v -h,--help for a short message.  
  
The key bindings is the summary of all the bindings of all the libraries.
  
  
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
     -s, --sockname=     set the socket name [required if --as= missing]\n"
         --as=           create the socket name in an inner environment [required if -s is missing]\n"
     -a, --attach        attach to the specified socket\n"
     -f, --force         connect to socket, even when socket exists\n"
         --send          send data to the specified socket from standard input and then exit
```
  
Refer to the sources for inner details and probably a more updated help message  
or|and api updates.
