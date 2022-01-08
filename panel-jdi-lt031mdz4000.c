// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>


struct jdi_panel {
	struct drm_panel base;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *bl_en_gpio;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	const struct drm_display_mode *mode;
};

static inline struct jdi_panel *to_jdi_panel(struct drm_panel *panel)
{
	return container_of(panel, struct jdi_panel, base);
}

static int jdi_panel_init(struct jdi_panel *jdi)
{
	struct mipi_dsi_device *dsi = jdi->dsi;
	struct device *dev = &jdi->dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0)
		return ret;

	usleep_range(10000, 20000);

	mipi_dsi_dcs_write(dsi,0xb0,(u8[]){0x00},1);
	msleep(5);
	
	mipi_dsi_dcs_write(dsi,0xe4,(u8[]){0x00,0x00,0x00,0x00,0x08},5);
	
	mipi_dsi_dcs_write(dsi,0xd0,(u8[]){0x45,0x45,0x71},3);
	
	mipi_dsi_dcs_write(dsi,0x6f,(u8[]){0x01},1);

	mipi_dsi_dcs_exit_sleep_mode(dsi);
	msleep(120);
	
	mipi_dsi_dcs_write(dsi,0xd0,(u8[]){0x52,0x4a,0x71},3);
	
	mipi_dsi_dcs_write(dsi,0xbe,(u8[]){0xff,0x0f,0x00,0x18,0x04,0x04,0x00,0x5d},8);
	
	mipi_dsi_dcs_write(dsi,0xbb,(u8[]){0x2f},1);
	
	mipi_dsi_dcs_write(dsi,0xb0,(u8[]){0x2f},1);
	mdelay(20);
	dev_info(dev, "LCD init finished \n");
	return 0;
}

static int jdi_panel_on(struct jdi_panel *jdi)
{
	struct mipi_dsi_device *dsi = jdi->dsi;
	struct device *dev = &jdi->dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0)
		dev_err(dev, "failed to set display on: %d\n", ret);
	msleep(120);

	return ret;
}

static void jdi_panel_off(struct jdi_panel *jdi)
{
	struct mipi_dsi_device *dsi = jdi->dsi;
	struct device *dev = &jdi->dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(dev, "failed to set display off: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		dev_err(dev, "failed to enter sleep mode: %d\n", ret);

	msleep(100);
}

static int jdi_panel_disable(struct drm_panel *panel)
{
	struct jdi_panel *jdi = to_jdi_panel(panel);

	if (!jdi->enabled)
		return 0;

	gpiod_set_value_cansleep(jdi->bl_en_gpio, 0);

	jdi->enabled = false;

	return 0;
}

static int jdi_panel_unprepare(struct drm_panel *panel)
{
	struct jdi_panel *jdi = to_jdi_panel(panel);

	if (!jdi->prepared)
		return 0;

	jdi_panel_off(jdi);
	gpiod_set_value_cansleep(jdi->reset_gpio, 1);
	jdi->prepared = false;
	return 0;
}

static int jdi_panel_prepare(struct drm_panel *panel)
{
	struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;
	int ret;

	if (jdi->prepared)
		return 0;

	gpiod_set_value_cansleep(jdi->reset_gpio, 0);
	usleep_range(100, 120);
	gpiod_set_value_cansleep(jdi->reset_gpio, 1);
	usleep_range(80, 100);
	gpiod_set_value_cansleep(jdi->reset_gpio, 0);
	usleep_range(50, 100);


	ret = jdi_panel_init(jdi);
	if (ret < 0) {
		dev_err(dev, "failed to init panel: %d\n", ret);
		goto poweroff;
	}

	ret = jdi_panel_on(jdi);
	if (ret < 0) {
		dev_err(dev, "failed to set panel on: %d\n", ret);
		goto poweroff;
	}

	jdi->prepared = true;

	return 0;

poweroff:

	gpiod_set_value_cansleep(jdi->reset_gpio, 1);
	return ret;
}

static int jdi_panel_enable(struct drm_panel *panel)
{
	struct jdi_panel *jdi = to_jdi_panel(panel);

	if (jdi->enabled)
		return 0;
	gpiod_set_value_cansleep(jdi->bl_en_gpio, 1);
	jdi->enabled = true;

	return 0;
}

static const struct drm_display_mode default_mode = {

		.clock = 34653,
		.hdisplay = 720,
		.hsync_start = 720 + 21,
		.hsync_end = 720 + 21 + 2,
		.htotal = 720 + 21 + 2 + 24,

		.vdisplay = 720,
		.vsync_start = 720 + 25,
		.vsync_end = 720 + 25 + 5,
		.vtotal = 720 + 25 + 5 + 3,
		.flags = 0,
};

static int jdi_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct jdi_panel *jdi = to_jdi_panel(panel);
	struct device *dev = &jdi->dsi->dev;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_err(dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 51;
	connector->display_info.height_mm = 51;

	return 1;
}


static const struct drm_panel_funcs jdi_panel_funcs = {
	.disable = jdi_panel_disable,
	.unprepare = jdi_panel_unprepare,
	.prepare = jdi_panel_prepare,
	.enable = jdi_panel_enable,
	.get_modes = jdi_panel_get_modes,
};

static const struct of_device_id jdi_of_match[] = {
	{ .compatible = "jdi,lt031mdz4000", },
	{ }
};
MODULE_DEVICE_TABLE(of, jdi_of_match);

static int jdi_panel_add(struct jdi_panel *jdi)
{
	struct device *dev = &jdi->dsi->dev;
	int ret;

	jdi->mode = &default_mode;

	jdi->bl_en_gpio = devm_gpiod_get(dev, "bl-en", GPIOD_OUT_LOW);
	if (IS_ERR(jdi->bl_en_gpio)) {
		ret = PTR_ERR(jdi->bl_en_gpio);
		dev_err(dev, "cannot get enable-gpio %d\n", ret);
		return ret;
	}

	jdi->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(jdi->reset_gpio)) {
		ret = PTR_ERR(jdi->reset_gpio);
		dev_err(dev, "cannot get reset-gpios %d\n", ret);
		return ret;
	}

	drm_panel_init(&jdi->base, &jdi->dsi->dev, &jdi_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&jdi->base);

	return 0;
}

static void jdi_panel_del(struct jdi_panel *jdi)
{
	if (jdi->base.dev)
		drm_panel_remove(&jdi->base);
}

static int jdi_panel_probe(struct mipi_dsi_device *dsi)
{
	struct jdi_panel *jdi;
	int ret;

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
			   MIPI_DSI_CLOCK_NON_CONTINUOUS;

	jdi = devm_kzalloc(&dsi->dev, sizeof(*jdi), GFP_KERNEL);
	if (!jdi)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, jdi);

	jdi->dsi = dsi;

	ret = jdi_panel_add(jdi);
	if (ret < 0)
		return ret;

	return mipi_dsi_attach(dsi);
}

static int jdi_panel_remove(struct mipi_dsi_device *dsi)
{
	struct jdi_panel *jdi = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = jdi_panel_disable(&jdi->base);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to disable panel: %d\n", ret);

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "failed to detach from DSI host: %d\n",
			ret);

	jdi_panel_del(jdi);

	return 0;
}

static void jdi_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct jdi_panel *jdi = mipi_dsi_get_drvdata(dsi);

	jdi_panel_disable(&jdi->base);
}

static struct mipi_dsi_driver jdi_panel_driver = {
	.driver = {
		.name = "panel-jdi-lt031mdz4000",
		.of_match_table = jdi_of_match,
	},
	.probe = jdi_panel_probe,
	.remove = jdi_panel_remove,
	.shutdown = jdi_panel_shutdown,
};
module_mipi_dsi_driver(jdi_panel_driver);

MODULE_AUTHOR("JamWu <312023299@qq.com>");
MODULE_DESCRIPTION("JDI LT031MDZ4000 XGA");
MODULE_LICENSE("GPL v2");
