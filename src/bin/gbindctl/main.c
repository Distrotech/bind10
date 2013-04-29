#include "config.h"
#include "gbc-main-window.h"
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <locale.h>

static gboolean flag_version = FALSE;
static gboolean flag_verbose = FALSE;

static void
i18n_init (void)
{
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, B10_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
}

static void
show_version (void)
{
  g_print ("%s version %s\n",
           g_get_application_name (),
           PACKAGE_VERSION);
}

static void
show_version_and_exit (void)
{
  show_version ();
  exit (EXIT_SUCCESS);
}

static const GOptionEntry arguments[] = {
  { "version", 'v', 0,
    G_OPTION_ARG_NONE, &flag_version,
    N_("Output version information and exit"), NULL
  },
  { "verbose", 0, 0,
    G_OPTION_ARG_NONE, &flag_verbose,
    N_("Be verbose"), NULL
  },
  { NULL, 0, 0,
    0, NULL,
    NULL, NULL
  }
};

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GError *error = NULL;
  GtkWidget *window;

  i18n_init ();

  context = g_option_context_new (_("[NAME]"));
  g_option_context_set_summary (context, g_get_application_name ());
  g_option_context_add_main_entries (context, arguments,
                                     GETTEXT_PACKAGE);
  if (! g_option_context_parse (context, &argc, &argv, &error)) {
    if (error) {
      g_print ("%s\n", error->message);
      g_error_free (error);
    }

    return EXIT_FAILURE;
  }

  g_option_context_free (context);

  if (flag_version) {
    show_version_and_exit ();
  }

  gtk_init (&argc, &argv);

  window = gbc_main_window_new ();
  gtk_widget_show (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
