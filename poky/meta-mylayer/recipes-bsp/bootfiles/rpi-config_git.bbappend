do_deploy:append() {
   echo "dtoverlay=uart0" >> $CONFIG
   echo "dtoverlay=dht11,gpiopin=4" >> $CONFIG
}