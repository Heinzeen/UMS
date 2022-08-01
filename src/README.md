# AOSV Final Project Sources
_A.Y. 2020/2021_

Author: Matteo Marini 1739106 

The sources are structured in this way:
- `library/` contains the user-mode library that needs to be imported in order to use UMS.
    - `examples` contains some example that shows how to use UMS as a user.
        - `UMSHeader.h` the header that a user should include in order to use UMS.
        - `1-n_sched_m_threads` example 1, n scheduler and n worker per scheduler
        - `2-n_sched_m_threads_same_cs` example 2, n scheduler and n worker per scheduler, scheduler with same cs
    - `Makefile` the makefile of the library.
    - `UMSLibrary.c` the source code of the library.
    - `UMSLibrary.h` the header of the library, it is not the one that a user should import.
    - `UMSList.c` the source of the lists implementation for the library.
    - `UMSList.h` the header used by the list implementation.
- `module/` contains the code of the kernel module that allows UMS to work properly.
    - `Makefile` the makefile of the kernel module.
    - `UMSProcManager.c` the source of the module that manages the /proc file system.
    - `UMSProcManager.h` the header used for the /proc fs management.
    - `UMSMain.c` the main source for the kernel module.
    - `UMSMain.h` the main header for the kernel module.
    - `common.h` a header containing some shared definitions.
    - `mount.sh` a script used to mount the module (once compiled).
    - `unmount.sh` a script used to unmount the module.

## Compiling the project
To compile the project a user has to compile both the kernel module and the library; to do so go in the respective directories and run _make_. Compiling the library will create a shared object called _libUMS.so_; this is the library that a user will have to include in order to use UMS. Compiling the module will create some object files and the kernel object _UMS.ko_ which is the one that a user will need to load before using UMS. To do so, mounting and unmounting scripts are given (respectiely, _mount.sh_ and _unmount.sh_). Obviously, the two scripts require super user privileges to be run.

## Running the project
After the kernel module was loaded, a user needs to create its application and run it with the shared object loaded; for a more specific guide on how to use the APIs take a look at the documentation under the _doc/_ folder, to link the library you can use the following command:
```console
gcc -Llib_dir -Wl,-rpath=lib_dir -Wall -o output_file input_file.c -lUMS -pthread
```
If you want to use a custom command (or a more complex makefile) remember to add the library (-L option), add its path (-rpath option) and to link it (-lUMS option). Moreover, UMS uses the pthread library, so please be sure to link it.
