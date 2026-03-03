SUMMARY = "Update server URLs for data-collector and image-uploader"
DESCRIPTION = "Provides /usr/sbin/url-updater to change server URLs in configs and signal daemons to reload (SIGHUP) or start them."
LICENSE = "CLOSED"

SRC_URI = " \
    file://url-updater \
"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sbindir}
    install -m 0755 ${WORKDIR}/url-updater ${D}${sbindir}/url-updater

    # Optional: symlinks for convenience
    ln -sf url-updater ${D}${sbindir}/update-data-collector-url
    ln -sf url-updater ${D}${sbindir}/update-image-uploader-url
}

FILES:${PN} = " \
    ${sbindir}/url-updater \
    ${sbindir}/update-data-collector-url \
    ${sbindir}/update-image-uploader-url \
"