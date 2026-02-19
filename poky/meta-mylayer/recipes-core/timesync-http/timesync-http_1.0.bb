SUMMARY = "HTTP Date based time synchronization init script"
LICENSE = "CLOSED"
SRC_URI = "file://timesync-http.init"

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "timesync-http"
INITSCRIPT_PARAMS = "defaults 20"

do_install() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/timesync-http.init ${D}${sysconfdir}/init.d/timesync-http
}

FILES:${PN} += "${sysconfdir}/init.d/timesync-http"
