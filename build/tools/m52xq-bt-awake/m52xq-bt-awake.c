/*
 * m52xq: the phone stays awake while a Bluetooth audio device is connected (the suspend of the BT UART cuts the link).
 * Holds the wake lock "m52xq-bt-audio" (Sailfish sleeps through /sys/power/autosleep).
 * And: while the screen is off and nothing is connected the Bluetooth controller is soft-blocked (its UART wakes the phone at once:
 * 2026-11-24 535 of 535 wake-ups, with Bluetooth off 90 % of the time asleep); the screen on gives it back.
 * System bus: org.bluez, com.nokia.mce. /dev/rfkill.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <gio/gio.h>
#include <glib-unix.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/rfkill.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LOCK "m52xq-bt-audio"
#define RELEASE_SECONDS 10	/* a short loss of the link does not let the phone sleep */

static GDBusConnection *bus;
static GMainLoop *loop;
static gboolean held;
static gboolean screen_off, blocked_by_us, anything_connected, bluez_present;
static guint block_id;
#define BLOCK_DELAY_MS 1500	/* the screen may come back on at once */
#define FLAG "/run/m52xq-bt-sleep-blocked"
static guint query_id, release_id;

static void write_lock(const char *file)
{
	FILE *f = fopen(file, "w");

	if (!f || fputs(LOCK, f) < 0)
		g_printerr("m52xq-bt-awake: %s: %m\n", file);
	if (f)
		fclose(f);
}

static void hold(void)
{
	if (release_id) {
		g_source_remove(release_id);
		release_id = 0;
	}
	if (!held) {
		held = TRUE;
		write_lock("/sys/power/wake_lock");
		g_print("audio device connected: awake\n");
	}
}

static gboolean release(gpointer data)
{
	(void)data;
	release_id = 0;
	if (held) {
		held = FALSE;
		write_lock("/sys/power/wake_unlock");
		g_print("no audio device: sleep allowed\n");
	}
	return G_SOURCE_REMOVE;
}

static int rfkill_set(gboolean block)
{
	struct rfkill_event ev;
	int fd = open("/dev/rfkill", O_RDWR | O_CLOEXEC), ret = -1;

	if (fd < 0) {
		g_printerr("m52xq-bt-awake: /dev/rfkill: %m\n");
		return -1;
	}
	memset(&ev, 0, sizeof(ev));
	ev.op = RFKILL_OP_CHANGE_ALL;
	ev.type = RFKILL_TYPE_BLUETOOTH;
	ev.soft = block;
	if (write(fd, &ev, RFKILL_EVENT_SIZE_V1) > 0)
		ret = 0;
	else
		g_printerr("m52xq-bt-awake: rfkill %s: %m\n", block ? "block" : "unblock");
	close(fd);
	return ret;
}

/* is any Bluetooth device soft-blocked or not (1 = not blocked, 0 = blocked/none) */
static int bt_unblocked(void)
{
	struct rfkill_event ev;
	int fd = open("/dev/rfkill", O_RDONLY | O_NONBLOCK | O_CLOEXEC), r = 0;

	if (fd < 0)
		return 0;
	while (read(fd, &ev, sizeof(ev)) >= RFKILL_EVENT_SIZE_V1)
		if (ev.type == RFKILL_TYPE_BLUETOOTH && !ev.soft && !ev.hard)
			r = 1;
	close(fd);
	return r;
}

static void flag_set(gboolean on)
{
	if (on) {
		FILE *f = fopen(FLAG, "w");
		if (f)
			fclose(f);
	} else {
		unlink(FLAG);
	}
}

static void bt_unblock(void)
{
	if (!blocked_by_us)
		return;
	blocked_by_us = FALSE;
	flag_set(FALSE);
	rfkill_set(FALSE);
	g_print("screen on: Bluetooth back\n");
}

static gboolean bt_block(gpointer data)
{
	(void)data;
	block_id = 0;
	if (!screen_off || anything_connected || blocked_by_us || !bt_unblocked())
		return G_SOURCE_REMOVE;
	if (!rfkill_set(TRUE)) {
		blocked_by_us = TRUE;
		flag_set(TRUE);
		g_print("screen off, nothing connected: Bluetooth off\n");
	}
	return G_SOURCE_REMOVE;
}

static void screen_set(gboolean off)
{
	if (screen_off == off)
		return;
	screen_off = off;
	if (block_id) {
		g_source_remove(block_id);
		block_id = 0;
	}
	if (off)
		block_id = g_timeout_add(BLOCK_DELAY_MS, bt_block, NULL);
	else
		bt_unblock();
}

static void on_display(GDBusConnection *c, const gchar *sender, const gchar *path, const gchar *iface, const gchar *signal,
		       GVariant *params, gpointer data)
{
	const gchar *state;

	(void)c; (void)sender; (void)path; (void)iface; (void)signal; (void)data;
	g_variant_get(params, "(&s)", &state);
	screen_set(!strcmp(state, "off"));
}

static void on_display_query(GObject *src, GAsyncResult *res, gpointer data)
{
	GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, NULL);
	const gchar *state;

	(void)data;
	if (!r)
		return;
	g_variant_get(r, "(&s)", &state);
	screen_set(!strcmp(state, "off"));
	g_variant_unref(r);
}

static gboolean is_audio(GVariant *props)
{
	GVariant *uuids = g_variant_lookup_value(props, "UUIDs", G_VARIANT_TYPE("as"));
	gboolean audio = FALSE;
	const gchar *u;
	GVariantIter it;

	if (!uuids)
		return FALSE;
	g_variant_iter_init(&it, uuids);
	while (g_variant_iter_next(&it, "&s", &u))	/* A2DP sink, HFP hands-free, HSP headset */
		if (g_str_has_prefix(u, "0000110b-") || g_str_has_prefix(u, "0000111e-") || g_str_has_prefix(u, "00001108-"))
			audio = TRUE;
	g_variant_unref(uuids);
	return audio;
}

static void on_objects(GObject *src, GAsyncResult *res, gpointer data)
{
	GVariant *r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, NULL);
	gboolean connected = FALSE, any = FALSE, flag;
	GVariantIter *objects, *ifaces;
	const gchar *path, *name;
	GVariant *props;

	(void)data;
	if (r) {
		g_variant_get(r, "(a{oa{sa{sv}}})", &objects);
		while (g_variant_iter_next(objects, "{&oa{sa{sv}}}", &path, &ifaces)) {
			while (g_variant_iter_next(ifaces, "{&s@a{sv}}", &name, &props)) {
				if (!strcmp(name, "org.bluez.Device1") && g_variant_lookup(props, "Connected", "b", &flag)
				    && flag) {
					any = TRUE;
					if (is_audio(props))
						connected = TRUE;
				}
				g_variant_unref(props);
			}
			g_variant_iter_free(ifaces);
		}
		g_variant_iter_free(objects);
		g_variant_unref(r);
	}
	anything_connected = any;
	if (connected)
		hold();
	else if (held && !release_id)
		release_id = g_timeout_add_seconds(RELEASE_SECONDS, release, NULL);
}

static gboolean query(gpointer data)
{
	(void)data;
	query_id = 0;
	g_dbus_connection_call(bus, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
			       G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NO_AUTO_START, 5000, NULL, on_objects, NULL);
	return G_SOURCE_REMOVE;
}

static void later(void)
{
	if (!query_id)
		query_id = g_timeout_add(200, query, NULL);
}

static void on_props(GDBusConnection *c, const gchar *sender, const gchar *path, const gchar *iface, const gchar *signal,
		     GVariant *params, gpointer data)
{
	const gchar *changed_iface = NULL;
	gboolean flag, changed;
	GVariant *dict;

	(void)c; (void)sender; (void)path; (void)iface; (void)signal; (void)data;
	g_variant_get(params, "(&s@a{sv}as)", &changed_iface, &dict, NULL);
	changed = !strcmp(changed_iface, "org.bluez.Device1") && g_variant_lookup(dict, "Connected", "b", &flag);
	g_variant_unref(dict);
	if (changed)
		later();
}

static void on_objmgr(GDBusConnection *c, const gchar *sender, const gchar *path, const gchar *iface, const gchar *signal,
		      GVariant *params, gpointer data)
{
	(void)c; (void)sender; (void)path; (void)iface; (void)signal; (void)params; (void)data;
	later();
}

static void on_name(GDBusConnection *c, const gchar *name, const gchar *owner, gpointer data)
{
	(void)c; (void)name; (void)owner; (void)data;
	bluez_present = TRUE;
	later();
}

static void on_gone(GDBusConnection *c, const gchar *name, gpointer data)
{
	(void)c; (void)name; (void)data;
	bluez_present = FALSE;
	anything_connected = FALSE;
	later();
}

static gboolean stop(gpointer data)
{
	(void)data;
	g_main_loop_quit(loop);
	return G_SOURCE_CONTINUE;
}

int main(void)
{
	GError *error = NULL;

	setvbuf(stdout, NULL, _IOLBF, 0);
	bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!bus) {
		g_printerr("m52xq-bt-awake: %s\n", error->message);
		return 1;
	}
	loop = g_main_loop_new(NULL, FALSE);
	g_dbus_connection_signal_subscribe(bus, "org.bluez", "org.freedesktop.DBus.Properties", "PropertiesChanged", NULL,
					   NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_props, NULL, NULL);
	g_dbus_connection_signal_subscribe(bus, "org.bluez", "org.freedesktop.DBus.ObjectManager", NULL, NULL,
					   NULL, G_DBUS_SIGNAL_FLAGS_NONE, on_objmgr, NULL, NULL);
	g_bus_watch_name_on_connection(bus, "org.bluez", G_BUS_NAME_WATCHER_FLAGS_NONE, on_name, on_gone, NULL, NULL);
	g_dbus_connection_signal_subscribe(bus, NULL, "com.nokia.mce.signal", "display_status_ind", NULL, NULL,
					   G_DBUS_SIGNAL_FLAGS_NONE, on_display, NULL, NULL);
	g_dbus_connection_call(bus, "com.nokia.mce", "/com/nokia/mce/request", "com.nokia.mce.request", "get_display_status", NULL,
			       G_VARIANT_TYPE("(s)"), G_DBUS_CALL_FLAGS_NONE, 5000, NULL, on_display_query, NULL);
	if (access(FLAG, F_OK) == 0) {	/* a previous run ended while it held Bluetooth blocked */
		blocked_by_us = TRUE;
		bt_unblock();
	}
	g_unix_signal_add(SIGTERM, stop, NULL);
	g_unix_signal_add(SIGINT, stop, NULL);
	g_main_loop_run(loop);
	if (held)
		write_lock("/sys/power/wake_unlock");
	bt_unblock();
	return 0;
}
