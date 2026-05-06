SUMMARY = "OpenVPN client configured for ProtonVPN"
DESCRIPTION = "Installs OpenVPN with a baked-in ProtonVPN .ovpn config file. \
Credentials (username/password) must be placed manually on the target \
at /etc/openvpn/credentials.txt after flashing."
LICENSE = "CLOSED"

SRC_URI = " \
    file://client.ovpn \
    file://openvpn-init \
"

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "openvpn-client"
INITSCRIPT_PARAMS = "defaults 21"

# openvpn binary is already provided by the 'openvpn' package in your IMAGE_INSTALL
RDEPENDS:${PN} = "openvpn"

do_compile() {
    :
}

do_install() {
    # OpenVPN config directory
    install -d ${D}${sysconfdir}/openvpn

    # Baked-in .ovpn config (no credentials inside — safe to bake)
    install -m 0644 ${WORKDIR}/client.ovpn ${D}${sysconfdir}/openvpn/client.ovpn

    # Credentials placeholder — reminds the user what to create post-flash
    # The actual file must be placed manually: /etc/openvpn/credentials.txt
    # Format:
    #   <ProtonVPN OpenVPN username>
    #   <ProtonVPN OpenVPN password>
    echo "# Place your ProtonVPN OpenVPN credentials here" \
        > ${WORKDIR}/credentials.txt.example
    echo "# Line 1: username" >> ${WORKDIR}/credentials.txt.example
    echo "# Line 2: password" >> ${WORKDIR}/credentials.txt.example
    install -m 0600 ${WORKDIR}/credentials.txt.example \
        ${D}${sysconfdir}/openvpn/credentials.txt.example

    # Init script
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/openvpn-init \
        ${D}${sysconfdir}/init.d/openvpn-client
}

FILES:${PN} += " \
    ${sysconfdir}/openvpn/client.ovpn \
    ${sysconfdir}/openvpn/credentials.txt.example \
    ${sysconfdir}/init.d/openvpn-client \
"