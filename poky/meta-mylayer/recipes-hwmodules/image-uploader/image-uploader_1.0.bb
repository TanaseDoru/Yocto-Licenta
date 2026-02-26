SUMMARY = "RPi Camera Image Uploader"
DESCRIPTION = "Captures JPEG images from RPi Camera via v4l2-ctl and uploads them to the central Flask server via HTTP multipart POST. \
Reads configuration from /etc/image-uploader/uploader.conf."
LICENSE = "CLOSED"


SRC_URI = " \
    file://image-uploader.c \
    file://image-uploader.h \
    file://image-uploader.initd \
    file://uploader.conf \
"

S = "${WORKDIR}"

# Runtime dependencies
RDEPENDS:${PN} = " \
    libcurl \
    v4l-utils \
    kernel-module-bcm2835-v4l2 \
"

# Build dependencies
DEPENDS = "curl"

inherit update-rc.d pkgconfig

INITSCRIPT_NAME   = "image-uploader"
INITSCRIPT_PARAMS = "defaults 91"

# Flags de compilare
TARGET_CC_ARCH += "${LDFLAGS}"

do_compile() {
    ${CC} ${CFLAGS} \
        image-uploader.c \
        -o image-uploader \
        $(pkg-config --cflags --libs libcurl) \
        -lpthread
}

do_install() {
    # Binar
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/image-uploader ${D}${bindir}/image-uploader

    # Init script SysVinit
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/image-uploader.initd \
        ${D}${sysconfdir}/init.d/image-uploader

    # Fisier de configurare
    install -d ${D}${sysconfdir}/image-uploader
    install -m 0644 ${WORKDIR}/uploader.conf \
        ${D}${sysconfdir}/image-uploader/uploader.conf

    # Director pentru imaginile capturate
    install -d ${D}/var/lib/image-uploader
}

FILES:${PN} = " \
    ${bindir}/image-uploader \
    ${sysconfdir}/init.d/image-uploader \
    ${sysconfdir}/image-uploader/uploader.conf \
    /var/lib/image-uploader \
"