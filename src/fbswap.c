#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <uapi/linux/keyboard.h>

#define CONFIG_DEBUG_ATOMIC_SLEEP

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxc3nsoredxx");
MODULE_DESCRIPTION("Swap between framebuffer views in the same tty");
MODULE_VERSION("0.0");

/**
 * struct notifier_block:
 * 	notifier_fn_t notifier_call: function to call
 *
 * typedef int (*notifier_fn_t) (struct notifier_block *nb, ulong action, void *data)
 *
 * struct keyboard_notifier_param:
 * 	struct vc_data *vc: virtual console where the keypress happened
 * 	int down: 1 -> pressed, 0 -> released
 * 	int shift: shift masks (KG_*)
 * 	int ledstate
 * 	uint value: event type dependent
 * 		KBD_KEYCODE -> keycode
 * 
 * struct vc_data:
 * 	ushort vc_num: console number
 */

static int kb_notified(struct notifier_block *nb, unsigned long code, void *data);

static struct notifier_block kb_nb = {
	.notifier_call = kb_notified
};

static int kb_notified(struct notifier_block *nb, unsigned long code, void *data)
{
	struct keyboard_notifier_param *param = data;
	/* struct vc_data *vc = param->vc; */
	int down = param->down;
	int keycode;

	if (code == KBD_KEYCODE)
		keycode = param->value;
	else
		return NOTIFY_DONE;
	
	/* if (keycode == 'a') */
		pr_info("fbdev: '%d' %s\n", keycode, ((down) ? "pressed": "released"));

	return NOTIFY_OK;
}

/**
 * Initialized the fbswap module
 * Currently just a sanity check
 */
static int __init fbswap_init(void)
{
	int err;

	err = register_keyboard_notifier(&kb_nb);
	if (err) {
		pr_err("fbswap: unable to register keyboard notifier!\n");
		/* TODO: turn into proper error code */
		return 1;
	}

	pr_info("fbswap: loaded\n");

	return 0;
}

/**
 * Called upon module unload
 * Cleans up after itslef
 */
static void __exit fbswap_exit(void)
{
	int err;

	err = unregister_keyboard_notifier(&kb_nb);
	if (err)
		pr_err("fbswap: unable to unregister keyboard notifier!\n");

	pr_info("fbswap: unloaded\n");
}

/* Defines the init function */
module_init(fbswap_init);

/* Defines the exit function */
module_exit(fbswap_exit);

/* vim: set tabstop=8 noexpandtab: */
