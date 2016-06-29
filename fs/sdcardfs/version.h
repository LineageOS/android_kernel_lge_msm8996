/*
 * The sdcardfs
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Kitae Lee, Yeongjin Gil
 *
 * Revision History
 * 2014.06.24 : Release Version 2.1.0
 *    - Add sdcardfs version
 *    - Add kernel log when put_super
 * 2014.07.21 : Release Version 2.1.1
 *    - Add sdcardfs_copy_inode_attr() to fix permission issue
 *    - Delete mmap_sem lock in sdcardfs_setattr() to avoid deadlock
 * 2014.09.03 : Release Version 2.1.2
 *    - Add PERM_ANDROID_MEDIA to sync with Android L
 * 2014.11.12 : Release Version 2.1.2
 *    - Add get_lower_file function pointer in file_operations
 * 2014.11.25 : Release Version 2.1.3
 *    - Add error handling routine in sdcardfs_d_revalidate
 *          when dentry is equal to lower_dentry
 */
/*
 * LG Electronics Revision History
 * 2015.05.10 : Release Version 2.1.3-1
 *		- Add multi-user gid support for Android M
 *			limit the access to the user's external storage
 *			to user's application
 *		- Change permission to support dynamic storage
 *			for Android M
 *		- Limit inode number to 32-bit
 *			from Android sdcard daemon
 *		- Allow updates for open file descriptors even if
 *			not having open permission
 *			from Android sdcard daemon
 * 2015.07.22 : Release Version 2.1.3-2
 *      - remove permission check based on appid
 *      - group id control based on runtime mount
 * 2015.08.19 : Release Version 2.1.3-3
 *      - use userid (mounted userID)
 *      - Top of multi-user view should always be visible
 *        to ensure secondary users can traverse inside
 *      - Block "other" access to Adroid directories, since only apps
 *        belonging to a speicfic user should be in there; we still
 *        leave +x open for the default view.
 */

#define SDCARDFS_VERSION "2.1.3-2"
