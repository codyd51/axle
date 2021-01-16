#!/bin/bash
# https://gist.github.com/EmbeddedAndroid/6572715
# https://gist.github.com/artembeloglazov/db8c16efc91443955fca
echo "Executing /etc/qemu-ifup"
echo "Creating bridge"
sysctl -w net.link.ether.inet.proxyall=1
sysctl -w net.inet.ip.forwarding=1
sysctl -w net.inet.ip.fw.enable=1
sysctl -w net.ipv4.ip_forward=1
sysctl -w net.link.tap.user_open=1
sysctl -w net.link.tap.up__on__open=1
ifconfig en0 down
ifconfig en0 inet delete
ifconfig bridge2 create
# ifconfig bridge2 192.168.13.1
ifconfig bridge2 inet 192.168.13.1/24
echo "Bringing up $1 for bridged mode"
# ifconfig $1 192.168.13.2 up
echo "Add $1 to bridge"
ifconfig bridge2 addm en0 addm $1
echo "Bring up bridge"
ifconfig bridge2 up
# ifconfig tap0 192.168.13.2 up
# route add -host 0.0.0.0 dev tap0 # add route to the client