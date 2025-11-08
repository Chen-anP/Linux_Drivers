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
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x2eef491c, "__register_chrdev" },
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


MODULE_INFO(srcversion, "60BA2A66BDC429949CE408B");
