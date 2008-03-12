#ifndef _xvc_DBUS_SERVER_OBJECT__
#define _xvc_DBUS_SERVER_OBJECT__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <dbus/dbus-glib-bindings.h>
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
extern "C"
{
#endif     // __cplusplus

/* Standard GObject class structures, etc */
    typedef struct
    {
        GObjectClass parent_class;
        DBusGConnection *connection;
    } XvcServerObjectClass;

    typedef struct
    {
        GObject parent_instance;
    } XvcServerObject;

/*
 * function declarations
 */
    XvcServerObject *xvc_server_object_new ();
    GType xvc_server_object_get_type ();

    gboolean xvc_dbus_stop (XvcServerObject * server, GError ** error);
    gboolean xvc_dbus_start (XvcServerObject * server, GError ** error);
    gboolean xvc_dbus_pause (XvcServerObject * server, GError ** error);

/*
 * macros
 */
#define XVC_SERVER_OBJECT_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), xvc_server_object_get_type(), XvcServerObjectClass))
#define XVC_SERVER_OBJECT(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), xvc_server_object_get_type(), XvcServerObject))

#ifdef __cplusplus
}
#endif     // __cplusplus
#endif     // _xvc_DBUS_SERVER_OBJECT__
