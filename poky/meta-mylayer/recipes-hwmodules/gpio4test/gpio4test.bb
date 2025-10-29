SUMMARY = "Test for GPIO4"
LICENSE = "CLOSED"


SRC_URI = "file://gpio4test.c"
S = "${WORKDIR}"

DEPENDS = "libgpiod"
RDEPENDS:${PN} = "libgpiod"

do_compile() {
    ${CC}   -o gpio4test gpio4test.c -lgpiod ${CFLAGS} ${LDFLAGS}
}


do_install() {  
    install -d ${D}${bindir}
    install -m 0755 gpio4test ${D}${bindir}
}
