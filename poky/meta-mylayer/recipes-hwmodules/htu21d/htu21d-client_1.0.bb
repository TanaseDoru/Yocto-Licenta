SUMMARY = "HTU21D sensor client over I2C"
DESCRIPTION = "Reads HTU21D sensor values and sends them to data-collector via UNIX socket"
LICENSE = "CLOSED"

SRC_URI = "file://htu21d-client.c \
           file://htu21d.c \
           file://htu21d.h \
           file://htu21d-init \
          "

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "htu21d-client"
INITSCRIPT_PARAMS = "defaults 99"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o htu21d-client htu21d-client.c htu21d.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 htu21d-client ${D}${bindir}/

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/htu21d-init ${D}${sysconfdir}/init.d/htu21d-client
}

FILES:${PN} += "${sysconfdir}/init.d/htu21d-client"
