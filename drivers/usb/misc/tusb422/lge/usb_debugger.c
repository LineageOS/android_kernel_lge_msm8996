static void usb_debugger_work(struct work_struct *w)
{
	struct hw_pd_dev *dev = container_of(w, struct hw_pd_dev,
					     usb_debugger_work);
	union lge_power_propval lge_val;
	int rc;

	if (!dev->lge_power_cd) {
		PRINT("%s: lge_power_cd is NULL\n", __func__);
		return;
	}

	if (dev->is_debug_accessory) {
		rc = dev->lge_power_cd->get_property(dev->lge_power_cd,
						     LGE_POWER_PROP_CHECK_ONLY_USB_ID,
						     &lge_val);
		if (rc != 0) {
			PRINT("%s: get_property CHECK_ONLY_USB_ID error %d\n",
			      __func__, rc);
			return;
		} else if (lge_val.intval == FACTORY_CABLE) {
			DEBUG("%s: factory cable connected\n", __func__);
			return;
		}

		if(dev->sbu_en_gpio)
			gpiod_direction_output(dev->sbu_en_gpio,0);
		gpiod_direction_output(dev->sbu_sel_gpio, 0);
		lge_uart_console_on_earjack_debugger_in();
		DEBUG("%s: uart debugger in\n", __func__);
	} else {
		if(dev->sbu_en_gpio)
			gpiod_direction_output(dev->sbu_en_gpio, 1);
		lge_uart_console_on_earjack_debugger_out();
		DEBUG("%s: uart debugger out\n", __func__);
	}

	return;
}

static int usb_debugger_init(struct hw_pd_dev *dev)
{
	lge_uart_console_set_config(UART_CONSOLE_ENABLE_ON_EARJACK_DEBUGGER);

	INIT_WORK(&dev->usb_debugger_work, usb_debugger_work);
	dev->lge_power_cd = lge_power_get_by_name("lge_cable_detect");

	dev->sbu_sel_gpio = devm_gpiod_get(dev->dev, "ti,sbu-sel",
					   GPIOD_OUT_LOW);
	if (IS_ERR(dev->sbu_sel_gpio)) {
		PRINT("failed to allocate sbu_sel gpio\n");
		dev->sbu_sel_gpio = NULL;
	}

	dev->sbu_en_gpio = devm_gpiod_get(dev->dev, "ti,sbu-en",
					  GPIOD_OUT_HIGH);
	if (IS_ERR(dev->sbu_en_gpio)) {
		PRINT("failed to allocate sbu_en gpio\n");
		dev->sbu_en_gpio = NULL;
	}

	return 0;
}
