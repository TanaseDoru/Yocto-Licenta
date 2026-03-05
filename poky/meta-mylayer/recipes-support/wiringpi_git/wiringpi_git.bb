# wiringpi_git.bb
# Plasează în:
#   meta-<proiect>/recipes-support/wiringpi/wiringpi_git.bb

DESCRIPTION = "A library to control Raspberry Pi GPIO channels"
HOMEPAGE = "https://github.com/WiringPi/WiringPi"
SECTION = "devel/libs"
LICENSE = "LGPL-3.0-only"
LIC_FILES_CHKSUM = "file://COPYING.LESSER;md5=e6a600fd5e1d9cbde2d983680233ad02"

# Fork oficial mentinut — ultimul tag stabil
SRCREV = "3.10"
SRC_URI = "git://github.com/WiringPi/WiringPi.git;branch=master;protocol=https"

S = "${WORKDIR}/git"

COMPATIBLE_MACHINE = "raspberrypi"

# Sintaxa noua Scarthgap (:prepend/:append in loc de _prepend/_append)
CFLAGS:prepend = "-I${S}/wiringPi -I${S}/devLib "
EXTRA_OEMAKE:append = " 'INCLUDE_DIR=${D}${includedir}' 'LIB_DIR=${D}${libdir}'"
EXTRA_OEMAKE:append = " 'DESTDIR=${D}/usr' 'PREFIX='"

do_compile() {
    # wiringPi PRIMUL — devLib si gpio depind de wiringPi.h
    oe_runmake -C wiringPi
    oe_runmake -C devLib
    oe_runmake -C gpio 'LDFLAGS=${LDFLAGS} -L${S}/wiringPi -L${S}/devLib'
}

do_install() {
    # Acelasi ordin la instalare
    oe_runmake -C wiringPi install
    oe_runmake -C devLib install
    oe_runmake -C gpio install
}