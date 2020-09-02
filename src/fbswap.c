#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define CONFIG_DEBUG_ATOMIC_SLEEP

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xxc3nsoredxx");
MODULE_DESCRIPTION("Swap between framebuffer views in the same tty");
MODULE_VERSION("0.0");

/**
 * Initialized the fbswap module
 * Currently just a sanity check
 */
static int __init fbswap_init(void)
{
	pr_info(KERN_INFO "fbswap: loaded\n");
	return 0;
}

/**
 * Called upon module unload
 * Cleans up after itslef
 */
static void __exit fbswap_exit(void)
{
	pr_info(KERN_INFO "fbswap: unloaded\n");
}

/* Defines the init function */
module_init(fbswap_init);

/* Defines the exit function */
module_exit(fbswap_exit);

/* vim: set tabstop=8 noexpandtab: */
