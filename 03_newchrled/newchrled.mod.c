#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x66eb21ff, "module_layout" },
	{ 0xb1049b6b, "device_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x7814f134, "cdev_del" },
	{ 0x91e6fb6, "class_destroy" },
	{ 0x1b0f79a4, "device_create" },
	{ 0xbdb8ad3a, "__class_create" },
	{ 0x10d0f8c2, "cdev_add" },
	{ 0x88233bce, "cdev_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x3fd78f3b, "register_chrdev_region" },
	{ 0xedc03953, "iounmap" },
	{ 0x6b4b2933, "__ioremap" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
	{ 0xdcb764ad, "memset" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x9f49dcc4, "__stack_chk_fail" },
	{ 0xc5850110, "printk" },
	{ 0x56470118, "__warn_printk" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "69605F792A564B36B50AD2F");
