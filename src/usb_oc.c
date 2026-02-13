#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("p0358");
MODULE_DESCRIPTION("Filter kernel module to set the polling rate of select USB devices to a custom value.");
MODULE_VERSION("1.0");

/* A struct associated with the interrupt_interval_override module parameter, representing
   an user's choice to force a specific interrupt interval upon all interrupt endpoints of
   a certain device. */
struct interrupt_interval_override {
	/* The vendor ID of the device of which the interrupt interval shall be overridden */
	u16 vendor;
	/* The product ID of the device of which the interrupt interval shall be overridden */
	u16 product;
	/* The new interval measured in milliseconds that shall be given to all endpoints of type interrupt on said device */
	unsigned short interval;
};

static char configured = 0;
static DEFINE_MUTEX(interrupt_interval_override_mutex);
static char interrupt_interval_override_param[128];
static struct interrupt_interval_override *interrupt_interval_override_list = NULL;
static size_t interrupt_interval_override_count = 0;

static int usb_device_cb(struct usb_device* udev, void* data);

/* Note that if this parameter is set at the time of module loading,
   this function will be called before on_module_init() */
static int interrupt_interval_override_param_set(const char *value, const struct kernel_param *kp)
{
	const char *p;
	unsigned short vendor, product;
	unsigned short interval;
	struct interrupt_interval_override* list;
	struct interrupt_interval_override param;
	size_t count, max_count, i, len;
	int err, res;

	mutex_lock(&interrupt_interval_override_mutex);

	if (!value || !value[0])
	{
		/* Unset the current variable. */
		kfree(interrupt_interval_override_list);
		interrupt_interval_override_list = NULL;
		interrupt_interval_override_count = 0;
		param_set_copystring(value, kp);  /* Does not fail: the empty string is short enough to fit. */
		mutex_unlock(&interrupt_interval_override_mutex);
		return 0;
	}

	/* Compute an upper bound on the amount of entries we need. */
	for (max_count = 1, i = 0; value[i]; i++)
	{
		if (value[i] == ',')
			max_count++;
	}

	/* Ensure we can allocate enough memory before overwriting the global variables. */
	list = kcalloc(max_count,
		sizeof(struct interrupt_interval_override),
		GFP_KERNEL);

	if (!list)
	{
		mutex_unlock(&interrupt_interval_override_mutex);
		return -ENOMEM;
	}

	err = param_set_copystring(value, kp);
	if (err)
	{
		kfree(list);
		mutex_unlock(&interrupt_interval_override_mutex);
		return err;
	}

	/* Parse the parameter. Example of a valid parameter: 045e:00db:16,1bcf:0005:2 */
	for (count = 0, p = (const char*)value; p && *p;)
	{
		res = sscanf(p, "%hx:%hx:%hu%zn", &vendor, &product, &interval, &len);

		/* Check whether all variables (vendor, product, interval, len) were assigned.
		   %zn does not increase the assignment count, so we need to check for value 3 instead of 4.
		   %zn does not consume input either, so setting len shouldn't fail if interval has been properly set. */
		if (res != 3)
		{
			pr_warn("Error while parsing USB interrupt interval override parameter %s.\n", value);
			break;
		}

		param.vendor = (u16)vendor;
		param.product = (u16)product;
		param.interval = interval;
		list[count++] = param;

		p += len;
		if (*p == ',' && *(p+1) != '\0')
		{
			p++;
			continue;
		} else if (*p == '\0' || (*p == '\n' && *(p+1) == '\0'))
		{
			break;
		} else
		{
			pr_warn("Error while parsing USB interrupt interval override parameter %s.\n", value);
			break;
		}
	}

	/* Overwrite the global variables with the local ones. */
	kfree(interrupt_interval_override_list);
	interrupt_interval_override_list = list;
	interrupt_interval_override_count = count;
	mutex_unlock(&interrupt_interval_override_mutex);

	/* Apply the new value (if the module is being reconfigured on runtime) */
	if (configured)
	{
		printk(KERN_INFO "usb_oc: Configuration changed, applying changes...\n");
		usb_for_each_dev(NULL, &usb_device_cb);
	}

	return 0;
}

/* Given an USB device, this checks whether the user has specified they want to override the interrupt
   polling interval on all interrupt-type endpoints of said device.

   This function returns the user-desired amount of milliseconds between interrupts on said endpoint.
   If this function returns zero, the device-requested interrupt interval should be used. */
static unsigned short usb_check_interrupt_interval_override(struct usb_device* udev)
{
	size_t i;
	unsigned short res;
	u16 vendor = le16_to_cpu(udev->descriptor.idVendor);
	u16 product = le16_to_cpu(udev->descriptor.idProduct);

	mutex_lock(&interrupt_interval_override_mutex);
	for (i = 0; i < interrupt_interval_override_count; i++) {
		if (interrupt_interval_override_list[i].vendor == vendor
				&& interrupt_interval_override_list[i].product == product) {

			res = interrupt_interval_override_list[i].interval;
			mutex_unlock(&interrupt_interval_override_mutex);
			return res;
		}
	}
	mutex_unlock(&interrupt_interval_override_mutex);
	return 0;
}

/* Patches all applicable endpoints. */
static unsigned int patch_endpoints(struct usb_device* udev, unsigned short interval)
{
	unsigned int patched_count = 0;

	if (udev != NULL && udev->actconfig != NULL)
	{
		struct usb_interface* interface = udev->actconfig->interface[0];

		if (interface != NULL)
		{
			for (unsigned int altsetting = 0; altsetting < interface->num_altsetting; altsetting++)
			{
				struct usb_host_interface* altsettingptr = &interface->altsetting[altsetting];

				for (__u8 endpoint = 0; endpoint < altsettingptr->desc.bNumEndpoints; endpoint++)
				{
					struct usb_endpoint_descriptor* endpoint_desc = &altsettingptr->endpoint[endpoint].desc;

					if (usb_endpoint_xfer_int(endpoint_desc))
					{
						if (endpoint_desc->bInterval != interval)
						{
							unsigned short old_interval = endpoint_desc->bInterval;
							endpoint_desc->bInterval = interval;

							printk(KERN_INFO "usb_oc: bInterval value of endpoint 0x%.2x set from %u to %u.\n", 
								endpoint_desc->bEndpointAddress, old_interval, interval);
							patched_count++;
						}
						else
						{
							printk(KERN_INFO "usb_oc: bInterval value of endpoint 0x%.2x is already %u. No change needed.\n",
								endpoint_desc->bEndpointAddress, endpoint_desc->bInterval);
						}
					}
					else
					{
						printk(KERN_INFO "usb_oc: Skipping non-interrupt endpoint 0x%.2x (bInterval: %u).\n",
							endpoint_desc->bEndpointAddress, endpoint_desc->bInterval);
					}
				}
			}

			if (patched_count > 0)
			{
				/*
				* Attempt to lock the device.
				* This is required by the kernel documentation but it seems that some systems won't let you lock the USB device.
				* Older versions before 1.2 never called this function and still worked so we proceed even if locking fails.
				*/
				int ret = usb_lock_device_for_reset(udev, NULL);
				if (ret)
				{
					printk(KERN_ERR "usb_oc: Failed to acquire lock for USB device (error: %d). Resetting device anyway...\n", ret);
				}
				/* TODO: It might be possible to make the new bInterval value take effect without calling usb_reset_device? */
				if (usb_reset_device(udev))
				{
					printk(KERN_ERR "usb_oc: Could not reset device (error: %d). bInterval value was NOT changed.\n", ret);
				}
				/* Only unlock the device if usb_lock_device_for_reset succeeded. */
				if (!ret)
				{
					usb_unlock_device(udev);
				}
			}
		}
	}

	if (patched_count == 0)
	{
		printk(KERN_WARNING "usb_oc: No endpoints were patched on device %.4x:%.4x. "
			"Either the device has no interrupt endpoints at all, "
			"or they're all already set to the desired interval of %u, "
			"making the overclock attempt redudant. "
			"In short: we did nothing.\n",
			udev->descriptor.idVendor, udev->descriptor.idProduct, interval);
	}

	return patched_count;
}

static int on_usb_notify(struct notifier_block* self, unsigned long action, void* _udev)
{
	struct usb_device* udev = _udev;

	switch (action)
	{
	case USB_DEVICE_ADD:
		{
			unsigned short interval = usb_check_interrupt_interval_override(udev);
			if (interval > 0)
			{
				printk(KERN_INFO "usb_oc: Connected new device %.4x:%.4x and it's configured to be overriden with bInterval %u.\n",
					udev->descriptor.idVendor, udev->descriptor.idProduct, interval);
				patch_endpoints(udev, interval);
			}
		}
		break;

	case USB_DEVICE_REMOVE:
		/* We currently don't support restoring the previous bInterval value per-device on unload, unfortunately.
		   That's because theoretically multiple interrupt endpoints on the device level may have different 
		   original bInterval value, so that'd be annoying to code. Patches welcome if you care.
		   And if you don't, just re-plug your device after unloading the module. */
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block usb_nb = { .notifier_call = on_usb_notify };

static int usb_device_cb(struct usb_device* udev, void* data)
{
	unsigned short interval = usb_check_interrupt_interval_override(udev);
	if (interval > 0)
	{
		printk(KERN_INFO "usb_oc: Detected connected device %.4x:%.4x and it's configured to be overriden with bInterval %u.\n",
			udev->descriptor.idVendor, udev->descriptor.idProduct, interval);
		patch_endpoints(udev, interval);
	}

	return 0;
}

static int __init on_module_init(void)
{
	printk(KERN_INFO "usb_oc: [DEBUG] [on_module_init]\n");
	usb_for_each_dev(NULL, &usb_device_cb);
	usb_register_notify(&usb_nb);
	
	configured = 1;

	return 0;
}

static void __exit on_module_exit(void)
{
	printk(KERN_INFO "usb_oc: Unloading the module. Note that you need to reconnect the "
		"affected USB devices in order to restore their original bInterval value.\n");
	usb_unregister_notify(&usb_nb);
}

module_init(on_module_init);
module_exit(on_module_exit);

static const struct kernel_param_ops interrupt_interval_override_param_ops = {
	.set = interrupt_interval_override_param_set,
	.get = param_get_string,
};

static struct kparam_string interrupt_interval_override_param_string = {
	.maxlen = sizeof(interrupt_interval_override_param),
	.string = interrupt_interval_override_param,
};

module_param_cb(interrupt_interval_override,
	&interrupt_interval_override_param_ops,
	&interrupt_interval_override_param_string,
	0644);
MODULE_PARM_DESC(interrupt_interval_override,
	"Override the polling interval of all interrupt-type endpoints of a specific USB"
	" device by specifying interrupt_interval_override=vendorID:productID:interval.");
