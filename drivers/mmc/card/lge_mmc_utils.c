/*
 * * lge_mmc_utils.c
 *
 * Please contact p1-fs@lge.com if you have any questions.
 */

#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/module.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/lge_mmc_utils.h>

#define SECTOR_SZ    512


int lge_pow(int val, int order)
{
	int i;
	if(order==0)
		return 1;
	for(i=0; i<(order-1); i++){
		val = val*val;
	}
	return val;
}

void lge_mmc_test_prepare_mrq(struct mmc_card *card,
		struct mmc_request *mrq, struct scatterlist *sg, unsigned sg_len,
		unsigned dev_addr, unsigned blocks, unsigned blksz, int write)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if(blocks > 1){
		mrq->cmd->opcode = write ?
			MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK;
	} else {
		mrq->cmd->opcode = write ?
			MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
	}

	mrq->cmd->arg = dev_addr;
	if(!mmc_card_blockaddr(card))
		mrq->cmd->arg <<=9;
	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	if(blocks == 1)
		mrq->stop = NULL;
	else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, card);
}

static int lge_mmc_test_busy(struct mmc_command *cmd)
{
	return !(cmd->resp[0] & R1_READY_FOR_DATA) ||
		(R1_CURRENT_STATE(cmd->resp[0]) == R1_STATE_PRG);
}

static int lge_mmc_test_wait_busy(struct mmc_card *card)
{
	int ret, busy;
	struct mmc_command cmd = {0};
	busy = 0;
	do {
		memset(&cmd, 0, sizeof(struct mmc_command));
		cmd.opcode = MMC_SEND_STATUS;
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if(ret)
			break;
		if(!busy && lge_mmc_test_busy(&cmd)) {
			busy = 1;
			if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
				pr_info("%s: Warning: Host did not "
					"wait for busy state to end.\n",
					mmc_hostname(card->host));
		}
	} while (lge_mmc_test_busy(&cmd));
	return ret;
}

static int lge_mmc_test_check_result(struct mmc_request *mrq)
{
	int ret;
	BUG_ON(!mrq || !mrq->cmd || !mrq->data);

	ret = 0;

	if (!ret && mrq->cmd->error){
		ret = mrq->cmd->error;
		printk("mrq->mcd->error %u\n", mrq->cmd->error);
	}
	if (!ret && mrq->data->error){
		ret = mrq->data->error;
		printk("mrq->data->error %u\n", mrq->data->error);
	}
	if (!ret && mrq->stop && mrq->stop->error){
		ret = mrq->stop->error;
		printk("mrq->stop->error %u\n", mrq->stop->error);
	}
	if (!ret && mrq->data->bytes_xfered !=
			mrq->data->blocks * mrq->data->blksz){
		printk("bytes_xfered :%u, blocks*blksz : %u\n", mrq->data->bytes_xfered, mrq->data->blocks*mrq->data->blksz);
		ret = RESULT_FAIL;
	}
	if (ret == -EINVAL){
		ret = RESULT_UNSUP_HOST;
	}
	return ret;
}

static void lge_mmc_test_free_mem(struct lge_mmc_test_mem *mem)
{
	if (!mem)
		return;
	while (mem->cnt--)
		__free_pages(mem->arr[mem->cnt].page,
				mem->arr[mem->cnt].order);
	kfree(mem->arr);
	kfree(mem);
}
static struct lge_mmc_test_mem *lge_mmc_test_alloc_mem(unsigned long min_sz,
							unsigned long max_sz,
							unsigned int max_segs,
							unsigned int max_seg_sz)
{
	unsigned long max_page_cnt = DIV_ROUND_UP(max_sz, PAGE_SIZE);
	unsigned long min_page_cnt = DIV_ROUND_UP(min_sz, PAGE_SIZE);
	unsigned long max_seg_page_cnt = DIV_ROUND_UP(max_seg_sz, PAGE_SIZE);
	unsigned long page_cnt = 0;
	unsigned long limit = nr_free_buffer_pages() >> 4;
	struct lge_mmc_test_mem *mem;

	if (max_page_cnt > limit)
		max_page_cnt = limit;

	if (min_page_cnt > max_page_cnt)
		min_page_cnt = max_page_cnt;

	if (max_seg_page_cnt > max_page_cnt)
		max_seg_page_cnt = max_page_cnt;

	if (max_segs > max_page_cnt)
		max_segs = max_page_cnt;

	mem = kzalloc(sizeof(struct lge_mmc_test_mem), GFP_KERNEL);
	if (!mem)
		return NULL;

	mem->arr = kzalloc(sizeof(struct lge_mmc_test_pages) * max_segs, GFP_KERNEL);
	if (!mem->arr)
		goto out_free;

	while (max_page_cnt) {
		struct page *page;
		unsigned int order;
		gfp_t flags = GFP_KERNEL | GFP_DMA | __GFP_NOWARN | __GFP_NORETRY;
		order = get_order(max_seg_page_cnt << PAGE_SHIFT);
		while (1) {
			page = alloc_pages(flags, order);
			if (page || !order)
				break;
			order -= 1;
		}
		if (!page) {
			if (page_cnt < min_page_cnt)
				goto out_free;
			break;
		}
		mem->arr[mem->cnt].page = page;
		mem->arr[mem->cnt].order = order;
		mem->cnt += 1;
		if (max_page_cnt <= (1UL << order))
			break;
		max_page_cnt -= 1UL << order;
		page_cnt += 1UL << order;
		if (mem->cnt >= max_segs) {
			if (page_cnt < min_page_cnt)
				goto out_free;
			break;
		}
	}

	return mem;

out_free:
	lge_mmc_test_free_mem(mem);
	return NULL;
}
/*
static unsigned int lge_mmc_test_capacity(struct mmc_card *card)
{
	if (!mmc_card_sd(card) && mmc_card_blockaddr(card))
		return card->ext_csd.sectors;
	else
		return card->csd.capacity << (card->csd.read_blkbits - 9);
}
static int lge_mmc_test_area_erase(struct lge_mmc_test_card *lge_test)
{
	struct lge_mmc_test_area *t = &lge_test->area;

	if (!mmc_can_erase(lge_test->card))
		return 0;

	return mmc_erase(lge_test->card, t->dev_addr, t->max_sz >> 9, MMC_ERASE_ARG);
}

static int lge_mmc_test_area_fill(struct lge_mmc_test_card *lge_test)
{
//	struct lge_mmc_test_area *t = &lge_test->area;
//	return lge_mmc_test_area_io(lge_test, t->max_tfr, t->dev_addr, 1, 0, 0);
	return 1;
}
*/

static int lge_mmc_test_area_cleanup(struct lge_mmc_test_area *t)
{
	kfree(t->sg);
	lge_mmc_test_free_mem(t->mem);
	return 0;
}

static int lge_mmc_test_map_sg(struct lge_mmc_test_mem *mem, unsigned long size,
		struct scatterlist *sglist, unsigned int max_segs,
		unsigned int max_seg_sz, unsigned int *sg_len,
		int min_sg_len)
{
	struct scatterlist *sg = NULL;
	unsigned int i;
	unsigned long sz = size;

	sg_init_table(sglist, max_segs);
	if (min_sg_len > max_segs)
		min_sg_len = max_segs;

	*sg_len = 0;
	do {
		for (i = 0; i < mem->cnt; i++) {
			unsigned long len = PAGE_SIZE << mem->arr[i].order;
			if (min_sg_len && (size / min_sg_len < len))
				len = ALIGN(size / min_sg_len, SECTOR_SZ);
			if (len > sz)
				len = sz;
			if (len > max_seg_sz)
				len = max_seg_sz;
			if (sg)
				sg = sg_next(sg);
			else
				sg = sglist;
			if (!sg)
				return -EINVAL;
			sg_set_page(sg, mem->arr[i].page, len, 0);
			sz -= len;
			*sg_len += 1;
			if (!sz)
				break;
		}
	} while (sz);
	if (sg)
		sg_mark_end(sg);
	return 0;
}


static int lge_mmc_test_area_init(struct lge_mmc_test_area *t, struct mmc_card *card, u8 *data, unsigned int size)
{
	int ret, i, length;

	t->max_segs = card->host->max_segs;
	t->max_seg_sz = card->host->max_seg_size & ~(CARD_BLOCK_SIZE - 1);
	t->max_tfr = size;

	if (t->max_tfr >> 9 > card->host->max_blk_count)
		t->max_tfr = card->host->max_blk_count << 9;
	if (t->max_tfr > card->host->max_req_size)
		t->max_tfr = card->host->max_req_size;
	if (t->max_tfr / t->max_seg_sz > t->max_segs)
		t->max_tfr = t->max_segs * t->max_seg_sz;

	t->mem = lge_mmc_test_alloc_mem(1, t->max_tfr, t->max_segs, t->max_seg_sz);
	if(!t->mem){
		return -ENOMEM;
	}

	length = 0;
	for (i = 0; i < t->mem->cnt; i++) {
		memcpy(page_address(t->mem->arr[i].page), data + length,
				min(size - length, t->max_seg_sz));
		length += t->max_seg_sz;
	}

	t->sg = kmalloc(sizeof(struct scatterlist) * t->max_segs, GFP_KERNEL);
	if(!t->sg){
		ret = -ENOMEM;
		goto out_free;
	}

	ret = lge_mmc_test_map_sg(t->mem, size, t->sg,
			t->max_segs, t->max_seg_sz, &t->sg_len, 1);
	if (ret != 0){
		goto out_free;
	}
	return 0;

out_free:
	lge_mmc_test_area_cleanup(t);
	return ret;
}

static int lge_mmc_test_simple_transfer(struct mmc_card *card,
		struct scatterlist *sg, unsigned sg_len, unsigned dev_addr,
		unsigned blocks, unsigned blksz, int write)
{
	struct mmc_request mrq = {0};
	struct mmc_command cmd = {0};
	struct mmc_command stop = {0};
	struct mmc_data data = {0};

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

	lge_mmc_test_prepare_mrq(card, &mrq, sg, sg_len, dev_addr, blocks, blksz, write);
	mmc_wait_for_req(card->host, &mrq);
	lge_mmc_test_wait_busy(card);
	return lge_mmc_test_check_result(&mrq);
}

int lge_mmc_basic_write(struct mmc_card *card, u32 arg, u8 *src, u32 size)
{
	struct lge_mmc_test_area mem;
	int rc;

	mem.sg = NULL;
	mem.mem = NULL;

	if(!src){
		pr_err("lge_test: %s: data buffer is NULL\n", mmc_hostname(card->host));
		rc = -1;
		goto exit;
	}
	rc = lge_mmc_test_area_init(&mem, card, src, size);
	if(rc != 0)
		goto exit;

	pr_info("alex arg : %u, sector : %d\n", arg, size/SECTOR_SZ);

        rc = lge_mmc_test_simple_transfer(card, mem.sg,
			mem.sg_len, arg, size/SECTOR_SZ, SECTOR_SZ, 1);
exit:
	lge_mmc_test_area_cleanup(&mem);
	return rc;
}

int lge_mmc_basic_read(struct mmc_card *card, u32 arg, u8 *src, u32 size)
{
	struct lge_mmc_test_area mem;
	int rc;

	mem.sg = NULL;
	mem.mem = NULL;

	if(!src){
		pr_err("lge_test: %s: data buffer is NULL\n", mmc_hostname(card->host));
		rc = -1;
		goto exit;
	}

	rc = lge_mmc_test_area_init(&mem, card, src, size);
	if(rc != 0)
		goto exit;

	rc = lge_mmc_test_simple_transfer(card, mem.sg, mem.sg_len, arg, size/SECTOR_SZ, SECTOR_SZ, 0);

exit:
	lge_mmc_test_area_cleanup(&mem);
	return rc;
}

static void lge_mmc_sector_transfer(struct mmc_card *card, u8 *buf, u32 arg, int write)
{
	struct mmc_request mrq = {NULL};
	struct mmc_command cmd = {0};
	struct mmc_data data = {0};
	struct scatterlist sg;

	mrq.cmd = &cmd;
	mrq.data = &data;

	if(write)
		cmd.opcode = MMC_WRITE_BLOCK;
	else
		cmd.opcode = MMC_READ_SINGLE_BLOCK;
	cmd.arg = arg;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = SECTOR_SZ;
	data.blocks = 1;
	if(write)
		data.flags = MMC_DATA_WRITE;
	else
		data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;

	sg_init_one(&sg, buf, SECTOR_SZ);
	mmc_set_data_timeout(&data, card);
	mmc_wait_for_req(card->host, &mrq);
	return ;
}

int lge_mmc_basic_verify(struct mmc_card *card, u32 arg, u8 *src, u32 size)
{
	struct lge_mmc_test_area mem;
	int rc;
	u32 i;
	u8 *buf;
	u8 *org;
	u8 val;

	mem.sg = NULL;
	mem.mem = NULL;

	if(!src){
		pr_err("lge_test: %s: data buffer is NULL\n", __func__);
		rc = -1;
		goto exit;
	}
	val = src[0];
	rc = lge_mmc_test_area_init(&mem, card, src, size);
	if(rc != 0)
		goto exit;

        rc = lge_mmc_test_simple_transfer(card, mem.sg,
			mem.sg_len, arg, size/SECTOR_SZ, SECTOR_SZ, 1);

	buf = kmalloc(SECTOR_SZ, GFP_KERNEL);
	org = kmalloc(SECTOR_SZ, GFP_KERNEL);
	if(!buf || !org){
		pr_err("lge_test: %s: single buffer is NULL\n", __func__);
		rc = -1;
		goto exit;
	}
	memset(org, val, SECTOR_SZ);
	for(i=0; i<(size/SECTOR_SZ); i++){
		lge_mmc_sector_transfer(card, buf, arg+i, 0);
		if(memcmp(buf, org, SECTOR_SZ)){
			pr_err("lge_test: %s: %d'st verify error : org[0] : %x, buf[0] : %x\n",
					__func__, i, org[0], buf[0]);
			rc = -1;
			goto exit;
		}
		else{
			pr_info("lge_test: %s: %d'st verify success : org[0] : %x, buf[0] : %x\n",
					__func__, i, org[0], buf[0]);
		}
	}
exit:
	lge_mmc_test_area_cleanup(&mem);
	return rc;
}


int lge_mmc_discard(struct mmc_card *card, u32 arg, u8 *data)
{
	int i, rc;
	u32 nr_sectors;
	int erase_mode;

	nr_sectors = 0;
	for(i=0; i<4; i++)
		nr_sectors += data[i]*lge_pow(100, i);

	pr_info("%s : start_address :%u, nr_sectors :%u\n", __func__, arg, nr_sectors);
	erase_mode = MMC_DISCARD_ARG;
	rc = mmc_erase(card, arg, nr_sectors, erase_mode);
	return rc;
}

int lge_mmc_read_bkops_info(struct mmc_card *card, u8 *data)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;

	err = mmc_send_ext_csd(card, ext_csd);
	if(err) {
		pr_err("BKOPS info: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
		goto exit;
	}
	*data = ext_csd[EXT_CSD_BKOPS_EN];
exit:
	return err;
}
int lge_mmc_read_packed_info(struct mmc_card *card, u8 *data)
{
	if(card->host->caps2 & MMC_CAP2_PACKED_WR)
		*data = 1;
	else
		*data = 0;
	return 0;
}
int lge_mmc_read_clkscaling_info(struct mmc_card *card, u8 *data)
{
	if(card->host->clk_scaling.enable)
		*data =1;
	else
		*data =0;
	return 0;
}
int lge_mmc_read_num_packed(struct mmc_card *card, u8 *data)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	err = mmc_send_ext_csd(card, ext_csd);
	if(err) {
		pr_err("NUM Packed: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
		goto exit;
	}
	*data = ext_csd[EXT_CSD_MAX_PACKED_WRITES];
exit:
	return err;
}
int lge_mmc_read_cache_info(struct mmc_card *card, u8 *data)
{
	u8 ext_csd[CARD_BLOCK_SIZE];
	int err;
	err = mmc_send_ext_csd(card, ext_csd);
	if(err) {
		pr_err("CACHE info: %s: error %d sending ext_csd\n",
				mmc_hostname(card->host), err);
		goto exit;
	}
	*data = ext_csd[EXT_CSD_CACHE_CTRL];
exit:
	return err;
}
