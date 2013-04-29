#include "config.h"
#include "gbc-main-window.h"

G_DEFINE_TYPE (GbcMainWindow,                 \
               gbc_main_window,               \
               GTK_TYPE_WINDOW)

#define GET_PRIVATE(o)                          \
  (G_TYPE_INSTANCE_GET_PRIVATE                  \
   ((o), GBC_TYPE_MAIN_WINDOW,                  \
    GbcMainWindowPrivate))

typedef struct {
  gpointer placeholder;
} GbcMainWindowPrivate;

static GObject *singleton = NULL;

static void
gbc_main_window_init (GbcMainWindow *self)
{
}

static void
app_quit (GbcMainWindow *self)
{
  gtk_main_quit ();
}

static void
app_window_destroy_cb (GtkObject *window,
                       gpointer   user_data)
{
  app_quit (user_data);
}

static GObject *
gbc_main_window_constructor (GType type,
                             guint n_construct_properties,
                             GObjectConstructParam *
                             construct_properties)
{
  /* Break early if the singleton instance already exists */
  if (singleton)
    return g_object_ref (singleton);

  singleton = G_OBJECT_CLASS
    (gbc_main_window_parent_class)->constructor
    (type, n_construct_properties, construct_properties);

  g_object_set (singleton, "type", GTK_WINDOW_TOPLEVEL, NULL);

  return singleton;
}

static void
gbc_main_window_constructed (GObject *obj)
{
  GbcMainWindow *self;
  GbcMainWindowPrivate *priv;
  GtkWidget *vbox;

  if (G_OBJECT_CLASS (gbc_main_window_parent_class)->constructed)
    G_OBJECT_CLASS (gbc_main_window_parent_class)->constructed
      (obj);

  self = GBC_MAIN_WINDOW (obj);
  priv = GET_PRIVATE (self);

  /* gtk_window_set_default_size (GTK_WINDOW (self), 960, 720); */
  g_signal_connect (self, "destroy",
                    G_CALLBACK (app_window_destroy_cb), self);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (self), vbox);

  gtk_widget_show (vbox);
}

static void
gbc_main_window_dispose (GObject *obj)
{
  singleton = NULL;

  G_OBJECT_CLASS (gbc_main_window_parent_class)->dispose (obj);
}

static void
gbc_main_window_class_init (GbcMainWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = gbc_main_window_constructor;
  object_class->constructed = gbc_main_window_constructed;
  object_class->dispose     = gbc_main_window_dispose;

  g_type_class_add_private (klass, sizeof (GbcMainWindowPrivate));
}

GtkWidget *
gbc_main_window_new (void)
{
  return g_object_new (GBC_TYPE_MAIN_WINDOW, NULL);
}
