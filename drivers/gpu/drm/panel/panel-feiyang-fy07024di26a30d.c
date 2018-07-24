// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Vasily Khoruzhick <anarsoul@gmail.com>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

struct fy07024di26a30d {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;

	struct backlight_device *backlight;
	struct regulator *dvdd;
	struct regulator *avdd;
	struct gpio_desc	*reset;
};

struct fy07024di26a30d_cmd {
	u8	cmd;
	u8	data;
};

static struct fy07024di26a30d_cmd fy07024di26a30d_init[] = {
	{ .cmd = 0x80, .data = 0x58 },
	{ .cmd = 0x81, .data = 0x47 },
	{ .cmd = 0x82, .data = 0xD4 },
	{ .cmd = 0x83, .data = 0x88 },
	{ .cmd = 0x84, .data = 0xA9 },
	{ .cmd = 0x85, .data = 0xC3 },
	{ .cmd = 0x86, .data = 0x82 },
};

static inline struct fy07024di26a30d *panel_to_fy07024di26a30d(struct drm_panel *panel)
{
	return container_of(panel, struct fy07024di26a30d, panel);
}

static int fy07024di26a30d_send_cmd_data(struct fy07024di26a30d *ctx, u8 cmd, u8 data)
{
	int ret;

	ret = mipi_dsi_dcs_write(ctx->dsi, cmd,
				 (u8[]){ data }, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static int fy07024di26a30d_prepare(struct drm_panel *panel)
{
	struct fy07024di26a30d *ctx = panel_to_fy07024di26a30d(panel);
	unsigned int i;
	int ret;
	u8 mode;

	gpiod_set_value(ctx->reset, 1);
	msleep(50);

	msleep(100);

	gpiod_set_value(ctx->reset, 0);
	msleep(20);

	gpiod_set_value(ctx->reset, 1);
	msleep(200);

	for (i = 0; i < ARRAY_SIZE(fy07024di26a30d_init); i++) {
		struct fy07024di26a30d_cmd *cmd = &fy07024di26a30d_init[i];

		ret = fy07024di26a30d_send_cmd_data(ctx, cmd->cmd,
					      cmd->data);

		if (ret)
			return ret;
	}

	//mipi_dsi_dcs_get_power_mode(ctx->dsi, &mode);
	//pr_info("%s: mode: %.8x\n", __func__, (unsigned)mode);

	//ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	//if (ret)
	//	return ret;

	//mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
	//if (ret)
	//	return ret;
	//mipi_dsi_dcs_get_power_mode(ctx->dsi, &mode);
	//pr_info("%s: mode: %.8x\n", __func__, (unsigned)mode);

	return 0;
}

static int fy07024di26a30d_enable(struct drm_panel *panel)
{
	struct fy07024di26a30d *ctx = panel_to_fy07024di26a30d(panel);

	msleep(120);

	backlight_enable(ctx->backlight);

	return 0;
}

static int fy07024di26a30d_disable(struct drm_panel *panel)
{
	struct fy07024di26a30d *ctx = panel_to_fy07024di26a30d(panel);

	backlight_disable(ctx->backlight);
	gpiod_set_value(ctx->reset, 0);

	return 0;
}

static int fy07024di26a30d_unprepare(struct drm_panel *panel)
{
	struct fy07024di26a30d *ctx = panel_to_fy07024di26a30d(panel);

	//mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	//gpiod_set_value(ctx->reset, 0);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 55000,
	.hdisplay = 1024,
	.hsync_start = 1024 + 396,
	.hsync_end = 1024 + 396 + 20,
	.htotal = 1024 + 396 + 20 + 100,
	.vdisplay = 600,
	.vsync_start = 600 + 12,
	.vsync_end = 600 + 12 + 2,
	.vtotal = 600 + 12 + 2 + 21,
	.vrefresh = 60,
};

static int fy07024di26a30d_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct fy07024di26a30d *ctx = panel_to_fy07024di26a30d(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(&ctx->dsi->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	panel->connector->display_info.width_mm = 62;
	panel->connector->display_info.height_mm = 110;

	return 1;
}

static const struct drm_panel_funcs fy07024di26a30d_funcs = {
	.prepare	= fy07024di26a30d_prepare,
	.unprepare	= fy07024di26a30d_unprepare,
	.enable		= fy07024di26a30d_enable,
	.disable	= fy07024di26a30d_disable,
	.get_modes	= fy07024di26a30d_get_modes,
};

static int fy07024di26a30d_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct device_node *np;
	struct fy07024di26a30d *ctx;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dvdd = devm_regulator_get(&dsi->dev, "dvdd");
	if (IS_ERR(ctx->dvdd))
		return PTR_ERR(ctx->dvdd);

	ctx->avdd = devm_regulator_get(&dsi->dev, "avdd");
	if (IS_ERR(ctx->avdd))
		return PTR_ERR(ctx->avdd);

	ret = regulator_enable(ctx->dvdd);
	if (ret < 0) {
		dev_err(&dsi->dev, "failed to enable dvdd: %d\n", ret);
		return ret;
	}

	msleep(100);

	ret = regulator_enable(ctx->avdd);
	if (ret < 0) {
		dev_err(&dsi->dev, "failed to enable avdd: %d\n", ret);
		regulator_disable(ctx->dvdd);
		return ret;
	}

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &fy07024di26a30d_funcs;

	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		dev_err(&dsi->dev, "Couldn't get our reset GPIO\n");
		return PTR_ERR(ctx->reset);
	}

	np = of_parse_phandle(dsi->dev.of_node, "backlight", 0);
	if (np) {
		ctx->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;

	return mipi_dsi_attach(dsi);
}

static int fy07024di26a30d_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct fy07024di26a30d *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	if (ctx->backlight)
		put_device(&ctx->backlight->dev);

	return 0;
}

static const struct of_device_id fy07024di26a30d_of_match[] = {
	{ .compatible = "feiyang,fy07024di26a30d" },
	{ }
};
MODULE_DEVICE_TABLE(of, fy07024di26a30d_of_match);

static struct mipi_dsi_driver fy07024di26a30d_dsi_driver = {
	.probe		= fy07024di26a30d_dsi_probe,
	.remove		= fy07024di26a30d_dsi_remove,
	.driver = {
		.name		= "fy07024di26a30d-dsi",
		.of_match_table	= fy07024di26a30d_of_match,
	},
};
module_mipi_dsi_driver(fy07024di26a30d_dsi_driver);

MODULE_AUTHOR("Vasily Khoruzhick <anarsoul@gmail.com>");
MODULE_DESCRIPTION("Feiyang FY07024DI26A30D Panel Driver");
MODULE_LICENSE("GPL v2");
