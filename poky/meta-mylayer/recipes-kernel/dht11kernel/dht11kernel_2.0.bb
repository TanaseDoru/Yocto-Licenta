SUMMARY = "DHT11 kernel module for Raspberry Pi"
# Poti folosi GPL-2.0, deoarece codul sursa il are. 'CLOSED' nu este ideal pentru un driver
LICENSE = "CLOSED"

# Pastreaza: mosteneste clasa 'module'
inherit module

SRC_URI = "file://dht11_kmod.c file://Makefile"
S = "${WORKDIR}"

# Pastreaza: asigura dependenta de kernel
do_compile[depends] += "virtual/kernel:do_shared_workdir"

# Pastreaza: specifica directorul de instalare (sub /lib/modules/version/)
INSTALL_MOD_DIR = "extra"

# Pastreaza: activeaza incarcarea automata la boot
KERNEL_MODULE_AUTOLOAD += "dht11_kmod"

# Temeiul rezolvarii: ELIMINA FUNCTIA MANUALĂ do_install()
# Clasa 'module' va genera automat o functie do_install() corecta
# bazata pe INSTALL_MOD_DIR si va evita eroarea Pseudo.

# Adauga aceasta pentru a te asigura ca pachetul final include modulul
FILES:${PN} += "/lib/modules/${KERNEL_VERSION}/extra/dht11_kmod.ko"
# Skip kernel's 'modules_install' and copy manually
do_install() {
    install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/${INSTALL_MOD_DIR}
    install -m 0644 ${B}/dht11_kmod.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/${INSTALL_MOD_DIR}/
}
