// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#define FEIYANG_INIT_CMD_LEN	2
#define REGFLAG_DELAY           0XFF
#define SINGLE_COMMAND			0xFE

struct feiyang {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	struct backlight_device	*backlight;
	struct regulator	*dvdd;
	struct regulator	*avdd;
	struct gpio_desc	*reset;
};

static inline struct feiyang *panel_to_feiyang(struct drm_panel *panel)
{
	return container_of(panel, struct feiyang, panel);
}

struct feiyang_init_cmd {
	u8 data[FEIYANG_INIT_CMD_LEN];
};

static const struct feiyang_init_cmd feiyang_init_cmds[] = {
	{.data={0xE0, 0x00}},	
	{.data={0xE1, 0x93}},
	{.data={0xE2, 0x65}},
	{.data={0xE3, 0xF8}},
	{.data={0xE0, 0x01}},
	{.data={0x01, 0xB7}},
	{.data={0x18, 0xB2}},
	{.data={0x19, 0x01}},
	{.data={0x1B, 0xB2}},
	{.data={0x1C, 0x01}},
	{.data={0x1F, 0x7E}},
	{.data={0x20, 0x28}},
	{.data={0x21, 0x28}},
	{.data={0x22, 0x0E}},
	{.data={0x24, 0xC8}},
	{.data={0x37, 0x29}},
	{.data={0x38, 0x05}},
	{.data={0x39, 0x08}},
	{.data={0x3A, 0x12}},
	{.data={0x3C, 0x78}},
	{.data={0x3E, 0xFF}},
	{.data={0x3E, 0xFF}},
	{.data={0x3F, 0xFF}},
	{.data={0x40, 0x06}},
	{.data={0x41, 0xA0}},
	{.data={0x55, 0x01}},
	{.data={0x56, 0x01}},
	{.data={0x57, 0x89}},
	{.data={0x58, 0x0A}},
	{.data={0x59, 0x2A}},
	{.data={0x5A, 0x31}},
	{.data={0x5B, 0x15}},
	{.data={0x5D, 0x7C}},
	{.data={0x5E, 0x64}},
	{.data={0x5F, 0x54}},
	{.data={0x60, 0x47}},
	{.data={0x61, 0x42}},
	{.data={0x62, 0x32}},
	{.data={0x63, 0x34}},
	{.data={0x64, 0x1C}},
	{.data={0x65, 0x32}},
	{.data={0x66, 0x2E}},
	{.data={0x67, 0x2B}},
	{.data={0x68, 0x46}},
	{.data={0x69, 0x32}},
	{.data={0x6A, 0x38}},
	{.data={0x6B, 0x2A}},
	{.data={0x6C, 0x28}},
	{.data={0x6D, 0x1B}},
	{.data={0x6E, 0x0D}},
	{.data={0x6F, 0x00}},
	{.data={0x70, 0x7C}},
	{.data={0x71, 0x64}},
	{.data={0x72, 0x54}},
	{.data={0x73, 0x47}},
	{.data={0x74, 0x42}},
	{.data={0x75, 0x32}},
	{.data={0x76, 0x34}},
	{.data={0x77, 0x1C}},
	{.data={0x78, 0x32}},
	{.data={0x79, 0x2E}},
	{.data={0x7A, 0x2B}},
	{.data={0x7B, 0x46}},
	{.data={0x7C, 0x32}},
	{.data={0x7D, 0x38}},
	{.data={0x7E, 0x2A}},
	{.data={0x7F, 0x28}},
	{.data={0x80, 0x1B}},
	{.data={0x81, 0x0D}},
	{.data={0x82, 0x00}},
	{.data={0xE0, 0x02}},
	{.data={0x00, 0x00}},
	{.data={0x01, 0x04}},
	{.data={0x02, 0x08}},
	{.data={0x03, 0x05}},
	{.data={0x04, 0x09}},
	{.data={0x05, 0x06}},
	{.data={0x06, 0x0A}},
	{.data={0x07, 0x07}},
	{.data={0x08, 0x0B}},
	{.data={0x09, 0x1f}},
	{.data={0x0A, 0x1f}},
	{.data={0x0B, 0x1f}},
	{.data={0x0C, 0x1f}},
	{.data={0x0D, 0x1f}},
	{.data={0x0E, 0x1f}},
	{.data={0x0F, 0x17}},
	{.data={0x10, 0x37}},
	{.data={0x11, 0x10}},
	{.data={0x12, 0x1F}},
	{.data={0x13, 0x1F}},
	{.data={0x14, 0x1F}},
	{.data={0x15, 0x1F}},
	{.data={0x16, 0x00}},
	{.data={0x17, 0x04}},
	{.data={0x18, 0x08}},
	{.data={0x19, 0x05}},
	{.data={0x1A, 0x09}},
	{.data={0x1B, 0x06}},
	{.data={0x1C, 0x0A}},
	{.data={0x1D, 0x07}},
	{.data={0x1E, 0x0B}},
	{.data={0x1F, 0x1f}},
	{.data={0x20, 0x1f}},
	{.data={0x21, 0x1f}},
	{.data={0x22, 0x1f}},
	{.data={0x23, 0x1f}},
	{.data={0x24, 0x1f}},
	{.data={0x25, 0x17}},
	{.data={0x26, 0x37}},
	{.data={0x27, 0x10}},
	{.data={0x28, 0x1F}},
	{.data={0x29, 0x1F}},
	{.data={0x2A, 0x1F}},
	{.data={0x2B, 0x1F}},
	{.data={0x58, 0x01}},
	{.data={0x5B, 0x00}},
	{.data={0x5C, 0x01}},
	{.data={0x5D, 0x30}},
	{.data={0x5E, 0x00}},
	{.data={0x5F, 0x00}},
	{.data={0x60, 0x30}},
	{.data={0x61, 0x00}},
	{.data={0x62, 0x00}},
	{.data={0x63, 0x03}},
	{.data={0x64, 0x6A}},
	{.data={0x65, 0x45}},
	{.data={0x66, 0x08}},
	{.data={0x67, 0x73}},
	{.data={0x68, 0x05}},
	{.data={0x69, 0x06}},
	{.data={0x6A, 0x6A}},
	{.data={0x6B, 0x08}},
	{.data={0x6C, 0x00}},
	{.data={0x6D, 0x00}},
	{.data={0x6E, 0x00}},
	{.data={0x6F, 0x88}},
	{.data={0x75, 0x80}},
	{.data={0x76, 0x00}},
	{.data={0x77, 0x05}},
	{.data={0x78, 0x10}},
	{.data={0xE0, 0x01}},
	{.data={0x0E, 0x01}},
	{.data={0xE0, 0x03}},
	{.data={0x98, 0x2F}},
	{.data={0xE0, 0x04}},
	{.data={0x2B, 0x2B}},
	{.data={0x2E, 0x44}},
	{.data={0xE0, 0x00}},
	{.data={0xE6, 0x02}},
	{.data={0xE7, 0x02}},

	//SLP OUT
	{.data={SINGLE_COMMAND, 0x11}},	   
	{.data={REGFLAG_DELAY, 120}},

	//DISP ON
	{.data={SINGLE_COMMAND, 0x29}}, 
	{.data={REGFLAG_DELAY, 35}},
};

static int feiyang_prepare(struct drm_panel *panel)
{
	struct feiyang *ctx = panel_to_feiyang(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	unsigned int i;
	int ret;

	ret = regulator_enable(ctx->dvdd);
	if (ret)
		return ret;

	/* T1 (dvdd start + dvdd rise) 0 < T1 <= 10ms */
	msleep(10);

	ret = regulator_enable(ctx->avdd);
	if (ret)
		return ret;

	/* T3 (dvdd rise + avdd start + avdd rise) T3 >= 20ms */
	msleep(20);

	gpiod_set_value(ctx->reset, 0);

	/*
	 * T5 + T6 (avdd rise + video & logic signal rise)
	 * T5 >= 10ms, 0 < T6 <= 10ms
	 */
	msleep(20);

	gpiod_set_value(ctx->reset, 1);

	/* T12 (video & logic signal rise + backlight rise) T12 >= 200ms */
	msleep(200);

	for (i = 0; i < ARRAY_SIZE(feiyang_init_cmds); i++) {
		const struct feiyang_init_cmd *cmd =
						&feiyang_init_cmds[i];

		if (cmd->data[0]==REGFLAG_DELAY)
		{
			msleep(cmd->data[1]);
			continue;
		}

#if 1
		if (cmd->data[0]==SINGLE_COMMAND)
		{
			ret=mipi_dsi_dcs_write_buffer(dsi, cmd->data+1, 1);
		}
		else
		{
			ret=mipi_dsi_dcs_write_buffer(dsi, cmd->data,
									2);			
		}
#else
		ret = mipi_dsi_dcs_write_buffer(dsi, cmd->data,
						FEIYANG_INIT_CMD_LEN);
#endif
		
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int feiyang_enable(struct drm_panel *panel)
{
	struct feiyang *ctx = panel_to_feiyang(panel);

	/* T12 (video & logic signal rise + backlight rise) T12 >= 200ms */
	msleep(200);

	mipi_dsi_dcs_set_display_on(ctx->dsi);
	backlight_enable(ctx->backlight);

	return 0;
}

static int feiyang_disable(struct drm_panel *panel)
{
	struct feiyang *ctx = panel_to_feiyang(panel);

	backlight_disable(ctx->backlight);
	return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int feiyang_unprepare(struct drm_panel *panel)
{
	struct feiyang *ctx = panel_to_feiyang(panel);
	int ret;

	ret = mipi_dsi_dcs_set_display_off(ctx->dsi);
	if (ret < 0)
		DRM_DEV_ERROR(panel->dev, "failed to set display off: %d\n",
			      ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	if (ret < 0)
		DRM_DEV_ERROR(panel->dev, "failed to enter sleep mode: %d\n",
			      ret);

	/* T13 (backlight fall + video & logic signal fall) T13 >= 200ms */
	msleep(200);

	gpiod_set_value(ctx->reset, 0);

	regulator_disable(ctx->avdd);

	/* T11 (dvdd rise to fall) 0 < T11 <= 10ms  */
	msleep(10);

	regulator_disable(ctx->dvdd);

	return 0;
}

/*
	.clock = 55000,
	.hdisplay = 1024,
	.hsync_start = 1024 + 310,
	.hsync_end = 1024 + 310 + 20,
	.htotal = 1024 + 310 + 20 + 90,

	.vdisplay = 600,
	.vsync_start = 600 + 12,
	.vsync_end = 600 + 12 + 2,
	.vtotal = 600 + 12 + 2 + 21,

			lcd_ht = <0x3d3>;
			//lcd_ht = <0x423>;
			lcd_hbp = <0x12>;
			lcd_hspw = <0x12>;

			lcd_vt = <0x510>;
			lcd_vbp = <0x7>;
			lcd_vspw = <0x2>;
*/
static const struct drm_display_mode feiyang_default_mode = {
	//.clock = 68000,
	.clock = 60000,

	.hdisplay = 800,
	.hsync_start = 800 + 160,
	.hsync_end = 800 + 160 + 20,
	.htotal = 800 + 160 + 20 + 90,

	.vdisplay = 1280,
	.vsync_start = 1280 + 2,
	.vsync_end = 1280 + 2 + 6,
	.vtotal = 1280 + 2 + 6 + 20,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int feiyang_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct feiyang *ctx = panel_to_feiyang(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &feiyang_default_mode);
	if (!mode) {
		DRM_DEV_ERROR(&ctx->dsi->dev, "failed to add mode %ux%ux@%u\n",
			      feiyang_default_mode.hdisplay,
			      feiyang_default_mode.vdisplay,
			      feiyang_default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs feiyang_funcs = {
	.disable = feiyang_disable,
	.unprepare = feiyang_unprepare,
	.prepare = feiyang_prepare,
	.enable = feiyang_enable,
	.get_modes = feiyang_get_modes,
};

static int feiyang_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct feiyang *ctx;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &feiyang_funcs;

	ctx->dvdd = devm_regulator_get(&dsi->dev, "dvdd");
	if (IS_ERR(ctx->dvdd)) {
		DRM_DEV_ERROR(&dsi->dev, "Couldn't get dvdd regulator\n");
		return PTR_ERR(ctx->dvdd);
	}

	ctx->avdd = devm_regulator_get(&dsi->dev, "avdd");
	if (IS_ERR(ctx->avdd)) {
		DRM_DEV_ERROR(&dsi->dev, "Couldn't get avdd regulator\n");
		return PTR_ERR(ctx->avdd);
	}

	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		DRM_DEV_ERROR(&dsi->dev, "Couldn't get our reset GPIO\n");
		return PTR_ERR(ctx->reset);
	}

	ctx->backlight = devm_of_find_backlight(&dsi->dev);
	if (IS_ERR(ctx->backlight))
		return PTR_ERR(ctx->backlight);

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

//	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;

	return mipi_dsi_attach(dsi);
}

static int feiyang_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct feiyang *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id feiyang_of_match[] = {
	{ .compatible = "feiyang,fy07024di26a30d", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, feiyang_of_match);

static struct mipi_dsi_driver feiyang_driver = {
	.probe = feiyang_dsi_probe,
	.remove = feiyang_dsi_remove,
	.driver = {
		.name = "feiyang-fy07024di26a30d",
		.of_match_table = feiyang_of_match,
	},
};
module_mipi_dsi_driver(feiyang_driver);

MODULE_AUTHOR("Jagan Teki <jagan@amarulasolutions.com>");
MODULE_DESCRIPTION("Feiyang FY07024DI26A30-D MIPI-DSI LCD panel");
MODULE_LICENSE("GPL");
