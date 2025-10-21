SUMMARY = "Wifi Configuration using connamnn"
DESCRIPTION = "Automatically connects to a predefined WiFi network at boot"
LICENSE = "CLOSED"

# Include ambele fișiere în SRC_URI
SRC_URI = " \
    file://wifi_TP-LINK_D33094_managed_psk.config \
    file://wifi_Network-Name_managed_psk.config \
"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${localstatedir}/lib/connman/
    
    # Instalează primul fișier
    install -m 0600 ${WORKDIR}/wifi_TP-LINK_D33094_managed_psk.config ${D}${localstatedir}/lib/connman/
    
    # Instalează al doilea fișier
    install -m 0600 ${WORKDIR}/wifi_Network-Name_managed_psk.config ${D}${localstatedir}/lib/connman/
}

# Include ambele fișiere în lista de instalare a pachetului
FILES:${PN} = " \
    ${localstatedir}/lib/connman/wifi_TP-LINK_D33094_managed_psk.config \
    ${localstatedir}/lib/connman/wifi_Network-Name_managed_psk.config \
"