#
# Makefile for lemote loongson2f family machines
#

obj-y += machtype.o irq.o reset.o ec_kb3310b.o platform.o

#
# Suspend Support
#

obj-$(CONFIG_LOONGSON_SUSPEND) += pm.o

#
# Platform Drivers
#
obj-$(CONFIG_LEMOTE_LYNLOONG2F_PDEV) += lynloong_pc.o
obj-$(CONFIG_LEMOTE_YEELOONG2F_PDEV) += yeeloong_laptop.o
