/**
 * \file xvidcap-intl.h
 */
#ifndef _xvc_XVIDCAP_INTL_H__
#define _xvc_XVIDCAP_INTL_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif     /* HAVE_CONFIG_H */
#endif     // DOXYGEN_SHOULD_SKIP_THIS

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else      /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain)
#define bind_textdomain_codeset(Domain,Codeset) (Codeset)
#endif     /* ENABLE_NLS */

#endif     /* _xvc_XVIDCAP_INTL_H__ */
