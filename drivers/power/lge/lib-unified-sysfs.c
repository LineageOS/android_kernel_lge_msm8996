/* Copyright (c) 2013-2014, LG Eletronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define DEBUG

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

#include "inc-unified-sysfs.h"

#define UNIFIED_SYSFS_PROPERTY	"lge,unified-sysfs"
#define UNIFIED_SYSFS_ROOT	"lge_power"

struct unified_sysfs {
	const char* group;
	const char* symlink;
	const char* source;
};

struct unified_group {
	struct list_head node;
	const char* name;
	struct proc_dir_entry* pde;
};

static struct proc_dir_entry* unified_group_search(struct proc_dir_entry* root,
	struct list_head* groups, const char* name) {

	struct unified_group* group;
	list_for_each_entry(group, groups, node) {
		if (!strcmp(group->name, name))
			goto success;
	}

	group = kzalloc(sizeof(struct unified_group), GFP_KERNEL);
	if (!group) {
		pr_err("%s : ERROR to alloc memory for unified_group\n", __func__);
		goto error;
	}
	group->pde = proc_mkdir(name, root);
	if (!group->pde) {
		pr_err("%s : ERROR to mkdir a group, %s\n", __func__, name);
		goto error;
	}

	group->name = name;
	list_add(&group->node, groups);
success:
	return group->pde;

error:
	kfree(group);
	return NULL;
}

static void unified_group_free(struct list_head* groups) {
	struct unified_group* iter;
	struct unified_group* safe;

	list_for_each_entry_safe(iter, safe, groups, node) {
		list_del(&iter->node);
		kfree(iter);
	}
}

static int unified_sysfs_array(struct device_node* of_node,
	struct unified_sysfs* sysfs_array, int sysfs_count) {

	int sysfs_index;
	for (sysfs_index=0; sysfs_index<sysfs_count; ++sysfs_index) {
		static int i = 0;

		if( of_property_read_string_index(of_node, UNIFIED_SYSFS_PROPERTY,
				i++, &sysfs_array[sysfs_index].group) ||
			of_property_read_string_index(of_node, UNIFIED_SYSFS_PROPERTY,
				i++, &sysfs_array[sysfs_index].symlink) ||
			of_property_read_string_index(of_node, UNIFIED_SYSFS_PROPERTY,
				i++, &sysfs_array[sysfs_index].source) ) {

			pr_err("%s : ERROR get %ith string\n", __func__, i);
			return -ENOMEM;
		}
	}
#ifdef DEBUG
	for (sysfs_index=0; sysfs_index<sysfs_count; ++sysfs_index)
		pr_err("%s : get %dth node is %s, %s, %s\n", __func__, sysfs_index,
				sysfs_array[sysfs_index].group, sysfs_array[sysfs_index].symlink,
				sysfs_array[sysfs_index].source);
#endif
	return 0;
}

static int unified_sysfs_mount(struct unified_sysfs* sysfs_array,
	int sysfs_count) {

	int ret = -ENOMEM;
	int sysfs_index;

	struct list_head group_list = LIST_HEAD_INIT(group_list);
	struct proc_dir_entry* sysfs_root = proc_mkdir(UNIFIED_SYSFS_ROOT, NULL);
	if (sysfs_root != NULL) {
		for (sysfs_index=0; sysfs_index<sysfs_count; sysfs_index++) {
			struct proc_dir_entry* sysfs_group;

			if (!strcmp(sysfs_array[sysfs_index].source, "NULL")) {
				pr_info("%s : %s user node didn't have kernel node \n",
					__func__, sysfs_array[sysfs_index].symlink);
				continue;
			}

			sysfs_group = unified_group_search(sysfs_root, &group_list, sysfs_array[sysfs_index].group);
			if (sysfs_group == NULL) {
				pr_err("%s : ERROR making group '%s'\n", __func__,
					sysfs_array[sysfs_index].group);
				goto out;
			}

			if (!proc_symlink(sysfs_array[sysfs_index].symlink, sysfs_group,
				sysfs_array[sysfs_index].source)) {
				pr_err("%s : ERROR making symlink '%s'\n", __func__,
					sysfs_array[sysfs_index].symlink);
				goto out;
			}
		}

		ret = 0;
	}
	else {
		pr_err("%s : ERROR making root sysfs\n", __func__);
	}

out :
	unified_group_free(&group_list);
	return ret;
}

int unified_sysfs_build(struct device_node* of_node) {
	int ret = -EINVAL;
	int sysfs_count = of_property_count_strings(of_node, UNIFIED_SYSFS_PROPERTY)
			/ 3;
	struct unified_sysfs* sysfs_array = kzalloc(sysfs_count
			* sizeof(struct unified_sysfs), GFP_KERNEL);

	if (sysfs_count <= 0 || sysfs_array == NULL) {
		pr_info("Failed to parsing device tree for unified-sysfs\n");
		goto err_get_array;
	}

	if (unified_sysfs_array(of_node, sysfs_array, sysfs_count)) {
		pr_info("Failed to make array for unified-sysfs\n");
		goto err_get_array;
	}

	if (unified_sysfs_mount(sysfs_array, sysfs_count)) {
		pr_info("Failed to make array for unified-sysfs\n");
		goto err_get_array;
	}

	pr_info("%s : Success Power sysfs Init\n", __func__);
	ret = 0;

err_get_array:
	kfree(sysfs_array);
	return ret;
}
