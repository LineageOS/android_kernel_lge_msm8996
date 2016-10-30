#
# Makefile for the Linux eCryptfs
#

obj-$(CONFIG_ECRYPT_FS) += ecryptfs.o
obj-$(CONFIG_SD_ENCRYPTION_MANAGER) += sdcard_encrypt_mgr.o
ecryptfs-y := dentry.o file.o inode.o main.o super.o mmap.o read_write.o \
	      crypto.o keystore.o kthread.o debug.o

ecryptfs-$(CONFIG_ECRYPT_FS_MESSAGING) += messaging.o miscdev.o
