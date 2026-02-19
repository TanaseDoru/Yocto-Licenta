SUMMARY = "Script de test pentru Raspberry Pi Camera Rev 1.3"
LICENSE = "CLOSED"

SRC_URI = " \
    file://camera-test.sh \
    file://camera-test.initd \
"

S = "${WORKDIR}"

RDEPENDS:${PN} = "v4l-utils bash"

inherit update-rc.d

INITSCRIPT_NAME = "camera-test"
INITSCRIPT_PARAMS = "defaults 90"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/camera-test.sh ${D}${bindir}/camera-test.sh

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/camera-test.initd \
        ${D}${sysconfdir}/init.d/camera-test
}

FILES:${PN} = " \
    ${bindir}/camera-test.sh \
    ${sysconfdir}/init.d/camera-test \
"