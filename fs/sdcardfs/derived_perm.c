/*
 * fs/sdcardfs/derived_perm.c
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#include "sdcardfs.h"

/* copy derived state from parent inode */
static void inherit_derived_state(struct inode *parent, struct inode *child)
{
	struct sdcardfs_inode_info *pi = SDCARDFS_I(parent);
	struct sdcardfs_inode_info *ci = SDCARDFS_I(child);

	ci->perm = PERM_INHERIT;
	ci->userid = pi->userid;
	ci->d_uid = pi->d_uid;
	ci->d_gid = pi->d_gid;
    ci->d_under_android = pi->d_under_android;
}

/* helper function for derived state */
void setup_derived_state(struct inode *inode, perm_t perm,
                        userid_t userid, uid_t uid, gid_t gid, bool under_android)
{
	struct sdcardfs_inode_info *info = SDCARDFS_I(inode);

	info->perm = perm;
	info->userid = userid;
	info->d_uid = uid;
	info->d_gid = gid;
    info->d_under_android = under_android;
}

void setup_derived_state_for_multiuser_gid(struct inode *inode, perm_t perm,
                        userid_t userid, uid_t uid, gid_t gid, bool under_android)
{
	struct sdcardfs_inode_info *info = SDCARDFS_I(inode);

    setup_derived_state(inode, perm, userid, uid, gid, under_android);
	info->d_gid = multiuser_get_uid(userid, gid);
}

void get_derived_permission(struct dentry *parent, struct dentry *dentry)
{
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dentry->d_sb);
	struct sdcardfs_inode_info *info = SDCARDFS_I(dentry->d_inode);
	struct sdcardfs_inode_info *parent_info= SDCARDFS_I(parent->d_inode);
	appid_t appid;

	/* By default, each inode inherits from its parent.
	 * the properties are maintained on its private fields
	 * because the inode attributes will be modified with that of
	 * its lower inode.
	 * The derived state will be updated on the last
	 * stage of each system call by fix_derived_permission(inode).
	 */

	inherit_derived_state(parent->d_inode, dentry->d_inode);

	/* Derive custom permissions based on parent and current node */
	switch (parent_info->perm) {
		case PERM_INHERIT:
			/* Already inherited above */
			break;
        case PERM_PRE_ROOT:
			/* Legacy internal layout places users at top level */
			info->perm = PERM_ROOT;
			info->userid = simple_strtoul(dentry->d_name.name, NULL, 10);
            if (sbi->options.sdfs_gid == AID_SDCARD_RW)
                info->d_gid = sbi->options.sdfs_gid;
            else
                info->d_gid = multiuser_get_uid(info->userid, sbi->options.sdfs_gid);
			break;
		case PERM_ROOT:
			/* Assume masked off by default. */
			if (!strcasecmp(dentry->d_name.name, "Android")) {
				/* App-specific directories inside; let anyone traverse */
				info->perm = PERM_ANDROID;
                info->d_under_android = true;
            }
			break;
		case PERM_ANDROID:
			if (!strcasecmp(dentry->d_name.name, "data")) {
				/* App-specific directories inside; let anyone traverse */
				info->perm = PERM_ANDROID_DATA;
			} else if (!strcasecmp(dentry->d_name.name, "obb")) {
				/* App-specific directories inside; let anyone traverse */
				info->perm = PERM_ANDROID_OBB;
				// FIXME : this feature will be implemented later.
				/* Single OBB directory is always shared */
                // ex. Fuse daemon..
                // node->graft_path = fuse->obb_path;
                // node->graft_pathlen = strlen(fuse->obb_path);
			} else if (!strcasecmp(dentry->d_name.name, "media")) {
				/* App-specific directories inside; let anyone traverse */
				info->perm = PERM_ANDROID_MEDIA;
			}
			break;
		/* same policy will be applied on PERM_ANDROID_DATA
		 * and PERM_ANDROID_OBB */
		case PERM_ANDROID_DATA:
		case PERM_ANDROID_OBB:
		case PERM_ANDROID_MEDIA:
			appid = get_appid(sbi->pkgl_id, dentry->d_name.name);
			if (appid != 0) {
				info->d_uid = multiuser_get_uid(parent_info->userid, appid);
			}
			break;
	}
}

/* Based on d_walk() at dcache.c to avoid using recursive calls */
void get_derived_permission_recursive(struct dentry *parent)
{
	struct dentry *this_parent;
	struct list_head *next;
	unsigned seq = 0;
	bool retry = true;

again:
	read_seqbegin_or_lock(&rename_lock, &seq);
	this_parent = parent;
	spin_lock(&this_parent->d_lock);

repeat:
	next = this_parent->d_subdirs.next;
resume:
	while (next != &this_parent->d_subdirs) {
		struct list_head *tmp = next;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);
		next = tmp->next;

		spin_lock_nested(&dentry->d_lock, DENTRY_D_LOCK_NESTED);
		/* Re-derive permission and just change uid/gid
		 * instead of calling the fix_derived_permission(). */
		if(dentry->d_inode){
			get_derived_permission(this_parent, dentry);
			dentry->d_inode->i_uid = SDCARDFS_I(dentry->d_inode)->d_uid;
			dentry->d_inode->i_gid = SDCARDFS_I(dentry->d_inode)->d_gid;
		}

		if (!list_empty(&dentry->d_subdirs)) {
			spin_unlock(&this_parent->d_lock);
			spin_release(&dentry->d_lock.dep_map, 1, _RET_IP_);
			this_parent = dentry;
			spin_acquire(&this_parent->d_lock.dep_map, 0, 1, _RET_IP_);
			goto repeat;
		}
		spin_unlock(&dentry->d_lock);
	}
	/*
	 * All done at this level ... ascend and resume the search.
	 */
	rcu_read_lock();
ascend:
	if (this_parent != parent) {
		struct dentry *child = this_parent;
		this_parent = child->d_parent;

		spin_unlock(&child->d_lock);
		spin_lock(&this_parent->d_lock);

		/* might go back up the wrong parent if we have had a rename. */
		if (need_seqretry(&rename_lock, seq))
			goto rename_retry;
		/* go into the first sibling still alive */
		do {
			next = child->d_child.next;
			if (next == &this_parent->d_subdirs)
				goto ascend;
			child = list_entry(next, struct dentry, d_child);
		} while (unlikely(child->d_flags & DCACHE_DENTRY_KILLED));
		rcu_read_unlock();
		goto resume;
	}
	if (need_seqretry(&rename_lock, seq))
		goto rename_retry;
	rcu_read_unlock();

	spin_unlock(&this_parent->d_lock);
	done_seqretry(&rename_lock, seq);
	return;

rename_retry:
	spin_unlock(&this_parent->d_lock);
	rcu_read_unlock();
	BUG_ON(seq & 1);
	if (!retry)
		return;
	seq = 1;
	goto again;
}

/* main function for updating derived permission */
inline void update_derived_permission(struct dentry *dentry)
{
	struct dentry *parent;

    struct sdcardfs_sb_info *sbi;
    int mask = 0;

	if(!dentry || !dentry->d_inode) {
		printk(KERN_ERR "sdcardfs: %s: invalid dentry\n", __func__);
		return;
	}
	/* FIXME:
	 * 1. need to check whether the dentry is updated or not
	 * 2. remove the root dentry update
	 */
	if(IS_ROOT(dentry)) {
		//setup_default_pre_root_state(dentry->d_inode);
	} else {
		parent = dget_parent(dentry);
		if(parent) {
			get_derived_permission(parent, dentry);
			dput(parent);
		}
	}
    sbi = SDCARDFS_SB(dentry->d_sb);
    mask = sbi->options.sdfs_mask;
    fix_derived_permission(dentry->d_inode, mask);
}

int need_graft_path(struct dentry *dentry)
{
	int ret = 0;
	struct dentry *parent = dget_parent(dentry);
	struct sdcardfs_inode_info *parent_info= SDCARDFS_I(parent->d_inode);
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dentry->d_sb);

	if(parent_info->perm == PERM_ANDROID &&
		!strcasecmp(dentry->d_name.name, "obb")) {
			if(sbi->options.multi_user)
				ret = 1;
	}
	dput(parent);
	return ret;
}

int is_obbpath_invalid(struct dentry *dent)
{
	int ret = 0;
	struct sdcardfs_dentry_info *di = SDCARDFS_D(dent);
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dent->d_sb);
	char *path_buf, *obbpath_s;

	/* check the base obbpath has been changed.
	 * this routine can check an uninitialized obb dentry as well.
	 * regarding the uninitialized obb, refer to the sdcardfs_mkdir() */
	spin_lock(&di->lock);
	if(di->orig_path.dentry) {
		if(!di->lower_path.dentry) {
			ret = 1;
		} else {
			path_get(&di->lower_path);
			//lower_parent = lock_parent(lower_path->dentry);

			path_buf = kmalloc(PATH_MAX, GFP_ATOMIC);
			if(!path_buf) {
				ret = 1;
				printk(KERN_ERR "sdcardfs: "
					"fail to allocate path_buf in %s.\n", __func__);
			} else {
				obbpath_s = d_path(&di->lower_path, path_buf, PATH_MAX);
				if (d_unhashed(di->lower_path.dentry) ||
					strcasecmp(sbi->obbpath_s, obbpath_s)) {
					ret = 1;
				}
				kfree(path_buf);
			}

			//unlock_dir(lower_parent);
			path_put(&di->lower_path);
		}
	}
	spin_unlock(&di->lock);
	return ret;
}

/* The lower_path will be stored to the dentry's orig_path
 * and the base obbpath will be copyed to the lower_path variable.
 * if an error returned, there's no change in the lower_path
 * returns: -ERRNO if error (0: no error) */
int setup_obb_dentry(struct dentry *dentry, struct path *lower_path)
{
	int err = 0;
	struct sdcardfs_sb_info *sbi = SDCARDFS_SB(dentry->d_sb);
	struct path obbpath;

	/* A local obb dentry must have its own orig_path to support rmdir
	 * and mkdir of itself. Usually, we expect that the sbi->obbpath
	 * is avaiable on this stage. */
	sdcardfs_set_orig_path(dentry, lower_path);

	err = kern_path(sbi->obbpath_s,
			LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &obbpath);

	if(!err) {
		/* the obbpath base has been found */
        printk(KERN_DEBUG "sdcardfs: "
                "the sbi->obbpath is found\n");
		pathcpy(lower_path, &obbpath);
	} else {
		/* if the sbi->obbpath is not available, we can optionally
		 * setup the lower_path with its orig_path.
		 * but, the current implementation just returns an error
		 * because the sdcard daemon also regards this case as
		 * a lookup fail. */
		printk(KERN_INFO "sdcardfs: "
				"the sbi->obbpath is not available\n");
	}
	return err;
}
