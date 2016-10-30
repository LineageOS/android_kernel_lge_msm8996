/*
 * lge_mmc_utils.h
 *
 * Please, contact p1-fs@lge.com if you have qustions anything.
 *
 */

#if !defined(_LGE_MMC_UTILS_H_)
#define _LGE_MMC_UTILS_H_

#include <linux/mmc/card.h>
#include <linux/mmc/ffu.h>

#define RESULT_OK           0
#define RESULT_FAIL         1
#define RESULT_UNSUP_HOST   2
#define RESULT_UNSUP_CARD   3

#define CARD_BLOCK_SIZE 512

#define TEST_AREA_MAX_SIZE (128 * 1024 * 1024)

/*
 * Index LGE MMC Utils test item
 */
#define MMC_FFU_DOWNLOAD_OP                         302
#define MMC_FFU_INSTALL_OP                          303

#define LGE_MMC_FEATURE_BKOPS_INFO_OP               411
#define LGE_MMC_FEATURE_PACKED_INFO_OP              412
#define LGE_MMC_FEATURE_NUMPACKED_OP                413
#define LGE_MMC_FEATURE_CLKSCALING_INFO_OP          414
#define LGE_MMC_FEATURE_CACHE_INFO_OP               415
#define LGE_MMC_FEATURE_PNM_OP                      430
#define LGE_MMC_FEATURE_MID_OP                      431

#define LGE_MMC_FEATURE_BASIC_WRITE_OP              450
#define LGE_MMC_FEATURE_BASIC_VERIFY_OP             451
#define LGE_MMC_FEATURE_DISCARD_OP                  452
#define LGE_MMC_FEATURE_BASIC_READ_OP               453

/*
 * Fieeld Frimware Update (FFU) opcodes
 */

#define MMC_FFU_ENABLE 0x0
#define MMC_FFU_CONFIG 0x1
#define MMC_FFU_SUPPORTED_MODES 0x1
#define MMC_FFU_FEATURES 0x1

#define FFU_ENABLED(ffu_enable) (ffu_enable & MMC_FFU_CONFIG)
#define FFU_SUPPORTED_MODE(ffu_sup_mode) \
        (ffu_sup_mode && MMC_FFU_SUPPORTED_MODES)
#define FFU_CONFIG(ffu_config) (ffu_config & MMC_FFU_CONFIG)
#define FFU_FEATURES(ffu_fetures) (ffu_fetures & MMC_FFU_FEATURES)

#define BUFFER_ORDER            2
#define BUFFER_SIZE             (PAGE_SIZE << BUFFER_ORDER)

struct lge_mmc_test_pages {
	struct page *page;
	unsigned int order;
};

struct lge_mmc_test_mem {
	struct lge_mmc_test_pages *arr;
	unsigned int		  cnt;
};

struct lge_mmc_test_area {
	unsigned long max_sz;
	unsigned int dev_addr;
	unsigned int max_tfr;
	unsigned int max_segs;
	unsigned int max_seg_sz;
	unsigned int blocks;
	unsigned int sg_len;
	struct lge_mmc_test_mem *mem;
	struct scatterlist *sg;
};







int lge_mmc_read_bkops_info(struct mmc_card *card, u8 *data);
int lge_mmc_read_packed_info(struct mmc_card *card, u8 *data);
int lge_mmc_read_num_packed(struct mmc_card *card, u8 *data);
int lge_mmc_read_clkscaling_info(struct mmc_card *card, u8 *data);
int lge_mmc_read_cache_info(struct mmc_card *card, u8 *data);
int lge_mmc_basic_write(struct mmc_card *card, u32 arg, u8 *data, u32 size);
int lge_mmc_basic_read(struct mmc_card *card, u32 arg, u8 *data, u32 size);
int lge_mmc_basic_verify(struct mmc_card *card, u32 arg, u8 *data, u32 size);
int lge_mmc_discard(struct mmc_card *card, u32 arg, u8 *data);

#endif
