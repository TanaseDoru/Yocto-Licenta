# Section 3: Yocto Project Basics
	Pasi pentru a face un Software Package:
		- Fetch
		- Decompress
		- Patch
		- Configure
		- Compile
		- Install
		- Deploy & Package
		- Q/A (Quality Assurance)
	
## Directoare/Fisiere din Poky:
		- build:
			- conf
				- local.conf -> fisier de configuratii globare pentru environment
				- bblayers.conf -> Ce layere sa se adauge in environment de unde se face reteta
			- downloads 	-> aici se descarca arhive "tar" si repo de git
			- tmp 		-> Fisiere temporare dupa build
				-log -> director de log-uri 
				- deploy -> output final dupa build
					- rpm -> packages de tip rpm
					- images -> final image
					- sdk -> final sdk output
				- work	
			- cache
			- sstate-cache
### Fisiere care se gasesc in build/tmp/work/core2-64-poky-linux/dropbear (dropbear este reteta compilata prin bitbake)
		- build -> Se gasesc fisierele de build (.o)
		- deploy-rpms -> fisiere de tip rpm specific
		- dropbear-2022.83 -> Source Folder
		- image -> Fisiere output pe care sa le dai deploy la rootFS image atunci cand se alege generare de imagini
		- package -> Fisiere output pentru a da deploy la packages cand se genereaza package
		- pkgdata -> Package gen related data
		- recipe-sysroot -> Filesystem temporar care contine dependenetele pentru dropbear pentru build pentru target
		- recipe-sysroot-native -> La fel ca la recip-sysroot dar este build pentru host
		- temp -> contine log-uri pentru build
			- log.task_order -> ordinea in timpul do_patch in care se executa bitbake build
			- log.do_patch -> Log in timpu do_patch task
			- log.do_compile -> log pentru do_compile task
			- run.do_compile -> lista de comenzi py/bash care sunt executate in ordine pentru a face do_compile



# Section 4: Layers
	Avem nevoie de layere pentru a modulariza componenta de Recipe
	Layerele se afla in fisierul meta-*
	Layerele mai sus in piramida ai prioritate mai mare, iar layerele mai la baza pirmaidei au prioritate mai mica
	Ordinea executarii este de la baza la varf

## Fisierul bblayers.conf
	- build/conf/bblayers.conf
	- Aici punem calea catre acel layer


## Cum sa creezi propriul layer:
	1. Folosirea script-ului din poky
	sau 2. Copy + Paste un layer existent si modificare de continut

	
## Explicatii variabile din fisierul layers.conf dintr-un layer
	- BBPATH -> Unde cauta bitbake pentru a cauta directory de layere
	- BBFILES -> Unde cauta pentru recipe files
	Acestea doua de mai sus nu trebuiesc modificate

	- BBFILE_COLLECTIONS -> aici se pune numele layer-ului
	- BBFILE_PATTERN_<Layer-Name> -> Aici ramane LAYERDIR
	- BBFILE_PRIORITY_<Layer-Name> -> Prioritatea Layer-ului curent, acesta ajuta ca sa poata suprascriere retete identice (se va alege layer-ul cu prioritatea mai mare)
	- LAYERSERIES_COMPAT_<Layer-Name> -> Cu ce branch-uri este compatibil acest layer
	- LAYERDEPENS_<Layer-Name> -> Despre ce layere este acesta dependent
	- LAYERCOMMANDS_<Layer-Name> -> BitBake va pune aici SW Package sau alte lucruri in momentul in care se face build

### Layere dinamice:
	- BBFILES_DYNAMIC -> Putem modifica alte layere fara sa schimbam continutul acestora

# Section 5: Raspberry Pi and Basic Configuration
	Imaginea de baza pentru distributie se numeste "core-image-minimal"


	bitbake -g <image> -> -g verifica dependentele de packaged pe care <image> le are

# Section 6: Recipes

## Terminologie
	${PV}	   		-> Package Version
	${PN} 			-> Package name
	${S} 		  	-> Source Directory (${WORKDIR}/${PN}_${PV})
	${D}			  -> Destination Directory (${WORKDIR}/Image)
	${B}		  	-> Build Directory (De obicei este ${S} sau ${WORKDIR}/build)
	${includedir}		-> Header files (.h) (${D}/usr/include)
	${bindir}		-> ${D}/usr/bin
	${libdir}		-> ${D}/usr/lib
	${WORKDIR}		-> poky/build/tmp/work/.../myapp/1.0/
	${STAGING_DIR_NATIVE}	-> Instalate instrumente native (recipe-sysroot-native)
	${STAGING_DIR_TARGET}	-> directorul "recipe-sysroot"
	${sysconfdir} 		-> ${D}${prefix}/etc
	bitbake <Recipe> -c cleanall -> sterge output-ul de la build din poky/build/tmp/work
## Task-urile pentru generarea unui build	
### Continutul fisierului poky/build/tmp/work/cortexa.../nano/7.2/temp/log.task_order
	- do_recipe_qa -> Quality Assurance: Aici Bitbake verifica daca reteta este corecta (definirea variabilelor de mediu) ex: SRC_URI
	- do_fetch -> Source_URI va fi descarcat in directorul curent
	- do_unpack -> Face unpack la ce s-a descarcat din pasul de fetch
	- do_prepare_recipe_sysroot -> Pregateste reteta sysroot: sysroot = depententele sunt copiate din work directory in recipe-sysroot
		- Librariile se gasesc in recipe-sysroot, iar tools se gasesc in recipe-sysroot-native
	- do_collect_spdx_deps -> standard pentru a descrie software packages
	- do_patch -> in SRC_URI vor fi adaugate variabile si vor fi patch-urite peste sursa initiala folosind un patching tool(default Quilt)
	- do_deploy_source_date_epoch -> nerelevant
	- do_populate_lic -> setat prin variabile LICENCE si LIC_FILES_CHKSUM (Folosit pentru licentiere: locatie) (ex: LICENSE = "CLOSED" -> Nu avem licenta)
	- do_configure -> Task de configurare (script de configurare) 
	- do_compile -> de obicei comenzi de tip GCC sau MAKE 
	- do_install -> face "run make install"
	- do_package -> task pentru packages
	- do_packagedata -> nerelevant
	- do_create_spdx -> software package related info
	- do_package_qa -> QA pentru package
	- do_package_write_rpm -> genereaza RPM output
	- do_create_runtime_spdx -> nerelevant
	- do_populate_sysroot -> Populeaza sysroot si face verificari pentru fisierele binare

### Task-urile principale
	- fetch
	- unpack
	- patch
	- configure
	- compile
	- install
	- package

## Cum sunt construite Retetele
### Exeplu Reteta:
	- DESCRIPTION = "Desc" -> Descrierea package-ului 
		- sau SUMMERY
	- SECTION -> Categorie pentru package (optional)
	- LICENCE -> Licenta
	- LIC_FILES_CHKSUM
	- PR -> package revision (optional)
	- PV -> Package version (optional)
	- PN -> Package name (optional)
	- SRC_URI -> URI catre sursa de pe care se face FETCH in Work Directory (Fisiere/foldere locale sau repo de git )
	- functia do_compile () { 
		${CC} ${WORKDIR}/helloyp.c -o ${WORKDIR}/helloyp 
		# WORKDIR -> Directorul poky/build/tmp/work/{locatie}/nano/7.2
	}
	- functia do_install () { 
		install -d ${D}${bindir}
		install -m 0755 ${WORKDIR}/helloyp ${D}${bindir}/ # -> Compiaza helloyp in image/usr/bin
		# ${D} 		= ${WORKDIR}/image (Destination directory)
		# ${bindir} 	= /usr/bin (alias)
		# ${S} 		= Source directory
		# ${B} 		= build dirextory
	}
	- python do_sometask () { ... } -> Avem un task folosind python

	Exemplul de reteta a fost facut in meta-mylayer/recipes-devtools/helloworldc

## Notes
	- DISTRO_FEATURES = Acel recipe va functiona doar daca avem acele distro FEATURE in local.conf
	- DISTRO_FEATURES in local.conf
	- REQUIRED_DISTRO_FEATURES in recipe file

# Section 8: Application SDK and DEVTOOL

## Meta-toolchain:
	- meta-toolchain -> App SDK generation (bitbake meta-toolchain)
	meta-toolchain.bb face referire la populate_sdk.bbclass care face referire la populate_sdk_base.bbclass

### Variabile din populate_sdk_base.bbclass
	TOOLCHAIN_HOST_TASK = " " (ex : " qtbase libopenssl ") -> Task-uri pentru app development  
	TOOLCHAIN_TARGET_TASK -> Target libraries care vor fi puse in target-sysroot
	Ce TASK-uri punem in aceste variabile, vor fi puse in *tmp/deploy/sdk*
	Aestea vor crea un script care incapsuleaza aceste task-uri . Instalation script: (tmp/deploy/sdk/<...>/toolchain.sh)
### Instalare/Folosire SDK
	Dupa ce se rukleaza script-ul vom avea un Filesystem cu mai multe fisiere si directoare:
		-> environment-setup-... 			::script pentru a incepe cross-compilarea
		-> site-config-target-poky-linux		::
		-> version-target-poky-linux			::
		-> sysroots
			-> target-poky-linux 			::target sysroot -> libarii pentru compilare
				-> ...
			-> host-pokysdk-linux			::host sysroot -> compilation tools
				-> ...

	- rulam environment-setup-...
	- cd myapp
	- make all
	
## Conclution
	- SDK este folosit pentru a seta mediul de compilare si este util pentru cross compilare. Astfel dupa ce rulam environemnt-setup imi face setup-ul pentru mediul pe care vreau sa rulez
	- De ex daca un SDK pentru qt-6 pentru arhitectura ARM-64 dupa ce setez mediul pot face make si sa compilez un program pentru statia TARGET
## Notes:
	- Pentru ca SDK sa fie valid, am nevoie de un Makefile care se foloseste de variabilele de mediu (ex: $(CXX)), nu de comenzi predefinite
	- Ex: Pentru a avea include-uri din SDK avem nevoie sa specificam INCLUDES += -I$(OECORE_TARGET_SYSROOT)/usr/include

## DEVTOOL
	. oe-init-build-env
	devtool modify <recipe>
	- Este folosit pentru a face mai usoara modificari ulterioare aduse unor retete (Sa nu facem override pe SRC_URI cu un fisier local)
	- Se creaza directorul poky/build/workspace unde se salveaza fisierele astfel:
		- ./appends 		:: Director care contine fisierele .bbappend pentru retetele pe care s-au folosit DEVTOOL
		- ./sources		:: Sursele retetelor peste care s-au folosit devtool. In aceste surse putem face modificarile necesare
	- Dupa care trebuie sa face patching -> Adica un commit daca avem un repo de git
	- Dupa care trebuie sa facem
		devtool update-recipe <recipe> -a <layerpath> 
		Pentru a face write pe layer cu .bbappend
	- La final trebuie sa facem
		devtool reset <recipe>
		Pentru a sterge fisierele din workspace
		In plus, putem sterge si directorul workspace, si sa stergem directorul workspace din bblayers
	
# Section 9: Machines:
    - Pentru a seta machine: se pune in build/conf/local.conf:
      - MACHINE = ""
## Variabile din fisierul Machine
    - TARGET_ARCH -> Architecture of the processor 
    - PREFFERED_PROVIDER_virtual/kernel -> setam un recipe pentru Kernel 
    - MACHINE_FEATURES 
    - SERIAL_CONSOLES
    - KERNEL_IMAGETYPE -> What kind of image is the kernel(compressed, u-boot image etc. )
    - IMAGE_FSTYPE -> output format 
    - KERNEL_DEVICETREE -> Specifica deviceTree-ul Kernel-ului
    - UBOOT_MACHINE -> Daca avem suport pentru u-boot 

    Pentru a elimina un feature se poate face:
      - MACHINE_FEATURES:remove = "keyboard" - intr-un layer separat pentru a face override separat, si sa nu modificam direct pe configurarile din layerele clonate




































