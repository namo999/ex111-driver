
/******************************************************************************
 * usbtouchscreen.c
 * Driver for 3M EX111 USB touchscreen
 *******************************************************************************
 * © Copyright 2014 , Rocket Gaming Systems. All Rights Reserved.
 * TRADE SECRETS: CONFIDENTIAL AND PROPRIETARY
 * This software, in all of its forms, and all of the code, algorithms,
 * formulas, techniques and processes embedded therein, and their structure,
 * sequence, selection and arrangement (the "Software") is owned by and
 * constitutes the valuable work, property and trade secrets of  Rocket Gaming
 * Systems or its licensors ("Rocket Gaming Systems"), is protected by
 * copyright law, trade secret law, and other intellectual property laws,
 * treaties, conventions and agreements, including the applicable laws of
 * the country in which it is being used, and any use, copying, revision,
 * adaptation, distribution or disclosure in whole or in part is strictly
 * prohibited except under the terms of an express written consent and license
 * from Rocket Gaming Systems. The Software is available under a license only,
 * and all ownership and other rights are retained by Rocket Gaming Systems.

 *****************************************************************************/
#include <linux/rgs_usbtouchscreen.h>

static int ex111_init(struct ex111_usb *ex111)
{
	int ret;
	char *buf;
	struct usb_device *udev = interface_to_usbdev(ex111->interface);

	buf = kzalloc(32, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/*  7730 USB controller part */
	/* soft reset command */

	memset(buf, 0, 32);
	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x07,
			      USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
			      1, 0, (void *)buf, 0x0, USB_CTRL_SET_TIMEOUT);
	dev_info(&ex111->interface->dev,
            "%s - RESET COMMAND REQUEST - bytes|ret: %d\n", __func__, ret);

	if (ret < 0)
		goto init_free;

	/* status report command */
	do {
		msleep(100);
		memset(buf, 0, 32);
		ret =
		    usb_control_msg(udev, usb_rcvctrlpipe(udev, 0), 0x06, 0xc0,
				    0x0, 0, (void *)buf, 0x14,
				    USB_CTRL_SET_TIMEOUT);
		dev_info(&ex111->interface->dev,
                "%s CONTROLLER_STATUS - bytes|ret: %d\n", __func__, ret);
	} while (buf[3] == 0x01);

	dev_info(&ex111->interface->dev,
            "%s - CONTROLLER_STATUS COMMAND STATUS buf[3]: %x\n", __func__,
	    buf[3]);

	if (ret < 0)
		goto init_free;

init_free:kfree(buf);

	if (ret < 0)
		return ret;

	return 0;
}

static void reset_halted_pipe(struct work_struct *ws)
{
	struct ex111_usb *ex111 =
	    container_of(ws, struct ex111_usb, reset_pipe_work);
	struct usb_device *udev = interface_to_usbdev(ex111->interface);

	int rv = usb_clear_halt(udev, pipe);
	dev_info(&ex111->interface->dev, "%s - usb_clear_halt SR", __func__);

	if (rv != 0)
		dev_err(&ex111->interface->dev,
                "%s - usb_clear_halt failed with %d", __func__, rv);

	rv = ex111_init(ex111);
	if (rv != 0)
		dev_err(&ex111->interface->dev,
                "%s - ex111_init failed with %d", __func__, rv);

	rv = usb_submit_urb(ex111->irq, GFP_ATOMIC);
	if (rv)
		dev_err(&ex111->interface->dev,
                "%s - usb_submit_urb failed with result: %d", __func__, rv);
}

static void ex111_process_paket(struct ex111_usb *ex111, unsigned char *pkt)
{
	ex111->x = (pkt[4] << 8) | pkt[3];
	ex111->y = (pkt[6] << 8) | pkt[5];
	ex111->touch = ((pkt[2] & 0xc0) == 0xc0) ? 1 : 0;

	/* dev_info(&ex111->interface->dev,
            "%s - touch %02x cal_x %d, cal_y %d\n",
	    __func__, ex111->touch, ex111->x, ex111->y); */

	input_report_key(ex111->input, BTN_TOUCH, ex111->touch);

	if (swap_xy) {
		input_report_abs(ex111->input, ABS_X, ex111->y);
		input_report_abs(ex111->input, ABS_Y, ex111->x);
	} else {
		input_report_abs(ex111->input, ABS_X, ex111->x);
		input_report_abs(ex111->input, ABS_Y, ex111->y);
	}
	input_sync(ex111->input);
}

static void ex111_irq(struct urb *urb)
{
	struct ex111_usb *ex111 = urb->context;
	int retval;

	switch (urb->status) {
	case 0:
		/* success */
		/* dev_info(&ex111->interface->dev,
                "%s - usb_submit_urb with result: %d", __func__,
		    urb->status); */
		break;
	case -ETIME:
		/* this urb is timing out */
		dev_err(&ex111->interface->dev,
                "%s - urb timed out - was the device unplugged?", __func__);
		return;
	case -EPIPE:
		dev_err(&ex111->interface->dev,
                "%s - urb stalled with status: %d, trying to resurrect",
		    __func__, urb->status);
		schedule_work(&ex111->reset_pipe_work);
		return;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		return;
	default:
		dev_err(&ex111->interface->dev, "%s - nonzero urb status received: %d",
		    __func__, urb->status);
		goto exit;
	}

	ex111_process_paket(ex111, ex111->data);

exit:
	usb_mark_last_busy(interface_to_usbdev(ex111->interface));
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		dev_err(&ex111->interface->dev,
                "%s - usb_submit_urb failed with result: %d", __func__,
		    retval);
}

static int ex111_open(struct input_dev *input)
{
	struct ex111_usb *ex111 = input_get_drvdata(input);
	int r, i;

	i = usb_submit_urb(ex111->irq, GFP_KERNEL);
	if (i < 0) {
		dev_err(&ex111->interface->dev,
                "%s - usb_submit_urb borked: %d", __func__, i);
		r = -EIO;
	}
	return 0;
}

static void ex111_close(struct input_dev *input)
{
	struct ex111_usb *ex111 = input_get_drvdata(input);
	int r;
	usb_kill_urb(ex111->irq);
	r = usb_autopm_get_interface(ex111->interface);
	ex111->interface->needs_remote_wakeup = 0;
	if (!r)
		usb_autopm_put_interface(ex111->interface);
}

static const struct usb_device_id ex111_devices[] = {
	{USB_DEVICE(USB_PRODUCT_ID_3M, USB_VENDOR_ID_7730UC),},
	{},
};

static int ex111_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct ex111_usb *ex111 = usb_get_intfdata(intf);

	usb_kill_urb(ex111->irq);

	return 0;
}

static int ex111_resume(struct usb_interface *intf)
{
	struct ex111_usb *ex111 = usb_get_intfdata(intf);
	struct input_dev *input = ex111->input;
	int res = 0;

	mutex_lock(&input->mutex);
	if (input->users)
		res = usb_submit_urb(ex111->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);

	return res;
}

static int ex111_reset_resume(struct usb_interface *intf)
{
	struct ex111_usb *ex111 = usb_get_intfdata(intf);
	struct input_dev *input = ex111->input;
	int err = 0;

	/* reinit the device */
	err = ex111_init(ex111);
	if (err) {
		dev_err(&ex111->interface->dev,
                "%s - ex111_init() failed, err: %d", __func__, err);
		return err;
	}

	/* restart IO if needed */
	mutex_lock(&input->mutex);
	if (input->users)
		err = usb_submit_urb(ex111->irq, GFP_NOIO);
	mutex_unlock(&input->mutex);

	return err;
}

static struct usb_endpoint_descriptor *ex111_get_input_endpoint(struct
usb_host_interface *interface)
{
	int i;
	for (i = 0; i < interface->desc.bNumEndpoints; i++)
		if (usb_endpoint_dir_in(&interface->endpoint[i].desc))
			return &interface->endpoint[i].desc;

	return NULL;
}

static int ex111_ioctl(struct usb_interface *intf, unsigned int cmd, void *arg)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct usb_packet *app_packet = arg;
	int ret, i;
	char *buf;

/* dev_info(&intf->dev, "request type : %x \t request : %d \t value : %x\n",
			app_packet->bmRequestType, app_packet->bRequest,
			app_packet->wValue);

	dev_info(&intf->dev, "index : %x \t length : %d\n", app_packet->wIndex,
			app_packet->wLength); */

	buf = kzalloc(32, GFP_KERNEL);
	if (!buf) {
		dev_err(&intf->dev, "insufficient memory in kernel\n");
		return -ENOMEM;
	}

	ret = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
            app_packet->bRequest, app_packet->bmRequestType,
            app_packet->wValue, app_packet->wIndex,
            (void *) buf, app_packet->wLength, USB_CTRL_SET_TIMEOUT);

	dev_info(&intf->dev, "%s - EX111_COMMAND_RESPONSE - bytes: %d\n",
			__func__, ret);

/*	for (i = 0; i < app_packet->wLength; i++)
		dev_info(&intf->dev, " %02x\t", buf[i]); */

	if (copy_to_user(app_packet->response, buf, app_packet->wLength)) {
		dev_err(&intf->dev, "data copy from kernel to user space error\n");
		return -EFAULT;
	}

    if (ret < 0)
        goto ioctl_out;

ioctl_out: kfree(buf);

    if (ret < 0)
        return ret;

    return 0;
}

static int ex111_probe(struct usb_interface *intf,
		       const struct usb_device_id *id)
{
	struct ex111_usb *ex111;
	struct input_dev *input_dev;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_device *udev = interface_to_usbdev(intf);
	int err = -ENOMEM;

	endpoint = ex111_get_input_endpoint(intf->cur_altsetting);
	if (!endpoint)
		return -ENXIO;
	pipe = usb_rcvintpipe(udev, endpoint->bEndpointAddress);

	ex111 = kzalloc(sizeof(struct ex111_usb), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ex111 || !input_dev)
		goto input_device_free;

	ex111->data =
	    usb_alloc_coherent(udev, CO_ORDINATE_REPORT_SIZE, GFP_ATOMIC,
			       &ex111->data_dma);

	if (!ex111->data)
		goto input_device_free;

	ex111->irq = usb_alloc_urb(0, GFP_KERNEL);
	if (!ex111->irq) {
		dev_err(&ex111->interface->dev,
                "%s - usb_alloc_urb failed: ex111->irq", __func__);
		goto prob_free_buffers;
	}

	ex111->interface = intf;
	ex111->input = input_dev;

	if (udev->manufacturer)
		strlcpy(ex111->name, udev->manufacturer, sizeof(ex111->name));

	if (udev->product) {
		if (udev->manufacturer)
			strlcat(ex111->name, " ", sizeof(ex111->name));
		strlcat(ex111->name, udev->product, sizeof(ex111->name));
	}

	if (!strlen(ex111->name))
		snprintf(ex111->name, sizeof(ex111->name),
			 "3M EX111 HID touchscreen %04x:%04x",
			 le16_to_cpu(udev->descriptor.idVendor),
			 le16_to_cpu(udev->descriptor.idProduct));

	usb_make_path(udev, ex111->phys, sizeof(ex111->phys));

	strlcat(ex111->phys, "/input0", sizeof(ex111->phys));

	input_dev->name = ex111->name;
	input_dev->phys = ex111->phys;
	usb_to_input_id(udev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	input_set_drvdata(input_dev, ex111);

	input_dev->open = ex111_open;
	input_dev->close = ex111_close;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X, MIN_XC, MAX_XC, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, MIN_YC, MAX_YC, 0, 0);

	usb_fill_int_urb(ex111->irq, udev,
			 pipe,
			 ex111->data, CO_ORDINATE_REPORT_SIZE,
			 ex111_irq, ex111, endpoint->bInterval);

	ex111->irq->dev = udev;
	ex111->irq->transfer_dma = ex111->data_dma;
	ex111->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	err = ex111_init(ex111);
	if (err) {
		dev_err(&ex111->interface->dev,
                "%s - ex111_init() failed, err: %d", __func__, err);
		goto prob_do_exit;
	}

	err = input_register_device(ex111->input);
	if (err) {
		dev_err(&ex111->interface->dev,
                "%s - input_register_device() failed, err: %d", __func__,
		    err);
		goto prob_do_exit;
	}

	usb_set_intfdata(intf, ex111);
	INIT_WORK(&ex111->reset_pipe_work, reset_halted_pipe);

	return 0;

prob_do_exit:
	usb_free_urb(ex111->irq);
prob_free_buffers:

	usb_free_coherent(interface_to_usbdev(intf), CO_ORDINATE_REPORT_SIZE,
			  ex111->data, ex111->data_dma);

	kfree(ex111->buffer);
input_device_free:
	input_free_device(input_dev);
	kfree(ex111);
	return err;
}

static void ex111_disconnect(struct usb_interface *intf)
{
	struct ex111_usb *ex111 = usb_get_intfdata(intf);

	if (!ex111)
		return;

	dev_info(&ex111->interface->dev,
            "%s - ex111 is initialized, cleaning up", __func__);
	usb_set_intfdata(intf, NULL);
	/* this will stop IO via close */
	input_unregister_device(ex111->input);
	usb_free_urb(ex111->irq);
	usb_free_coherent(interface_to_usbdev(intf), CO_ORDINATE_REPORT_SIZE,
			  ex111->data, ex111->data_dma);

	kfree(ex111->buffer);
	kfree(ex111);
}

static struct usb_driver ex111_driver = {
	.name = "ex111touchscreen",
	.probe = ex111_probe,
	.disconnect = ex111_disconnect,
	.id_table = ex111_devices,
	.suspend = ex111_suspend,
	.resume = ex111_resume,
	.reset_resume = ex111_reset_resume,
	.unlocked_ioctl = ex111_ioctl,
	.supports_autosuspend = 1,
};

static int __init ex111_modinit(void)
{
	int retval = usb_register(&ex111_driver);
	if (retval == 0)
		err(KBUILD_MODNAME ": " DRIVER_VERSION ":" DRIVER_DESC "\n");
	return retval;
}

static void __exit ex111_exit(void)
{
	usb_deregister(&ex111_driver);
}

module_init(ex111_modinit);
module_exit(ex111_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_ALIAS("usb:v0596p0001d*dc*dsc*dp*ic*isc*ip*");
