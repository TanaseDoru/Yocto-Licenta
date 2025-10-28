SUMMARY = "DHT11 temperature/humidity sensor reader using libgpiod"
DESCRIPTION = "Reads DHT11 sensor via libgpiod with GPIO4 on Raspberry Pi"
LICENSE = "CLOSED"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://driver_dht11.c \
           file://driver_dht11.h \
           file://dht11_gpio.c \
           file://dht11_gpio.h \
           file://main.c"

S = "${WORKDIR}"

DEPENDS = "libgpiod"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        driver_dht11.c dht11_gpio.c main.c \
        -o dht11-reader \
        -lgpiod -lrt
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 dht11-reader ${D}${bindir}/
}
