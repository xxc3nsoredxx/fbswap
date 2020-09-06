#define CONFIG_DEBUG_ATOMIC_SLEEP

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <uapi/linux/keyboard.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxc3nsoredxx");
MODULE_DESCRIPTION("Swap between framebuffer views in the same tty");
MODULE_VERSION("0.1");

#define K_L_CTRL	0x1d
#define K_R_CTRL	0x61
#define K_L_ALT		0x38
#define K_R_ALT		0x64
#define K_1		0x02
#define K_2		0x03
#define K_3		0x04
#define K_4		0x05
#define K_5		0x06
#define K_6		0x07
#define K_7		0x08
#define K_8		0x09
#define K_9		0x0a
#define K_0		0x0b

/**
 * The possible states for the state machine
 * EMPTY: no keys currently pressed
 * LC: Left-Ctrl pressed
 * RC: Right-Ctrl pressed
 * LA: Left-Alt pressed
 * RA: Right-Alt pressed
 * *C2: Alt followed by Ctrl
 * *A2: Ctrl followed by Alt
 * SWITCH: Ctrl-Alt-[Num] detected
 * RESET: wait for no keys to be pressed
 */
enum states {
	EMPTY,
	LC, RC, LA, RA,
	LC2, RC2, LA2, RA2,
	SWITCH, RESET
};

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
 *
 * struct fb_info:
 * 	struct fb_var_screeninfo var: current variable info
 * 	struct fb_fix_screeninfo fix: current fixed screen info
 *
 * struct fb_var_screeninfo:
 * 	__u32 xres: visible resolution
 * 	__u32 yres
 * 	__u32 bits_per_pixel
 *
 * struct fb_fix_screeninfo:
 * 	__u32 type: the pixel type (FB_TYPE_* in uapi/linux/fb.h)
 * 	__u32 visual: the type of color used (FB_VISUAL_* in uapi/linux/fb.h)
 */

static struct fb_info *get_fb_info(unsigned int idx);
static void put_fb_info(struct fb_info *fb_info);
static int fb_swap(unsigned int target);
static int kb_notified(struct notifier_block *nb, unsigned long code, void *data);

static struct notifier_block kb_nb = {
	.notifier_call = kb_notified
};
static DEFINE_MUTEX(registration_lock);
static struct fb_info *fb_info;

/* From  drivers/video/core/fbdev/fbmem.c */
static struct fb_info *get_fb_info(unsigned int idx)
{
	struct fb_info *fb_info;

	if (idx >= FB_MAX)
		return ERR_PTR(-ENODEV);
	
	mutex_lock(&registration_lock);
	fb_info = registered_fb[idx];
	if (fb_info)
		atomic_inc(&fb_info->count);
	mutex_unlock(&registration_lock);

	return fb_info;
}

static void put_fb_info(struct fb_info *fb_info)
{
	if (!atomic_dec_and_test(&fb_info->count))
		return;
	if (fb_info->fbops->fb_destroy)
		fb_info->fbops->fb_destroy(fb_info);
}

static int fb_swap(unsigned int target)
{
	int ret = 0;
	struct fb_fix_screeninfo *fix;
	struct fb_var_screeninfo *var;
	int is_true;

	fix = &fb_info->fix;
	var = &fb_info->var;
	is_true = fix->visual == FB_VISUAL_TRUECOLOR;

	/* Print some framebuffer stats */
	pr_info("fbswap: x resolution: %d\n", var->xres);
	pr_info("fbswap: y resolution: %d\n", var->yres);
	pr_info("fbswap: bits per pixel: %d\n", var->bits_per_pixel);
	pr_info("fbswap: truecolor: %s\n", ((is_true) ? "yes" : "no"));

	return ret;
}

/**
 * Listens for key-presses and detects when to switch framebuffer views
 */
static int kb_notified(struct notifier_block *nb, unsigned long code, void *data)
{
	struct keyboard_notifier_param *param = data;
	/* struct vc_data *vc = param->vc; */
	int down = param->down;
	int keycode;
	int i;
	static int keys_pressed[256] = {0};
	static int num_pressed = 0;
	int dup_key = 0;
	static enum states s = EMPTY;

	if (code == KBD_KEYCODE)
		keycode = param->value;
	else
		return NOTIFY_DONE;
	
	if (!down && num_pressed == 0)
		return NOTIFY_DONE;
	
	if (down) {
		for (i = 0; i < num_pressed; i++) {
			if (keys_pressed[i] == keycode) {
				dup_key = 1;
				break;
			}
		}

		if (dup_key)
			return NOTIFY_DONE;

		keys_pressed[num_pressed] = keycode;
		num_pressed++;
	} else {
		/* Find the index of the released key */
		for (i = 0; i < num_pressed && keys_pressed[i] != keycode; i++);

		for (; i < num_pressed - 1; i++) {
			keys_pressed[i] = keys_pressed[i+1];
		}
		num_pressed--;
	}


	/* State machine to detect Ctrl-Alt-[Num] */
	switch (s) {
	case EMPTY:
		if (keycode == K_L_CTRL) {
			pr_info("fbswap: Left-Ctrl pressed\n");
			s = LC;
		} else if (keycode == K_R_CTRL) {
			pr_info("fbswap: Right-Control pressed\n");
			s = RC;
		} else if (keycode == K_L_ALT) {
			pr_info("fbswap: Left-Alt pressed\n");
			s = LA;
		} else if (keycode == K_R_ALT) {
			pr_info("fbswap: Right-Alt pressed\n");
			s = RA;
		} else {
			pr_info("fbswap: Invalid key pressed - RESET initiated\n");
			s = RESET;
		}
		break;
	case LC:
	case RC:
		if (keycode == K_L_ALT) {
			pr_info("fbswap: Ctrl + Left-Alt pressed\n");
			s = LA2;
		} else if (keycode == K_R_ALT) {
			pr_info("fbswap: Ctrl + Right-Alt pressed\n");
			s = RA2;
		} else {
			pr_info("fbswap: Invalid key pressed - RESET initiated\n");
			s = RESET;
		}
		break;
	case LA:
	case RA:
		if (keycode == K_L_CTRL) {
			pr_info("fbswap: Alt + Left-Ctrl pressed\n");
			s = LA2;
		} else if (keycode == K_R_CTRL) {
			pr_info("fbswap: Alt + Right-Ctrl pressed\n");
			s = RA2;
		} else {
			pr_info("fbswap: Invalid key pressed - RESET initiated\n");
			s = RESET;
		}
		break;
	case LC2:
	case RC2:
	case LA2:
	case RA2:
		if (keycode >= K_1 && keycode <= K_0) {
			pr_info("fbswap: Ctrl-Alt-[Num] pressed - switching framebuffers\n");
			s = SWITCH;
			fb_swap(keycode - K_1);
		} else {
			pr_info("fbswap: Invalid key pressed - RESET initiated\n");
			s = RESET;
		}
		break;
	case SWITCH:
		if (num_pressed > 0) {
			pr_info("fbswap: SWITCH - total pressed: %d\n", num_pressed);
		} else {
			pr_info("fbswap: SWITCH - complete\n");
			s = EMPTY;
		}
		break;
	case RESET:
		if (num_pressed > 0) {
			pr_info("fbswap: RESET - total pressed: %d\n", num_pressed);
		} else {
			pr_info("fbswap: RESET - complete\n");
			s = EMPTY;
			return NOTIFY_STOP;
		}
		break;
	default:
		pr_err("fbswap: invalid state detected!\n");
		break;
	}

	return NOTIFY_OK;
}

/**
 * Initializes the fbswap module
 */
static int __init fbswap_init(void)
__acquires(&info->lock)
__releases(&info->lock)
{
	int err;
	int fbidx = 0;

	err = register_keyboard_notifier(&kb_nb);
	if (err) {
		pr_err("fbswap: unable to register keyboard notifier!\n");
		return err;
	}

	/* From drivers/video/fbdev/core/fbmem.c */
	fb_info = get_fb_info(fbidx);
	if (!fb_info) {
		request_module("fb%d", fbidx);
		fb_info = get_fb_info(fbidx);
		if (!fb_info) {
			pr_err("fbswap: unable to find fb%d!\n", fbidx);
			return -ENODEV;
		}
	}
	if (IS_ERR(fb_info)) {
		pr_err("fbswap: unable to get info for fb%d!\n", fbidx);
		return PTR_ERR(fb_info);
	}

	lock_fb_info(fb_info);
	if (!try_module_get(fb_info->fbops->owner)) {
		err = -ENODEV;
		pr_err("fbswap: unable to find fb%d!\n", fbidx);
		goto out;
	}
	if (fb_info->fbops->fb_open) {
		err = fb_info->fbops->fb_open(fb_info, 1);
		if (err) {
			pr_err("fbswap: unable to open fb%d!\n", fbidx);
			module_put(fb_info->fbops->owner);
		}
	}
out:
	unlock_fb_info(fb_info);
	if (err)
		put_fb_info(fb_info);

	pr_info("fbswap: loaded\n");

	return err;
}

/**
 * Called upon module unload
 * Cleans up after itslef
 */
static void __exit fbswap_exit(void)
__acquires(&info->lock)
__releases(&info->lock)
{
	int err;

	lock_fb_info(fb_info);
	if (fb_info->fbops->fb_release)
		fb_info->fbops->fb_release(fb_info, 1);
	module_put(fb_info->fbops->owner);
	unlock_fb_info(fb_info);
	put_fb_info(fb_info);

	err = unregister_keyboard_notifier(&kb_nb);
	if (err)
		pr_err("fbswap: unable to unregister keyboard notifier!\n");

	pr_info("fbswap: unloaded\n");
}

module_init(fbswap_init);
module_exit(fbswap_exit);

/* vim: set tabstop=8 noexpandtab: */
