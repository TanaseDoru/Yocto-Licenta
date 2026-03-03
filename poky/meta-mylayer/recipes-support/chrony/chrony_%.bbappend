# meta-<proiect>/recipes-support/chrony/chrony_%.bbappend

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " \
    file://chrony.init \
    file://chrony.conf \
"

PACKAGECONFIG:remove = "systemd"

inherit update-rc.d

INITSCRIPT_NAME   = "chrony"
INITSCRIPT_PARAMS = "defaults 80 20"

do_install:append() {
    # Șterge directoarele goale create de upstream în /var/volatile
    # (cauza erorii QA empty-dirs — chrony upstream creează
    #  /var/volatile/lib/chrony ca director gol)
    find ${D}${localstatedir}/volatile -depth -type d -empty -delete 2>/dev/null || true
    rmdir --ignore-fail-on-non-empty -p \
        ${D}${localstatedir}/volatile 2>/dev/null || true

    # Elimină orice urmă de unit systemd
    rm -rf ${D}${systemd_system_unitdir}

    # Init script SysV
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/chrony.init \
        ${D}${sysconfdir}/init.d/chrony

    # Configurație custom (suprascrie upstream)
    install -m 0644 ${WORKDIR}/chrony.conf \
        ${D}${sysconfdir}/chrony.conf

    # Directoare runtime în căi compatibile SysVinit
    install -d ${D}${localstatedir}/lib/chrony
    install -d ${D}${localstatedir}/log/chrony


    rm -rf ${D}${localstatedir}/log
}