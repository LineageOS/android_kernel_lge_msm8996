/*
 * fs/sdcardfs/packagelist.c
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
#include <linux/hashtable.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/inotify.h>
#include <linux/delay.h>

#define STRING_BUF_SIZE		(512)

#define MAX_MOUNT_CNT       (3)
#define MAX_PKGL            (16)

static DEFINE_MUTEX(pkgl_lock);

struct hashtable_entry {
	struct hlist_node hlist;
	void *key;
	int value;
};
// Set 1 in case that SPIN LOCK is used for packagelist hashtable_lock
// Set 0 in case that Mutex LOCK is used for packagelist hashtable_lock
#define USE_SPIN	0

struct packagelist_data {
	DECLARE_HASHTABLE(package_to_appid,8);
#if !USE_SPIN// use spin
	struct mutex hashtable_lock;
#else
	spinlock_t  hashtable_lock;
#endif
	struct task_struct *thread_id;
	char *strtok_last;
	char read_buf[STRING_BUF_SIZE];
	char event_buf[STRING_BUF_SIZE];
	char app_name_buf[STRING_BUF_SIZE];
	char gids_buf[STRING_BUF_SIZE];
	struct super_block *sb;
};

// Global data control
struct global_packagelist_data {
	char            dev_name[STRING_BUF_SIZE];
	struct packagelist_data *pkgl_id;
	int             access_no;
	int             max_access_no;
};

static struct global_packagelist_data *g_pkgls[MAX_PKGL];
static struct kmem_cache *hashtable_entry_cachep;


/* This function used for get_derived_permission_recursive() */
void packagelist_lock_init(void *pkgl_id)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)pkgl_id;
#if !USE_SPIN// use spin
	mutex_init(&pkgl_dat->hashtable_lock);
#else
	spin_lock_init(&pkgl_dat->hashtable_lock);
#endif
}
void packagelist_lock(void *pkgl_id)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)pkgl_id;
#if !USE_SPIN// use spin
	mutex_lock(&pkgl_dat->hashtable_lock);
#else
	spin_unlock(&pkgl_dat->hashtable_lock);
#endif
}

/* This function used for get_derived_permission_recursive() */
void packagelist_unlock(void *pkgl_id)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)pkgl_id;
#if !USE_SPIN// use spin
	mutex_unlock(&pkgl_dat->hashtable_lock);
#else
	spin_unlock(&pkgl_dat->hashtable_lock);
#endif
}

static void* get_pkgl_dat_devname(const char *dev_name)
{
	int     i;

	for (i=0; i < MAX_PKGL; i++)
	{
		if (g_pkgls[i] == NULL)
			continue;

		// find one.
		if (g_pkgls[i]->dev_name != NULL &&
				!strncmp(g_pkgls[i]->dev_name, dev_name, strlen(dev_name)))
			return (void *)g_pkgls[i];
	}

	return NULL;
}

static void* get_pkgl_dat_pkglid(struct packagelist_data *pkgl_id)
{
	int     i;

	for (i=0; i < MAX_PKGL; i++)
	{
		if (g_pkgls[i] == NULL)
			continue;

		// find one.
		if (g_pkgls[i]->pkgl_id == pkgl_id)
			return (void *)g_pkgls[i];
	}

	return NULL;
}

static int put_pkgl_dat(void *pkgl)
{
	int     i;

	for (i=0; i < MAX_PKGL; i++)
	{
		if (g_pkgls[i] == NULL)
		{
			g_pkgls[i] = (struct global_packagelist_data *)pkgl;
			break;
		}
	}

	if (i >= MAX_PKGL)
		printk(KERN_INFO "sdcardfs: pkgls is over limit:%d.\n", i);

	return i;
}

static int clear_pkgl_dat(struct packagelist_data *pkgl_id)
{
	int     i;

	for (i=0; i < MAX_PKGL; i++)
	{
		if (g_pkgls[i] == NULL)
			continue;

		// find one.
		if (g_pkgls[i]->pkgl_id == pkgl_id)
		{
			g_pkgls[i] = NULL;
		}
	}

	return i;
}

/* Path to system-provided mapping of package name to appIds */
static const char* const kpackageslist_file = "/data/system/packages.list";
/* Supplementary groups to execute with */
static const gid_t kgroups[1] = { AID_PACKAGE_INFO };

static unsigned int str_hash(void *key) {
	int i;
	unsigned int h = strlen(key);
	char *data = (char *)key;

	for (i = 0; i < strlen(key); i++) {
		h = h * 31 + *data;
		data++;
	}
	return h;
}

appid_t get_appid(void *pkgl_id, const char *app_name)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)pkgl_id;
	struct hashtable_entry *hash_cur;
	unsigned int hash = str_hash((void *)app_name);
    appid_t ret_id = 0;

    if (pkgl_dat == NULL) {
        return 0;
    }

    if (!in_atomic()) {
        packagelist_lock(pkgl_id);
    }
    hash_for_each_possible(pkgl_dat->package_to_appid, hash_cur, hlist, hash) {
        if (!strcasecmp(app_name, hash_cur->key)) {
            ret_id = (appid_t)hash_cur->value;

            if (!in_atomic()) {
                packagelist_unlock(pkgl_id);
			}
			return ret_id;
		}
	}
	if (!in_atomic()) {
		packagelist_unlock(pkgl_id);
	}

	return 0;
}

/* Kernel has already enforced everything we returned through
 * derive_permissions_locked(), so this is used to lock down access
 * even further, such as enforcing that apps hold sdcard_rw. */
int check_caller_access_to_name(struct inode *parent_node, const char* name,
		int w_ok) {

	/* Always block security-sensitive files at root */
	if (parent_node && SDCARDFS_I(parent_node)->perm == PERM_ROOT) {
		if (!strcasecmp(name, "autorun.inf")
				|| !strcasecmp(name, ".android_secure")
				|| !strcasecmp(name, "android_secure")) {
			return 0;
		}
	}

	/* Root always has access; access for any other UIDs should always
	 * be controlled through packages.list. */
	if (current_fsuid() == 0) {
		return 1;
	}

	/* No extra permissions to enforce */
	return 1;
}

/* This function is used when file opening. The open flags must be
 * checked before calling check_caller_access_to_name() */
int open_flags_to_access_mode(int open_flags) {
	if((open_flags & O_ACCMODE) == O_RDONLY) {
		return 0; /* R_OK */
	} else if ((open_flags & O_ACCMODE) == O_WRONLY) {
		return 1; /* W_OK */
	} else {
		/* Probably O_RDRW, but treat as default to be safe */
		return 1; /* R_OK | W_OK */
	}
}

static int insert_str_to_int(struct packagelist_data *pkgl_dat, void *key, int value) {
	struct hashtable_entry *hash_cur;
	struct hashtable_entry *new_entry;
	unsigned int hash = str_hash(key);

	//printk(KERN_INFO "sdcardfs: %s: %s: %d, %u\n", __func__, (char *)key, value, hash);
	hash_for_each_possible(pkgl_dat->package_to_appid, hash_cur, hlist, hash) {
		if (!strcasecmp(key, hash_cur->key)) {
			hash_cur->value = value;
			return 0;
		}
	}
	new_entry = kmem_cache_alloc(hashtable_entry_cachep, GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;
	new_entry->key = kstrdup(key, GFP_KERNEL);
	new_entry->value = value;
	hash_add(pkgl_dat->package_to_appid, &new_entry->hlist, hash);
	return 0;
}

static void remove_str_to_int(struct hashtable_entry *h_entry) {
	//printk(KERN_INFO "sdcardfs: %s: %s: %d\n", __func__, (char *)h_entry->key, h_entry->value);
	kfree(h_entry->key);
	kmem_cache_free(hashtable_entry_cachep, h_entry);
}

static void remove_all_hashentrys(struct packagelist_data *pkgl_dat)
{
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_t;
	int i;

	hash_for_each_safe(pkgl_dat->package_to_appid, i, h_t, hash_cur, hlist)
		remove_str_to_int(hash_cur);

	hash_init(pkgl_dat->package_to_appid);
}

static int read_package_list(struct packagelist_data *pkgl_dat) {
	int ret;
	int fd;
	int read_amount;

	printk(KERN_DEBUG "sdcardfs: read_package_list\n");

	packagelist_lock(pkgl_dat);
	remove_all_hashentrys(pkgl_dat);

	fd = sys_open(kpackageslist_file, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_ERR "sdcardfs: failed to open package list\n");
		packagelist_unlock(pkgl_dat);
		return fd;
	}

	while ((read_amount = sys_read(fd, pkgl_dat->read_buf,
					sizeof(pkgl_dat->read_buf))) > 0) {
		int appid;
		int one_line_len = 0;
		int additional_read;

		while (one_line_len < read_amount) {
			if (pkgl_dat->read_buf[one_line_len] == '\n') {
				one_line_len++;
				break;
			}
			one_line_len++;
		}
		additional_read = read_amount - one_line_len;
		if (additional_read > 0)
			sys_lseek(fd, -additional_read, SEEK_CUR);

		if (sscanf(pkgl_dat->read_buf, "%s %d %*d %*s %*s %s",
					pkgl_dat->app_name_buf, &appid,
					pkgl_dat->gids_buf) == 3) {
			ret = insert_str_to_int(pkgl_dat, pkgl_dat->app_name_buf, appid);
			if (ret) {
				sys_close(fd);
				packagelist_unlock(pkgl_dat);
				return ret;
			}
        }
    }

	sys_close(fd);
    /* Regenerate ownership details using newly loaded mapping */
    if (pkgl_dat->sb->s_root && pkgl_dat->sb->s_root->d_inode) {
        get_derived_permission_recursive(pkgl_dat->sb->s_root);
    }
    packagelist_unlock(pkgl_dat);
    return 0;
}

static int packagelist_reader(void *thread_data)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)thread_data;
	struct inotify_event *event;
	bool active = false;
	int event_pos;
	int event_size;
	int res = 0;
	int nfd;

	allow_signal(SIGINT);

	nfd = sys_inotify_init();
	if (nfd < 0) {
		printk(KERN_ERR "sdcardfs: inotify_init failed: %d\n", nfd);
		return nfd;
	}

	while (!kthread_should_stop()) {
		if (signal_pending(current)) {
			ssleep(1);
			continue;
		}

		if (!active) {
			res = sys_inotify_add_watch(nfd, kpackageslist_file, IN_DELETE_SELF);
			if (res < 0) {
				if (res == -ENOENT || res == -EACCES) {
					/* Framework may not have created yet, sleep and retry */
					printk(KERN_ERR "sdcardfs: missing packages.list; retrying\n");
					ssleep(2);
					printk(KERN_ERR "sdcardfs: missing packages.list_end; retrying\n");
					continue;
				} else {
					printk(KERN_ERR "sdcardfs: inotify_add_watch failed: %d\n", res);
					goto interruptable_sleep;
				}
			}
			/* Watch above will tell us about any future changes, so
			 * read the current state. */
			res = read_package_list(pkgl_dat);
			if (res) {
				printk(KERN_ERR "sdcardfs: read_package_list failed: %d\n", res);
				goto interruptable_sleep;
			}
			active = true;
		}

		event_pos = 0;
		res = sys_read(nfd, pkgl_dat->event_buf, sizeof(pkgl_dat->event_buf));
		if (res < (int) sizeof(*event)) {
			if (res == -EINTR)
				continue;
			printk(KERN_ERR "sdcardfs: failed to read inotify event: %d\n", res);
			goto interruptable_sleep;
		}

		while (res >= (int) sizeof(*event)) {
			event = (struct inotify_event *) (pkgl_dat->event_buf + event_pos);

			printk(KERN_DEBUG "sdcardfs: inotify event: %08x\n", event->mask);
			if ((event->mask & IN_IGNORED) == IN_IGNORED) {
				/* Previously watched file was deleted, probably due to move
				 * that swapped in new data; re-arm the watch and read. */
				active = false;
			}

			event_size = sizeof(*event) + event->len;
			res -= event_size;
			event_pos += event_size;
		}
		continue;

interruptable_sleep:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	flush_signals(current);
	sys_close(nfd);
	return res;
}

void * packagelist_create(const char *dev_name, struct super_block *sb)
{
	struct global_packagelist_data *g_pkgl;

	mutex_lock (&pkgl_lock);

	g_pkgl = (struct global_packagelist_data*) get_pkgl_dat_devname(dev_name);

	if (g_pkgl == NULL)
	{
		struct packagelist_data *pkgl_dat;
		struct task_struct *packagelist_thread;

		// global variables.
		g_pkgl = kmalloc(sizeof(*g_pkgl), GFP_KERNEL | __GFP_ZERO);
		if (!g_pkgl) {
			printk(KERN_ERR "sdcardfs: creating g_pkgl failed\n");
			mutex_unlock (&pkgl_lock);
			return ERR_PTR(-ENOMEM);
		}

		strncpy(g_pkgl->dev_name, dev_name,
				(strlen(dev_name) < sizeof(g_pkgl->dev_name)-1) ? strlen(dev_name) : sizeof(g_pkgl->dev_name)-1);
		g_pkgl->access_no = 0;
		g_pkgl->max_access_no = MAX_MOUNT_CNT;

		// pkgl_dat
		pkgl_dat = kmalloc(sizeof(*pkgl_dat), GFP_KERNEL | __GFP_ZERO);
		if (!pkgl_dat) {
			printk(KERN_ERR "sdcardfs:  creating pkgl_dat failed\n");
			kfree(g_pkgl);
			mutex_unlock (&pkgl_lock);
			return ERR_PTR(-ENOMEM);
		}

        packagelist_lock_init(pkgl_dat);
        hash_init(pkgl_dat->package_to_appid);
        pkgl_dat->sb = sb;

        packagelist_thread = kthread_run(packagelist_reader, (void *)pkgl_dat, "pkgld");
		if (IS_ERR(packagelist_thread)) {
			printk(KERN_ERR "sdcardfs: creating kthread failed\n");
			kfree(pkgl_dat);
			kfree(g_pkgl);
			mutex_unlock (&pkgl_lock);
			return ERR_PTR(-ENOMEM);
		}
		pkgl_dat->thread_id = packagelist_thread;

		g_pkgl->pkgl_id = pkgl_dat;
		put_pkgl_dat(g_pkgl);

		printk(KERN_INFO "sdcardfs: created packagelist pkgld/%d\n",
				(int)pkgl_dat->thread_id->pid);
	}

	g_pkgl->access_no++;

	pr_info ("%s: devname:%s, pkgl info(%d):%p\n", __func__, dev_name, g_pkgl->access_no, g_pkgl->pkgl_id);

	if (g_pkgl->access_no > MAX_MOUNT_CNT)
		printk(KERN_INFO "sdcardfs: something long, access_no exceed max mount count.\n");

	mutex_unlock (&pkgl_lock);
	return (void *)g_pkgl->pkgl_id;
}

void packagelist_destroy(void *pkgl_id)
{
	pid_t pkgl_pid  = 0;
	struct task_struct *thread = NULL;
	struct global_packagelist_data *g_pkgl;
	struct packagelist_data *pkgl_dat;

	mutex_lock (&pkgl_lock);

	g_pkgl = (struct global_packagelist_data *)get_pkgl_dat_pkglid(pkgl_id);

	if (g_pkgl != NULL)
	{
		g_pkgl->access_no--;

		pr_info ("%s: pid:%d ,comm:%s, devname:%s, pkgl info(%d):%p\n", __func__, current->pid, current->comm, g_pkgl->dev_name, g_pkgl->access_no, g_pkgl->pkgl_id);
		// remove pkgl_data on last unmount.
		if (g_pkgl->access_no == 0)
		{
			pkgl_dat = g_pkgl->pkgl_id;

			pkgl_pid = pkgl_dat->thread_id->pid;
			thread = pkgl_dat->thread_id;
			force_sig_info(SIGINT, SEND_SIG_PRIV, thread);
			kthread_stop(thread);
			remove_all_hashentrys(pkgl_dat);
			printk(KERN_INFO "sdcardfs: destroyed packagelist pkgld/%d\n", (int)pkgl_pid);
			kfree(pkgl_dat);
			pkgl_dat = NULL;

			clear_pkgl_dat(pkgl_id);
		}
	}
	mutex_unlock (&pkgl_lock);
}

int packagelist_init(void)
{
	hashtable_entry_cachep =
		kmem_cache_create("packagelist_hashtable_entry",
				sizeof(struct hashtable_entry), 0, 0, NULL);
	if (!hashtable_entry_cachep) {
		printk(KERN_ERR "sdcardfs: failed creating pkgl_hashtable entry slab cache\n");
		return -ENOMEM;
	}

	return 0;
}

void packagelist_exit(void)
{
	if (hashtable_entry_cachep)
		kmem_cache_destroy(hashtable_entry_cachep);
}
