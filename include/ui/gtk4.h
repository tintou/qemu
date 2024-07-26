#ifndef UI_GTK4_H
#define UI_GTK4_H

/* Work around an -Wstrict-prototypes warning in GTK headers */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include <gtk/gtk.h>
#pragma GCC diagnostic pop

#include "ui/clipboard.h"
#include "ui/console.h"
#include "ui/kbd-state.h"
#include "ui/egl-helpers.h"
#include "ui/egl-context.h"

typedef struct Gtk4DisplayState Gtk4DisplayState;

typedef struct VirtualConsole {
    Gtk4DisplayState *s;
    char *label;
    GtkWidget *window;
    GtkWidget *menu_item;
    GtkWidget *tab_item;
    GtkWidget *focus;
    GtkWidget *view;
    DisplayGLCtx dgc;
    DisplayChangeListener dcl;
    QKbdState *kbd;
    DisplaySurface *ds;
    pixman_image_t *convert;
    cairo_surface_t *surface;
    gboolean free_scale;
    double scale_x;
    double scale_y;
    QemuGLShader *gls;
    EGLContext ectx;
    EGLSurface esurface;
    int glupdates;
    int x, y, w, h;
    egl_fb guest_fb;
    egl_fb win_fb;
    egl_fb cursor_fb;
    int cursor_x;
    int cursor_y;
    bool y0_top;
    bool scanout_mode;
    bool has_dmabuf;
} VirtualConsole;

struct Gtk4DisplayState {
    DisplayOptions *opts;

    Notifier mouse_mode_notifier;

    GPtrArray *vc;

    VirtualConsole *kbd_owner;
    VirtualConsole *ptr_owner;

    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *notebook;
};

#endif /* UI_GTK4_H */
