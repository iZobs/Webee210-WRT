
OpenWrt Backfire_10.03.1
===
---------------------------------
SVN仓：[svn://svn.openwrt.org.cn/dreambox/branches/openosom](https://dev.openwrt.org.cn/browser/branches/openosom)

GIT仓：[git://github.com/openosom/backfire_10.03.1](https://github.com/openosom/backfire_10.03.1)


OpenWrt-DreamBox/branches/sun分支是基于OpenWrt官网tags/backfire_10.03.1进行修改：

    1 package/base-files/files/etc/banner   ：
    2 rules.mk                              ：对toolchain加入_v7-a前缀
    3 scripts/metadata.pl                   ：修改默认硬件设备为TARGET_s3c24xx
    4 feeds.conf.default                    ：
    5 toolchain/Config.in                   ：默认libc从uClibc改为eglibc
    6 toolchain/eglibc/Config.version       ：当默认libc为eglibc时，默认的eglibc配置
    7 toolchain/glibc/Config.version        ：当默认libc为glibc时，默认的glibc配置
    8 tools/yaffs2/Makefile                 ：加入另外一种larger page NAND的yaffs工具
      tools/yaffs2/patches/101-mkyaffs2image-page2k.patch


OpenWrt的官网SVN仓架构：

    1 tags：
        存放各个稳定版本的OpenWrt
            
    2 trunk和branches/{kamikaze-before-brng, whiterussian, 8.09, backfire, attitude_adjustment}：
        存放开发版本和各个分支的OpenWrt

    3 branches/packages_xxx：
        存放各个稳定版本的packages feeds
        
    4 packages和feeds/xorg：
        存放开发版本的packagefeeds



---------------------------------
_Copyright 2014 (C) Neil([openosom@gamil.com](gmail.google.com))_

_2014-05-30_


