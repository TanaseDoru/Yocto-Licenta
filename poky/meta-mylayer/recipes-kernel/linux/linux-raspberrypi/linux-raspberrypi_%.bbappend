# Adaugă fișierul tău de configurare a kernel-ului la lista de surse (SRC_URI)
# Yocto il va folosi apoi pentru a configura kernel-ul inainte de compilare.
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI:append = " file://dht11.cfg"

# Setează configurarea kernel-ului (KCONFIG_MODE)
# 'merge' este modul recomandat: adaugă/modifică doar setarile din cfg,
# lăsând restul configurației kernel-ului intactă.
KCONFIG_MODE = "merge"