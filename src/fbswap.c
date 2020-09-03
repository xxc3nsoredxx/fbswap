#define CONFIG_DEBUG_ATOMIC_SLEEP

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/keyboard.h>
#include <linux/module.h>
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
 */
static int kb_notified(struct notifier_block *nb, unsigned long code, void *data);

static struct notifier_block kb_nb = {
	.notifier_call = kb_notified
};

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
		/* pr_info("fbswap: '%02x' pressed, total pressed: %d\n", keycode, num_pressed); */
	} else {
		/* Find the index of the released key */
		for (i = 0; i < num_pressed && keys_pressed[i] != keycode; i++);

		for (; i < num_pressed - 1; i++) {
			keys_pressed[i] = keys_pressed[i+1];
		}
		num_pressed--;
		/* pr_info("fbswap: '%02x' released, total pressed: %d\n", keycode, num_pressed); */
	}


	/**
	 * State machine to detect Ctrl-Alt-[Num]
	 * Any combination of Left/Right-Ctrl and Left/Right-Alt is allowed
	 * Invalid key goes to the "RESET" state
	 */
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
			/* TODO: call function to switch framebuffers */
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
