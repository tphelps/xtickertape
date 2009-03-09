/***************************************************************

  Copyright (C) 1997-2009 by Mantara Software (ABN 17 105 665 594).
  All Rights Reserved.

  The software contained on this media is the property of the
  DSTC Pty Ltd.  Use of this software is strictly in accordance
  with the license agreement in the accompanying LICENSE.DOC
  file.  If your distribution of this software does not contain
  a LICENSE.DOC file then you have no rights to use this
  software in any manner and should contact DSTC at the address
  below to determine an appropriate licensing arrangement.

     DSTC Pty Ltd
     Level 7, GP South
     University of Queensland
     St Lucia, 4072
     Australia
     Tel: +61 7 3365 4310
     Fax: +61 7 3365 4311
     Email: enquiries@dstc.edu.au

  This software is being provided "AS IS" without warranty of
  any kind.  In no event shall DSTC Pty Ltd be liable for
  damage of any kind arising out of or in connection with
  the use or performance of this software.

****************************************************************/

#ifndef lint
static const char cvsid[] = "$Id: main.c,v 1.123 2009/03/09 05:26:27 phelps Exp $";
#endif /* lint */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h> /* fprintf */
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* exit, getenv */
#endif
#ifdef HAVE_STRING_H
#include <string.h> /* strdup */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* getopt */
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h> /* getopt_long */
#endif
#ifdef HAVE_PWD_H
#include <pwd.h> /* getpwuid */
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h> /* signal */
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h> /* gethostbyname */
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h> /* uname */
#endif
#include <X11/Xlib.h> /* X.* */
#include <X11/Intrinsic.h> /* Xt* */
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/extensions/shape.h>
#include <X11/Xmu/Editres.h>
#include <elvin/elvin.h>
#include <elvin/xt_mainloop.h>
#include "globals.h"
#include "replace.h"
#include "message.h"
#include "tickertape.h"

#include "red.xbm"
#include "white.xbm"
#include "mask.xbm"

#define DEFAULT_DOMAIN "no.domain"

#if defined(HAVE_GETOPT_LONG)
/* The list of long options */
static struct option long_options[] =
{
    { "elvin", required_argument, NULL, 'e' },
    { "scope", required_argument, NULL, 'S' },
    { "proxy", required_argument, NULL, 'H' },
    { "idle", required_argument, NULL, 'I' },
    { "user", required_argument, NULL, 'u' },
    { "domain" , required_argument, NULL, 'D' },
#if defined(ENABLE_LISP_INTERPRETER)
    { "config", required_argument, NULL, 'c' },
#endif
    { "ticker-dir", required_argument, NULL, 'T' },
    { "groups", required_argument, NULL, 'G' },
    { "usenet", required_argument, NULL, 'U' },
    { "keys", required_argument, NULL, 'K' },
    { "keys-dir", required_argument, NULL, 'k' },
    { "version", no_argument, NULL, 'v' },
    { "help", no_argument, NULL, 'h' },
    { NULL, no_argument, NULL, '\0' }
};
#endif /* GETOPT_LONG */

#if defined(ENABLE_LISP_INTERPRETER)
#define OPTIONS "e:c:D:G:H:hI:K:k:S:T:u:U:v"
#else
#define OPTIONS "e:D:G:H:hI:K:k:S:T:u:U:v"
#endif

#if defined(HAVE_GETOPT_LONG)
/* Print out usage message */
static void usage(int argc, char *argv[])
{
    fprintf(
	stderr,
	"usage: %s [OPTION]...\n"
	"  -e elvin-url,   --elvin=elvin-url\n"
	"  -S scope,       --scope=scope\n"
	"  -H http-proxy,  --proxy=http-proxy\n"
	"  -I idle-period, --idle=idle-period\n"
	"  -u user,        --user=user\n"
	"  -D domain,      --domain=domain\n"
	"  -T ticker-dir,  --ticker-dir=ticker-dir\n"
#if defined(ENABLE_LISP_INTERPRETER)
	"  -c config-file, --config=config-file\n"
#endif
	"  -G groups-file, --groups=groups-file\n"
	"  -U usenet-file, --usenet=usenet-file\n"
	"  -K keys-file,   --keys=keys-file\n"
	"  -k keys-dir,    --keys-dir=keys-dir\n"
	"  -v,             --version\n"
	"  -h,             --help\n",
	argv[0]);
}
#else
/* Print out usage message */
static void usage(int argc, char *argv[])
{
    fprintf(
	stderr,
	"usage: %s [OPTION]...\n"
	"  -e elvin-url\n"
	"  -S scope\n"
	"  -H http-proxy\n"
	"  -I idle-period\n"
	"  -u user\n"
	"  -D domain\n"
	"  -T ticker-dir\n"
#if defined(ENABLE_LISP_INTERPRETER)
	"  -c config-file\n"
#endif
	"  -G groups-file\n"
	"  -U usenet-file\n"
	"  -K keys-file\n"
	"  -k keys-dir\n"
	"  -v\n"
	"  -h\n",
	argv[0]);
}
#endif


#define XtNversionTag "versionTag"
#define XtCVersionTag "VersionTag"
#define XtNmetamail "metamail"
#define XtCMetamail "Metamail"
#define XtNsendHistoryCapacity "sendHistoryCapacity"
#define XtCSendHistoryCapacity "SendHistoryCapacity"

/* The application shell window also has resources */
#define offset(field) XtOffsetOf(XTickertapeRec, field)
static XtResource resources[] =
{
    /* char *version_tag */
    {
	XtNversionTag, XtCVersionTag, XtRString, sizeof(char *),
	offset(version_tag), XtRString, (XtPointer)NULL
    },

    /* Char *metamail */
    {
	XtNmetamail, XtCMetamail, XtRString, sizeof(char *),
	offset(metamail), XtRString, (XtPointer)NULL
    },

    /* Cardinal sendHistoryCapacity */
    {
	XtNsendHistoryCapacity, XtCSendHistoryCapacity, XtRInt, sizeof(int),
	offset(send_history_count), XtRImmediate, (XtPointer)8
    }
};
#undef offset

/* Static function headers */
static void do_quit(
    Widget widget, XEvent *event,
    String *params, Cardinal *nparams);
static void do_history_prev(
    Widget widget, XEvent *event,
    String *params, Cardinal *nparams);
static void do_history_next(
    Widget widget, XEvent *event,
    String *params, Cardinal *nparams);

/* The Tickertape */
static tickertape_t tickertape;

#if defined(ELVIN_VERSION_AT_LEAST)
#if ELVIN_VERSION_AT_LEAST(4, 1, -1)
/* The global elvin client information */
elvin_client_t client = NULL;
#else
#error "Unsupported Elvin library version"
#endif
#endif


/* The default application actions table */
static XtActionsRec actions[] =
{
    { "history-prev", do_history_prev },
    { "history-next", do_history_next },
    { "quit", do_quit}
};

/* Callback for when the Window Manager wants to close a window */
static void do_quit(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    tickertape_quit(tickertape);
}

/* Callback for going backwards through the history */
static void do_history_prev(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    tickertape_history_prev(tickertape);
}

/* Callback for going forwards through the history */
static void do_history_next(Widget widget, XEvent *event, String *params, Cardinal *nparams)
{
    tickertape_history_next(tickertape);
}

/* Returns the name of the user who started this program */
static char *get_user()
{
    char *user;

    /* First try the `USER' environment variable */
    if ((user = getenv("USER")) != NULL)
    {
	return user;
    }

    /* If that isn't set try `LOGNAME' */
    if ((user = getenv("LOGNAME")) != NULL)
    {
	return user;
    }

    /* Go straight to the source for an unequivocal answer */
    return getpwuid(getuid()) -> pw_name;
}

/* Looks up the domain name of the host */
static char *get_domain()
{
    char *domain;
#ifdef HAVE_UNAME
    struct utsname name;
#ifdef HAVE_GETHOSTBYNAME
    struct hostent *host;
#endif /* GETHOSTBYNAME */
    char *point;
    int ch;
#endif /* UNAME */

    /* If the `DOMAIN' environment variable is set then use it */
    if ((domain = getenv("DOMAIN")) != NULL)
    {
	return strdup(domain);
    }

#ifdef HAVE_UNAME
    /* Look up the node name */
    if (uname(&name) < 0)
    {
	return strdup(DEFAULT_DOMAIN);
    }

#ifdef HAVE_GETHOSTBYNAME
    /* Use that to get the canonical name */
    if ((host = gethostbyname(name.nodename)) == NULL)
    {
	domain = strdup(name.nodename);
    }
    else
    {
	domain = strdup(host -> h_name);
    }
#else /* GETHOSTBYNAME */
    domain = name.nodename;
#endif /* GETHOSTBYNAME */

    /* Strip everything up to and including the first `.' */
    point = domain;
    while ((ch = *(point++)) != 0)
    {
	if (ch == '.')
	{
	    return strdup(point);
	}
    }

    /* No dots; just use what we have */
    return strdup(domain);
#else /* UNAME */
    return strdup(DEFAULT_DOMAIN);
#endif /* UNAME */
}

/* Parses arguments and sets stuff up */
static void parse_args(
    int argc, char *argv[],
    elvin_handle_t handle,
    char **user_return, char **domain_return,
    char **ticker_dir_return,
    char **config_file_return,
    char **groups_file_return,
    char **usenet_file_return,
    char **keys_file_return,
    char **keys_dir_return,
    elvin_error_t error)
{
    char *http_proxy = NULL;

    /* Initialize arguments to sane values */
    *user_return = NULL;
    *domain_return = NULL;
    *ticker_dir_return = NULL;
    *config_file_return = NULL;
    *groups_file_return = NULL;
    *keys_file_return = NULL;
    *keys_dir_return = NULL;
    *usenet_file_return = NULL;

    /* Read each argument using getopt */
    while (1)
    {
	int choice;

#if defined(HAVE_GETOPT_LONG)
	choice = getopt_long(argc, argv, OPTIONS, long_options, NULL);
#else
	choice = getopt(argc, argv, OPTIONS);
#endif
	/* End of options? */
	if (choice < 0)
	{
	    break;
	}

	/* Which option was it then? */
	switch (choice)
	{
	    /* --elvin= or -e */
	    case 'e':
	    {
		if (elvin_handle_append_url(handle, optarg, error) == 0)
		{
		    fprintf(stderr, PACKAGE ":bad URL: no doughnut \"%s\"\n", optarg);
		    elvin_error_fprintf(stderr, error);
		    exit(1);
		}

		break;
	    }

	    /* --scope= or -S */
	    case 'S':
	    {
		if (! elvin_handle_set_discovery_scope(handle, optarg, error))
		{
		    fprintf(stderr, PACKAGE ": unable to set scope to %s\n", optarg);
		    elvin_error_fprintf(stderr, error);
		    exit(1);
		}

		break;
	    }

	    /* --proxy= or -H */
	    case 'H':
	    {
		http_proxy = optarg;
		break;
	    }

	    /* --idle= or -I */
	    case 'I':
	    {
		if (! elvin_handle_set_idle_period(handle, atoi(optarg), error))
		{
		    fprintf(stderr, PACKAGE ": unable to set idle period to %d\n", atoi(optarg));
		    elvin_error_fprintf(stderr, error);
		    exit(1);
		}

		break;
	    }

	    /* --user= or -u */
	    case 'u':
	    {
		*user_return = optarg;
		break;
	    }

	    /* --domain= or -D */
	    case 'D':
	    {
		*domain_return = strdup(optarg);
		break;
	    }


	    /* --ticker-dir= or -T */
	    case 'T':
	    {
		*ticker_dir_return = optarg;
		break;
	    }

	    /* --config= or -c */
	    case 'c':
	    {
		*config_file_return = optarg;
		break;
	    }

	    /* --groups= or -G */
	    case 'G':
	    {
		*groups_file_return = optarg;
		break;
	    }

	    /* --usenet= or -U */
	    case 'U':
	    {
		*usenet_file_return = optarg;
		break;
	    }

	    /* --keys= or -K */
	    case 'K':
	    {
		*keys_file_return = optarg;
		break;
	    }

	    /* --keys-dir= or -k */
	    case 'k':
	    {
		*keys_dir_return = optarg;
		break;
	    }

	    /* --version or -v */
	    case 'v':
	    {
		printf(PACKAGE " version " VERSION "\n");
		exit(0);
	    }

	    /* --help or -h */
	    case 'h':
	    {
		usage(argc, argv);
		exit(0);
	    }

	    /* Unknown option */
	    default:
	    {
		usage(argc, argv);
		exit(1);
	    }
	}
    }

    /* Generate a user name if none provided */
    if (*user_return == NULL)
    {
	*user_return = get_user();
    }

    /* Generate a domain name if none provided */
    if (*domain_return == NULL)
    {
	*domain_return = get_domain();
    }

    /* If we now have a proxy, then set its property */
    if (http_proxy != NULL)
    {
	elvin_handle_set_property(handle, "http.proxy", http_proxy, NULL);
    }
    
    return;
}


/* Create the icon window */
static Window create_icon(Widget shell)
{
    Display *display = XtDisplay(shell);
    Screen *screen = XtScreen(shell);
    Colormap colormap = XDefaultColormapOfScreen(screen);
    int depth = DefaultDepthOfScreen(screen);
    unsigned long black = BlackPixelOfScreen(screen);
    Window window;
    Pixmap pixmap, mask;
    XColor color;
    GC gc;
    XGCValues values;

    /* Create the actual icon window */
    window = XCreateSimpleWindow(
	display, RootWindowOfScreen(screen),
	0, 0, mask_width, mask_height, 0,
	CopyFromParent, CopyFromParent);

    /* Allocate the color red by name */
    XAllocNamedColor(display, colormap, "red", &color, &color);

    /* Create a pixmap from the red bitmap data */
    pixmap = XCreatePixmapFromBitmapData(
	display, window, (char *)red_bits, red_width, red_height,
	color.pixel, black, depth);

    /* Create a graphics context */
    values.function = GXxor;
    gc = XCreateGC(display, pixmap, GCFunction, &values);

    /* Create a pixmap for the white 'e' and paint it on top */
    mask = XCreatePixmapFromBitmapData(
	display, pixmap, (char *)white_bits, white_width, white_height,
	WhitePixelOfScreen(screen) ^ black, 0, depth);
    XCopyArea(display, mask, pixmap, gc, 0, 0, white_width, white_height, 0, 0);
    XFreePixmap(display, mask);
    XFreeGC(display, gc);

#ifdef HAVE_LIBXEXT
    /* Create a shape mask and apply it to the window */
    mask = XCreateBitmapFromData(display, pixmap, (char *)mask_bits, mask_width, mask_height);
    XShapeCombineMask(display, window, ShapeBounding, 0, 0, mask, ShapeSet);
#endif /* HAVE_LIBXEXT */

    /* Set the window's background to be the pixmap */
    XSetWindowBackgroundPixmap(display, window, pixmap);
    return window;
}

/* Signal handler which causes xtickertape to reload its subscriptions */
static RETSIGTYPE reload_subs(int signum)
{
    /* Put the signal handler back in place */
    signal(SIGHUP, reload_subs);

    /* Reload configuration files */
    tickertape_reload_all(tickertape);
}

/* Print an error message indicating that the app-defaults file is bogus */
static void app_defaults_version_error(char *message)
{
    fprintf(stderr, PACKAGE ": %s\n\n"
	    "This probably because the app-defaults file (XTickertape) is not installed in\n"
	    "the X11 directory tree.  This may be fixed either by moving the app-defaults\n"
	    "file to the correct location or by setting your XFILESEARCHPATH environment\n"
	    "variable to something like /usr/local/lib/X11/%%T/%%N:%%D.  See the man page for\n"
	    "XtResolvePathname for more information.\n", message);
}


/* Parse args and go */
int main(int argc, char *argv[])
{
    XtAppContext context;
#ifndef HAVE_XTVAOPENAPPLICATION
    Display *display;
#endif
    XTickertapeRec rc;
    elvin_handle_t handle;
    elvin_error_t error;
    char *user;
    char *domain;
    char *ticker_dir;
    char *config_file;
    char *groups_file;
    char *usenet_file;
    char *keys_file;
    char *keys_dir;
    Widget top;

#ifdef HAVE_XTVAOPENAPPLICATION
    /* Create the toplevel widget */
    top = XtVaOpenApplication(
	&context, "XTickertape",
	NULL, 0,
	&argc, argv, NULL,
	applicationShellWidgetClass,
	XtNborderWidth, 0,
	NULL);
#else    
    /* Initialize the X Toolkit */
    XtToolkitInitialize();

    /* Create an application context */
    context = XtCreateApplicationContext();

    /* Open the display */
    if ((display = XtOpenDisplay(
	     context, NULL, NULL, "XTickertape",
	     NULL, 0,
	     &argc, argv)) == NULL)
    {

	fprintf(stderr, "Error: Can't open display\n");
	exit(1);
    }

    /* Create the toplevel widget */
    top = XtAppCreateShell(NULL, "XTickertape", applicationShellWidgetClass, display, NULL, 0);
#endif

    /* Load the application shell resources */
    XtGetApplicationResources(top, &rc, resources, XtNumber(resources), NULL, 0);

    /* Make sure our app-defaults file has a version number */
    if (rc.version_tag == NULL)
    {
	app_defaults_version_error("app-defaults file not found or or out of date");
	exit(1);
    }

    /* Make sure that version number is the one we want */
    if (strcmp(rc.version_tag, PACKAGE "-" VERSION) != 0)
    {
	app_defaults_version_error("app-defaults file has the wrong version number");
	exit(1);
    }

    /* Add a calback for when it gets destroyed */
    XtAppAddActions(context, actions, XtNumber(actions));

#if ! defined(ELVIN_VERSION_AT_LEAST)
    /* Initialize the elvin client library */
    if ((error = elvin_xt_init(context)) == NULL)
    {
	fprintf(stderr, "*** elvin_xt_init(): failed\n");
	exit(1);
    }

    /* Double-check that the initialization worked */
    if (elvin_error_is_error(error))
    {
	fprintf(stderr, "*** elvin_xt_init(): failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Create a new elvin connection handle */
    if ((handle = elvin_handle_alloc(error)) == NULL)
    {
	fprintf(stderr, PACKAGE ": elvin_handle_alloc() failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

#elif ELVIN_VERSION_AT_LEAST(4, 1, -1)
    /* Allocate an error context */
    if (! (error = elvin_error_alloc(NULL, NULL))) {
	fprintf(stderr, PACKAGE ": elvin_error_alloc() failed\n");
	exit(1);
    }

    /* Initialize the elvin client library */
    if ((client = elvin_xt_init_default(context, error)) == NULL)
    {
	fprintf(stderr, PACKAGE ": elvin_xt_init() failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }

    /* Create a new elvin connection handle */
    if ((handle = elvin_handle_alloc(client, error)) == NULL)
    {
	fprintf(stderr, PACKAGE ": elvin_handle_alloc() failed\n");
	elvin_error_fprintf(stderr, error);
	exit(1);
    }
#else /* ELVIN_VERSION_AT_LEAST */
#error "Unsupported Elvin library version"
#endif /* ELVIN_VERSION_AT_LEAST */

    /* Scan what's left of the arguments */
    parse_args(argc, argv, handle, &user, &domain,
	       &ticker_dir, &config_file,
	       &groups_file, &usenet_file,
	       &keys_file, &keys_dir,
	       error);

    /* Create an Icon for the root shell */
    XtVaSetValues(top, XtNiconWindow, create_icon(top), NULL);

    /* Create a tickertape */
    tickertape = tickertape_alloc(
	&rc, handle,
	user, domain,
	ticker_dir, config_file,
	groups_file, usenet_file,
	keys_file, keys_dir,
	top,
	error);

    /* Set up SIGHUP to reload the subscriptions */
    signal(SIGHUP, reload_subs);

#ifdef HAVE_LIBXMU
    /* Enable editres support */
    XtAddEventHandler(top, (EventMask)0, True, _XEditResCheckMessages, NULL);
#endif /* HAVE_LIBXMU */

    /* Let 'er rip! */
    XtAppMainLoop(context);

    /* Keep the compiler happy */
    return 0;
}

