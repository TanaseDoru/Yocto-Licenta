SUMMARY = "Wifi Configuration using connamnn"
DESCRIPTION = "Automatically connects to a predefined WiFi network at boot"
LICENSE = "CLOSED"
SRC_URI = "file://wifi.config"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${localstatedir}/lib/connman/
    install -m 0644 ${WORKDIR}/wifi.config ${D}${localstatedir}/lib/connman/
}

FILES:${PN} = "${localstatedir}/lib/connman/wifi.config"
