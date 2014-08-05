---
layout: post
title: "openWRT hack记录 "
date: 2014-7-29 15:25
category: "openwrt"
tags : "openwrt"
---

###openWRT的源码的获取
下载的openWRT版本是Dreambox的openosom,因为我打算将openWRT移植到webee210(s5pv210)，而openosom是针对嵌入式有做过优化，而且支持s5pv210.openosom是从openWRT官方的backfire分支演变来的，由Dreambox团队维护。详情看这里:[openosm-blog](http://www.openosom.org/).
获取代码:

	$ git clone git://github.com/openosom/backfire_10.03.1 openosom

or

	$ svn co svn://svn.openwrt.org.cn/dreambox/branches/openosom openosom

获取使用webee210的openWRT源码:    

	$ git clone https://github.com/iZobs/Webee210-WRT


###openWRT的目录框架

代码上来看有几个重要目录package, target, build_root, bin, dl....

---build_dir/host目录是建立工具链时的临时目录

---build_dir/toolchain-<arch>*是对应硬件的工具链的目录

---staging_dir/toolchain-<arch>* 则是工具链的安装位置

---target/linux/<platform>目录里面是各个平台(arch)的相关代码

---target/linux/<platform>/config-3.10文件就是配置文件了

---dl目录是'download'的缩写, 在编译前期，需要从网络下载的数据包都会放在这个目录下，这些软件包的一个特点就是，会自动安装在所编译的固件中，也就是我们make menuconfig的时候，为固件配置的一些软件包。如果我们需要更改这些源码包，只需要将更改好的源码包打包成相同的名字放在这个目录下，然后开始编译即可。编译时，会将软件包解压到build_dir目录下。

---而在build_dir/目录下进行解压，编译和打补丁等。

---package目录里面包含了我们在配置文件里设定的所有编译好的软件包。默认情况下，会有默认选择的软件包。在openwrt中ipk就是一切, 我们可以使用

###编译openWRT
推荐使用`ubuntu12.04-32bit`,用较新的发行版可能出现编译错误，因为他编译依赖的软件库比较旧。   
在ubuntu下安装：     

    $ sudo apt-get install build-essential asciidoc binutils bzip2 gawk gettext git libncurses5-dev libz-dev patch unzip zlib1g-dev subversion git-core


cd 进入webee210-WRT目录：
    $ make menuconfig
勾选你要的模块，或者直接复制目录下的`webee210config`

    $ cp webee210config .config

__注意：因为编译过程中会从网上下载大量源码包，所有保证host机是可以联网的。从网上下载的源码包存在根目录的dl目录下，可以直接解压，这样可以减少下载时间__

[百度网盘](http://pan.baidu.com/s/1bnH6UN9) 


然后编译:

    $ make -j2 V=99

__注意：V=99指的是打印详细编译信息__

勾选了SDK编译选项的编译时间要较长，具体还要看host的性能。= =!          

编译完成后在目录的bin下面会有SDK和uImage,yaffs文件系统。     

     ⇒  ls
     md5sums
     OpenWrt-ImageBuilder-s5pc1xx-for-Linux-i686.tar.bz2
     openwrt-s5pc1xx-squashfs.img
     openwrt-s5pc1xx-webee210-uImage
     openwrt-s5pc1xx-webee210-zImage
     openwrt-s5pc1xx-yaffs2-128k.img
     OpenWrt-SDK-s5pc1xx-for-Linux-i686-gcc-4.3.3+cs_eglibc-2.8.tar.bz2
     OpenWrt-Toolchain-s5pc1xx-for-arm_v7-a-gcc-4.3.3+cs_eglibc-2.8_eabi.tar.bz2
     packages
###烧写镜像到webee210v2
可以用webee210v2的uboot进行镜像的烧写，详情看这里：

[webee210v2 SD卡镜像烧写教程](http://bbs.smartwebee.com/forum/view/253)

启动系统，可以看到这个：
```
Please press Enter to activate this console.                                                                              
BusyBox v1.15.3 (2014-08-05 16:31:45 CST) built-in shell (ash)                                                            
Enter 'help' for a list of built-in commands.                                                                             
_______                     _______  ______  _______  ________                                                          
|       |.-----.-----.-----.|       ||       |       ||        |                                                         
|   -   ||  _  |  -__|     ||   -   |`------.|   -   ||  |  |  |                                                         
|_______||   __|_____|__|__||_______| ______||_______||__|__|__|                                                         
|__| O P E N    S O U R C E    O P E N    M I N D                                                               
OpenWrt Backfire (10.03.1, unknown) ---------------------------                                                          
* Copyright 2013-2014 (C)                                                                                               
* Neil <openosom@gmail.com>                                                                                             
* https://github.com/openosom                                                                                           
----------------------------------------------------------                                                               
root@(none):/#                                                                                                            
root@(none):/#                                                                                                            
root@(none):/# 

```

###编译openWRT内核

清除内核并准备:

      $ make target/linux/{clean,prepare} V=99
      
      make[4]: Leaving directory `/home/webee-wrt/openWRT/backfire_10.03.1/build_dir/linux-s5pc1xx_webee210/linux-2.6.35.7'
      rm -rf /home/webee-wrt/openWRT/backfire_10.03.1/build_dir/linux-s5pc1xx_webee210/modules
      touch /home/webee-wrt/openWRT/backfire_10.03.1/build_dir/linux-s5pc1xx_webee210/linux-2.6.35.7/.configured
      make[3]: Leaving directory `/home/webee-wrt/openWRT/backfire_10.03.1/target/linux/s5pc1xx'
      make[2]: Leaving directory `/home/webee-wrt/openWRT/backfire_10.03.1/target/linux'
      make[1]: Leaving directory `/home/webee-wrt/openWRT/backfire_10.03.1'


编译内核:

    $ make target/linux/{compile,install} V=99

     make -C /home/webee-wrt/openWRT/backfire_10.03.1/build_dir/linux-s5pc1xx_webee210/linux-2.6.35.7 CROSS_COMPILE="arm-openwrt-linux-gnueabi-" ARCH="arm" KBUILD_HAVE_NLS=no CONFIG_SHELL="/bin/bash" CC="arm-openwrt-linux-gnueabi-gcc" oldconfig prepare scripts


修改uImage的入口地址:

`/home/izobs/virtualbox-share/openWRT/backfire_10.03.1/target/linux/s5pc1xx/image/Makefile`

查看uImage信息:

	$ mkimage -l uImage

###生成补丁

__1.git生成补丁__
初始化一个git仓库:                   

      $ git init
      $ git add -A
      $ git commit -a -m "init"

切换到一个开发分支如：   

      $ git checkout -b develop
      Switched to a new branch 'develop

做修改，然后提交，然后生成补丁。                  

      $ git commit -a -m "fix-s3c-nand-driver"
      $ git format-patch -M master
      

__2.diff工具生成补丁。__

- file-org:修改前的文件夹
- file-cha:修改后的文件夹

diff 两个文件夹中的不同：              
     $ diff -uNr file-cha file-org > 0001-xxxx.patch
你可以用vim 0001-xxxx.patch打开补丁来查看。            

用patch应用补丁,cd 到你你要修改的文件夹，即修改前目录file-org:                    
     $ patch -p1 < 0001-xxxx.patch
如果要恢复，取消补丁，则:                

     $ patch -R p1 < 0001-xxxx.patch

这里只是简单介绍diff和patch，关于他的详细使用说明在这：                       

     http://linux-wiki.cn/wiki/zh-cn/%E8%A1%A5%E4%B8%81(patch)%E7%9A%84%E5%88%B6%E4%BD%9C%E4%B8%8E%E5%BA%94%E7%94%A8 

__3.openWRT的补丁管理工具quilt__

quilt新建一个补丁，                         
     $ cd backfire_10.03.1/build_dir/linux-s5pc1xx_webee210/linux-2.6.35.7
     $ quilt new platform/0001-webee210-SDRAM.patch
     Patch platform/0001-webee210-SDRAM.patch is now on top                        

将0001-webee210-SDRAM.patch复制到linux-2.6.35.7目录下的`patches`目录，              

     $ cd ./patches/platform
     $ cp ../../0001-webee210-SDRAM.patch ./

回到backfire_10.03.1的根目录：                    
     $ cd ~/backfire_10.03.1
     $ make target/linux/update V=99
在`backfire_10.03.1/target/linux/s5pc1xx/patches-2.6.35`目录下，你将会看到你的补丁`0001-webee210-SDRAM.patch`

     $ ls
     0001-webee210-SDRAM.patch
关于quilt详细使用说明：                      
[Linux之旅(1): diff, patch和quilt （下）](http://blog.csdn.net/fmddlmyy/article/details/2140097 ) 








