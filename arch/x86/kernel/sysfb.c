/*
 * Generic System Framebuffers on x86
 * Copyright (c) 2012-2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/*
 * Simple-Framebuffer support for x86 systems
 * Create a platform-device for any available boot framebuffer. The
 * simple-framebuffer platform device is already available on DT systems, so
 * this module parses the global "screen_info" object and creates a suitable
 * platform device compatible with the "simple-framebuffer" DT object. If
 * the framebuffer is incompatible, we instead create a legacy
 * "vesa-framebuffer", "efi-framebuffer" or "platform-framebuffer" device and
 * pass the screen_info as platform_data. This allows legacy drivers
 * to pick these devices up without messing with simple-framebuffer drivers.
 * The global "screen_info" is still valid at all times.
 *
 * If CONFIG_X86_SYSFB is not selected, we never register "simple-framebuffer"
 * platform devices, but only use legacy framebuffer devices for
 * backwards compatibility.
 *
 * TODO: We set the dev_id field of all platform-devices to 0. This allows
 * other x86 OF/DT parsers to create such devices, too. However, they must
 * start at offset 1 for this to work.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/platform_data/simplefb.h>
#include <linux/platform_device.h>
#include <linux/screen_info.h>
#include <asm/sysfb.h>
#include <asm/bootparam.h>
#include <asm/setup.h>

static __init int sysfb_init(void)
{
	struct screen_info *si = &screen_info;
	struct simplefb_platform_data mode;
	struct platform_device *pd;
	const char *name;
	bool compatible;
	int ret;
printk(KERN_ERR "sysfb: init\n");//XXX
#if 1
if (!screen_info.lfb_base) {
screen_info.orig_x = 0x00000000;
screen_info.orig_y = 0x00000009;
screen_info.ext_mem_k = 0x0000ec00;
screen_info.orig_video_page = 0x00008c07;
screen_info.orig_video_mode = 0x00000015;
screen_info.orig_video_cols = 0x0000005a;
screen_info.flags = 0x00000000;
screen_info.unused2 = 0x00000000;
screen_info.orig_video_ega_bx = 0x00000000;
screen_info.unused3 = 0x00000000;
screen_info.orig_video_lines = 0x0000001e;
screen_info.orig_video_isVGA = 0x00000023;
screen_info.orig_video_points = 0x00000010;
screen_info.lfb_width = 0x000002d0;
screen_info.lfb_height = 0x000001e0;
screen_info.lfb_depth = 0x00000020;
screen_info.lfb_base = 0xf7c00000;
screen_info.lfb_size = 0x00000040;
screen_info.cl_magic = 0x0000a33f;
screen_info.cl_offset = 0x00000000;
screen_info.lfb_linelength = 0x00000b40;
screen_info.red_size = 0x00000008;
screen_info.red_pos = 0x00000010;
screen_info.green_size = 0x00000008;
screen_info.green_pos = 0x00000008;
screen_info.blue_size = 0x00000008;
screen_info.blue_pos = 0x00000000;
screen_info.rsvd_size = 0x00000008;
screen_info.rsvd_pos = 0x00000018;
screen_info.vesapm_seg = 0x00000000;
screen_info.vesapm_off = 0x00000000;
screen_info.pages = 0x00000001;
screen_info.vesa_attributes = 0x00000000;
screen_info.capabilities = 0x00000000;
screen_info.ext_lfb_base = 0x00000000;
}
#define _(x) printk("sysfb: dump: boot_params.screen_info." #x " = 0x%.08x\n", boot_params.screen_info.x)
printk("sysfb: dump: boot_params.screen_info = 0x%.08x\n", &boot_params.screen_info);
_(orig_x);
_(orig_y);
_(ext_mem_k);
_(orig_video_page);
_(orig_video_mode);
_(orig_video_cols);
_(flags);
_(unused2);
_(orig_video_ega_bx);
_(unused3);
_(orig_video_lines);
_(orig_video_isVGA);
_(orig_video_points);
	/* VESA graphic mode -- linear frame buffer */
_(lfb_width);
_(lfb_height);
_(lfb_depth);
_(lfb_base);
_(lfb_size);
_(cl_magic);
_(cl_offset);
_(lfb_linelength);
_(red_size);
_(red_pos);
_(green_size);
_(green_pos);
_(blue_size);
_(blue_pos);
_(rsvd_size);
_(rsvd_pos);
_(vesapm_seg);
_(vesapm_off);
_(pages);
_(vesa_attributes);
_(capabilities);
_(ext_lfb_base);
#undef _
#endif
#if 1
#define _(x) printk("sysfb: dump: screen_info." #x " = 0x%.08x\n", screen_info.x)
printk("sysfb: dump: screen_info = 0x%.08x\n", &screen_info);
_(orig_x);
_(orig_y);
_(ext_mem_k);
_(orig_video_page);
_(orig_video_mode);
_(orig_video_cols);
_(flags);
_(unused2);
_(orig_video_ega_bx);
_(unused3);
_(orig_video_lines);
_(orig_video_isVGA);
_(orig_video_points);
	/* VESA graphic mode -- linear frame buffer */
_(lfb_width);
_(lfb_height);
_(lfb_depth);
_(lfb_base);
_(lfb_size);
_(cl_magic);
_(cl_offset);
_(lfb_linelength);
_(red_size);
_(red_pos);
_(green_size);
_(green_pos);
_(blue_size);
_(blue_pos);
_(rsvd_size);
_(rsvd_pos);
_(vesapm_seg);
_(vesapm_off);
_(pages);
_(vesa_attributes);
_(capabilities);
_(ext_lfb_base);
#undef _
#endif
	sysfb_apply_efi_quirks();

	/* try to create a simple-framebuffer device */
	compatible = parse_mode(si, &mode);
printk(KERN_ERR "sysfb: compat? %i\n",compatible);//XXX
	if (compatible) {
		ret = create_simplefb(si, &mode);
printk(KERN_ERR "sysfb: %i\n",ret);//XXX
		if (!ret)
			return 0;
	}

	/* if the FB is incompatible, create a legacy framebuffer device */
	if (si->orig_video_isVGA == VIDEO_TYPE_EFI)
		name = "efi-framebuffer";
	else if (si->orig_video_isVGA == VIDEO_TYPE_VLFB)
		name = "vesa-framebuffer";
	else
		name = "platform-framebuffer";

	pd = platform_device_register_resndata(NULL, name, 0,
					       NULL, 0, si, sizeof(*si));
	return PTR_ERR_OR_ZERO(pd);
}

/* must execute after PCI subsystem for EFI quirks */
device_initcall(sysfb_init);
