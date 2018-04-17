static irqreturn_t cc_protect_irq_thread(int irq, void *data)
{
	struct hw_pd_dev *dev = data;

	if (dev->is_sbu_ov)
		return IRQ_HANDLED;

	if (dev->sbu_ov_cnt++ < 10) {
		dev_dbg(dev->dev, "%s: sbu_ov_cnt(%d)\n", __func__, dev->sbu_ov_cnt);
		return IRQ_HANDLED;
	}

	PRINT("SBU_OV: Waiting until VBUS is removed.\n");

	tcpm_cc_fault_set(0, TCPC_STATE_CC_FAULT_SBU_OV);

	dev->is_sbu_ov = true;
	disable_irq_nosync(dev->cc_protect_irq);

	return IRQ_HANDLED;
}

static int cc_protect_init(struct hw_pd_dev *dev)
{
	int ret;

	if (!(dev->moisture_detect_use_sbu && IS_CHARGERLOGO))
		return 0;

	dev->cc_protect_gpio = devm_gpiod_get(dev->dev, "ti,cc-protect",
					      GPIOD_IN);
	if (IS_ERR(dev->cc_protect_gpio)) {
		dev_err(dev->dev, "failed to allocate cc_protect gpio\n");
		return PTR_ERR(dev->cc_protect_gpio);
	}

	dev->cc_protect_irq = gpiod_to_irq(dev->cc_protect_gpio);
	if (dev->cc_protect_irq < 0) {
		dev_err(dev->dev, "failed to get cc_protect irq\n");
		return dev->cc_protect_irq;
	}

	irq_set_status_flags(dev->cc_protect_irq, IRQ_NOAUTOEN);
	ret = devm_request_threaded_irq(dev->dev,
			       dev->cc_protect_irq,
			       NULL,
			       cc_protect_irq_thread,
			       IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			       "cc_protect",
			       dev);
	if (ret) {
		dev_err(dev->dev, "unable to request cc_protect irq\n");
		return ret;
	}

	enable_irq_wake(dev->cc_protect_irq);

	return 0;
}
