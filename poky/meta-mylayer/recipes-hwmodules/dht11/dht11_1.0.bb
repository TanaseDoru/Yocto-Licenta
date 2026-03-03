# dht11_1.0.bb
#
# Recipe Yocto pentru clientul DHT11 — SysV init.
# Plasează în:
#   meta-<proiect>/recipes-sensors/dht11-client/dht11-client_1.0.bb
#
# Structura fișierelor:
#   meta-<proiect>/recipes-sensors/dht11-client/files/
#       dht11.h
#       dht11.c
#       dht11-client.c
#       dht11.conf
#       dht11-client.init

SUMMARY = "DHT11 temperature and humidity sensor client for data-collector"
DESCRIPTION = "Citeste temperatura si umiditatea de la senzorul DHT11 \
prin bit-banging GPIO (libgpiod) si le trimite prin Unix socket \
catre data-collector."
LICENSE = "CLOSED"

# -----------------------------------------------------------------------
# Surse locale
# -----------------------------------------------------------------------
SRC_URI = " \
    file://dht11.h \
    file://dht11.c \
    file://dht11-client.c \
    file://dht11.conf \
    file://dht11-client.init \
"

S = "${WORKDIR}"

# -----------------------------------------------------------------------
# Dependențe — libgpiod pentru acces GPIO userspace
# -----------------------------------------------------------------------
DEPENDS = "libgpiod"

inherit update-rc.d

INITSCRIPT_NAME   = "dht11-client"
INITSCRIPT_PARAMS = "defaults 91 10"

# -----------------------------------------------------------------------
# Compilare
# -----------------------------------------------------------------------
do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -O2 -Wall -Wextra \
        -I${S} \
        ${S}/dht11.c ${S}/dht11-client.c \
        -lgpiod \
        -o ${S}/dht11-client
}

# -----------------------------------------------------------------------
# Instalare
# -----------------------------------------------------------------------
do_install() {
    # Binar
    install -d ${D}${bindir}
    install -m 0755 ${S}/dht11-client ${D}${bindir}/dht11-client

    # Configurație
    install -d ${D}${sysconfdir}
    install -m 0644 ${S}/dht11.conf ${D}${sysconfdir}/dht11.conf

    # Init script SysV
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${S}/dht11-client.init \
        ${D}${sysconfdir}/init.d/dht11-client
}

FILES:${PN} = " \
    ${bindir}/dht11-client \
    ${sysconfdir}/dht11.conf \
    ${sysconfdir}/init.d/dht11-client \
"