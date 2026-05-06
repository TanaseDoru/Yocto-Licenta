SUMMARY = "Tailscale VPN client and daemon"
DESCRIPTION = "Installs tailscale and tailscaled from the official release tarball \
and manages the daemon via a SysV init script."
LICENSE = "CLOSED"

TAILSCALE_VERSION = "1.96.2"
TAILSCALE_ARCH = "arm64"

SRC_URI = " \
    https://pkgs.tailscale.com/stable/tailscale_${TAILSCALE_VERSION}_${TAILSCALE_ARCH}.tgz;downloadfilename=tailscale_${TAILSCALE_VERSION}_${TAILSCALE_ARCH}.tgz \
    file://tailscaled-init \
"

SRC_URI[sha256sum] = "9ad430177fcdd0c236684a9e22da6d0f9afb785231b400765180b10ca016f2f2"

# The tarball extracts into a directory named tailscale_<version>_<arch>
S = "${WORKDIR}/tailscale_${TAILSCALE_VERSION}_${TAILSCALE_ARCH}"

inherit update-rc.d

INITSCRIPT_NAME = "tailscaled"
INITSCRIPT_PARAMS = "defaults 20"

# No compilation needed — pre-built Go binaries
do_compile() {
    :
}

do_install() {
    # Binaries
    install -d ${D}${bindir}
    install -m 0755 ${S}/tailscale  ${D}${bindir}/tailscale
    install -m 0755 ${S}/tailscaled ${D}${bindir}/tailscaled

    # State directory
    install -d ${D}/var/lib/tailscale

    # Init script
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/tailscaled-init ${D}${sysconfdir}/init.d/tailscaled
}

FILES:${PN} += " \
    ${bindir}/tailscale \
    ${bindir}/tailscaled \
    ${sysconfdir}/init.d/tailscaled \
    /var/lib/tailscale \
"