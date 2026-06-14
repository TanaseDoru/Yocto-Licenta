SUMMARY = "A data collector from all the sensors and sender"
DESCRIPTION = "Collecting data from all the sensors and sending them via Tailscale to web server"
LICENSE = "CLOSED"

SRC_URI = "file://data-collector.c \
           file://data-collector.h \
           file://init-script \
           file://api.key \
           file://server.conf \
          "

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "data-collector"
INITSCRIPT_PARAMS = "defaults 99"

DEPENDS = "curl"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o data-collector data-collector.c -lcurl -lpthread -lm
}

do_install() {
    # Binary
    install -d ${D}${bindir}
    install -m 0755 data-collector ${D}${bindir}/

    # Init script
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/init-script ${D}${sysconfdir}/init.d/data-collector

    # Config directory
    install -d ${D}${sysconfdir}/data-collector

    # Server URL (Tailscale IP of the server machine)
    install -m 0644 ${WORKDIR}/server.conf ${D}${sysconfdir}/data-collector/server.conf

    # API key (kept 0600 so only root can read it)
    install -m 0600 ${WORKDIR}/api.key ${D}${sysconfdir}/data-collector/api.key

    # Offline queue directory (persists across reboots on rootfs)
    install -d ${D}/var/spool/data-collector
}

FILES:${PN} += " \
    ${sysconfdir}/init.d/data-collector \
    ${sysconfdir}/data-collector/server.conf \
    ${sysconfdir}/data-collector/api.key \
    /var/spool/data-collector \
"