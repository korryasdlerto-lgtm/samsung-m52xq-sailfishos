/*
 * m52xq: the flashlight of the top menu toggle of Sailfish (jolla-settings-system-flashlight) through the LED of the mainline kernel.
 * Same D-Bus interface as droidmedia-flashlight: org.sailfish.flashlight.provider on the session bus.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>

#define NAME "org.sailfish.flashlight.provider"
#define PATH "/org/sailfish/flashlight/provider"
#define LED "/sys/class/leds/white:flash/"
#define IDLE_SECONDS 120

static const char *xml =
	"<node><interface name='" NAME "'>"
	"<property name='flashlightOn' type='b' access='read'/>"
	"<method name='toggleFlashlight'><arg type='b' direction='out'/></method>"
	"<method name='setFlashlight'><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
	"<signal name='flashlightOnChanged'><arg name='newOn' type='b'/></signal>"
	"</interface></node>";

static GMainLoop *loop;
static guint idle_id;
static int exit_code;

static int led_read(const char *file)
{
	char *path = g_strconcat(LED, file, NULL), *text = NULL;
	int value = -1;

	if (g_file_get_contents(path, &text, NULL, NULL))
		value = atoi(text);
	g_free(text);
	g_free(path);
	return value;
}

static gboolean led_is_on(void)
{
	return led_read("brightness") > 0;
}

static gboolean led_set(gboolean on)
{
	int max = led_read("max_brightness");
	FILE *f = fopen(LED "brightness", "w");
	gboolean ok;

	if (!f) {
		g_printerr("m52xq-flashlight: cannot open the LED: %m\n");
		return FALSE;
	}
	ok = fprintf(f, "%d\n", on ? MAX(max, 1) : 0) > 0;
	if (fclose(f))
		ok = FALSE;
	if (!ok)
		g_printerr("m52xq-flashlight: cannot set the LED: %m\n");
	return ok;
}

static gboolean idle_quit(gpointer data)
{
	(void)data;
	if (led_is_on())	/* keep running while the light is on */
		return G_SOURCE_CONTINUE;
	idle_id = 0;
	g_main_loop_quit(loop);
	return G_SOURCE_REMOVE;
}

static void arm_idle(void)
{
	if (idle_id)
		g_source_remove(idle_id);
	idle_id = g_timeout_add_seconds(IDLE_SECONDS, idle_quit, NULL);
}

static void on_call(GDBusConnection *bus, const gchar *sender, const gchar *path, const gchar *iface,
		    const gchar *method, GVariant *args, GDBusMethodInvocation *inv, gpointer data)
{
	gboolean was = led_is_on(), now;
	GVariantBuilder changed, none;

	(void)sender; (void)path; (void)iface; (void)data;
	if (g_strcmp0(method, "setFlashlight") == 0) {	/* the camera flash: a fixed state */
		gboolean want;

		g_variant_get(args, "(b)", &want);
		if (want != was)
			led_set(want);
	} else {
		led_set(!was);
	}
	now = led_is_on();
	arm_idle();
	g_dbus_method_invocation_return_value(inv, g_variant_new("(b)", now != was));
	if (now == was)
		return;
	g_dbus_connection_emit_signal(bus, NULL, PATH, NAME, "flashlightOnChanged", g_variant_new("(b)", now), NULL);
	g_variant_builder_init(&changed, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&changed, "{sv}", "flashlightOn", g_variant_new_boolean(now));
	g_variant_builder_init(&none, G_VARIANT_TYPE("as"));
	g_dbus_connection_emit_signal(bus, NULL, PATH, "org.freedesktop.DBus.Properties", "PropertiesChanged",
				      g_variant_new("(sa{sv}as)", NAME, &changed, &none), NULL);
}

static GVariant *on_get(GDBusConnection *bus, const gchar *sender, const gchar *path, const gchar *iface,
			const gchar *prop, GError **error, gpointer data)
{
	(void)bus; (void)sender; (void)path; (void)iface; (void)prop; (void)error; (void)data;
	arm_idle();
	return g_variant_new_boolean(led_is_on());
}

static const GDBusInterfaceVTable vtable = { on_call, on_get, NULL, { 0 } };

static void on_bus(GDBusConnection *bus, const gchar *name, gpointer data)
{
	GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, NULL);

	(void)name; (void)data;
	if (!g_dbus_connection_register_object(bus, PATH, info->interfaces[0], &vtable, NULL, NULL, NULL)) {
		exit_code = 1;
		g_main_loop_quit(loop);
	}
}

static void on_lost(GDBusConnection *bus, const gchar *name, gpointer data)
{
	(void)bus; (void)name; (void)data;
	exit_code = 1;
	g_main_loop_quit(loop);
}

int main(void)
{
	loop = g_main_loop_new(NULL, FALSE);
	g_bus_own_name(G_BUS_TYPE_SESSION, NAME, G_BUS_NAME_OWNER_FLAGS_NONE, on_bus, NULL, on_lost, NULL, NULL);
	arm_idle();
	g_main_loop_run(loop);
	if (led_is_on())	/* never leave the light on without a process that can switch it off */
		led_set(FALSE);
	return exit_code;
}
