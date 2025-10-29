SUMMARY = "DHT11 kernel module for Raspberry Pi"
LICENSE = "CLOSED"

inherit module

SRC_URI = "file://dht11_kmod.c file://Makefile"
S = "${WORKDIR}"

do_compile[depends] += "virtual/kernel:do_shared_workdir"

INSTALL_MOD_DIR = "extra"

# Autoload optional
KERNEL_MODULE_AUTOLOAD += "dht11_kmod"

# Skip kernel's 'modules_install' and copy manually
do_install() {
    install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/${INSTALL_MOD_DIR}
    install -m 0644 ${B}/dht11_kmod.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/${INSTALL_MOD_DIR}/
}
