---
layout: post
title: "openWRT hack记录 "
date: 2014-8-29 15:25
category: "openwrt"
tags : "openwrt"
---

###获取RT3070 usb wifi SoftAp驱动
webee210-WRT内核自动的RT3070驱动对hostapd的支持不好，所以不能实现ap模式。去其官网找到一个可以使用的master Ap模式的RT3070驱动。这个驱动只有一种模式就是Ap模式。获取代码:

```
$git clone https://github.com/iZobs/RT3070_SoftAP.git

```
根目录下的Wireless目录存放已经编译好的驱动，可以直接使用。如果想自己编译这个驱动，请看这里:

[移植RT3070驱动到webee210-WRT](http://izobs.github.io/openwrt/openwrt-RT3070-softap/)

将Wireless目录cp到U盘，通过U盘cp到系统中,这里u盘的挂载目录为/mnt/usb

```
$ mkdir -p /etc/Wireless/RT2870AP/
$ mkdir -p /lib/RT3070/

$ cp -f /mnt/usb/openWRT/Wireless/RT2870AP.dat /etc/Wireless/RT2870AP/
$ cp -rf /mnt/usb/openWRT/Wireless /lib/RT3070/

```

在/etc/init.d/目录下，touch setup_ra0.sh

```
insmod /lib/RT3070/Wireless/rtutil3070ap.ko
insmod /lib/RT3070/Wireless/rt3070ap.ko
insmod /lib/RT3070/Wireless/rtnet3070ap.ko
echo "insmod rt3070 done"
ifconfig ra0 192.168.1.1 netmask 255.255.255.0 up

```

```
$ chmod 777 ./setup_ra0.sh

```

`注意:Wrieless存放的路径`

`RT2870AP.dat`是wifi热点的配置文件，默认:SSID=RT2860AP,没有加密。
可以这样修改加密:

```
AuthMode=WPA2PSK（加密方式）
EncrypType=TKIP;AES
WPAPSK=loongson1234（这个是密码）

```

###配置/etc/config/network

```
config interface wan
        option ifname 'eth0'
        option proto 'dhcp'
        option peerdns '0'
        option dns '114.114.114.114 8.8.8.8'

config interface lan
        option ifname 'ra0'
        option proto 'static'
        option ipaddr   192.168.1.1
        option netmask  255.255.255.0 

```

###修改/etc/ini.d/rcS

```
mkdir -p /var/run                                           
cd /var/run                    
touch dnsmasq.pid              
cd /                           
/etc/init.d/setup_ra0        
                             
/usr/sbin/telnetd -l /bin/login.sh
/etc/init.d/firewall restart      
/etc/init.d/network restart       
/etc/init.d/dnsmasq start         
                                  
echo 1 >/proc/sys/net/ipv4/ip_forward

```

###连线
将有线网卡接入上级路由器，这个eth0就是有线网卡dm9000的驱动.这里的IP是用动态获取的，所以你的上级路由器要具备这个功能。开机前把usb wifi网卡接入开发板，然后启动.
成功挂载显示如下：
```

[   54.250000] Key4Str is Invalid key length(0) or Type(0)
[   54.260000] 1. Phy Mode = 9
[   54.260000] 2. Phy Mode = 9
[   54.260000] NVM is Efuse and its size =2d[2d0-2fc] 
[   55.570000] 3. Phy Mode = 9
[   56.395000] MCS Set = ff 00 00 00 01
[   56.825000] SYNC - BBP R4 to 20MHz.l
[   57.375000] SYNC - BBP R4 to 20MHz.l
[   57.895000] SYNC - BBP R4 to 20MHz.l
[   58.445000] SYNC - BBP R4 to 20MHz.l
[   58.960000] SYNC - BBP R4 to 20MHz.l
[   59.530000] SYNC - BBP R4 to 20MHz.l
[   60.045000] SYNC - BBP R4 to 20MHz.l
[   60.595000] SYNC - BBP R4 to 20MHz.l
[   64.780000] Main bssid = 98:3f:9f:23:c4:28
[   64.780000] <==== rt28xx_init, Status=0
[   64.805000] 0x1300 = 00064320
[   65.785000] ---> RTMPFreeTxRxRingMemory
[   65.785000] <--- RTMPFreeTxRxRingMemory
udhcpc: setting default routers: 192.168.0.1
[   71.420000] <-- RTMPAllocTxRxRingMemory, Status=0
[   71.440000] -->RTUSBVenderReset
[   71.445000] <--RTUSBVenderReset
[   76.115000] CfgSetCountryRegion():CountryRegion in eeprom was programmed
[   76.115000] CfgSetCountryRegion():CountryRegion in eeprom was programmed
[   76.120000] Key1Str is Invalid key length(0) or Type(0)
[   76.120000] Key2Str is Invalid key length(0) or Type(0)
[   76.120000] Key3Str is Invalid key length(0) or Type(0)
[   76.125000] Key4Str is Invalid key length(0) or Type(0)
[   76.130000] 1. Phy Mode = 9
[   76.130000] 2. Phy Mode = 9

```

用ifconfig查看ip

```
root@(none):/etc/init.d# ifconfig 
eth0      Link encap:Ethernet  HWaddr 08:90:00:A0:02:10  
          inet addr:192.168.0.132  Bcast:192.168.0.255  Mask:255.255.255.0
          inet6 addr: fe80::a90:ff:fea0:210/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:191 errors:0 dropped:0 overruns:0 frame:0
          TX packets:12 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:19103 (18.6 KiB)  TX bytes:2164 (2.1 KiB)
          Interrupt:39 Base address:0x8000 

lo        Link encap:Local Loopback  
          inet addr:127.0.0.1  Mask:255.0.0.0
          inet6 addr: ::1/128 Scope:Host
          UP LOOPBACK RUNNING  MTU:16436  Metric:1
          RX packets:0 errors:0 dropped:0 overruns:0 frame:0
          TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:0 
          RX bytes:0 (0.0 B)  TX bytes:0 (0.0 B)

ra0       Link encap:Ethernet  HWaddr 98:3F:9F:23:C4:28  
          inet addr:192.168.1.1  Bcast:192.168.1.255  Mask:255.255.255.0
          inet6 addr: fe80::9a3f:9fff:fe23:c428/64 Scope:Link
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:0 errors:0 dropped:0 overruns:0 frame:0
          TX packets:0 errors:0 dropped:0 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:9223231 (8.7 MiB)  TX bytes:206064 (201.2 KiB)
																					

```

链接wifi热点，用telnet登陆192.168.1.1（telnet 默认是192.168.1.1。所以跟pc链接的网卡的ip必须是这个网段的）
用浏览器登陆192.168.1.1，可以查看路由配置界面：
![webee210-WRT](/picture/webee210-WRT.png)



