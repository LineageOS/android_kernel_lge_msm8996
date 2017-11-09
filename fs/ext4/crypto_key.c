/*
 * linux/fs/ext4/crypto_key.c
 *
 * Copyright (C) 2015, Google, Inc.
 *
 * This contains encryption key functions for ext4
 *
 * Written by Michael Halcrow, Ildar Muslukhov, and Uday Savagaonkar, 2015.
 */

#include <crypto/algapi.h>
#include <keys/encrypted-type.h>
#include <keys/user-type.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <uapi/linux/keyctl.h>

#include "ext4.h"
#include "ext4_ice.h"
#include "xattr.h"

static void derive_crypt_complete(struct crypto_async_request *req, int rc)
{
	struct ext4_completion_result *ecr = req->data;

	if (rc == -EINPROGRESS)
		return;

	ecr->res = rc;
	complete(&ecr->completion);
}

/**
 * ext4_derive_key_aes() - Derive a key using AES-128-ECB
 * @deriving_key: Encryption key used for derivation.
 * @source_key:   Source key to which to apply derivation.
 * @derived_key:  Derived key.
 *
 * Return: Zero on success; non-zero otherwise.
 */
static int ext4_derive_key_aes(char deriving_key[EXT4_AES_128_ECB_KEY_SIZE],
			       char source_key[EXT4_AES_256_XTS_KEY_SIZE],
			       char derived_key[EXT4_AES_256_XTS_KEY_SIZE])
{
	int res = 0;
	struct ablkcipher_request *req = NULL;
	DECLARE_EXT4_COMPLETION_RESULT(ecr);
	struct scatterlist src_sg, dst_sg;
	struct crypto_ablkcipher *tfm = crypto_alloc_ablkcipher("ecb(aes)", 0,
								0);

	if (IS_ERR(tfm)) {
		res = PTR_ERR(tfm);
		tfm = NULL;
		goto out;
	}
	crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_REQ_WEAK_KEY);
	req = ablkcipher_request_alloc(tfm, GFP_NOFS);
	if (!req) {
		res = -ENOMEM;
		goto out;
	}
	ablkcipher_request_set_callback(req,
			CRYPTO_TFM_REQ_MAY_BACKLOG | CRYPTO_TFM_REQ_MAY_SLEEP,
			derive_crypt_complete, &ecr);
	res = crypto_ablkcipher_setkey(tfm, deriving_key,
				       EXT4_AES_128_ECB_KEY_SIZE);
	if (res < 0)
		goto out;
	sg_init_one(&src_sg, source_key, EXT4_AES_256_XTS_KEY_SIZE);
	sg_init_one(&dst_sg, derived_key, EXT4_AES_256_XTS_KEY_SIZE);
	ablkcipher_request_set_crypt(req, &src_sg, &dst_sg,
				     EXT4_AES_256_XTS_KEY_SIZE, NULL);
	res = crypto_ablkcipher_encrypt(req);
	if (res == -EINPROGRESS || res == -EBUSY) {
		wait_for_completion(&ecr.completion);
		res = ecr.res;
	}

out:
	if (req)
		ablkcipher_request_free(req);
	if (tfm)
		crypto_free_ablkcipher(tfm);
	return res;
}

/*
 * Since we don't do key derivation for ICE-encrypted files, typically there
 * will be many such files that use the same raw key.  To save memory and reduce
 * the number of copies we make of the raw key, these files can share their
 * ext4_crypt_info structures.  Implement this using a simple hash table, keyed
 * by master key descriptor; but all other parts of the ext4_crypt_info,
 * including the key itself, must also match to actually share an entry.
 */
static DEFINE_HASHTABLE(ext4_crypt_infos, 6); /* 6 bits = 64 buckets */
static DEFINE_SPINLOCK(ext4_crypt_infos_lock);

void ext4_free_crypt_info(struct ext4_crypt_info *ci)
{
	if (!ci)
		return;

	if (atomic_read(&ci->ci_dedup_refcnt) != 0) {
		/* dropping reference to deduplicated key */
		if (!atomic_dec_and_lock(&ci->ci_dedup_refcnt,
					 &ext4_crypt_infos_lock))
			return;
		hash_del(&ci->ci_dedup_node);
		spin_unlock(&ext4_crypt_infos_lock);
	}
	crypto_free_ablkcipher(ci->ci_ctfm);
	memset(ci, 0, sizeof(*ci));
	kmem_cache_free(ext4_crypt_info_cachep, ci);
}


static struct ext4_crypt_info *ext4_dedup_crypt_info(struct ext4_crypt_info *ci)
{
	unsigned long hash_key;
	struct ext4_crypt_info *existing;

	BUILD_BUG_ON(sizeof(hash_key) > sizeof(ci->ci_master_key));
	memcpy(&hash_key, ci->ci_master_key, sizeof(hash_key));

	spin_lock(&ext4_crypt_infos_lock);

	hash_for_each_possible(ext4_crypt_infos, existing, ci_dedup_node,
			       hash_key) {
		if (existing->ci_data_mode != ci->ci_data_mode)
			continue;
		if (existing->ci_filename_mode != ci->ci_filename_mode)
			continue;
		if (existing->ci_flags != ci->ci_flags)
			continue;
		if (existing->ci_ctfm != ci->ci_ctfm)
			continue;
		if (crypto_memneq(existing->ci_master_key, ci->ci_master_key,
				  sizeof(ci->ci_master_key)))
			continue;
		if (crypto_memneq(existing->ci_raw_key, ci->ci_raw_key,
				  sizeof(ci->ci_raw_key)))
			continue;
		/* using existing copy of key */
		atomic_inc(&existing->ci_dedup_refcnt);
		spin_unlock(&ext4_crypt_infos_lock);
		ext4_free_crypt_info(ci);
		return existing;
	}

	/* inserting new key */
	atomic_set(&ci->ci_dedup_refcnt, 1);
	hash_add(ext4_crypt_infos, &ci->ci_dedup_node, hash_key);
	spin_unlock(&ext4_crypt_infos_lock);
	return ci;
}

void ext4_free_encryption_info(struct inode *inode,
			       struct ext4_crypt_info *ci)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	struct ext4_crypt_info *prev;

	if (ci == NULL)
		ci = ACCESS_ONCE(ei->i_crypt_info);
	if (ci == NULL)
		return;
	prev = cmpxchg(&ei->i_crypt_info, ci, NULL);
	if (prev != ci)
		return;

	ext4_free_crypt_info(ci);
}

int ext4_get_encryption_info(struct inode *inode)
{
	struct ext4_inode_info *ei = EXT4_I(inode);
	struct ext4_crypt_info *crypt_info;
	char full_key_descriptor[EXT4_KEY_DESC_PREFIX_SIZE +
				 (EXT4_KEY_DESCRIPTOR_SIZE * 2) + 1];
	struct key *keyring_key = NULL;
	struct ext4_encryption_key *master_key;
	struct ext4_encryption_context ctx;
	struct user_key_payload *ukp;
	struct ext4_sb_info *sbi = EXT4_SB(inode->i_sb);
	struct crypto_ablkcipher *ctfm;
	const char *cipher_str;
	int for_fname = 0;
	int mode;
	int res;

	if (ei->i_crypt_info)
		return 0;

	if (!ext4_read_workqueue) {
		res = ext4_init_crypto();
		if (res)
			return res;
	}

	res = ext4_xattr_get(inode, EXT4_XATTR_INDEX_ENCRYPTION,
				 EXT4_XATTR_NAME_ENCRYPTION_CONTEXT,
				 &ctx, sizeof(ctx));
	if (res < 0) {
		if (!DUMMY_ENCRYPTION_ENABLED(sbi) ||
		    ext4_encrypted_inode(inode))
			return res;
		/* Fake up a context for an unencrypted directory */
		memset(&ctx, 0, sizeof(ctx));
		ctx.contents_encryption_mode =
			EXT4_ENCRYPTION_MODE_AES_256_XTS;
		ctx.filenames_encryption_mode =
			EXT4_ENCRYPTION_MODE_AES_256_CTS;
		memset(ctx.master_key_descriptor, 0x42,
		       EXT4_KEY_DESCRIPTOR_SIZE);
	} else if (res != sizeof(ctx))
		return -EINVAL;
	res = 0;

	crypt_info = kmem_cache_alloc(ext4_crypt_info_cachep, GFP_KERNEL);
	if (!crypt_info)
		return -ENOMEM;

	crypt_info->ci_flags = ctx.flags;
	crypt_info->ci_data_mode = ctx.contents_encryption_mode;
	crypt_info->ci_filename_mode = ctx.filenames_encryption_mode;
	crypt_info->ci_ctfm = NULL;
	memcpy(crypt_info->ci_master_key, ctx.master_key_descriptor,
	       sizeof(crypt_info->ci_master_key));
	atomic_set(&crypt_info->ci_dedup_refcnt, 0);
	if (S_ISDIR(inode->i_mode) || S_ISLNK(inode->i_mode))
		for_fname = 1;
	else if (!S_ISREG(inode->i_mode))
		BUG();
	mode = for_fname ? crypt_info->ci_filename_mode :
		crypt_info->ci_data_mode;
	switch (mode) {
	case EXT4_ENCRYPTION_MODE_AES_256_XTS:
		cipher_str = "xts(aes)";
		break;
	case EXT4_ENCRYPTION_MODE_AES_256_CTS:
		cipher_str = "cts(cbc(aes))";
		break;
	case EXT4_ENCRYPTION_MODE_PRIVATE:
		cipher_str = "bugon";
		break;
	default:
		printk_once(KERN_WARNING
			    "ext4: unsupported key mode %d (ino %u)\n",
			    mode, (unsigned) inode->i_ino);
		res = -ENOKEY;
		goto out;
	}
	memcpy(full_key_descriptor, EXT4_KEY_DESC_PREFIX,
	       EXT4_KEY_DESC_PREFIX_SIZE);
	sprintf(full_key_descriptor + EXT4_KEY_DESC_PREFIX_SIZE,
		"%*phN", EXT4_KEY_DESCRIPTOR_SIZE,
		ctx.master_key_descriptor);
	full_key_descriptor[EXT4_KEY_DESC_PREFIX_SIZE +
			    (2 * EXT4_KEY_DESCRIPTOR_SIZE)] = '\0';
	keyring_key = request_key(&key_type_logon, full_key_descriptor, NULL);
	if (IS_ERR(keyring_key)) {
		res = PTR_ERR(keyring_key);
		keyring_key = NULL;
		goto out;
	}
	if (keyring_key->type != &key_type_logon) {
		printk_once(KERN_WARNING
			    "ext4: key type must be logon\n");
		res = -ENOKEY;
		goto out;
	}
	down_read(&keyring_key->sem);
	ukp = ((struct user_key_payload *)keyring_key->payload.data);
	if (ukp->datalen != sizeof(struct ext4_encryption_key)) {
		res = -EINVAL;
		up_read(&keyring_key->sem);
		goto out;
	}
	master_key = (struct ext4_encryption_key *)ukp->data;
	BUILD_BUG_ON(EXT4_AES_128_ECB_KEY_SIZE !=
		     EXT4_KEY_DERIVATION_NONCE_SIZE);
	if (master_key->size != EXT4_AES_256_XTS_KEY_SIZE) {
		printk_once(KERN_WARNING
			    "ext4: key size incorrect: %d\n",
			    master_key->size);
		res = -ENOKEY;
		up_read(&keyring_key->sem);
		goto out;
	}

	/*
	 * For performance reasons, key derivation is skipped for ICE-encrypted
	 * files (data mode PRIVATE).  This is understood not to result in IV
	 * reuse because ICE chooses IVs based on physical sector number.
	 */
	if (for_fname ||
	    (crypt_info->ci_data_mode != EXT4_ENCRYPTION_MODE_PRIVATE)) {
		res = ext4_derive_key_aes(ctx.nonce, master_key->raw,
					  crypt_info->ci_raw_key);
	} else if (ext4_is_ice_enabled()) {
		memcpy(crypt_info->ci_raw_key, master_key->raw,
		       EXT4_MAX_KEY_SIZE);
		crypt_info = ext4_dedup_crypt_info(crypt_info);
		res = 0;
	} else {
		pr_warn("%s: ICE support not available\n", __func__);
		res = -EINVAL;
	}

	up_read(&keyring_key->sem);
	if (res)
		goto out;
	if (for_fname ||
	    (crypt_info->ci_data_mode != EXT4_ENCRYPTION_MODE_PRIVATE)) {
		ctfm = crypto_alloc_ablkcipher(cipher_str, 0, 0);
		if (!ctfm || IS_ERR(ctfm)) {
			res = ctfm ? PTR_ERR(ctfm) : -ENOMEM;
			pr_debug("%s: error %d (inode %u) allocating crypto tfm\n",
				__func__, res, (unsigned) inode->i_ino);
			goto out;
		}
		crypt_info->ci_ctfm = ctfm;
		crypto_ablkcipher_clear_flags(ctfm, ~0);
		crypto_tfm_set_flags(crypto_ablkcipher_tfm(ctfm),
				     CRYPTO_TFM_REQ_WEAK_KEY);
		res = crypto_ablkcipher_setkey(ctfm, crypt_info->ci_raw_key,
					       ext4_encryption_key_size(mode));
		if (res)
			goto out;
		memzero_explicit(crypt_info->ci_raw_key,
			sizeof(crypt_info->ci_raw_key));
	}
	if (cmpxchg(&ei->i_crypt_info, NULL, crypt_info) == NULL)
		crypt_info = NULL;
out:
	if (res == -ENOKEY)
		res = 0;
	key_put(keyring_key);
	ext4_free_crypt_info(crypt_info);
	return res;
}

int ext4_has_encryption_key(struct inode *inode)
{
	struct ext4_inode_info *ei = EXT4_I(inode);

	return (ei->i_crypt_info != NULL);
}
