SUMMARY = "A Simple button module for using GPIO26 that will send the signal to the rpi"
DESCRIPTION = "Sending the data received from the button to the rpi on GPIO26"
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
