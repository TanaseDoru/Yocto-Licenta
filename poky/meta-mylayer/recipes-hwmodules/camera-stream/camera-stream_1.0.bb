SUMMARY = "RPi Camera Stream Pusher"
DESCRIPTION = "Captures YUYV frames from the RPi camera via V4L2, compresses \
them to JPEG on the CPU with libjpeg-turbo (bypassing GPU MMAL/VCHIQ encoding \
to prevent firmware timeout crashes), and pushes them to the Flask backend."
LICENSE = "CLOSED"

SRC_URI = " \
    file://camera-stream.c \
    file://camera-stream.h \
    file://camera-stream.initd \
    file://stream.conf \
"

S = "${WORKDIR}"

RDEPENDS:${PN} = " \
    libcurl \
    libjpeg-turbo \
    v4l-utils \
    kernel-module-bcm2835-v4l2 \
"

DEPENDS = "curl jpeg"

inherit update-rc.d pkgconfig

INITSCRIPT_NAME   = "camera-stream"
INITSCRIPT_PARAMS = "defaults 92"

TARGET_CC_ARCH += "${LDFLAGS}"

do_compile() {
    ${CC} ${CFLAGS} \
        camera-stream.c \
        -o camera-stream \
        $(pkg-config --cflags --libs libcurl) \
        $(pkg-config --cflags --libs libjpeg) \
        -lpthread -lm
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/camera-stream ${D}${bindir}/camera-stream

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/camera-stream.initd \
        ${D}${sysconfdir}/init.d/camera-stream

    install -d ${D}${sysconfdir}/camera-stream
    install -m 0600 ${WORKDIR}/stream.conf \
        ${D}${sysconfdir}/camera-stream/stream.conf
}

FILES:${PN} = " \
    ${bindir}/camera-stream \
    ${sysconfdir}/init.d/camera-stream \
    ${sysconfdir}/camera-stream/stream.conf \
"
