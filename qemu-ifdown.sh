#!/bin/bash
echo "Executing /etc/qemu-ifdown"
echo "Bringing TAP interface down"
ifconfig $1 down
echo "Removing interfaces"
ifconfig bridge2 deletem en0 deletem $1
echo "Bring down bridge"
ifconfig bridge2 down
echo "Removing bridge"
ifconfig bridge2 destroy
sysctl -w net.link.tap.user_open=0
sysctl -w net.link.tap.up__on__open=0
sysctl -w net.ipv4.ip_forward=0
sysctl -w net.link.ether.inet.proxyall=0
sysctl -w net.inet.ip.forwarding=0
sysctl -w net.inet.ip.fw.enable=0
echo "Removing ipfw rule"
# ipfw del `ipfw list | grep 'ip from any to any via en1' | sed -e 's/ .*//g'`
echo "Stop natd"
# killall -9 natd
