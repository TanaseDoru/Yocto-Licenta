SUMMARY = "BMP180 sensor client over I2C"
DESCRIPTION = "Reads BMP180 sensor values and sends them to data-collector via UNIX socket"
LICENSE = "CLOSED"

SRC_URI = "file://bmp180-client.c \
           file://bmp180.c \
           file://bmp180.h \
           file://bmp180-init \
           file://bmp180.conf \
          "

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "bmp180-client"
INITSCRIPT_PARAMS = "defaults 99"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o bmp180-client bmp180-client.c bmp180.c
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 bmp180-client ${D}${bindir}/

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/bmp180-init ${D}${sysconfdir}/init.d/bmp180-client

    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/bmp180.conf ${D}${sysconfdir}/bmp180.conf
}

FILES:${PN} += " \
    ${sysconfdir}/init.d/bmp180-client \
    ${sysconfdir}/bmp180.conf \
"