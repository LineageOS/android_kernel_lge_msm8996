/* FEATURE_SDCARD_ENCRYPTION */

#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <linux/mutex.h>
#include <linux/uidgid.h>
#include "sdcard_encrypt_mgr.h"

static DEFINE_MUTEX(media_ext_list_lock);

char saved_file_ext_list[MAX_MEDIA_EXT_LENGTH];
const char *asec_extension = "ASEC";
/* global flag to check whether the media file extsion list set or not */
bool is_saved_file_ext_list_set = false;

/*
*  check uid if it's not root(0) nor system(1000)
*/
static inline long check_uid(kuid_t uid)
{
	if (!uid_eq(uid, GLOBAL_ROOT_UID) &&
			!uid_eq(uid, ANDROID_SYSTEM_SERVER_UID))
		return -EPERM;
	/* uid is OK */
	return 0;
}

/*
 * Saves extension list of media file
 */
long set_media_ext(const char *media_ext_list)
{
	long len, rc = 0;
	kuid_t uid;

	/* check uid if it's not root(0) nor system(1000) */
	uid = current_uid();
	if (check_uid(uid)) {
		pr_err("%s: %s not permitted.\n",
				__func__, current->comm);
		pr_err(" [CCAudit] %s: %s not permitted.\n",
				__func__, current->comm);
		return -EPERM;
	}

	mutex_lock(&media_ext_list_lock);
	/*
	 *   The media file extension list set on each boot-up time
	 *   and never set again while runtime. is_saved_file_ext_list_set
	 *   is a global flag to check whether the list has been set or not.
	 *   If it's already set, this function just return 0 for success.
	 */
	if (is_saved_file_ext_list_set) {
		pr_info("%s: the file list already set.\n", __func__);
		goto out;
	}

	/* check if media_ext_list is not userspace */
	if (!media_ext_list || ((len = strlen(media_ext_list)) <= 0)) {
		pr_err("%s: media_ext_list is Null value.\n", __func__);
		pr_err(" [CCAudit] %s: media_ext_list is Null value.\n", __func__);
		rc = -EFAULT;
		goto out;
	}

	/* check overflow */
	if (len >= MAX_MEDIA_EXT_LENGTH) {
		pr_err("%s: media_ext_list is too large.\n", __func__);
		pr_err(" [CCAudit] %s: media_ext_list is too large.\n", __func__);
		rc = -EOVERFLOW;
		goto out;
	}

	memset(saved_file_ext_list, 0, sizeof(saved_file_ext_list));
	strncpy(saved_file_ext_list, media_ext_list, len);

	is_saved_file_ext_list_set = true;
	/* set return value 0 for success */
	rc = 0;

out:
	mutex_unlock(&media_ext_list_lock);
	return rc;
}

static inline char *ecryptfs_extfilename(const unsigned char *filename)
{
	char *pos = NULL;

	if (filename == NULL) {
		return pos;
	}

	/* extract extension of file : ex> a.txt -> .txt */
	pos = strrchr(filename, '.');
	if (pos == NULL) {
		return pos;
	}

	return pos+1;
}

int ecryptfs_asec_file_search(const unsigned char *filename)
{
	char *ext_p = NULL;

	/* extract extension in filename */
	ext_p = ecryptfs_extfilename(filename);
	if (ext_p == NULL || strlen(ext_p) != strlen(asec_extension)) {
		pr_debug("Extfilename is NULL\n");
		return 0;
	}

	/* check if the extension is asec */
	if (!strncasecmp(asec_extension, ext_p, strlen(asec_extension))) {
		return 1;
	}
	return 0;
}
int ecryptfs_media_file_search(const unsigned char *filename)
{
	char *ext_p = NULL;
	char *tok;
	char buf[MAX_MEDIA_EXT_LENGTH];
	char *ext_list_p;
	/* extract extension in filename */
	ext_p = ecryptfs_extfilename(filename);
	if (ext_p == NULL || strlen(ext_p) < 2) {
		pr_debug("%s :: Extfilename is NULL\n", __func__);
		return 0;
	}

	pr_info("%s :: saved_file_ext_list: %s\n", __func__, saved_file_ext_list);

	/* check if the extension exists in MediaType
	 *	* if exists status = 1 meaning the file is media file */
	if (is_saved_file_ext_list_set) {
		strlcpy(buf, saved_file_ext_list, MAX_MEDIA_EXT_LENGTH);
		ext_list_p = buf;

		while ((tok = strsep(&ext_list_p, "/")) != NULL) {
			if (!*tok)
				continue;

			if (strlen(ext_p) != strlen(tok))
				continue;

			if (!strncasecmp(tok, ext_p, strlen(tok))) {
				pr_debug("%s :: ECRYPTFS_MEDIA_EXCEPTION\n", __func__);
				return 1;
			}
		}
		pr_debug("%s :: NOT ECRYPTFS_MEDIA_EXCEPTION\n", __func__);
	} else {
		pr_debug("%s :: getMediaExtList() = NULL\n", __func__);
		return 0;
	}

	return 0;
}
