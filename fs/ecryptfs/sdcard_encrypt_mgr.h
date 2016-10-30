#ifndef SDCARD_ENCRYPT_MGR_H
#define SDCARD_ENCRYPT_MGR_H

#include <keys/user-type.h>
#include <keys/encrypted-type.h>
#include <linux/fs.h>
#include <linux/fs_stack.h>
#include <linux/namei.h>
#include <linux/scatterlist.h>
#include <linux/hash.h>
#include <linux/nsproxy.h>
#include <linux/backing-dev.h>
#include <linux/ecryptfs.h>
#include <linux/unistd.h>

#define FEATURE_SDCARD_ENCRYPTION
#define MAX_MEDIA_EXT_LENGTH 512
#define ANDROID_SYSTEM_SERVER_UID KUIDT_INIT(1000)

extern int ecryptfs_media_file_search(const unsigned char *filename);
extern int ecryptfs_asec_file_search(const unsigned char *filename);
extern long set_media_ext(const char *media_ext_list);

#endif /* #ifndef SDCARD_ENCRYPT_MGR_H*/
