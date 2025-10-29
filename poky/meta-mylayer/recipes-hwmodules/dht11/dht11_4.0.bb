SUMMARY = "[WARNING] NOT WORKING. DHT11 Data Collector using libgpiod for Raspberry Pi"
DESCRIPTION = "DHT11 Data collection using libgpio v1.6"
LICENSE = "CLOSED"


SRC_URI = "file://dht11_collector.c"
S = "${WORKDIR}"

DEPENDS = "libgpiod"
RDEPENDS:${PN} = "libgpiod"

do_compile() {
    ${CC}   -o dht11_collector dht11_collector.c -lgpiod ${CFLAGS} ${LDFLAGS}
}


do_install() {
    install -d ${D}${bindir}
    install -m 0755 dht11_collector ${D}${bindir}
}
