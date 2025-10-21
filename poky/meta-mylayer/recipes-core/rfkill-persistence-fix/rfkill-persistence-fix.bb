# Calea de plasare: meta-mylayer/recipes-kernel/rfkill/rfkill-persistence-fix.bb

SUMMARY = "Fix pentru Soft-Block persistent prin dezactivarea salvarii starii Rfkill"
DESCRIPTION = "Instaleaza un fisier de configurare modprobe pentru a preveni modulul kernel rfkill sa salveze si sa restaureze starea blocata a radioului."
LICENSE = "CLOSED"

# Clasa features_check este esentiala pentru QA in Scarthgap
inherit features_check

SRC_URI = "file://rfkill-options.conf"

S = "${WORKDIR}"

do_install() {
    # modprobe.d este directorul standard pentru configurarile modulelor
    install -d ${D}${sysconfdir}/modprobe.d
    # Instaleaza fisierul de optiuni in directorul modprobe.d
    install -m 0644 ${WORKDIR}/rfkill-options.conf ${D}${sysconfdir}/modprobe.d/
}

FILES:${PN} = "${sysconfdir}/modprobe.d/rfkill-options.conf"