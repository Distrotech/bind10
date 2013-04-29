#ifndef GBC_MAIN_WINDOW_H
#define GBC_MAIN_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GBC_TYPE_MAIN_WINDOW            \
  (gbc_main_window_get_type ())

#define GBC_MAIN_WINDOW(obj)                \
  (G_TYPE_CHECK_INSTANCE_CAST               \
   ((obj), GBC_TYPE_MAIN_WINDOW, GbcMainWindow))

#define GBC_MAIN_WINDOW_CLASS(klass)            \
  (G_TYPE_CHECK_CLASS_CAST              \
   ((klass), GBC_TYPE_MAIN_WINDOW, GbcMainWindowClass))

#define GBC_IS_MAIN_WINDOW(obj)         \
  (G_TYPE_CHECK_INSTANCE_TYPE           \
   ((obj), GBC_TYPE_MAIN_WINDOW))

#define GBC_IS_MAIN_WINDOW_CLASS(klass)     \
  (G_TYPE_CHECK_CLASS_TYPE          \
   ((klass), GBC_TYPE_MAIN_WINDOW))

#define GBC_MAIN_WINDOW_GET_CLASS(obj)          \
  (G_TYPE_INSTANCE_GET_CLASS                \
   ((obj), GBC_TYPE_MAIN_WINDOW, GbcMainWindowClass))

typedef struct _GbcMainWindow GbcMainWindow;
typedef struct _GbcMainWindowClass GbcMainWindowClass;

struct _GbcMainWindow {
  GtkWindow parent_instance;
};

struct _GbcMainWindowClass {
  GtkWindowClass parent_class;
};

GType gbc_main_window_get_type (void);
GtkWidget *gbc_main_window_new (void);

G_END_DECLS

#endif /* GBC_MAIN_WINDOW_H */
