/* touch_lg4945_abt.c
 *
 * Copyright (C) 2015 LGE.
 *
 * Author: hoyeon.jang@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define TS_MODULE "[abt]"

#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>

/*
 *  Include to Local Header File
 */
#include "touch_lg4945.h"
#include "touch_lg4945_abt.h"

#if USE_ABT_MONITOR_APP

#define MAX_REPORT_SLOT			16
#define P_CONTOUR_POINT_MAX		8
#define DEF_RNDCPY_EVERY_Nth_FRAME	(4)
#define PACKET_SIZE			128

static int ocd_piece_size;
static u32 prev_rnd_piece_no = DEF_RNDCPY_EVERY_Nth_FRAME;

struct mutex abt_comm_lock;
struct mutex abt_socket_lock;
struct sock_comm_t abt_comm;
int abt_socket_mutex_flag;
int abt_socket_report_mode;

u16 frame_num = 0;
int abt_report_mode;
u8 abt_report_point;
u8 abt_report_ocd;
u32 CMD_GET_ABT_DEBUG_REPORT;
int abt_report_mode_onoff;
enum ABT_CONNECT_TOOL abt_conn_tool = NOTHING;

/*sysfs*/
u16 abt_ocd[2][ACTIVE_SCREEN_CNT_X * ACTIVE_SCREEN_CNT_Y];
u8 abt_ocd_read;
u8 abt_reportP[256];
char abt_head[128];
int abt_show_mode;
int abt_ocd_on;
u32 abt_compress_flag;
int abt_head_flag;

static int abt_ocd_off = 1;
int set_get_data_func = 0;

struct T_TouchInfo {
	u8 wakeUpType;
	u8 touchCnt:5;
	u8 buttonCnt:3;
	u16 palmBit;
} __packed;

struct T_TouchData {
	u8 toolType:4;
	u8 event:4;
	s8 track_id;
	u16 x;
	u16 y;
	union {
		u8 pressure;
		u8 contourPcnt;
	} byte;
	u8 angle;
	u16 width_major;
	u16 width_minor;
} __packed;


struct T_TCRegCopy2Report {
	u32 tc_reg_copy[5];
};

struct T_OnChipDebug {
	u32 rnd_addr;
	u32 rnd_piece_no;
};

struct T_ReportP {
	struct T_TouchInfo touchInfo;
	struct T_TouchData touchData[MAX_REPORT_SLOT];
	u16 contourP[P_CONTOUR_POINT_MAX];
	struct T_TCRegCopy2Report tc_reg;
	struct T_OnChipDebug ocd;
	u8 dummy[16];
};

int abt_force_set_report_mode(struct device *dev, u32 mode);

int sic_abt_is_set_func(void)
{
	return set_get_data_func;
}

static void sic_set_get_data_func(u8 mode)
{
	if (mode == 1) {
		set_get_data_func = 1;
		TOUCH_I("(%s)change get_data \"sic_ts_get_data_debug_mode\"\n",
			                        __func__);
	} else {
		set_get_data_func = 0;
		TOUCH_I("(%s)change get_data \"sic_ts_get_data\"\n",
			                        __func__);
	}
}

static int32_t abt_ksocket_init_send_socket(struct sock_comm_t *abt_socket)
{
	int ret;
	struct socket *sock;
	struct sockaddr_in *addr = &abt_socket->addr_send;
	int *connected = &abt_socket->send_connected;
	char *ip = (char *)abt_socket->send_ip;

	ret = sock_create(AF_INET,
			  SOCK_DGRAM,
			  IPPROTO_UDP,
			  &(abt_socket->sock_send));
	sock = abt_socket->sock_send;

	if (ret >= 0) {
		memset(addr, 0, sizeof(struct sockaddr));
		addr->sin_family = AF_INET;
		addr->sin_addr.s_addr = in_aton(ip);
		addr->sin_port = htons(SEND_PORT);
	} else {
		TOUCH_I(MODULE_NAME": can not create socket %d\n",
			-ret);
		goto error;
	}

	ret = sock->ops->connect(sock,
				 (struct sockaddr *)addr,
				 sizeof(struct sockaddr),
				 !O_NONBLOCK);

	if (ret < 0) {
		TOUCH_I(
			MODULE_NAME": Could not connect to send socket, error = %d\n",
			-ret);
		goto error;
	} else {
		*connected = 1;
		TOUCH_I(MODULE_NAME ": connect send socket (%s,%d)(\n",
			ip, SEND_PORT);
	}

	return ret;
error:
	sock = NULL;
	*connected = 0;
	return ret;
}

static int abt_ksocket_receive(unsigned char *buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	struct socket *sock = abt_comm.sock;
	struct sockaddr_in *addr = &abt_comm.addr;
	mm_segment_t oldfs;
	unsigned int flag = 0;
	int size = 0;

	if (abt_conn_tool == ABT_PCTOOL) {
		sock = abt_comm.client_sock;
		addr = &abt_comm.client_addr;
	}

	if (sock == NULL)
		return 0;

	if (abt_conn_tool == ABT_PCTOOL) {
		abt_comm.client_sock->ops = abt_comm.sock->ops;
		flag = MSG_WAITALL;
	}

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = flag;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	size = sock_recvmsg(sock, &msg, len, msg.msg_flags);
	set_fs(oldfs);

	abt_comm.sock_listener(buf, size);

	return size;
}

static void abt_ksocket_start_for_pctool(struct device *dev)
{
	static int client_connected;
	int size, err;
	unsigned char *buf;

	/* kernel thread initialization */
	abt_comm.running = 1;
	abt_comm.dev = dev;

	/* create a socket */
	err = sock_create_kern(AF_INET,
			       SOCK_STREAM,
			       IPPROTO_TCP,
			       &abt_comm.sock);
	if (err < 0) {
		TOUCH_I(
			MODULE_NAME
			": could not create a datagram socket, error = %d\n\n",
			-ENXIO);
		goto out;
	}

	err = sock_create_lite(AF_INET,
			       SOCK_STREAM,
			       IPPROTO_TCP,
			       &abt_comm.client_sock);
	if (err < 0) {
		TOUCH_I(
			MODULE_NAME
			": could not create a Client socket, error = %d\n\n",
			-ENXIO);
		goto out;
	}

	memset(&abt_comm.addr, 0, sizeof(struct sockaddr));
	abt_comm.addr.sin_family = AF_INET;
	abt_comm.addr.sin_addr.s_addr = htonl(INADDR_ANY);
	abt_comm.addr.sin_port = htons(DEFAULT_PORT);
	err = abt_comm.sock->ops->bind(abt_comm.sock,
				       (struct sockaddr *)&abt_comm.addr,
				       sizeof(struct sockaddr_in));

	if (err < 0)
		goto out;

	err = abt_comm.sock->ops->listen(abt_comm.sock, 10);

	if (err < 0)
		goto out;

	/* init  data send socket */
	abt_ksocket_init_send_socket(&abt_comm);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (abt_comm.running != 1)
			break;

		if (client_connected != 1) {
			msleep(200);
			err = abt_comm.sock->ops->accept(abt_comm.sock,
							 abt_comm.client_sock,
							 O_NONBLOCK);

			if (err < 0)
				continue;
		 else if (err >= 0) {
				TOUCH_I(
					MODULE_NAME": accept ok(%d)\n", err);
				client_connected = 1;
			}
		}

		size = 0;

		buf = kzalloc(sizeof(struct s_comm_packet), GFP_KERNEL);
		size = abt_ksocket_receive(&buf[0],
					   sizeof(struct s_comm_packet));

		if (size <= 0)
			client_connected = 0;

		TOUCH_I(
			MODULE_NAME ": RECEIVE OK, size(%d)\n\n", size);

		if (kthread_should_stop()) {
			TOUCH_I(": kthread_should_stop\n");
			break;
		}

		if (size < 0)
			TOUCH_I(
				": error getting datagram, sock_recvmsg error = %d\n",
				size);
		if (buf != NULL)
			kfree(buf);
	}

out:
	__set_current_state(TASK_RUNNING);
	abt_comm.running = 0;
}

static void abt_ksocket_start_for_abtstudio(struct device *dev)
{
	int size, err;
	int bufsize = 10;
	unsigned char buf[bufsize+1];

	frame_num = 0;

	/* kernel thread initialization */
	abt_comm.running = 1;
	abt_comm.dev = dev;

	/* create a socket */
	err = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &abt_comm.sock);
	if (err < 0) {
		TOUCH_I(
			": could not create a datagram socket, error = %d\n",
			-ENXIO);
		goto out;
	}

	memset(&abt_comm.addr, 0, sizeof(struct sockaddr));
	abt_comm.addr.sin_family = AF_INET;
	abt_comm.addr.sin_addr.s_addr = htonl(INADDR_ANY);
	abt_comm.addr.sin_port = htons(DEFAULT_PORT);
	err = abt_comm.sock->ops->bind(abt_comm.sock,
					(struct sockaddr *)&abt_comm.addr,
					sizeof(struct sockaddr));
	if (err < 0) {
		TOUCH_I(
			MODULE_NAME
			": Could not bind to receive socket, error = %d\n",
			-err);
		goto out;
	}

	TOUCH_I(": listening on port %d\n", DEFAULT_PORT);

	/* init raw data send socket */
	abt_ksocket_init_send_socket(&abt_comm);

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		memset(&buf, 0, bufsize+1);

		size = 0;
		if (abt_comm.running == 1) {
			size = abt_ksocket_receive(buf, bufsize);
			TOUCH_I(": receive packet\n");
		} else {
			TOUCH_I(": running off\n");
			break;
		}

		if (kthread_should_stop())
			break;

		if (size < 0)
			TOUCH_I(
			": error getting datagram, sock_recvmsg error = %d\n",
			size);

		msleep(50);
	}

out:
	__set_current_state(TASK_RUNNING);
	abt_comm.running = 0;

}

static void abt_recv_packet_parsing(struct device *dev,
				struct s_comm_packet *p_packet_from_pc,
				struct s_comm_packet *p_packet_to_pc)
{
	int idx = 0;
	int ret = -1;
	u32 addr_ptr = p_packet_from_pc->addr;
	u8 *data_ptr_from_pc = (u8 *)&(p_packet_from_pc->data[0]);
	u8 *data_ptr_to_pc = (u8 *)&(p_packet_to_pc->data[0]);
	int packet_cnt = p_packet_from_pc->data_size / PACKET_SIZE;

	if (p_packet_from_pc->flag.rw == READ_TYPE) {

		for (idx = 0; idx < packet_cnt; idx++) {
			ret = lg4945_reg_write(dev,
						addr_ptr,
						data_ptr_to_pc,
						PACKET_SIZE);
			if (ret < 0)
				break;

			addr_ptr += (PACKET_SIZE / 4);
			data_ptr_to_pc += PACKET_SIZE;
		}

		if (p_packet_from_pc->data_size % PACKET_SIZE != 0) {
			ret = lg4945_reg_read(dev,
						addr_ptr,
						data_ptr_to_pc,
						p_packet_from_pc->data_size %
						PACKET_SIZE);
		}
	} else if (p_packet_from_pc->flag.rw == WRITE_TYPE) {
		for (idx = 0; idx < packet_cnt; idx++) {
			ret = lg4945_reg_write(dev,
						addr_ptr,
						data_ptr_from_pc,
						PACKET_SIZE);
			addr_ptr += (PACKET_SIZE / 4);
			data_ptr_from_pc += PACKET_SIZE;
		}

		if (p_packet_from_pc->data_size % PACKET_SIZE != 0) {
			ret = lg4945_reg_write(dev, addr_ptr,
						data_ptr_from_pc,
						p_packet_from_pc->data_size %
						PACKET_SIZE);
		}
	}

	p_packet_to_pc->cmd = DEBUG_DATA_RW_MODE;
	p_packet_to_pc->flag.rw = p_packet_from_pc->flag.rw;
	p_packet_to_pc->status = ret;
	p_packet_to_pc->addr = p_packet_from_pc->addr;

	if (p_packet_from_pc->flag.rw == READ_TYPE)
		p_packet_to_pc->data_size = p_packet_from_pc->data_size;
	else if (p_packet_from_pc->flag.rw == WRITE_TYPE)
		p_packet_to_pc->data_size = 0;
}

static uint32_t abt_ksocket_send_exit(void)
{
	uint32_t ret = 0;
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	struct socket *sock;
	struct sockaddr_in addr;
	uint8_t buf = 1;

	ret = sock_create(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
	if (ret < 0) {
		TOUCH_I(
			MODULE_NAME
			": could not create a datagram socket, error = %d\n",
			-ENXIO);
		goto error;
	}

	memset(&addr, 0, sizeof(struct sockaddr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = in_aton("127.0.0.1");
	addr.sin_port = htons(DEFAULT_PORT);

	ret = sock->ops->connect(
			sock,
			(struct sockaddr *)&addr,
			sizeof(struct sockaddr),
			!O_NONBLOCK);

	if (ret < 0) {
		TOUCH_I(
			MODULE_NAME
			": Could not connect to send socket, error = %d\n",
			-ret);
		goto error;
	} else {
		TOUCH_I(
			MODULE_NAME": connect send socket (%s,%d)\n",
			"127.0.0.1",
			DEFAULT_PORT);
	}

	iov.iov_base = &buf;
	iov.iov_len = 1;

	msg.msg_flags = 0;
	msg.msg_name = &addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();

	set_fs(KERNEL_DS);
	ret = sock_sendmsg(sock, &msg, 1);
	TOUCH_I(": exit send message return : %d\n", ret);
	set_fs(oldfs);
	sock_release(sock);

error:
	return ret;
}

static int abt_ksocket_send(struct socket *sock,
			struct sockaddr_in *addr,
			unsigned char *buf, int len)
{
	struct msghdr msg;
	struct iovec iov;
	mm_segment_t oldfs;
	int size = 0;

	if (sock == NULL)
		return 0;

	iov.iov_base = buf;
	iov.iov_len = len;

	msg.msg_flags = 0;
	msg.msg_name = addr;
	msg.msg_namelen  = sizeof(struct sockaddr_in);
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = NULL;

	oldfs = get_fs();

	set_fs(KERNEL_DS);
	size = sock_sendmsg(sock, &msg, len);
	set_fs(oldfs);

	return size;
}

static int abt_set_report_mode(struct device *dev, u32 mode)
{
	int ret;
	TOUCH_I(
		"[ABT](%s)mode:%d\n", __func__, mode);
	if (abt_report_mode == mode) {
		TOUCH_I(
			"[ABT](%s) mode(%d) is already set\n",
			__func__, mode);
		return 0;
	}

	if (mode < 0) {
		TOUCH_I(
			"[ABT](%s) mode(%d) is invalid\n",
			__func__, mode);
		return -EINVAL;
	}

	ret = abt_force_set_report_mode(dev, mode);

	return ret;
}

static uint32_t abt_ksocket_rcv_from_pctool(uint8_t *buf, uint32_t len)
{
	abt_comm.recv_packet = (struct s_comm_packet *)buf;

	TOUCH_I(
		MODULE_NAME
		": rev_cmd(%d), rev_addr(%d), rev_rw(%d), recv_dataS(%d)\n\n",
		abt_comm.recv_packet->cmd,
		abt_comm.recv_packet->addr,
		abt_comm.recv_packet->flag.rw,
		abt_comm.recv_packet->data_size);

	if (abt_comm.recv_packet->cmd == DEBUG_DATA_CAPTURE_MODE) {
		TOUCH_I(
			MODULE_NAME ": DEBUG_DATA_CAPTURE_MODE\n\n");
		abt_set_report_mode(abt_comm.dev,
				    abt_comm.recv_packet->flag.data_type);
		sic_set_get_data_func(1);

		abt_report_point = 1;
		abt_report_ocd = 1;
	} else if (abt_comm.recv_packet->cmd == DEBUG_DATA_RW_MODE) {
		TOUCH_I(
			MODULE_NAME ": DEBUG_DATA_RW_MODE\n\n");
		abt_recv_packet_parsing(abt_comm.dev,
					abt_comm.recv_packet,
					&abt_comm.send_packet);
		/*abt_ksocket_send(abt_comm.sock,
		  &abt_comm.addr,
		  (u8 *)&abt_comm.send_packet,
		  sizeof(s_comm_packet));*/
		abt_ksocket_send(abt_comm.client_sock,
				 &abt_comm.client_addr,
				 (u8 *)&abt_comm.send_packet,
				 sizeof(struct s_comm_packet));
	} else if (abt_comm.recv_packet->cmd == DEBUG_DATA_CMD_MODE) {
		TOUCH_I(": DEBUG_DATA_CMD_MODE\n");
		mutex_lock(&abt_comm_lock);

		switch (abt_comm.recv_packet->addr) {
		case CMD_RAW_DATA_REPORT_MODE_WRITE:
			TOUCH_I(
				MODULE_NAME": mode setting - %d\n",
				abt_comm.recv_packet->report_type);
			abt_set_report_mode(abt_comm.dev,
					    abt_comm.recv_packet->report_type);
			break;

		case CMD_RAW_DATA_COMPRESS_WRITE:
			TOUCH_I(
				MODULE_NAME": data_compress val(%d)\n",
				abt_comm.recv_packet->report_type);
			lg4945_reg_write(abt_comm.dev,
				      CMD_RAW_DATA_COMPRESS_WRITE,
				      (u8 *)&(abt_comm.recv_packet->report_type),
				      sizeof(abt_comm.recv_packet->report_type));

			abt_compress_flag =
				abt_comm.recv_packet->report_type;
			break;

		case DEBUG_REPORT_POINT:
			abt_report_point =
				abt_comm.recv_packet->report_type;
			TOUCH_I(
				": report_point = %d\n",
				abt_report_point);
			break;

		case DEBUG_REPORT_OCD:
			abt_report_ocd =
				abt_comm.recv_packet->report_type;
			TOUCH_I(": report_ocd = %d\n",
				abt_report_ocd);
			break;

		default:
			TOUCH_I(": unknown command\n");
			break;
		}

		mutex_unlock(&abt_comm_lock);
	}

	return 0;
}

static uint32_t abt_ksocket_rcv_from_abtstudio(uint8_t *buf, uint32_t len)
{
	uint32_t cmd = (uint32_t)*((uint32_t *)buf);
	uint32_t val = (uint32_t)*((uint32_t *)(buf+4));
	TOUCH_I(": CMD=%d VAL=%d\n", cmd, val);
	mutex_lock(&abt_comm_lock);
	switch (cmd) {
	case CMD_RAW_DATA_REPORT_MODE_WRITE:
		TOUCH_I(
			MODULE_NAME": mode setting - %d\n",
			val);
		abt_set_report_mode(abt_comm.dev, val);
		break;

	case CMD_RAW_DATA_COMPRESS_WRITE:
		lg4945_write_value(abt_comm.dev,
			CMD_RAW_DATA_COMPRESS_WRITE, val);
		abt_compress_flag = val;
		break;

	case DEBUG_REPORT_POINT:
		abt_report_point = val;
		break;

	case DEBUG_REPORT_OCD:
		abt_report_ocd = val;
		break;

	default:
		TOUCH_I(": unknown command\n");
		break;
	}
	mutex_unlock(&abt_comm_lock);
	return 0;
}


static void abt_ksocket_exit(void)
{
	int err;

	if (abt_comm.thread == NULL) {
		if (abt_socket_mutex_flag == 1)
			mutex_destroy(&abt_socket_lock);
		abt_socket_mutex_flag = 0;
		abt_socket_report_mode = 0;
		abt_comm.running = 0;

		TOUCH_I(
			MODULE_NAME
			": no kernel thread to kill\n");

		return;
	}

	TOUCH_I(
		MODULE_NAME
		": start killing thread\n");

	abt_comm.running = 2;
	if (abt_ksocket_send_exit() < 0)
		TOUCH_I(": send_exit error\n");

	err = kthread_stop(abt_comm.thread);

	if (err < 0) {
		TOUCH_I(
			MODULE_NAME
			": unknown error %d while trying to terminate kernel thread\n",
			-err);
	} else {
		while (abt_comm.running != 0)
			usleep(10000);
		TOUCH_I(
			MODULE_NAME
			": succesfully killed kernel thread!\n");
	}

	abt_comm.thread = NULL;

	if (abt_comm.sock != NULL) {
		sock_release(abt_comm.sock);
		abt_comm.sock = NULL;
	}

	if (abt_comm.sock_send != NULL) {
		sock_release(abt_comm.sock_send);
		abt_comm.sock_send = NULL;
	}

	if (abt_comm.client_sock != NULL) {
		sock_release(abt_comm.client_sock);
		abt_comm.client_sock = NULL;
	}

	mutex_destroy(&abt_socket_lock);
	abt_socket_mutex_flag = 0;
	abt_socket_report_mode = 0;

	TOUCH_I(": module unloaded\n");
}

int32_t abt_ksocket_raw_data_send(uint8_t *buf, uint32_t len)
{
	int ret = 0;
	static uint32_t connect_error_count;

	if (abt_comm.send_connected == 0)
		abt_ksocket_init_send_socket(&abt_comm);

	if (abt_comm.send_connected == 1) {
		ret = abt_ksocket_send(abt_comm.sock_send,
				&abt_comm.addr_send,
				buf, len);
	} else {
		connect_error_count++;
		if (connect_error_count > 10) {
			TOUCH_I(
					": connection error - socket release\n");
			abt_force_set_report_mode(abt_comm.dev, 0);
			abt_ksocket_exit();
		}
	}

	return ret;
}

static int32_t abt_ksocket_init(struct device *dev,
			char *ip,
			uint32_t (*listener)(uint8_t *buf, uint32_t len))
{
	mutex_init(&abt_socket_lock);
	abt_socket_mutex_flag = 1;
	abt_socket_report_mode = 1;
	memcpy(abt_comm.send_ip, ip, 16);

	if (abt_conn_tool == ABT_STUDIO) {
		abt_comm.thread =
			kthread_run((void *)abt_ksocket_start_for_abtstudio,
						dev, MODULE_NAME);
	} else if (abt_conn_tool == ABT_PCTOOL) {
		abt_comm.thread =
			kthread_run((void *)abt_ksocket_start_for_pctool,
						dev, MODULE_NAME);
	}

	/* abt_comm.thread =
				kthread_run((void *)tcp_init_module,
				dev, MODULE_NAME);*/
	if (IS_ERR(abt_comm.thread)) {
		TOUCH_I(
			MODULE_NAME
			": unable to start kernel thread\n");
		abt_comm.thread = NULL;
		return -ENOMEM;
	}

	abt_comm.sock_listener = listener;
	/* TOUCH_I(
			MODULE_NAME": init OK\n",
			__func__);*/

	return 0;
}

ssize_t show_abtApp(struct device *dev, char *buf)
{
	int i;
	ssize_t retVal = 0;

	if (abt_head_flag) {
		if (abt_show_mode == REPORT_SEG1)
			abt_head[14] = DATA_TYPE_SEG1;
	 else if (abt_show_mode == REPORT_SEG2)
			abt_head[14] = DATA_TYPE_SEG2;
	 else if (abt_show_mode == REPORT_RAW)
			abt_head[14] = DATA_TYPE_RAW;
	 else if (abt_show_mode == REPORT_BASELINE)
			abt_head[14] = DATA_TYPE_BASELINE;
	 else if (abt_show_mode == REPORT_RNORG)
			abt_head[14] = DATA_TYPE_RN_ORG;

		retVal = sizeof(abt_head);
		memcpy(&buf[0], (u8 *)&abt_head[0], retVal);
		abt_head_flag = 0;
		return retVal;
	}

	switch (abt_show_mode) {
	case REPORT_RNORG:
	case REPORT_RAW:
	case REPORT_BASELINE:
	case REPORT_SEG1:
	case REPORT_SEG2:
		i = ACTIVE_SCREEN_CNT_X * ACTIVE_SCREEN_CNT_Y*2;
		memcpy(&buf[0], (u8 *)&abt_ocd[abt_ocd_read^1][0], i);
		memcpy(&buf[i], (u8 *)&abt_reportP, sizeof(abt_reportP));
		i += sizeof(abt_reportP);
		retVal = i;
		break;
	case REPORT_DEBUG_ONLY:
		memcpy(&buf[0], (u8 *)&abt_reportP, sizeof(abt_reportP));
		i = sizeof(abt_reportP);
		retVal = i;
		break;
	default:
		abt_show_mode = 0;
		retVal = 0;
		break;
	}
	return retVal;
}

ssize_t store_abtApp(struct device *dev, const char *buf, size_t count)
{
	u32 mode;

	/* sscanf(buf, "%d", &mode); */
	mode = count;

	if (mode == HEAD_LOAD) {
		abt_head_flag = 1;
		abt_ocd_on = 1;
		TOUCH_I("[ABT] abt_head load\n");
		return abt_head_flag;
	} else {
		abt_show_mode = mode;
		abt_head_flag = 0;
	}

	switch (abt_show_mode) {
	case REPORT_RNORG:
		TOUCH_I("[ABT] show mode : RNORG\n");
		break;
	case REPORT_RAW:
		TOUCH_I("[ABT] show mode : RAW\n");
		break;
	case REPORT_BASELINE:
		TOUCH_I("[ABT] show mode : BASELINE\n");
		break;
	case REPORT_SEG1:
		TOUCH_I("[ABT] show mode : SEG1\n");
		break;
	case REPORT_SEG2:
		TOUCH_I("[ABT] show mode : SEG2\n");
		break;
	case REPORT_DEBUG_ONLY:
		TOUCH_I("[ABT] show mode : DEBUG ONLY\n");
		break;
	case REPORT_OFF:
		TOUCH_I("[ABT] show mode : OFF\n");
		break;
	default:
		TOUCH_I(
			"[ABT] show mode unknown : %d\n", mode);
		break;
	}

	return abt_show_mode;
}

ssize_t show_abtTool(struct device *dev, char *buf)
{
	buf[0] = abt_report_mode_onoff;
	memcpy((u8 *)&buf[1], (u8 *)&(abt_comm.send_ip[0]), 16);
	TOUCH_I(
		MODULE_NAME
		":read raw report mode - mode:%d ip:%s\n",
		buf[0], (char *)&buf[1]);

	return 20;
}

ssize_t store_abtTool(struct device *dev,
				const char *buf,
				size_t count)
{
	int mode = buf[0];
	char *ip = (char *)&buf[1];
	bool setFlag = false;
	TOUCH_I(
		":set raw report mode - mode:%d IP:%s\n", mode, ip);

	if (mode > 47)
		mode -= 48;

	switch (mode) {
	case 1:
		abt_conn_tool = ABT_STUDIO;
		if (abt_comm.thread == NULL) {
			TOUCH_I(":  mode ABT STUDIO Start\n\n");
			abt_ksocket_init(dev,
					 (u8 *)ip,
					 abt_ksocket_rcv_from_abtstudio);
			sic_set_get_data_func(1);
			setFlag = true;
		} else {
			if (memcmp((u8 *)abt_comm.send_ip,
				   (u8 *)ip, 16) != 0) {
				TOUCH_I(
					":IP change->ksocket exit n init\n\n");
				abt_ksocket_exit();
				abt_ksocket_init(dev, (u8 *)ip,
						 abt_ksocket_rcv_from_abtstudio);
				setFlag = true;
			}
		}
		break;
	case 2:
		abt_conn_tool = ABT_PCTOOL;

		if (abt_comm.thread == NULL) {
			TOUCH_I(
				": mode PC TOOL Start\n\n");
			abt_ksocket_init(dev, (u8 *)ip,
					 abt_ksocket_rcv_from_pctool);
			sic_set_get_data_func(1);
			setFlag = true;

			abt_report_point = 1;
			abt_report_ocd = 1;
		} else {
			TOUCH_I(
				": abt_comm.thread Not NULL\n\n");
			if (memcmp((u8 *)abt_comm.send_ip,
				   (u8 *)ip, 16) != 0) {
				abt_ksocket_exit();
				abt_ksocket_init(dev, (u8 *)ip,
						 abt_ksocket_rcv_from_pctool);
				setFlag = true;

			} else {
				TOUCH_I(
					": same IP\n\n");
			}
		}
		break;
	default:
		abt_conn_tool = NOTHING;
		abt_ksocket_exit();
		sic_set_get_data_func(0);

		mutex_lock(&abt_comm_lock);
		abt_set_report_mode(dev, 0);
		mutex_unlock(&abt_comm_lock);
	}

	if (setFlag) {
		mutex_lock(&abt_comm_lock);
		abt_set_report_mode(dev, mode);
		mutex_unlock(&abt_comm_lock);
	}

	return mode;
}

int abt_force_set_report_mode(struct device *dev, u32 mode)
{
	int ret = 0;
	u32 rdata = 0;
	u32 wdata = mode;

	/* send debug mode*/
	ret = lg4945_write_value(dev,
				 CMD_RAW_DATA_REPORT_MODE_WRITE,
				 wdata);
	abt_report_mode = mode;

	/* receive debug report buffer*/
	if (mode >= 0) {
		ret = lg4945_read_value(dev,
					CMD_RAW_DATA_REPORT_MODE_READ,
					&rdata);
		TOUCH_I("(%d)rdata\n", rdata);
		if (ret < 0 || rdata <= 0) {
			TOUCH_I("(%s)debug report buffer pointer error\n",
				__func__);
			goto error;
		}

		TOUCH_I("(%s)debug report buffer pointer : 0x%x\n",
			__func__, rdata);
		CMD_GET_ABT_DEBUG_REPORT = (rdata - 0x20000000)/4;
		CMD_GET_ABT_DEBUG_REPORT += 0x8000;
	}

	return 0;

error:
	wdata = 0;
	lg4945_write_value(dev,
			   CMD_RAW_DATA_REPORT_MODE_WRITE,
			   wdata);
	abt_report_mode = 0;
	abt_report_mode_onoff = 0;
	return ret;
}

/*****************************************************************************/
void lg4945_sic_abt_probe(void)
{
	mutex_init(&abt_comm_lock);
}

void lg4945_sic_abt_remove(void)
{
	mutex_destroy(&abt_comm_lock);
	if (abt_socket_mutex_flag == 1)
		mutex_destroy(&abt_socket_lock);
}

void lg4945_sic_abt_init(struct device *dev)
{
	static int is_init = 0;
	u32 head_loc[4];
	struct T_ABTLog_FileHead tHeadBuffer;

	if (is_init)
		return;

	is_init = 1;

	if (abt_report_mode != 0)
		abt_force_set_report_mode(dev, abt_report_mode);

	lg4945_read_value(dev, CMD_ABT_LOC_X_START_READ, &head_loc[0]);
	lg4945_read_value(dev, CMD_ABT_LOC_X_END_READ, &head_loc[1]);
	lg4945_read_value(dev, CMD_ABT_LOC_Y_START_READ, &head_loc[2]);
	lg4945_read_value(dev, CMD_ABT_LOC_Y_END_READ, &head_loc[3]);

	tHeadBuffer.resolution_x = 1440;
	tHeadBuffer.resolution_y = 2720;
	tHeadBuffer.node_cnt_x = ACTIVE_SCREEN_CNT_X;
	tHeadBuffer.node_cnt_y = ACTIVE_SCREEN_CNT_Y;
	tHeadBuffer.additional_node_cnt = 0;
	tHeadBuffer.rn_min = 1000;
	tHeadBuffer.rn_max = 1300;
	tHeadBuffer.raw_data_size = sizeof(u16);
	tHeadBuffer.rn_data_size = sizeof(u16);
	tHeadBuffer.frame_data_type = DATA_TYPE_RN_ORG;
	tHeadBuffer.frame_data_size = sizeof(u16);
	tHeadBuffer.loc_x[0] = (u16) head_loc[0];
	tHeadBuffer.loc_x[1] = (u16) head_loc[1];
	tHeadBuffer.loc_y[0] = (u16) head_loc[2];
	tHeadBuffer.loc_y[1] = (u16) head_loc[3];

	memcpy((u8 *)&abt_head[0], (u8 *)&tHeadBuffer,
	       sizeof(struct T_ABTLog_FileHead));
}

int lg4945_sic_abt_is_debug_mode(void)
{
	if (abt_show_mode >= REPORT_RNORG && abt_show_mode <= REPORT_DEBUG_ONLY)
		return 1;

	return 0;
}

void lg4945_sic_abt_ocd_off(struct device *dev)
{
	u32 wdata = 0;

	if (abt_ocd_off) {
		lg4945_write_value(dev, CMD_ABT_OCD_ON_WRITE, wdata);
		lg4945_read_value(dev, CMD_ABT_OCD_ON_READ, &wdata);
		TOUCH_I("[ABT] onchipdebug off: wdata=%d\n", wdata);
		abt_ocd_off = 0;
	}
}

void lg4945_sic_abt_onchip_debug(struct device *dev, u8 *all_data)
{
	u32 i;
	u32 wdata;
	u16 addr;
	int node;
	int ret;

	struct T_ReportP local_reportP;
	struct T_OnChipDebug *ocd = &local_reportP.ocd;

	abt_ocd_off = 1;

	memcpy(&local_reportP, all_data, sizeof(struct T_ReportP));

	/* Write onchipdebug on */
	if (abt_ocd_on) {
		wdata = abt_show_mode;
		TOUCH_I("[ABT] onchipdebug on(before write): wdata=%d\n",
			wdata);
		lg4945_write_value(dev, CMD_ABT_OCD_ON_WRITE, wdata);
		lg4945_read_value(dev, CMD_ABT_OCD_ON_READ, &wdata);
		TOUCH_I("[ABT] onchipdebug on(after write): wdata=%d\n",
			wdata);

		abt_ocd_on = 0;
	} else if (abt_show_mode < REPORT_DEBUG_ONLY) {
		if (ocd->rnd_piece_no == 0 &&
		    ocd->rnd_piece_no != prev_rnd_piece_no)
			abt_ocd_read ^= 1;

		ocd_piece_size = ACTIVE_SCREEN_CNT_X *
			ACTIVE_SCREEN_CNT_Y / DEF_RNDCPY_EVERY_Nth_FRAME;

		if (ocd_piece_size % 2)
			ocd_piece_size -= 1;
		addr = ((ocd->rnd_addr - 0x20000000) / 4 +
			(ocd->rnd_piece_no * ocd_piece_size) / 2);
		addr += 0x8000;
		node = ocd->rnd_piece_no * ocd_piece_size;

		if (ocd->rnd_piece_no != prev_rnd_piece_no) {
			if (ocd->rnd_piece_no
			    != DEF_RNDCPY_EVERY_Nth_FRAME - 1) {
				ret = lg4945_reg_read(dev, addr,
						   &abt_ocd[abt_ocd_read][node],
						   ocd_piece_size * 2);
				if (ret < 0)
					TOUCH_E(
						"RNdata reg addr write fail [%d]\n",
						ocd->rnd_piece_no);
			} else {
				i = ACTIVE_SCREEN_CNT_X * ACTIVE_SCREEN_CNT_Y;
				i -= ocd_piece_size
					* (DEF_RNDCPY_EVERY_Nth_FRAME - 1);
				ret = lg4945_reg_read(dev, addr,
						   &abt_ocd[abt_ocd_read][node],
						   i * 2);
				if (ret < 0)
					TOUCH_I(
						"RNdata reg addr write fail [%d]\n",
						ocd->rnd_piece_no);
			}
		}
		prev_rnd_piece_no = ocd->rnd_piece_no;
	}
}

void lg4945_sic_abt_report_mode(struct device *dev, u8 *all_data)
{
	struct send_data_t *packet_ptr = &abt_comm.data_send;
	struct debug_report_header *d_header =
		(struct debug_report_header *)(&abt_comm.data_send.data[0]);
	int d_header_size = sizeof(struct debug_report_header);
	u32 TC_debug_data_ptr = CMD_GET_ABT_DEBUG_REPORT;
	u32 i2c_pack_count = 0;
	struct timeval t_stamp;
	int i;
	struct lg4945_touch_info *t_info =
		(struct lg4945_touch_info *) all_data;

	u8 *d_data_ptr = (u8 *)d_header + d_header_size;

	if (abt_report_mode) {
		lg4945_reg_read(dev,
				TC_debug_data_ptr, d_header, d_header_size);
		if (d_header->type == abt_report_mode) {
			i2c_pack_count = d_header->data_size / PACKET_SIZE;
			TC_debug_data_ptr += d_header_size / 4;
			for (i = 0; i < i2c_pack_count; i++) {
				lg4945_reg_read(dev, TC_debug_data_ptr,
						(u8 *)d_data_ptr,
						PACKET_SIZE);
				d_data_ptr += PACKET_SIZE;
				TC_debug_data_ptr += PACKET_SIZE / 4;
			}

			if (d_header->data_size % PACKET_SIZE != 0) {
				lg4945_reg_read(dev, TC_debug_data_ptr,
						d_data_ptr,
						d_header->data_size % PACKET_SIZE);
				d_data_ptr += d_header->data_size % PACKET_SIZE;
			}
		} else {
			TOUCH_I("debug data load error !!\n");
			TOUCH_I("type : %d\n", d_header->type);
			TOUCH_I("size : %d\n", d_header->data_size);
		}
	} else {
		packet_ptr->touchCnt = 0;
	}

	/* ABS0 */
	if (t_info->wakeup_type == 0) {
		if (abt_report_ocd) {
			memcpy(d_data_ptr, &all_data[34 * 4], sizeof(u8) * 60);
			d_data_ptr += 60;
		}

		/* check palm */
		if (t_info->data[0].track_id != 15) {
			if (abt_report_point) {
				packet_ptr->touchCnt =
					t_info->touch_cnt;
				memcpy(d_data_ptr, &t_info->data[0],
				       sizeof(struct lg4945_touch_data)
				       * t_info->touch_cnt);
				d_data_ptr +=
				       sizeof(struct lg4945_touch_data)
				       * t_info->touch_cnt;
			} else {
				packet_ptr->touchCnt = 0;
			}
		}
	}


	if ((u8 *)d_data_ptr - (u8 *)packet_ptr > 0) {
		do_gettimeofday(&t_stamp);
		frame_num++;
		packet_ptr->type = DEBUG_DATA;
		packet_ptr->mode = abt_report_mode;
		packet_ptr->frame_num = frame_num;
		packet_ptr->timestamp =
			t_stamp.tv_sec * 1000000 + t_stamp.tv_usec;

		packet_ptr->flag = 0;
		if (abt_report_point)
			packet_ptr->flag |= 0x1;
		if (abt_report_ocd)
			packet_ptr->flag |= (0x1)<<1;

		abt_ksocket_raw_data_send((u8 *)packet_ptr,
					  (u8 *)d_data_ptr - (u8 *)packet_ptr);
	}
}

static TOUCH_ABT_ATTR(abt_monitor, show_abtApp, store_abtApp);
static TOUCH_ABT_ATTR(raw_report, show_abtTool, store_abtTool);

static struct attribute *lg4945_abt_attribute_list[] = {
	&touch_attr_abt_monitor.attr,
	&touch_attr_raw_report.attr,
	NULL,
};

static const struct attribute_group lg4945_abt_attribute_group = {
	.attrs = lg4945_abt_attribute_list,
};

void lg4945_sic_abt_register_sysfs(struct kobject *kobj)
{
	int ret = sysfs_create_group(kobj, &lg4945_abt_attribute_group);

	if (ret < 0)
		TOUCH_E("failed to create sysfs for abt\n");
}

#else
void lg4945_sic_abt_probe(void) {}
void lg4945_sic_abt_remove(void) {}
void lg4945_sic_abt_init(struct device *dev) {}
int lg4945_sic_abt_is_debug_mode(void) { return 0; }
void lg4945_sic_abt_ocd_off(struct device *dev) {}
void lg4945_sic_abt_onchip_debug(struct device *dev, u8 *all_data) {}
void lg4945_sic_abt_report_mode(struct device *dev, u8 *all_data) {}
void lg4945_sic_abt_register_sysfs(struct kobject *kobj) {}
#endif

