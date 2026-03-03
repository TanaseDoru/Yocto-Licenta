# gy63-client_1.0.bb
#
# Recipe Yocto pentru clientul GY-63 (MS5611, SPI) — SysV init.
# Plasează acest fișier în:
#   meta-<proiect>/recipes-sensors/gy63-client/gy63-client_1.0.bb
#
# Structura așteptată a surselor:
#   meta-<proiect>/recipes-sensors/gy63-client/files/
#       gy63.h
#       gy63.c
#       gy63-client.c
#       gy63.conf
#       gy63-client.init

SUMMARY = "GY-63 (MS5611) SPI sensor client for data-collector"
DESCRIPTION = "Citeste presiunea si temperatura de la senzorul GY-63 \
prin SPI si le trimite prin Unix domain socket catre data-collector."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

# -----------------------------------------------------------------------
# Surse locale
# -----------------------------------------------------------------------
SRC_URI = " \
    file://gy63.h \
    file://gy63.c \
    file://gy63-client.c \
    file://gy63.conf \
    file://gy63-client.init \
"

S = "${WORKDIR}"

# -----------------------------------------------------------------------
# Dependente
# -----------------------------------------------------------------------
DEPENDS = "linux-libc-headers"

inherit update-rc.d

# start la runlevel 2-5 (priority 90), stop la 0,1,6 (priority 10)
INITSCRIPT_NAME   = "gy63-client"
INITSCRIPT_PARAMS = "defaults 90 10"

# -----------------------------------------------------------------------
# Compilare
# -----------------------------------------------------------------------
do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -O2 -Wall -Wextra \
        -I${S} \
        ${S}/gy63.c ${S}/gy63-client.c \
        -lm \
        -o ${S}/gy63-client
}

# -----------------------------------------------------------------------
# Instalare
# -----------------------------------------------------------------------
do_install() {
    # Binar
    install -d ${D}${bindir}
    install -m 0755 ${S}/gy63-client ${D}${bindir}/gy63-client

    # Configuratie implicita
    install -d ${D}${sysconfdir}
    install -m 0644 ${S}/gy63.conf ${D}${sysconfdir}/gy63.conf

    # Init script in /etc/init.d/
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${S}/gy63-client.init \
        ${D}${sysconfdir}/init.d/gy63-client
}

FILES:${PN} = " \
    ${bindir}/gy63-client \
    ${sysconfdir}/gy63.conf \
    ${sysconfdir}/init.d/gy63-client \
"