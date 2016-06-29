#ifndef TTY_LGE_DM_H_
#define TTY_LGE_DM_H_

#ifdef CONFIG_LGE_DM_APP

struct dm_tty {
    wait_queue_head_t   waitq;
    struct task_struct *tty_ts;
    struct tty_driver *tty_drv;
    struct tty_struct *tty_str;
    int tty_state;
    int logging_mode;
    int set_logging;
    struct workqueue_struct *dm_wq;
    struct work_struct dm_usb_work;
    struct work_struct dm_dload_work;
    struct diag_request *dm_usb_req;
    int id;
    int ctx;
    int num_tbl_entries;
	spinlock_t lock;	
    struct diag_buf_tbl_t *tbl;
    struct diag_mux_ops *ops;
};

extern struct dm_tty *lge_dm_tty;

#endif
#endif /*TTY_LGE_DM_H_ */
