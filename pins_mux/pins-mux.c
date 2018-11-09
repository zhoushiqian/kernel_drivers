#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/machine.h>
#include <linux/platform_device.h>

enum pin_state {
	GPIO = 0,
	UART = 1,
	SPI = 2,
	MAX
};

struct pins_ctrl {
	struct pinctrl * pinctrl;
	struct pinctrl_state ** states;
	int state_count;
	enum pin_state state;
	//struct mutex lock;
};


static ssize_t pin_mux_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pins_ctrl *ctrl = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", ctrl->state);
}

static ssize_t pin_mux_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	int n;
	struct pins_ctrl *ctrl = dev_get_drvdata(dev);

	sscanf(buf, "%du", &n);
	if (n >= MAX) {
		dev_info(dev, "func too big\n");
		return count;
	}
	ctrl->state = n;
	pr_info("%s state %d\n", __func__, ctrl->state);

	dev_info(dev, "state no  %d pinctr %p state%p\n", ctrl->state, ctrl->pinctrl, ctrl->states[ctrl->state]);
	pinctrl_select_state(ctrl->pinctrl, ctrl->states[ctrl->state]);
	return count;
}


static DEVICE_ATTR(pin_mux, 0644, pin_mux_show,
				           pin_mux_store);


static int pins_init_sysfs(struct platform_device *pdev)
{
	int ret = -EINVAL;

	ret = device_create_file(&pdev->dev, &dev_attr_pin_mux);
	if (ret) {
		dev_err(&pdev->dev, "%s: sysfs create group failed %d\n",
							__func__, ret);
		goto error_return;
	}
	return 0;

error_return:
	return ret;
}

static int pins_parse_dt(struct device *dev, struct pins_ctrl *ctrl)
{
	int ret = 0;
	int state = 0;
	const char *statename;
	struct device_node *np = dev->of_node;
	ret = of_property_count_strings(np, "pinctrl-names");
	ctrl->state_count = ret;
	if (ret < 0) {
		dev_err(dev, "Look up states error\n");
		return -EFAULT;
	}
	dev_info(dev, "state count %d\n", ctrl->state_count);

	ctrl->states = devm_kmalloc(dev, sizeof(struct pinctrl_state *) * ctrl->state_count, GFP_KERNEL);
	while (state < ctrl->state_count) {
		ret = of_property_read_string_index(np, "pinctrl-names",
						    state, &statename);
		ctrl->states[state] = pinctrl_lookup_state(ctrl->pinctrl, statename);
		if (IS_ERR(ctrl->states[state])) {
			pr_err("%s: could not get %s pinstate\n", __func__, statename);
		}
		dev_info(dev, "state name  %s %p\n", statename, ctrl->states[state]);
		state++;
	}

	return ret;
}

static int pins_probe(struct platform_device *pdev)
{
	struct pins_ctrl *ctrl;
	int ret;
	ctrl = devm_kzalloc(&pdev->dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl) {
		dev_err(&pdev->dev, "%s: memory alloc failed\n", __func__);
		ret = -ENOMEM;
		return ret;
	}
	dev_set_drvdata(&pdev->dev, ctrl);
	ctrl->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctrl->pinctrl)) {
		dev_err(&pdev->dev, "%s Unable get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	ret = pins_parse_dt(&pdev->dev, ctrl);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s:dt parse failed %d\n",
							__func__, ret);
		goto error_return;
	}
	ret = pins_init_sysfs(pdev);
	return 0;

error_return:
	return ret;
}

static int pins_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id pins_dt_match[] = {
	{ .compatible = "pins-mux" },
	{ }
};
MODULE_DEVICE_TABLE(of, pins_dt_match);

static struct platform_driver pins_driver = {
	.driver = {
		.name = "pins-mux",
		.owner = THIS_MODULE,
		.of_match_table = pins_dt_match,
	},
	.probe = pins_probe,
	.remove = pins_remove,
};

static int __init pins_init(void)
{
	return platform_driver_register(&pins_driver);
}
module_init(pins_init);

static void __exit pins_exit(void)
{
	platform_driver_unregister(&pins_driver);
}
module_exit(pins_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("zhoushiqian@gmail.com");
