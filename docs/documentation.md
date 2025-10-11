# List of definitions
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
- conf/local.conf file affect every build

## Container Layer
- A directory structure which contains mutiple valid OpenEmbedded layers

## Cross-Development Toolchain
- Collection of SW dev tools that allow development for a diferent arch
- In yocto project we have Bitbake and a relocatable tollchain out side Bitbake

## Extensible Software Development Kit (eSDK)
- A custom SDK for application developers

## Image
- Arifact of the BitBake build process given collection of recipes and 
related Metadata

## Initramfs
- It is optional
- Initial RAM Filesystem
- Used for replacing the init RAM disk(initrd) technique
- Is an archive extracted by the Kernel into RAM in special tmpfs
used as initial root filesystem

## Layer
- A collection of related recipes
- Hierarchical structure(will override previous specs)

## LTS (Long Term Support)
- Selecting stable releases for bugs and security fixes are
provided for at least four years

## Metadata
- Includes recipes, conf files and other information refering
build instructions and things that affects the build or how it's
being built

## Mixin
- Mixin layer is a lyer created by the community to add specific
feature or support a new verios of some package for LTS release

## OpenEmbedded-Core (OE-Core)
- OE-Core is metadata that includes foundational recipes, classes
and associcated files that are common among different OE-derived systems

## OpenEmbedded Build System
- Build system specific to the Yocto Project
- Is based on "poky" which uses BitBake as the task executor

## Package
- Recpie's packaged output prodced by BitBake
- Is the compiled binaries produced from the recipe's sources

## Package Groups
- Groups of software Recpies
- Package groups end with .bb extention

## Poky
- Is a reference embedded distro and a reference test configuration
- Provides a base-level function distro, a means to test the Yocto
Project components and a vehicle through which you can download the
Yocto project
- It is just a good starting point
- As chatGTP describes it:
Poky = BitBake + OE-Core + Reference metadata + Example distro config

## Recipe
- Set of instructions for building packages
- Logical unit of execution
- .bb file extentention

## SBOM (Software Bill of Materials)
- Documentation for the project
- Description of all the components used, licences, dependencies,
changes 

## Source Directory
- The directory structure made by copying the repo of "poky"
- File or directory names must not use spaces

## SPDX (Software Package Data Exchange)
- Is used as an open standard for SBOM through Linux Foundation
Project

## Sysroot
- The directory made to look like the target filesystem
- Contains C library and kernel headers and binaries for the C lib
- Each recipe has a target sysroot that contains all the target libs
and headers needed to build the recipe and the native sysroot that
contains all the host files and execs needed to build the recipe

## Task
- A per-recipe unit of execution for BitBake (do_compile, do_fetch)
- Can be excuted in prallel

## Toaster
- Web interface to the Yocto Project's OpenEmbedded Build System
- Enables you to configure and run your builds

## Upstream
- Source code or repos that are not local to the development system
but located in a remote area

