OpenOSOM - Open Source Open Mind
-----------------
---------------------------------


###Loongson2F target platform of OpenWrt Backfire_10.03.1
###用于OpenWrt Backfire_10.03.1/target/linux/添加龙芯2F新设备

####Toolchain:

* Binutils：

    \>= 2.20.2加入了对-mfix-ls2f-kernel的支持。

    之前的软件包则需要打补丁。

* GCC：

    \>=4.4才有专门支持loongson选型-march=loongson2f。

    如果gcc小于4.4，直接使用-march=r4600，指令集与MIPS R4600处理器兼容。


<img src="http://img.my.csdn.net/uploads/201405/25/1401012988_2372.jpg" alt="龙芯2F笔记本运行OpenWrt" width="300" height="400"/>


