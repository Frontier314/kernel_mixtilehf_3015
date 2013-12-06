/**
 * Copyright (C) ARM Limited 2010-2012. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "gator.h"
#include <linux/netdevice.h>

#define NETRX		0
#define NETTX		1
#define TOTALNET	2

static ulong netrx_enabled;
static ulong nettx_enabled;
static ulong netrx_key;
static ulong nettx_key;
static int rx_total, tx_total;
static ulong netPrev[TOTALNET];
static int netGet[TOTALNET * 2];

static void get_network_stats(struct work_struct *wsptr) {
	int rx = 0, tx = 0;
	struct net_device *dev;

	for_each_netdev(&init_net, dev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
		const struct net_device_stats *stats = dev_get_stats(dev);
#else
		struct rtnl_link_stats64 temp;
		const struct rtnl_link_stats64 *stats = dev_get_stats(dev, &temp);
#endif
		rx += stats->rx_bytes;
		tx += stats->tx_bytes;
	}
	rx_total = rx;
	tx_total = tx;
}
DECLARE_WORK(wq_get_stats, get_network_stats);

static void calculate_delta(int *rx, int *tx)
{
	int rx_calc, tx_calc;

	rx_calc = (int)(rx_total - netPrev[NETRX]);
	if (rx_calc < 0)
		rx_calc = 0;
	netPrev[NETRX] += rx_calc;

	tx_calc = (int)(tx_total - netPrev[NETTX]);
	if (tx_calc < 0)
		tx_calc = 0;
	netPrev[NETTX] += tx_calc;

	*rx = rx_calc;
	*tx = tx_calc;
}

static int gator_events_net_create_files(struct super_block *sb, struct dentry *root)
{
	struct dentry *dir;

	dir = gatorfs_mkdir(sb, root, "Linux_net_rx");
	if (!dir) {
		return -1;
	}
	gatorfs_create_ulong(sb, dir, "enabled", &netrx_enabled);
	gatorfs_create_ro_ulong(sb, dir, "key", &netrx_key);

	dir = gatorfs_mkdir(sb, root, "Linux_net_tx");
	if (!dir) {
		return -1;
	}
	gatorfs_create_ulong(sb, dir, "enabled", &nettx_enabled);
	gatorfs_create_ro_ulong(sb, dir, "key", &nettx_key);

	return 0;
}

static int gator_events_net_start(void)
{
	get_network_stats(NULL);
	netPrev[NETRX] = rx_total;
	netPrev[NETTX] = tx_total;
	return 0;
}

static void gator_events_net_stop(void)
{
	netrx_enabled = 0;
	nettx_enabled = 0;
}

static int gator_events_net_read(int **buffer)
{
	int len, rx_delta, tx_delta;
	static int last_rx_delta = 0, last_tx_delta = 0;

	if (smp_processor_id() != 0)
		return 0;

	schedule_work(&wq_get_stats);
	calculate_delta(&rx_delta, &tx_delta);

	len = 0;
	if (netrx_enabled && last_rx_delta != rx_delta) {
		last_rx_delta = rx_delta;
		netGet[len++] = netrx_key;
		netGet[len++] = rx_delta;
	}

	if (nettx_enabled && last_tx_delta != tx_delta) {
		last_tx_delta = tx_delta;
		netGet[len++] = nettx_key;
		netGet[len++] = tx_delta;
	}

	if (buffer)
		*buffer = netGet;

	return len;
}

static struct gator_interface gator_events_net_interface = {
	.create_files = gator_events_net_create_files,
	.start = gator_events_net_start,
	.stop = gator_events_net_stop,
	.read = gator_events_net_read,
};

int gator_events_net_init(void)
{
	netrx_key = gator_events_get_key();
	nettx_key = gator_events_get_key();

	netrx_enabled = 0;
	nettx_enabled = 0;

	return gator_events_install(&gator_events_net_interface);
}
gator_events_init(gator_events_net_init);
