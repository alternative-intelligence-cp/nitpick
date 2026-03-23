/*  aria_gtk4_shim.c — flat-parameter bridge between Aria FFI and GTK4
 *
 *  GTK4 is GObject-based and relies heavily on signals/callbacks.
 *  Since Aria's FFI cannot pass function pointers, this shim:
 *    1) Manages a GtkApplication + main window internally
 *    2) Uses a flag-based event model instead of callbacks
 *    3) Provides iterative main-loop stepping (one iteration at a time)
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_gtk4_shim.so aria_gtk4_shim.c \
 *       $(pkg-config --cflags --libs gtk4)
 */

#include <stdint.h>
#include <string.h>
#include <gtk/gtk.h>

/* ── internal state ──────────────────────────────────────────────────── */

static GtkApplication *g_app     = NULL;
static GtkWidget      *g_window  = NULL;
static GtkWidget      *g_box     = NULL;    /* default container */
static int             g_running = 0;

/* Track the most recent signal that fired */
#define EVT_NONE       0
#define EVT_ACTIVATE   1
#define EVT_DESTROY    2
#define EVT_CLICKED    3

static int32_t g_last_event = EVT_NONE;

/* Storage for dynamically created widgets (simple indexed array) */
#define MAX_WIDGETS 256
static GtkWidget *g_widgets[MAX_WIDGETS];
static int32_t    g_widget_count = 0;

/* ── internal callbacks ──────────────────────────────────────────────── */

static void on_window_destroy(GtkWidget *widget, gpointer user_data);
static void on_button_clicked(GtkButton *button, gpointer user_data);

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    g_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_window), "Aria GTK4");
    gtk_window_set_default_size(GTK_WINDOW(g_window), 800, 600);

    /* Default vertical box container */
    g_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(g_window), g_box);

    gtk_window_present(GTK_WINDOW(g_window));
    g_signal_connect(g_window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    g_running = 1;
    g_last_event = EVT_ACTIVATE;
}

static void on_button_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    g_last_event = EVT_CLICKED;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    (void)user_data;
    g_running = 0;
    g_last_event = EVT_DESTROY;
}

/* ── init / quit ─────────────────────────────────────────────────────── */

int32_t aria_gtk4_init(const char *app_id) {
    g_app = gtk_application_new(app_id, G_APPLICATION_DEFAULT_FLAGS);
    if (!g_app) return 0;
    g_signal_connect(g_app, "activate", G_CALLBACK(on_activate), NULL);
    g_widget_count = 0;
    memset(g_widgets, 0, sizeof(g_widgets));
    return 1;
}

/* Register + activate the application (non-blocking).
 * Triggers the "activate" signal which creates the window,
 * then drains pending events so everything is realised.
 * Returns 0 on success, -1 on failure. */
int32_t aria_gtk4_run(void) {
    if (!g_app) return -1;
    GError *error = NULL;
    if (!g_application_register(G_APPLICATION(g_app), NULL, &error)) {
        if (error) g_error_free(error);
        return -1;
    }
    g_application_activate(G_APPLICATION(g_app));
    /* Drain pending events so the window is fully created */
    while (g_main_context_pending(NULL))
        g_main_context_iteration(NULL, FALSE);
    return 0;
}

void aria_gtk4_quit(void) {
    if (g_app) {
        g_object_unref(g_app);
        g_app = NULL;
    }
    g_window = NULL;
    g_box = NULL;
    g_running = 0;
}

/* ── main loop: iterative stepping ───────────────────────────────────── */

int32_t aria_gtk4_is_running(void) {
    return g_running;
}

int32_t aria_gtk4_iteration(int32_t blocking) {
    return (int32_t)g_main_context_iteration(NULL, blocking ? TRUE : FALSE);
}

/* ── events ──────────────────────────────────────────────────────────── */

int32_t aria_gtk4_get_last_event(void) {
    int32_t e = g_last_event;
    g_last_event = EVT_NONE;
    return e;
}

int32_t aria_gtk4_events_pending(void) {
    return (int32_t)g_main_context_pending(NULL);
}

/* ── window ──────────────────────────────────────────────────────────── */

void aria_gtk4_set_title(const char *title) {
    if (g_window) gtk_window_set_title(GTK_WINDOW(g_window), title);
}

void aria_gtk4_set_default_size(int32_t w, int32_t h) {
    if (g_window) gtk_window_set_default_size(GTK_WINDOW(g_window), w, h);
}

int32_t aria_gtk4_has_window(void) {
    return g_window ? 1 : 0;
}

void aria_gtk4_window_close(void) {
    if (g_window) gtk_window_close(GTK_WINDOW(g_window));
}

void aria_gtk4_window_present(void) {
    if (g_window) gtk_window_present(GTK_WINDOW(g_window));
}

void aria_gtk4_window_fullscreen(void) {
    if (g_window) gtk_window_fullscreen(GTK_WINDOW(g_window));
}

void aria_gtk4_window_unfullscreen(void) {
    if (g_window) gtk_window_unfullscreen(GTK_WINDOW(g_window));
}

void aria_gtk4_window_maximize(void) {
    if (g_window) gtk_window_maximize(GTK_WINDOW(g_window));
}

void aria_gtk4_window_minimize(void) {
    if (g_window) gtk_window_minimize(GTK_WINDOW(g_window));
}

/* ── widget creation ─────────────────────────────────────────────────── */

static int32_t register_widget(GtkWidget *w) {
    if (g_widget_count >= MAX_WIDGETS) return -1;
    int32_t id = g_widget_count;
    g_widgets[id] = w;
    g_widget_count++;
    return id;
}

/* Label */
int32_t aria_gtk4_add_label(const char *text) {
    if (!g_box) return -1;
    GtkWidget *label = gtk_label_new(text);
    gtk_box_append(GTK_BOX(g_box), label);
    return register_widget(label);
}

void aria_gtk4_label_set_text(int32_t id, const char *text) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_label_set_text(GTK_LABEL(g_widgets[id]), text);
    }
}

/* Button */
int32_t aria_gtk4_add_button(const char *label) {
    if (!g_box) return -1;
    GtkWidget *btn = gtk_button_new_with_label(label);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_button_clicked), NULL);
    gtk_box_append(GTK_BOX(g_box), btn);
    return register_widget(btn);
}

/* Entry (text input) */
int32_t aria_gtk4_add_entry(const char *placeholder) {
    if (!g_box) return -1;
    GtkWidget *entry = gtk_entry_new();
    if (placeholder && placeholder[0]) {
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    }
    gtk_box_append(GTK_BOX(g_box), entry);
    return register_widget(entry);
}

const char *aria_gtk4_entry_get_text(int32_t id) {
    if (id < 0 || id >= g_widget_count || !g_widgets[id]) return "";
    GtkEntryBuffer *buf = gtk_entry_get_buffer(GTK_ENTRY(g_widgets[id]));
    return gtk_entry_buffer_get_text(buf);
}

/* Separator */
int32_t aria_gtk4_add_separator(void) {
    if (!g_box) return -1;
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(g_box), sep);
    return register_widget(sep);
}

/* Check button */
int32_t aria_gtk4_add_check_button(const char *label) {
    if (!g_box) return -1;
    GtkWidget *cb = gtk_check_button_new_with_label(label);
    gtk_box_append(GTK_BOX(g_box), cb);
    return register_widget(cb);
}

int32_t aria_gtk4_check_button_get_active(int32_t id) {
    if (id < 0 || id >= g_widget_count || !g_widgets[id]) return 0;
    return gtk_check_button_get_active(GTK_CHECK_BUTTON(g_widgets[id])) ? 1 : 0;
}

void aria_gtk4_check_button_set_active(int32_t id, int32_t active) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(g_widgets[id]), active ? TRUE : FALSE);
    }
}

/* ── widget properties ───────────────────────────────────────────────── */

void aria_gtk4_widget_set_visible(int32_t id, int32_t visible) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_widget_set_visible(g_widgets[id], visible ? TRUE : FALSE);
    }
}

void aria_gtk4_widget_set_sensitive(int32_t id, int32_t sensitive) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_widget_set_sensitive(g_widgets[id], sensitive ? TRUE : FALSE);
    }
}

void aria_gtk4_widget_set_size_request(int32_t id, int32_t w, int32_t h) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_widget_set_size_request(g_widgets[id], w, h);
    }
}

void aria_gtk4_widget_add_css_class(int32_t id, const char *css_class) {
    if (id >= 0 && id < g_widget_count && g_widgets[id]) {
        gtk_widget_add_css_class(g_widgets[id], css_class);
    }
}

/* ── CSS styling ─────────────────────────────────────────────────────── */

void aria_gtk4_load_css(const char *css_string) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css_string);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/* ── box layout ──────────────────────────────────────────────────────── */

void aria_gtk4_box_set_spacing(int32_t spacing) {
    if (g_box) gtk_box_set_spacing(GTK_BOX(g_box), spacing);
}

void aria_gtk4_box_set_homogeneous(int32_t homogeneous) {
    if (g_box) gtk_box_set_homogeneous(GTK_BOX(g_box), homogeneous ? TRUE : FALSE);
}
