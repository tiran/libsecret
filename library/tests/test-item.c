/* GSecret - GLib wrapper for Secret Service
 *
 * Copyright 2012 Red Hat Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Stef Walter <stefw@gnome.org>
 */


#include "config.h"

#include "gsecret-item.h"
#include "gsecret-service.h"
#include "gsecret-private.h"

#include "mock-service.h"

#include "egg/egg-testing.h"

#include <glib.h>

#include <errno.h>
#include <stdlib.h>

typedef struct {
	GDBusConnection *connection;
	GSecretService *service;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
	GError *error = NULL;
	const gchar *mock_script = data;

	mock_service_start (mock_script, &error);
	g_assert_no_error (error);

	test->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	g_assert_no_error (error);

	test->service = _gsecret_service_bare_instance (test->connection, NULL);
}

static void
teardown (Test *test,
          gconstpointer unused)
{
	GError *error = NULL;

	g_object_unref (test->service);
	egg_assert_not_object (test->service);

	mock_service_stop ();

	g_dbus_connection_flush_sync (test->connection, NULL, &error);
	g_assert_no_error (error);
	g_object_unref (test->connection);
}

static void
on_async_result (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
	GAsyncResult **ret = user_data;
	g_assert (ret != NULL);
	g_assert (*ret == NULL);
	*ret = g_object_ref (result);
	egg_test_wait_stop ();
}

static void
on_notify_stop (GObject *obj,
                GParamSpec *spec,
                gpointer user_data)
{
	guint *sigs = user_data;
	g_assert (sigs != NULL);
	g_assert (*sigs > 0);
	if (--(*sigs) == 0)
		egg_test_wait_stop ();
g_printerr ("sigs: %u\n", *sigs);
}

static void
test_new_sync (Test *test,
               gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	g_assert_cmpstr (g_dbus_proxy_get_object_path (G_DBUS_PROXY (item)), ==, item_path);

	g_object_unref (item);
}

static void
test_new_async (Test *test,
               gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GSecretItem *item;

	gsecret_item_new (test->service, item_path, NULL, on_async_result, &result);
	g_assert (result == NULL);

	egg_test_wait ();

	item = gsecret_item_new_finish (result, &error);
	g_assert_no_error (error);
	g_object_unref (result);

	g_assert_cmpstr (g_dbus_proxy_get_object_path (G_DBUS_PROXY (item)), ==, item_path);

	g_object_unref (item);
}

static void
test_properties (Test *test,
                 gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GHashTable *attributes;
	GSecretItem *item;
	guint64 created;
	guint64 modified;
	gboolean locked;
	gchar *label;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	g_assert (gsecret_item_get_locked (item) == FALSE);
	g_assert_cmpuint (gsecret_item_get_created (item), <=, time (NULL));
	g_assert_cmpuint (gsecret_item_get_modified (item), <=, time (NULL));

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Item One");
	g_free (label);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "one");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "1");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "parity"), ==, "odd");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 3);
	g_hash_table_unref (attributes);

	g_object_get (item,
	              "locked", &locked,
	              "created", &created,
	              "modified", &modified,
	              "label", &label,
	              "attributes", &attributes,
	              NULL);

	g_assert (locked == FALSE);
	g_assert_cmpuint (created, <=, time (NULL));
	g_assert_cmpuint (modified, <=, time (NULL));

	g_assert_cmpstr (label, ==, "Item One");
	g_free (label);

	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "one");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "1");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "parity"), ==, "odd");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 3);
	g_hash_table_unref (attributes);

	g_object_unref (item);
}

static void
test_set_label_sync (Test *test,
                     gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	gboolean ret;
	gchar *label;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Item One");
	g_free (label);

	ret = gsecret_item_set_label_sync (item, "Another label", NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Another label");
	g_free (label);

	g_object_unref (item);
}

static void
test_set_label_async (Test *test,
                      gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GSecretItem *item;
	gboolean ret;
	gchar *label;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Item One");
	g_free (label);

	gsecret_item_set_label (item, "Another label", NULL, on_async_result, &result);
	g_assert (result == NULL);

	egg_test_wait ();

	ret = gsecret_item_set_label_finish (item, result, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);
	g_object_unref (result);

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Another label");
	g_free (label);

	g_object_unref (item);
}

static void
test_set_label_prop (Test *test,
                     gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	guint sigs = 2;
	gchar *label;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Item One");
	g_free (label);

	g_signal_connect (item, "notify::label", G_CALLBACK (on_notify_stop), &sigs);
	g_object_set (item, "label", "Blah blah", NULL);

	/* Wait for the property to actually 'take' */
	egg_test_wait ();

	label = gsecret_item_get_label (item);
	g_assert_cmpstr (label, ==, "Blah blah");
	g_free (label);

	g_object_unref (item);
}

static void
test_set_attributes_sync (Test *test,
                           gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	gboolean ret;
	GHashTable *attributes;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "one");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "1");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "parity"), ==, "odd");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 3);
	g_hash_table_unref (attributes);

	attributes = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (attributes, "string", "five");
	g_hash_table_insert (attributes, "number", "5");
	ret = gsecret_item_set_attributes_sync (item, attributes, NULL, &error);
	g_hash_table_unref (attributes);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "five");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "5");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 2);
	g_hash_table_unref (attributes);

	g_object_unref (item);
}

static void
test_set_attributes_async (Test *test,
                           gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GHashTable *attributes;
	GError *error = NULL;
	GAsyncResult *result = NULL;
	GSecretItem *item;
	gboolean ret;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "one");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "1");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "parity"), ==, "odd");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 3);
	g_hash_table_unref (attributes);

	attributes = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (attributes, "string", "five");
	g_hash_table_insert (attributes, "number", "5");
	gsecret_item_set_attributes (item, attributes, NULL, on_async_result, &result);
	g_assert (result == NULL);

	egg_test_wait ();

	ret = gsecret_item_set_attributes_finish (item, result, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);
	g_object_unref (result);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "five");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "5");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 2);
	g_hash_table_unref (attributes);

	g_object_unref (item);
}

static void
test_set_attributes_prop (Test *test,
                          gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	GHashTable *attributes;
	guint sigs = 2;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "one");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "1");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "parity"), ==, "odd");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 3);
	g_hash_table_unref (attributes);

	g_signal_connect (item, "notify::attributes", G_CALLBACK (on_notify_stop), &sigs);

	attributes = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (attributes, "string", "five");
	g_hash_table_insert (attributes, "number", "5");
	g_object_set (item, "attributes", attributes, NULL);
	g_hash_table_unref (attributes);

	/* Wait for the property to actually 'take' */
	egg_test_wait ();

	attributes = gsecret_item_get_attributes (item);
	g_assert_cmpstr (g_hash_table_lookup (attributes, "string"), ==, "five");
	g_assert_cmpstr (g_hash_table_lookup (attributes, "number"), ==, "5");
	g_assert_cmpuint (g_hash_table_size (attributes), ==, 2);
	g_hash_table_unref (attributes);

	g_object_unref (item);
}

static void
test_get_secret_sync (Test *test,
                      gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	GSecretValue *value;
	gconstpointer data;
	gsize length;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	value = gsecret_item_get_secret_sync (item, NULL, &error);
	g_assert_no_error (error);
	g_assert (value != NULL);

	data = gsecret_value_get (value, &length);
	egg_assert_cmpmem (data, length, ==, "uno", 3);

	gsecret_value_unref (value);

	g_object_unref (item);
}

static void
test_get_secret_async (Test *test,
                       gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GSecretItem *item;
	GSecretValue *value;
	gconstpointer data;
	gsize length;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	gsecret_item_get_secret (item, NULL, on_async_result, &result);
	g_assert (result == NULL);

	egg_test_wait ();

	value = gsecret_item_get_secret_finish (item, result, &error);
	g_assert_no_error (error);
	g_assert (value != NULL);
	g_object_unref (result);

	data = gsecret_value_get (value, &length);
	egg_assert_cmpmem (data, length, ==, "uno", 3);

	gsecret_value_unref (value);

	g_object_unref (item);
}

static void
test_delete_sync (Test *test,
                  gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GError *error = NULL;
	GSecretItem *item;
	gboolean ret;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	ret = gsecret_item_delete_sync (item, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_object_unref (item);

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
	g_assert (item == NULL);
}

static void
test_delete_async (Test *test,
                   gconstpointer unused)
{
	const gchar *item_path = "/org/freedesktop/secrets/collection/collection/item_one";
	GAsyncResult *result = NULL;
	GError *error = NULL;
	GSecretItem *item;
	gboolean ret;

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_no_error (error);

	gsecret_item_delete (item, NULL, on_async_result, &result);
	g_assert (result == NULL);

	egg_test_wait ();

	ret = gsecret_item_delete_finish (item, result, &error);
	g_assert_no_error (error);
	g_assert (ret == TRUE);

	g_object_unref (item);

	item = gsecret_item_new_sync (test->service, item_path, NULL, &error);
	g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
	g_assert (item == NULL);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_set_prgname ("test-item");
	g_type_init ();

	g_test_add ("/item/new-sync", Test, "mock-service-normal.py", setup, test_new_sync, teardown);
	g_test_add ("/item/new-async", Test, "mock-service-normal.py", setup, test_new_async, teardown);
	g_test_add ("/item/properties", Test, "mock-service-normal.py", setup, test_properties, teardown);
	g_test_add ("/item/set-label-sync", Test, "mock-service-normal.py", setup, test_set_label_sync, teardown);
	g_test_add ("/item/set-label-async", Test, "mock-service-normal.py", setup, test_set_label_async, teardown);
	g_test_add ("/item/set-attributes-sync", Test, "mock-service-normal.py", setup, test_set_attributes_sync, teardown);
	g_test_add ("/item/set-attributes-async", Test, "mock-service-normal.py", setup, test_set_attributes_async, teardown);
	g_test_add ("/item/get-secret-sync", Test, "mock-service-normal.py", setup, test_get_secret_sync, teardown);
	g_test_add ("/item/get-secret-async", Test, "mock-service-normal.py", setup, test_get_secret_async, teardown);
	g_test_add ("/item/delete-sync", Test, "mock-service-normal.py", setup, test_delete_sync, teardown);
	g_test_add ("/item/delete-async", Test, "mock-service-normal.py", setup, test_delete_async, teardown);

	g_test_add ("/item/set-attributes-prop", Test, "mock-service-normal.py", setup, test_set_attributes_prop, teardown);
	g_test_add ("/item/set-label-prop", Test, "mock-service-normal.py", setup, test_set_label_prop, teardown);

	return egg_tests_run_with_loop ();
}
