/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/kthread.h>
#include <linux/export.h>

#include "mausb_common.h"
#include "mausb_util.h"

int event_handler(struct mausb_device *ud)
{
	mausb_dbg_eh("---> event_handler\n");

	/*
	 * Events are handled by only this thread.
	 */
	while (mausb_event_happened(ud)) {
		mausb_dbg_eh("pending event %lu\n", ud->event);

		/*
		 * NOTE: shutdown must come first.
		 * Shutdown the device.
		 */
		if (ud->event & MAUSB_EH_SHUTDOWN) {
			ud->mausb_eh_ops.shutdown(ud);
			ud->event &= ~MAUSB_EH_SHUTDOWN;
		}

		/* Reset the device. */
		if (ud->event & MAUSB_EH_RESET) {
			ud->mausb_eh_ops.reset(ud);
			ud->event &= ~MAUSB_EH_RESET;
		}

		/* Mark the device as unusable. */
		if (ud->event & MAUSB_EH_UNUSABLE) {
			ud->mausb_eh_ops.unusable(ud);
			ud->event &= ~MAUSB_EH_UNUSABLE;
		}

		/* Stop the error handler. */
		if (ud->event & MAUSB_EH_BYE)
		{
			mausb_dbg_eh("<--- event_handler\n ret -1");
			return -1;
		}
	}
	mausb_dbg_eh("<--- event_handler\n ret 0");
	return 0;
}
EXPORT_SYMBOL_GPL(event_handler);

static int event_handler_loop(void *data)
{
	struct mausb_device *ud = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(ud->eh_waitq, mausb_event_happened(ud) || kthread_should_stop());

		if (event_handler(ud) < 0)
			break;
	}

	return 0;
}

int mausb_start_eh(struct mausb_device *ud)
{
	init_waitqueue_head(&ud->eh_waitq);
	ud->event = 0;

	ud->eh = kthread_run(event_handler_loop, ud, "mausb_eh");
	mausb_dbg_eh("---> mausb_start_eh  %p\n", ud->eh);
	if (IS_ERR(ud->eh)) {
		return PTR_ERR(ud->eh);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mausb_start_eh);

void mausb_stop_eh(struct mausb_device *ud)
{
	mausb_dbg_eh("---> mausb_stop_eh  %p\n", ud->eh);
	if (ud->eh)
		if (ud->eh == current)
			return; /* do not wait for myself */
	if (ud->eh)
	{
		kthread_stop(ud->eh);
		ud->eh = NULL;
	}
	mausb_dbg_eh("mausb_eh has finished\n");
}
EXPORT_SYMBOL_GPL(mausb_stop_eh);

void mausb_event_add(struct mausb_device *ud, unsigned long event)
{
	unsigned long flags;
	mausb_dbg_eh(" ---> mausb_event_add %ld \n",event);
	spin_lock_irqsave(&ud->lock, flags);
	ud->event |= event;
	spin_unlock_irqrestore(&ud->lock, flags);
	//wake_up(&ud->eh_waitq);
	mausb_dbg_eh(" <-- mausb_event_add  ud->event: %lu \n",ud->event);
}
EXPORT_SYMBOL_GPL(mausb_event_add);

int mausb_event_happened(struct mausb_device *ud)
{
	int happened = 0;
//	unsigned long flags;
	mausb_dbg_eh(" ---> mausb_event_happend %lu \n",ud->event);
//	spin_lock_irqsave(&ud->lock, flags);
	if (ud->event != 0) {
		happened = 1;
	}
//	spin_unlock_irqrestore(&ud->lock, flags);
	mausb_dbg_eh(" <-- mausb_event_happened \n");
	return happened;
}
EXPORT_SYMBOL_GPL(mausb_event_happened);
