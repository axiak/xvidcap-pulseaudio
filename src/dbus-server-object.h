#ifndef __DBUS_SERVER_OBJECT__
#define __DBUS_SERVER_OBJECT__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef __cplusplus
extern "C"
{
#endif     /* __cplusplus */

#define XVC_SERVER_OBJECT_GET_CLASS(object)  (G_TYPE_INSTANCE_GET_CLASS ((object), xvc_server_object_get_type(), XvcServerObjectClass))

/* Standard GObject class structures, etc */
typedef struct
{
        DBusGConnection *connection;
}XvcServerObjectClass;

typedef struct
{
	gboolean truth;
}XvcServerObject;


gboolean xvc_server_echo_string (XvcServerObject *server, gchar *original, gchar **echo, GError **error);


#ifdef __cplusplus
}
#endif     /* __cplusplus */
#endif // __DBUS_SERVER_OBJECT__

