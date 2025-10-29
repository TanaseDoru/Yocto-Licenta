SUMMARY = "A Simple button module for using GPIO5 that will send the signal to the rpi"
DESCRIPTION = "DHT11 Data collection using libgpio v1.6"
LICENSE = "CLOSED"


SRC_URI = "file://buttonmodule.c"
S = "${WORKDIR}"

DEPENDS = "libgpiod"
RDEPENDS:${PN} = "libgpiod"

do_compile() {
    ${CC}   -o buttonmodule buttonmodule.c -lgpiod ${CFLAGS} ${LDFLAGS}
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 buttonmodule ${D}${bindir}
}
