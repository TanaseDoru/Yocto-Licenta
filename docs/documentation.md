# Lista de definitii
## Append files:
- Files that append to the recipe files(.bbappend files)
- Every append file should have a .bb file
- Must use the same rootfile as .bb file
- % can be used to allow for matching recipe names (ex: busybox_1.%.bbappend should work
 for busbox_1.1.bbappend or busybox_1.2.bbappend etc)
- % may only be used in front of .bbappend

## BitBake
- Task executor and scheduler

## Board Support Package(BSP)
- A group of drivers, definitions and other components that provide
support for specific HW conf

## Build Directory
- The are used for build
- $TOPDIR points to the build dir
- Create custom build dir: source oe-init-build-env <NAME_DIR>

## Build Host
- System used to build images

## Buildtools
- Build tools in binary form(git, gcc, python, make)

## Classes
- Classes that are being used as normal
- .bbclass file extention

## Configuration file
- Files that hold definitions, variables, HW config
- .conf file extention
- 

