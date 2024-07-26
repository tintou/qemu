/*
 * QEMU graphical console
 *
 * Copyright 2024 Corentin Noël <corentin.noel@collabora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define GETTEXT_PACKAGE "qemu"
#define LOCALEDIR "po"

#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include "ui/console.h"
#include "ui/shader.h"
#include "ui/gtk4.h"

#include <glib/gi18n.h>
#include <locale.h>

#include "sysemu/runstate.h"
#include "sysemu/sysemu.h"

#define HOTKEY_MODIFIERS "<Ctrl><Alt>"

#define VC_WINDOW_X_MIN  320
#define VC_WINDOW_Y_MIN  240
#define VC_TERM_X_MIN     80
#define VC_TERM_Y_MIN     25
#define VC_SCALE_MIN    0.25
#define VC_SCALE_STEP   0.25

static gboolean gtkinit;


static VirtualConsole *find_vc_by_page(Gtk4DisplayState *s, gint page)
{
    VirtualConsole *vc;
    gint i, p;

    for (i = 0; i < s->vc->len; i++) {
        vc = g_ptr_array_index(s->vc, i);
        p = gtk_notebook_page_num(GTK_NOTEBOOK(s->notebook), vc->tab_item);
        if (p == page) {
            return vc;
        }
    }

    return NULL;
}

static VirtualConsole *find_current_vc(Gtk4DisplayState *s)
{
    gint page;

    page = gtk_notebook_get_current_page(GTK_NOTEBOOK(s->notebook));
    return find_vc_by_page(s, page);
}

static void
activate_reset(GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
    qemu_system_reset_request(SHUTDOWN_CAUSE_HOST_UI);
}

static void
activate_power_down(GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    qemu_system_powerdown_request();
}

static void
activate_quit(GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
}

static void
on_fullscreen(GSimpleAction *action,
              GVariant      *state,
              gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;

    if (g_variant_get_boolean(state)) {
        gtk_window_fullscreen(GTK_WINDOW(s->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(s->window));
    }

    g_simple_action_set_state(action, state);
}

static void
activate_zoom_in(GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;
    VirtualConsole *vc = find_current_vc(s);

    assert(vc);

    vc->scale_x += VC_SCALE_STEP;
    vc->scale_y += VC_SCALE_STEP;
}

static void
activate_zoom_out(GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;
    VirtualConsole *vc = find_current_vc(s);

    assert(vc);

    vc->scale_x -= VC_SCALE_STEP;
    vc->scale_y -= VC_SCALE_STEP;
}

static void
activate_best_fit(GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  g_warning("activate_best_fit");
}

static void
activate_detach_tab(GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  g_warning("activate_detach_tab");
}

static void
on_pause_actioned(GSimpleAction *action,
                  GVariant      *state,
                  gpointer       user_data)
{
  /*GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);*/
}

static void
on_zoom_fit(GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;
    VirtualConsole *vc = find_current_vc(s);

    assert(vc);

    vc->free_scale = g_variant_get_boolean(state);
    if (vc->free_scale) {
        vc->scale_x = 1.0;
        vc->scale_y = 1.0;
    }

    g_simple_action_set_state(action, state);
}

static void
on_grab_hover(GSimpleAction *action,
              GVariant      *state,
              gpointer       user_data)
{
  /*GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);

  g_simple_action_set_state (action, state);*/
}

static void
on_grab_input(GSimpleAction *action,
              GVariant      *state,
              gpointer       user_data)
{
  /*GtkSettings *settings = gtk_settings_get_default ();

  g_object_set (G_OBJECT (settings),
                "gtk-application-prefer-dark-theme",
                g_variant_get_boolean (state),
                NULL);
  g_simple_action_set_state (action, state);*/
}

static void
on_show_tab(GSimpleAction *action,
            GVariant      *state,
            gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(s->notebook), g_variant_get_boolean(state));

    g_simple_action_set_state(action, state);
}

static void
on_show_tabs(GSimpleAction *action,
             GVariant      *state,
             gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(s->notebook), g_variant_get_boolean(state));

    g_simple_action_set_state(action, state);
}


static void
on_show_menubar(GSimpleAction *action,
                GVariant      *state,
                gpointer       user_data)
{
    Gtk4DisplayState *s = user_data;

    gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW(s->window), g_variant_get_boolean(state));
    g_simple_action_set_state(action, state);
}

static void update_title(Gtk4DisplayState *s)
{
    const char *status = "";
    gchar *prefix;
    gchar *title;
    const char *grab = "";
    bool is_paused = !runstate_is_running();
    int i;

    if (qemu_name) {
        prefix = g_strdup_printf("QEMU (%s)", qemu_name);
    } else {
        prefix = g_strdup_printf("QEMU");
    }

    if (s->ptr_owner != NULL &&
        s->ptr_owner->window == NULL) {
        grab = _(" - Press Ctrl+Alt+G to release grab");
    }

    if (is_paused) {
        status = _(" [Paused]");
    }
    /*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(s->pause_item),
                                   is_paused);*/

    title = g_strdup_printf("%s%s%s", prefix, status, grab);
    gtk_window_set_title(GTK_WINDOW(s->window), title);
    g_free(title);

    for (i = 0; i < s->vc->len; i++) {
        VirtualConsole *vc = g_ptr_array_index(s->vc, i);

        if (!vc->window) {
            continue;
        }
        title = g_strdup_printf("%s: %s%s%s", prefix, vc->label,
                                vc == s->kbd_owner ? " +kbd" : "",
                                vc == s->ptr_owner ? " +ptr" : "");
        gtk_window_set_title(GTK_WINDOW(vc->window), title);
        g_free(title);
    }

    g_free(prefix);
}

static void on_vm_state_changed(void *opaque, bool running, RunState state)
{
    Gtk4DisplayState *s = opaque;

    update_title(s);
}

static void on_mouse_mode_change(Notifier *notify, void *data)
{
    /*Gtk4DisplayState *s;

    s = container_of(notify, Gtk4DisplayState, mouse_mode_notifier);*/
}

static gboolean on_window_close_request(Gtk4DisplayState *s, GtkWindow *window)
{
    if (!s->opts->has_window_close || s->opts->window_close) {
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
    }

    return true;
}

static GMenuModel *create_menu(Gtk4DisplayState *s)
{
    GMenu *main_menu, *machine_menu, *view_menu, *section;

    main_menu = g_menu_new();

    machine_menu = g_menu_new();

    section = g_menu_new();
    g_menu_append(section, _("Pause"), "win.paused");
    g_menu_append_section(machine_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, _("Reset"), "win.reset");
    g_menu_append(section, _("Power Down"), "win.power-down");
    g_menu_append_section(machine_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, _("Quit"), "win.quit");
    g_menu_append_section(machine_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    view_menu = g_menu_new();

    section = g_menu_new();
    g_menu_append(section, _("_Fullscreen"), "win.fullscreen");
    g_menu_append_section(view_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, _("Zoom _In"), "win.zoom-in");
    g_menu_append(section, _("Zoom _Out"), "win.zoom-out");
    g_menu_append(section, _("Best _Fit"), "win.best-fit");
    g_menu_append(section, _("Zoom To _Fit"), "win.zoom-fit");
    g_menu_append_section(view_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, _("Grab On _Hover"), "win.grab-hover");
    g_menu_append(section, _("_Grab Input"), "win.grab-input");
    g_menu_append_section(view_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    if (s->vc->len > 0) {
        section = g_menu_new();
        for(unsigned int i = 0; i < s->vc->len; i++) {
            VirtualConsole *vc;
            char *action;

            vc = g_ptr_array_index (s->vc, i);
            action = g_action_print_detailed_name ("win.show-tab", g_variant_new_uint32(i));
            g_menu_append(section, vc->label, action);
            g_free(action);
        }

        g_menu_append_section(view_menu, NULL, G_MENU_MODEL(section));
        g_object_unref(section);
    }

    section = g_menu_new();
    g_menu_append(section, _("Show _Tabs"), "win.show-tabs");
    g_menu_append(section, _("Detach Tab"), "win.detach-tab");
    g_menu_append(section, _("Show Menubar"), "win.show-menubar");
    g_menu_append_section(view_menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    g_menu_append_submenu(main_menu, _("Machine"), G_MENU_MODEL(machine_menu));
    g_menu_append_submenu(main_menu, _("View"), G_MENU_MODEL(view_menu));
    g_object_unref(machine_menu);
    g_object_unref(view_menu);

    return G_MENU_MODEL(main_menu);
}

static void on_app_startup(Gtk4DisplayState *s, GtkApplication *app)
{
    GMenuModel *menu;

    const GActionEntry entries[] = {
      { "paused", NULL, NULL, "false", on_pause_actioned },
      { "reset", activate_reset, NULL, NULL, NULL },
      { "power-down", activate_power_down, NULL, NULL, NULL },
      { "quit", activate_quit, NULL, NULL, NULL },
      { "fullscreen", NULL, NULL, "false", on_fullscreen },
      { "zoom-in", activate_zoom_in, NULL, NULL, NULL },
      { "zoom-out", activate_zoom_out, NULL, NULL, NULL },
      { "best-fit", activate_best_fit, NULL, NULL, NULL },
      { "zoom-fit", NULL, NULL, "false", on_zoom_fit },
      { "grab-hover", NULL, NULL,  "false", on_grab_hover },
      { "grab-input", NULL, NULL,  "false", on_grab_input },
      { "show-tab", NULL, NULL, "0", on_show_tab },
      { "show-tabs", NULL, NULL, "false", on_show_tabs },
      { "detach-tab", activate_detach_tab, NULL, NULL, NULL },
      { "show-menubar", NULL, NULL, "true", on_show_menubar },
    };

    menu = create_menu(s);
    gtk_application_set_menubar(app, menu);
    g_object_unref(menu);

    s->window = gtk_application_window_new(app);
    g_action_map_add_action_entries(G_ACTION_MAP(s->window), entries, G_N_ELEMENTS(entries), s);

    s->notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(s->notebook), FALSE);
    gtk_window_set_child(GTK_WINDOW(s->window), s->notebook);
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(s->window), TRUE);

    for(unsigned int i = 0; i < s->vc->len; i++) {
        VirtualConsole *vc;

        vc = g_ptr_array_index (s->vc, i);
        vc->view = gtk_image_new();
        vc->tab_item = gtk_label_new(vc->label);
        gtk_notebook_append_page(GTK_NOTEBOOK(s->notebook), vc->view, vc->tab_item);

        register_displaychangelistener(&vc->dcl);
    }

    s->mouse_mode_notifier.notify = on_mouse_mode_change;
    qemu_add_mouse_mode_change_notifier(&s->mouse_mode_notifier);
    qemu_add_vm_change_state_handler(on_vm_state_changed, s);

    update_title(s);

    g_signal_connect_swapped(s->window, "close-request", G_CALLBACK(on_window_close_request), s);

    gtk_window_present(GTK_WINDOW(s->window));
}

static void on_mouse_set(DisplayChangeListener *dcl,
                         int x, int y, int on)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);

    if (!qemu_console_is_graphic(vc->dcl.con)) {
        return;
    }

    if (on) {
        gtk_widget_set_cursor_from_name(vc->view, NULL);
    } else {
        gtk_widget_set_cursor_from_name(vc->view, "none");
    }
}

static void on_gfx_update(DisplayChangeListener *dcl,
                          int x, int y, int w, int h)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);

    gdk_gl_context_make_current(gdk_gl_context_get_current());
    surface_gl_update_texture(vc->gls, vc->ds, x, y, w, h);

    g_warning("on_gfx_update: UNIMPLEMENTED");
}

static void on_gfx_switch(DisplayChangeListener *dcl,
                          struct DisplaySurface *new_surface)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);

    gdk_gl_context_make_current(gdk_gl_context_get_current());
    //SDL_GL_MakeCurrent(scon->real_window, scon->winctx);
    surface_gl_destroy_texture(vc->gls, vc->ds);

    vc->ds = new_surface;

    if (surface_is_placeholder(new_surface) && qemu_console_get_index(dcl->con)) {
        qemu_gl_fini_shader(vc->gls);
        vc->gls = NULL;
        //sdl2_window_destroy(scon);
        return;
    }

    vc->gls = qemu_gl_init_shader();
/*
    if (!scon->real_window) {
        //sdl2_window_create(scon);
        scon->gls = qemu_gl_init_shader();
    } else if (old_surface &&
               ((surface_width(old_surface)  != surface_width(new_surface)) ||
                (surface_height(old_surface) != surface_height(new_surface)))) {
        //sdl2_window_resize(scon);
    }
*/
    surface_gl_create_texture(vc->gls, vc->ds);
}

static void on_refresh(DisplayChangeListener *dcl)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);

    gtk_widget_queue_draw(vc->view);
    g_warning("on_refresh: UNIMPLEMENTED");
}

static void on_cursor_define(DisplayChangeListener *dcl,
                             QEMUCursor *cursor)
{
    g_warning("on_cursor_define: UNIMPLEMENTED");
}

static void on_gl_scanout_disable(DisplayChangeListener *dcl)
{
    g_warning("on_gl_scanout_disable: UNIMPLEMENTED");
}

static void on_gl_scanout_texture(DisplayChangeListener *dcl,
                                  uint32_t backing_id,
                                  bool backing_y_0_top,
                                  uint32_t backing_width,
                                  uint32_t backing_height,
                                  uint32_t x, uint32_t y,
                                  uint32_t w, uint32_t h,
                                  void *d3d_tex2d)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);
    egl_fb_setup_for_tex(&vc->guest_fb, backing_width, backing_height,
                         backing_id, false);
}

static void on_gl_scanout_dmabuf(DisplayChangeListener *dcl,
                                 QemuDmaBuf *dmabuf)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);
    GdkDmabufTextureBuilder *builder;

    g_warning("on_gl_scanout_dmabuf");
    builder = gdk_dmabuf_texture_builder_new();

    gdk_dmabuf_texture_builder_set_display(builder, gtk_widget_get_display(GTK_WIDGET(vc->view)));
    gdk_dmabuf_texture_builder_set_fourcc(builder, qemu_dmabuf_get_fourcc(dmabuf));
    gdk_dmabuf_texture_builder_set_modifier(builder, qemu_dmabuf_get_modifier(dmabuf));
    gdk_dmabuf_texture_builder_set_width(builder, qemu_dmabuf_get_width(dmabuf));
    gdk_dmabuf_texture_builder_set_height(builder, qemu_dmabuf_get_height(dmabuf));
    gdk_dmabuf_texture_builder_set_n_planes(builder, 1);

    gdk_dmabuf_texture_builder_set_fd(builder, 0, qemu_dmabuf_get_fd(dmabuf));
    gdk_dmabuf_texture_builder_set_offset(builder, 0, 0);
    gdk_dmabuf_texture_builder_set_stride(builder, 0, qemu_dmabuf_get_stride(dmabuf));

    GdkTexture* texture = NULL;
    GError *error = NULL;
    texture = gdk_dmabuf_texture_builder_build(builder, NULL, NULL, &error);
    if (error)
        g_critical ("%s", error->message);

    gtk_image_set_from_paintable(GTK_IMAGE(vc->view), GDK_PAINTABLE(texture));

    /* egl_dmabuf_import_texture(dmabuf); */
    /* texture = qemu_dmabuf_get_texture(dmabuf); */
    /* if (!texture) { */
    /*     return; */
    /* } */

    /* x = qemu_dmabuf_get_x(dmabuf); */
    /* y = qemu_dmabuf_get_y(dmabuf); */
    /* width = qemu_dmabuf_get_width(dmabuf); */
    /* height = qemu_dmabuf_get_height(dmabuf); */
    /* backing_width = qemu_dmabuf_get_backing_width(dmabuf); */
    /* backing_height = qemu_dmabuf_get_backing_height(dmabuf); */
    /* y0_top = qemu_dmabuf_get_y0_top(dmabuf); */

    /* gd_gl_area_scanout_texture(dcl, texture, y0_top, */
    /*                            backing_width, backing_height, */
    /*                            x, y, width, height, NULL); */

    if (qemu_dmabuf_get_allow_fences(dmabuf)) {
         vc->guest_fb.dmabuf = dmabuf;
    }
}

static void on_gl_release_dmabuf(DisplayChangeListener *dcl,
                                 QemuDmaBuf *dmabuf)
{
    VirtualConsole *vc = container_of(dcl, VirtualConsole, dcl);

    g_warning("on_gl_release_dmabuf");
    egl_dmabuf_release_texture(dmabuf);
    if (vc->guest_fb.dmabuf == dmabuf) {
        vc->guest_fb.dmabuf = NULL;
    }
}

static void on_gl_update(DisplayChangeListener *dcl,
                         uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    g_warning("UNIMPLEMENTED");
}

static const DisplayChangeListenerOps virtual_console_ops = {
    .dpy_name             = "gtk4",
    .dpy_mouse_set        = on_mouse_set,
    .dpy_cursor_define    = on_cursor_define,

    .dpy_gfx_update         = on_gfx_update,
    .dpy_gfx_switch         = on_gfx_switch,
    .dpy_refresh            = on_refresh,
    .dpy_gl_scanout_disable = on_gl_scanout_disable,
    .dpy_gl_scanout_texture = on_gl_scanout_texture,
    .dpy_gl_scanout_dmabuf  = on_gl_scanout_dmabuf,
    .dpy_gl_release_dmabuf  = on_gl_release_dmabuf,
    .dpy_gl_update          = on_gl_update
};

static bool
is_compatible_dcl(DisplayGLCtx *dgc,
                  DisplayChangeListener *dcl)
{
    return
        dcl->ops == &virtual_console_ops;
}

static QEMUGLContext
on_create_context(DisplayGLCtx *dgc,
                  QEMUGLParams *params)
{
    eglMakeCurrent(qemu_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   qemu_egl_rn_ctx);
    return qemu_egl_create_context(dgc, params);
}

static void
on_create_texture(DisplayGLCtx *ctx, DisplaySurface *surface)
{
    GError *error = NULL;
    GdkGLContext *gdk_ctx;
    VirtualConsole *vc = container_of(ctx, VirtualConsole, dgc);

    GdkDisplay *disp = gtk_widget_get_display(vc->view);
    gdk_ctx = gdk_display_create_gl_context(disp, &error);
    if (!gdk_ctx)
        g_critical ("%s", error->message);
    gdk_gl_context_make_current(gdk_ctx);

    vc->gls = qemu_gl_init_shader();
    surface_gl_create_texture(vc->gls, surface);
    g_object_unref(gdk_ctx);
}

static void
on_destroy_texture(DisplayGLCtx *ctx, DisplaySurface *surface)
{
    VirtualConsole *vc = container_of(ctx, VirtualConsole, dgc);

    surface_gl_destroy_texture(vc->gls, surface);
}

static void
on_update_texture(DisplayGLCtx *ctx, DisplaySurface *surface,
                  int x, int y, int w, int h)
{
    VirtualConsole *vc = container_of(ctx, VirtualConsole, dgc);

    surface_gl_update_texture(vc->gls, surface, x, y, w, h);
}

static const DisplayGLCtxOps gl_ctx_ops = {
    .dpy_gl_ctx_is_compatible_dcl = is_compatible_dcl,
    .dpy_gl_ctx_create       = on_create_context,
    .dpy_gl_ctx_destroy      = qemu_egl_destroy_context,
    .dpy_gl_ctx_make_current = qemu_egl_make_context_current,
    .dpy_gl_ctx_create_texture = on_create_texture,
    .dpy_gl_ctx_destroy_texture = on_destroy_texture,
    .dpy_gl_ctx_update_texture = on_update_texture,
};

static VirtualConsole *virtual_console_new(Gtk4DisplayState *s, QemuConsole *con) {
    VirtualConsole *vc;

    vc = g_new0(VirtualConsole, 1);
    vc->label = qemu_console_get_label(con);
    vc->s = s;
    vc->scale_x = 1.0;
    vc->scale_y = 1.0;
    vc->dcl.ops = &virtual_console_ops;
    vc->dgc.ops = &gl_ctx_ops;
    vc->dcl.con = con;

    if (qemu_console_is_graphic(con)) {
        qemu_console_set_display_gl_ctx(con, &vc->dgc);
    }

    return vc;
}

static void early_gtk4_display_init(DisplayOptions *opts)
{
    /* The QEMU code relies on the assumption that it's always run in
     * the C locale. Therefore it is not prepared to deal with
     * operations that produce different results depending on the
     * locale, such as printf's formatting of decimal numbers, and
     * possibly others.
     *
     * Since GTK calls setlocale() by default -importing the locale
     * settings from the environment- we must prevent it from doing so
     * using gtk_disable_setlocale().
     *
     * QEMU's GTK UI, however, _does_ have translations for some of
     * the menu items. As a trade-off between a functionally correct
     * QEMU and a fully internationalized UI we support importing
     * LC_MESSAGES from the environment (see the setlocale() call
     * earlier in this file). This allows us to display translated
     * messages leaving everything else untouched.
     */
    gtk_disable_setlocale();
    gtkinit = gtk_init_check();
    if (!gtkinit) {
        /* don't exit yet, that'll break -help */
        return;
    }

    assert(opts->type == DISPLAY_TYPE_GTK4);
}

static void gtk4_display_init(DisplayState *ds, DisplayOptions *opts)
{
    Gtk4DisplayState *s;
    GtkIconTheme *theme;
    GdkDisplay *display;
    char *dir;

    /* Mostly LC_MESSAGES only. See early_gtk_display_init() for details. For
     * LC_CTYPE, we need to make sure that non-ASCII characters are considered
     * printable, but without changing any of the character classes to make
     * sure that we don't accidentally break implicit assumptions.  */
    setlocale(LC_MESSAGES, "");
    setlocale(LC_CTYPE, "C.UTF-8");
    dir = get_relocated_path(CONFIG_QEMU_LOCALEDIR);
    bindtextdomain("qemu", dir);
    g_free(dir);
    bind_textdomain_codeset("qemu", "UTF-8");
    textdomain("qemu");

    display = gdk_display_get_default();
    theme = gtk_icon_theme_get_for_display(display);
    dir = get_relocated_path(CONFIG_QEMU_ICONDIR);
    gtk_icon_theme_add_search_path(theme, dir);
    g_free(dir);
    g_set_prgname("qemu");

    s = g_new0(Gtk4DisplayState, 1);
    s->opts = opts;
    s->vc = g_ptr_array_new_with_free_func(g_free);

    s->app = gtk_application_new("org.qemu.Viewer", G_APPLICATION_NON_UNIQUE);
    gtk_application_set_accels_for_action(s->app, "win.fullscreen", (const char*[]) { HOTKEY_MODIFIERS "f", NULL });
    gtk_application_set_accels_for_action(s->app, "win.zoom-in", (const char*[]) { HOTKEY_MODIFIERS "plus", NULL });
    gtk_application_set_accels_for_action(s->app, "win.zoom-out", (const char*[]) { HOTKEY_MODIFIERS "minus", NULL });
    gtk_application_set_accels_for_action(s->app, "win.best-fit", (const char*[]) { HOTKEY_MODIFIERS "0", NULL });
    gtk_application_set_accels_for_action(s->app, "win.grab-input", (const char*[]) { HOTKEY_MODIFIERS "g", NULL });
    gtk_application_set_accels_for_action(s->app, "win.show-menubar", (const char*[]) { HOTKEY_MODIFIERS "m", NULL });
    g_signal_connect_swapped(s->app, "startup", G_CALLBACK(on_app_startup), s);

    for (unsigned int vc_index = 0u;; vc_index++) {
        QemuConsole *con;
        VirtualConsole *vc;

        con = qemu_console_lookup_by_index(vc_index);
        if (!con) {
            break;
        }

        vc = virtual_console_new(s, con);
        g_ptr_array_add(s->vc, vc);
    }

    g_application_register(G_APPLICATION(s->app), NULL, NULL);
}

static QemuDisplay qemu_display_gtk4 = {
    .type       = DISPLAY_TYPE_GTK4,
    .early_init = early_gtk4_display_init,
    .init       = gtk4_display_init,
    .vc         = "vc",
};

static void register_gtk4(void)
{
    qemu_display_register(&qemu_display_gtk4);
}

type_init(register_gtk4);

module_dep("ui-opengl");
