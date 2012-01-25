//
// shoes/native-gtk.c
// GTK+ code for Shoes.
//
#include "shoes/app.h"
#include "shoes/ruby.h"
#include "shoes/config.h"
#include "shoes/world.h"
#include "shoes/native.h"
#include "shoes/internal.h"

#include <fontconfig/fontconfig.h>
#include <curl/curl.h>
#include <pthread.h>

#define GTK_CHILD(child, ptr) \
  GList *children = gtk_container_get_children(GTK_CONTAINER(ptr)); \
  child = children->data

#define HEIGHT_PAD 0

#define SHOES_GTK_INVISIBLE_CHAR (gunichar)0x2022

static VALUE
shoes_make_font_list(FcFontSet *fonts, VALUE ary)
{
  int i = 0;
  for (i = 0; i < fonts->nfont; i++)
  {
    FcValue val;
    FcPattern *p = fonts->fonts[i];
    if (FcPatternGet(p, FC_FAMILY, 0, &val) == FcResultMatch)
      rb_ary_push(ary, rb_str_new2((char *)val.u.s));
  }
  rb_funcall(ary, rb_intern("uniq!"), 0);
  rb_funcall(ary, rb_intern("sort!"), 0);
  return ary;
}

VALUE
shoes_font_list()
{
  VALUE ary = rb_ary_new();
  FcConfig *fc = FcConfigGetCurrent();
  FcFontSet *fonts = FcConfigGetFonts(fc, FcSetApplication);
  if (fonts) shoes_make_font_list(fonts, ary);
  fonts = FcConfigGetFonts(fc, FcSetSystem);
  if (fonts) shoes_make_font_list(fonts, ary);
  return ary;
}

VALUE
shoes_load_font(const char *filename)
{
  FcConfig *fc = FcConfigGetCurrent();
  FcFontSet *fonts = FcFontSetCreate();
  if (!FcFileScan(fonts, NULL, NULL, NULL, (const FcChar8 *)filename, FcTrue))
    return Qnil;

  VALUE ary = rb_ary_new();
  shoes_make_font_list(fonts, ary);
  FcFontSetDestroy(fonts);

  if (!FcConfigAppFontAddFile(fc, (const FcChar8 *)filename))
    return Qnil;

  // refresh the FONTS list
  shoes_update_fonts(shoes_font_list());
  return ary;
}

void shoes_native_init()
{
  curl_global_init(CURL_GLOBAL_ALL);
  gtk_init(NULL, NULL);
}

void shoes_native_cleanup(shoes_world_t *world)
{
  curl_global_cleanup();
}

void shoes_native_quit()
{
  gtk_main_quit();
}

void shoes_get_time(SHOES_TIME *ts)
{
  clock_gettime(CLOCK_REALTIME, ts);
}

unsigned long shoes_diff_time(SHOES_TIME *start, SHOES_TIME *end)
{
  unsigned long usec;
  if ((end->tv_nsec-start->tv_nsec)<0) {
    usec = (end->tv_sec-start->tv_sec - 1) * 1000;
    usec += (1000000000 + end->tv_nsec - start->tv_nsec) / 1000000;
  } else {
    usec = (end->tv_sec - start->tv_sec) * 1000;
    usec += (end->tv_nsec - start->tv_nsec) / 1000000;
  }
  return usec;
}

typedef struct {
  unsigned int name;
  VALUE obj;
  void *data;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int ret;
} shoes_gtk_msg;

static gboolean 
shoes_gtk_catch_message(gpointer user) {
  shoes_gtk_msg *msg = (shoes_gtk_msg *)user;
  pthread_mutex_lock(&msg->mutex);
  msg->ret = shoes_catch_message(msg->name, msg->obj, msg->data);
  pthread_cond_signal(&msg->cond);
  pthread_mutex_unlock(&msg->mutex);
  return FALSE;
}

int shoes_throw_message(unsigned int name, VALUE obj, void *data)
{
  int ret;
  shoes_gtk_msg *msg = SHOE_ALLOC(shoes_gtk_msg);
  msg->name = name;
  msg->obj = obj;
  msg->data = data;
  pthread_mutex_init(&msg->mutex, NULL);
  pthread_cond_init(&msg->cond, NULL);
  msg->ret = 0;

  pthread_mutex_lock(&msg->mutex);
  g_idle_add_full(G_PRIORITY_DEFAULT, shoes_gtk_catch_message, msg, NULL);
  pthread_cond_wait(&msg->cond, &msg->mutex);
  ret = msg->ret;
  pthread_mutex_unlock(&msg->mutex);

  free(msg);
  return ret;
}

void shoes_native_slot_mark(SHOES_SLOT_OS *slot) {}
void shoes_native_slot_reset(SHOES_SLOT_OS *slot) {}
void shoes_native_slot_clear(shoes_canvas *canvas)
{
  if (canvas->slot->vscroll)
  {
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(canvas->slot->vscroll));
    gtk_adjustment_set_value(adj, adj->lower);
  }
}

void shoes_native_slot_paint(SHOES_SLOT_OS *slot)
{
  gtk_widget_queue_draw(slot->canvas);
}

void shoes_native_slot_lengthen(SHOES_SLOT_OS *slot, int height, int endy)
{
  if (slot->vscroll)
  {
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(slot->vscroll));
    if (adj->upper != (gdouble)endy)
    {
      gtk_range_set_range(GTK_RANGE(slot->vscroll), 0., (gdouble)endy);
      if (adj->page_size >= adj->upper)
        gtk_widget_hide(slot->vscroll);
      else
        gtk_widget_show(slot->vscroll);
    }
  }
}

void shoes_native_slot_scroll_top(SHOES_SLOT_OS *slot)
{
  if (slot->vscroll)
    gtk_range_set_value(GTK_RANGE(slot->vscroll), slot->scrolly);
}

int shoes_native_slot_gutter(SHOES_SLOT_OS *slot)
{
  if (slot->vscroll)
  {
    GtkRequisition req;
    gtk_widget_size_request(slot->vscroll, &req);
    return req.width;
  }
  return 0;
}

void shoes_native_remove_item(SHOES_SLOT_OS *slot, VALUE item, char c)
{
}

//
// Window-level events
//
static gboolean 
shoes_app_gtk_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data) 
{  
  GdkModifierType state; 
  shoes_app *app = (shoes_app *)data; 
  if (!event->is_hint) 
  { 
    shoes_canvas *canvas;
    Data_Get_Struct(app->canvas, shoes_canvas, canvas);
    state = (GdkModifierType)event->state; 
    shoes_app_motion(app, (int)event->x, (int)event->y + canvas->slot->scrolly);
  } 
  return TRUE; 
} 

static gboolean
shoes_app_gtk_button(GtkWidget *widget, GdkEventButton *event, gpointer data)
{ 
  shoes_app *app = (shoes_app *)data; 
  shoes_canvas *canvas;
  Data_Get_Struct(app->canvas, shoes_canvas, canvas);
  if (event->type == GDK_BUTTON_PRESS)
  {
    shoes_app_click(app, event->button, event->x, event->y + canvas->slot->scrolly);
  }
  else if (event->type == GDK_BUTTON_RELEASE)
  {
    shoes_app_release(app, event->button, event->x, event->y + canvas->slot->scrolly);
  }
  return TRUE;
}

static gboolean
shoes_app_gtk_wheel(GtkWidget *widget, GdkEventScroll *event, gpointer data)
{ 
  ID wheel;
  shoes_app *app = (shoes_app *)data; 
  switch (event->direction)
  {
    case GDK_SCROLL_UP:    wheel = s_up;    break;
    case GDK_SCROLL_DOWN:  wheel = s_down;  break;
    case GDK_SCROLL_LEFT:  wheel = s_left;  break;
    case GDK_SCROLL_RIGHT: wheel = s_right; break;
    default: return TRUE;
  }

  shoes_app_wheel(app, wheel, event->x, event->y);
  return TRUE;
}

static void
shoes_app_gtk_paint (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{ 
  shoes_app *app = (shoes_app *)data;
  gtk_window_get_size(GTK_WINDOW(app->os.window), &app->width, &app->height);
  shoes_canvas_size(app->canvas, app->width, app->height);
}

#define KEY_SYM(name, sym) \
  else if (event->keyval == GDK_##name) \
    v = ID2SYM(rb_intern("" # sym))

static gboolean
shoes_app_gtk_keypress (GtkWidget *widget, GdkEventKey *event, gpointer data)
{ 
  VALUE v = Qnil;
  guint modifiers = event->state;
  shoes_app *app = (shoes_app *)data;
  if (event->keyval == GDK_Return)
  {
    v = rb_str_new2("\n");
  }
  KEY_SYM(Escape, escape);
  else if (event->length > 0)
  {
    if ((event->state & GDK_CONTROL_MASK) || (event->state & GDK_MOD1_MASK))
    {
      gint len;
      gunichar ch;
      char chbuf[7] = {0};

      ch = gdk_keyval_to_unicode(event->keyval);
      len = g_unichar_to_utf8(ch, chbuf);
      chbuf[len] = '\0';

      v = ID2SYM(rb_intern(chbuf));
      if (modifiers & GDK_SHIFT_MASK) modifiers ^= GDK_SHIFT_MASK;
    }
    else
    {
      if (event->string[0] == '\r' && event->length == 1)
        v = rb_str_new2("\n");
      else
        v = rb_str_new(event->string, event->length);
    }
  }
  KEY_SYM(Insert, insert);
  KEY_SYM(Delete, delete);
  KEY_SYM(BackSpace, backspace);
  KEY_SYM(Tab, tab);
  KEY_SYM(ISO_Left_Tab, tab);
  KEY_SYM(Page_Up, page_up);
  KEY_SYM(Page_Down, page_down);
  KEY_SYM(Home, home);
  KEY_SYM(End, end);
  KEY_SYM(Left, left);
  KEY_SYM(Up, up);
  KEY_SYM(Right, right);
  KEY_SYM(Down, down);
  KEY_SYM(F1, f1);
  KEY_SYM(F2, f2);
  KEY_SYM(F3, f3);
  KEY_SYM(F4, f4);
  KEY_SYM(F5, f5);
  KEY_SYM(F6, f6);
  KEY_SYM(F7, f7);
  KEY_SYM(F8, f8);
  KEY_SYM(F9, f9);
  KEY_SYM(F10, f10);
  KEY_SYM(F11, f11);
  KEY_SYM(F12, f12);

  if (v != Qnil) {
    if (event->type == GDK_KEY_PRESS) {
      shoes_app_keydown(app, v);

      if (event->keyval == GDK_Return)
        if ((event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0)
          v = ID2SYM(rb_intern("enter"));

      if (SYMBOL_P(v))
      {
        if (modifiers & GDK_MOD1_MASK)
          KEY_STATE(alt);
        if (modifiers & GDK_SHIFT_MASK)
          KEY_STATE(shift);
        if (modifiers & GDK_CONTROL_MASK)
          KEY_STATE(control);
      }

      shoes_app_keypress(app, v);
    } else {
      shoes_app_keyup(app, v);
    }
  } 

  return FALSE;
}

static gboolean
shoes_app_gtk_quit(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  shoes_app *app = (shoes_app *)data;
  if (shoes_app_remove(app))
    gtk_main_quit();
  return FALSE;
}

static void
shoes_canvas_gtk_paint_children(GtkWidget *widget, gpointer data)
{
  shoes_canvas *canvas = (shoes_canvas *)data;
  gtk_container_propagate_expose(GTK_CONTAINER(canvas->slot->canvas), widget, canvas->slot->expose);
}

static void
shoes_canvas_gtk_paint(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{ 
  VALUE c = (VALUE)data;
  shoes_canvas *canvas;
  INFO("EXPOSE: (%d, %d) (%d, %d) %lu, %d, %d\n", event->area.x, event->area.y,
    event->area.width, event->area.height, c, (int)event->send_event, event->count);
  Data_Get_Struct(c, shoes_canvas, canvas);

  //
  // Since I'm using a GtkFixed container, I need to force it to be clipped on its boundaries.
  // This could be done by using a whole lot of gdk_window_begin_paint_region calls, but that
  // would also mean masking every region for every element... This approach is simple.  Clip
  // the expose region and pass it on.
  //
  canvas->slot->expose = event;
  GdkRegion *region = event->region;
  GdkRectangle rect = event->area;
  event->region = gdk_region_rectangle(&canvas->slot->canvas->allocation);
  gdk_region_intersect(event->region, region);
  gdk_region_get_clipbox(event->region, &event->area);

  shoes_canvas_paint(c);
  gtk_container_forall(GTK_CONTAINER(widget), shoes_canvas_gtk_paint_children, canvas);

  //
  // Restore the full region to the event.
  //
  gdk_region_destroy(event->region);
  event->region = region;
  event->area = rect;
  canvas->slot->expose = NULL;
}

static void
shoes_canvas_gtk_size(GtkWidget *widget, GtkAllocation *size, gpointer data)
{
  VALUE c = (VALUE)data;
  shoes_canvas *canvas;
  Data_Get_Struct(c, shoes_canvas, canvas);
  if (canvas->slot->vscroll && 
    (size->height != canvas->slot->scrollh || size->width != canvas->slot->scrollw))
  {
    GtkAdjustment *adj = gtk_range_get_adjustment(GTK_RANGE(canvas->slot->vscroll));
    gtk_widget_set_size_request(canvas->slot->vscroll, -1, size->height);
    gtk_fixed_move(GTK_FIXED(canvas->slot->canvas), canvas->slot->vscroll,
      size->width - canvas->slot->vscroll->allocation.width, 0);
    adj->page_size = size->height;
    adj->page_increment = size->height - 32;
    if (adj->page_size >= adj->upper)
      gtk_widget_hide(canvas->slot->vscroll);
    else
      gtk_widget_show(canvas->slot->vscroll);
    canvas->slot->scrollh = size->height;
    canvas->slot->scrollw = size->width;
  }
}

static void
shoes_canvas_gtk_scroll(GtkRange *r, gpointer data)
{ 
  VALUE c = (VALUE)data;
  shoes_canvas *canvas;
  Data_Get_Struct(c, shoes_canvas, canvas);
  canvas->slot->scrolly = (int)gtk_range_get_value(r);
  shoes_slot_repaint(canvas->app->slot);
}

static gint                                                           
shoes_app_g_poll (GPollFD *fds, guint nfds, gint timeout)
{
  struct timeval tv;
  fd_set rset, wset, xset;
  GPollFD *f;
  int ready;
  int maxfd = 0;

  FD_ZERO (&rset);
  FD_ZERO (&wset);
  FD_ZERO (&xset);

  for (f = fds; f < &fds[nfds]; ++f)
     if (f->fd >= 0)
     {
       if (f->events & G_IO_IN)
         FD_SET (f->fd, &rset);
       if (f->events & G_IO_OUT)
         FD_SET (f->fd, &wset);
       if (f->events & G_IO_PRI)
         FD_SET (f->fd, &xset);
       if (f->fd > maxfd && (f->events & (G_IO_IN|G_IO_OUT|G_IO_PRI)))
         maxfd = f->fd;
     }

  //
  // If we poll indefinitely, then the window updates will
  // pile up for as long as Ruby is churning away.
  //
  // Give Ruby half-seconds in which to work, in order to
  // keep it from completely blocking the GUI.
  //
  if (timeout == -1 || timeout > 500)
    timeout = 500;
  
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  ready = rb_thread_select (maxfd + 1, &rset, &wset, &xset, &tv);
  if (ready > 0)
     for (f = fds; f < &fds[nfds]; ++f)
     {
       f->revents = 0;
       if (f->fd >= 0)
       {
         if (FD_ISSET (f->fd, &rset))
           f->revents |= G_IO_IN;
         if (FD_ISSET (f->fd, &wset))
           f->revents |= G_IO_OUT;
         if (FD_ISSET (f->fd, &xset))
           f->revents |= G_IO_PRI;
       }
     }

  return ready;
}

shoes_code
shoes_app_cursor(shoes_app *app, ID cursor)
{
  if (app->os.window == NULL || app->os.window->window == NULL || app->cursor == cursor)
    goto done;

  GdkCursor *c;
  if (cursor == s_hand || cursor == s_link)
  {
    c = gdk_cursor_new(GDK_HAND2);
  }
  else if (cursor == s_arrow)
  {
    c = gdk_cursor_new(GDK_ARROW);
  }
  else if (cursor == s_text)
  {
    c = gdk_cursor_new(GDK_XTERM);
  }
  else
    goto done;

  gdk_window_set_cursor(app->os.window->window, c);

  app->cursor = cursor;

done:
  return SHOES_OK;
}

void
shoes_native_app_resized(shoes_app *app)
{
  if (app->os.window != NULL)
    gtk_widget_set_size_request(app->os.window, app->width, app->height);
}

void
shoes_native_app_title(shoes_app *app, char *msg)
{
  gtk_window_set_title(GTK_WINDOW(app->os.window), _(msg));
}

void
shoes_native_app_fullscreen(shoes_app *app, char yn)
{
  gtk_window_set_keep_above(GTK_WINDOW(app->os.window), (gboolean)yn);
  if (yn)
    gtk_window_fullscreen(GTK_WINDOW(app->os.window));
  else
    gtk_window_unfullscreen(GTK_WINDOW(app->os.window));
}

shoes_code
shoes_native_app_open(shoes_app *app, char *path, int dialog)
{
  char icon_path[SHOES_BUFSIZE];
  shoes_app_gtk *gk = &app->os;

  sprintf(icon_path, "%s/static/app-icon.png", shoes_world->path);
  gtk_window_set_default_icon_from_file(icon_path, NULL);
  gk->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(gk->window), GTK_WIN_POS_CENTER);
  if (app->minwidth < app->width || app->minheight < app->height)
  {
    GdkGeometry hints;
    hints.min_width = app->minwidth;
    hints.min_height = app->minheight;
    gtk_window_set_geometry_hints(GTK_WINDOW(gk->window), NULL,
      &hints, GDK_HINT_MIN_SIZE);
  }
  if (!app->resizable)
    gtk_window_set_resizable(GTK_WINDOW(gk->window), FALSE);
  if (app->fullscreen)
    shoes_native_app_fullscreen(app, 1);
  gtk_widget_set_events(gk->window, GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  g_signal_connect(G_OBJECT(gk->window), "size-allocate",
                   G_CALLBACK(shoes_app_gtk_paint), app);
  g_signal_connect(G_OBJECT(gk->window), "motion-notify-event", 
                   G_CALLBACK(shoes_app_gtk_motion), app);
  g_signal_connect(G_OBJECT(gk->window), "button-press-event",
                   G_CALLBACK(shoes_app_gtk_button), app);
  g_signal_connect(G_OBJECT(gk->window), "button-release-event",
                   G_CALLBACK(shoes_app_gtk_button), app);
  g_signal_connect(G_OBJECT(gk->window), "scroll-event",
                   G_CALLBACK(shoes_app_gtk_wheel), app);
  g_signal_connect(G_OBJECT(gk->window), "key-press-event",
                   G_CALLBACK(shoes_app_gtk_keypress), app);
  g_signal_connect(G_OBJECT(gk->window), "key-release-event",
                   G_CALLBACK(shoes_app_gtk_keypress), app);
  g_signal_connect(G_OBJECT(gk->window), "delete-event",
                   G_CALLBACK(shoes_app_gtk_quit), app);
  app->slot->canvas = gk->window;
  return SHOES_OK;
}

void
shoes_native_app_show(shoes_app *app)
{
  gtk_widget_show_all(app->os.window);
}

void
shoes_native_loop()
{
  g_main_set_poll_func(shoes_app_g_poll);
  gtk_main();
}

void
shoes_native_app_close(shoes_app *app)
{
  shoes_app_gtk_quit(app->os.window, NULL, (gpointer)app);
  gtk_widget_destroy(app->os.window);
  app->os.window = NULL;
}

void
shoes_browser_open(char *url)
{
  VALUE browser = rb_str_new2("/etc/alternatives/x-www-browser '");
  rb_str_cat2(browser, url);
  rb_str_cat2(browser, "' 2>/dev/null &");
  shoes_sys(RSTRING_PTR(browser), 1);
}

void
shoes_slot_init(VALUE c, SHOES_SLOT_OS *parent, int x, int y, int width, int height, int scrolls, int toplevel)
{
  shoes_canvas *canvas;
  SHOES_SLOT_OS *slot;
  Data_Get_Struct(c, shoes_canvas, canvas);

  slot = shoes_slot_alloc(canvas, parent, toplevel);
  slot->canvas = gtk_fixed_new();
  INFO("shoes_slot_init(%lu)\n", c);
  g_signal_connect(G_OBJECT(slot->canvas), "expose-event",
                   G_CALLBACK(shoes_canvas_gtk_paint), (gpointer)c);
  g_signal_connect(G_OBJECT(slot->canvas), "size-allocate",
                   G_CALLBACK(shoes_canvas_gtk_size), (gpointer)c);
  if (toplevel)
    gtk_container_add(GTK_CONTAINER(parent->canvas), slot->canvas);
  else
    gtk_fixed_put(GTK_FIXED(parent->canvas), slot->canvas, x, y);

  slot->scrollh = slot->scrollw = 0;
  slot->vscroll = NULL;
  if (scrolls)
  {
    slot->vscroll = gtk_vscrollbar_new(NULL);
    gtk_range_get_adjustment(GTK_RANGE(slot->vscroll))->step_increment = 16;
    gtk_range_get_adjustment(GTK_RANGE(slot->vscroll))->page_increment = height - 32;
    g_signal_connect(G_OBJECT(slot->vscroll), "value-changed",
                     G_CALLBACK(shoes_canvas_gtk_scroll), (gpointer)c);
    gtk_fixed_put(GTK_FIXED(slot->canvas), slot->vscroll, -100, -100);
  }

  gtk_widget_set_size_request(slot->canvas, width, height);
  slot->expose = NULL;
  if (toplevel) shoes_canvas_size(c, width, height);
  else
  {
    gtk_widget_show_all(slot->canvas);
    canvas->width = 100;
    canvas->height = 100;
  }
}

void
shoes_slot_destroy(shoes_canvas *canvas, shoes_canvas *pc)
{
  if (canvas->slot->vscroll)
    gtk_container_remove(GTK_CONTAINER(canvas->slot->canvas), canvas->slot->vscroll);
  gtk_container_remove(GTK_CONTAINER(pc->slot->canvas), canvas->slot->canvas);
}

cairo_t *
shoes_cairo_create(shoes_canvas *canvas)
{
  cairo_t *cr = gdk_cairo_create(canvas->slot->canvas->window);
  if (canvas->slot->expose != NULL)
  {
    GdkRegion *region = gdk_region_rectangle(&canvas->slot->canvas->allocation);
    gdk_region_intersect(region, canvas->slot->expose->region);
    gdk_cairo_region(cr, region);
    gdk_region_destroy(region);
    cairo_clip(cr);
    cairo_translate(cr, canvas->slot->canvas->allocation.x, canvas->slot->canvas->allocation.y - canvas->slot->scrolly);
  }
  return cr;
}

void
shoes_cairo_destroy(shoes_canvas *canvas)
{
}

void
shoes_group_clear(SHOES_GROUP_OS *group)
{
  group->radios = NULL;
  group->layout = NULL;
}

void
shoes_native_canvas_place(shoes_canvas *self_t, shoes_canvas *pc)
{
  int x, y, newy;
  GtkAllocation *a = &self_t->slot->canvas->allocation;
  gtk_widget_translate_coordinates(self_t->slot->canvas, pc->slot->canvas, 0, 0, &x, &y);
  newy = (self_t->place.iy + self_t->place.dy) - pc->slot->scrolly;

  if (x != self_t->place.ix + self_t->place.dx || y != newy)
    gtk_fixed_move(GTK_FIXED(pc->slot->canvas), self_t->slot->canvas, 
        self_t->place.ix + self_t->place.dx, newy);

  if (a->width != self_t->place.iw || a->height != self_t->place.ih)
    gtk_widget_set_size_request(self_t->slot->canvas, self_t->place.iw, self_t->place.ih);
}

void
shoes_native_canvas_resize(shoes_canvas *canvas)
{
}

static void
shoes_widget_changed(GtkWidget *ref, gpointer data)
{ 
  VALUE self = (VALUE)data;
  shoes_control_send(self, s_change);
}

void
shoes_native_control_hide(SHOES_CONTROL_REF ref)
{
  gtk_widget_hide(ref);
}

void
shoes_native_control_show(SHOES_CONTROL_REF ref)
{
  gtk_widget_show(ref);
}

void
shoes_native_control_position(SHOES_CONTROL_REF ref, shoes_place *p1, VALUE self,
  shoes_canvas *canvas, shoes_place *p2)
{
  PLACE_COORDS();
  gtk_widget_set_size_request(ref, p2->iw, p2->ih);
  gtk_fixed_put(GTK_FIXED(canvas->slot->canvas), 
    ref, p2->ix + p2->dx, p2->iy + p2->dy);
  gtk_widget_show_all(ref);
}

void
shoes_native_control_repaint(SHOES_CONTROL_REF ref, shoes_place *p1,
  shoes_canvas *canvas, shoes_place *p2)
{
  p2->iy -= canvas->slot->scrolly;
  if (CHANGED_COORDS()) {
    PLACE_COORDS();
    gtk_fixed_move(GTK_FIXED(canvas->slot->canvas), 
      ref, p2->ix + p2->dx, p2->iy + p2->dy);
    gtk_widget_set_size_request(ref, p2->iw, p2->ih);
  }
  p2->iy += canvas->slot->scrolly;
}

void
shoes_native_control_focus(SHOES_CONTROL_REF ref)
{
  if (GTK_WIDGET_CAN_FOCUS(ref)) gtk_widget_grab_focus(ref);
}

void
shoes_native_control_state(SHOES_CONTROL_REF ref, gboolean sensitive, gboolean setting)
{
  gtk_widget_set_sensitive(ref, sensitive);
  if (GTK_IS_EDITABLE(ref))
    gtk_editable_set_editable(GTK_EDITABLE(ref), setting);
  else if (GTK_IS_SCROLLED_WINDOW(ref))
  {
    GtkWidget *textview;
    GTK_CHILD(textview, ref);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), setting);
  }
}

void
shoes_native_control_remove(SHOES_CONTROL_REF ref, shoes_canvas *canvas)
{
  gtk_container_remove(GTK_CONTAINER(canvas->slot->canvas), ref);
}

void
shoes_native_control_free(SHOES_CONTROL_REF ref)
{
  //
  // no need to free gtk widgets, since gtk seems
  // to garbage collect them fine.  and memory
  // addresses often get reused.
  //
}

SHOES_SURFACE_REF
shoes_native_surface_new(shoes_canvas *canvas, VALUE self, shoes_place *place)
{
  return gtk_drawing_area_new();
}

void
shoes_native_surface_position(SHOES_SURFACE_REF ref, shoes_place *p1, 
  VALUE self, shoes_canvas *canvas, shoes_place *p2)
{
  shoes_native_control_position(ref, p1, self, canvas, p2);
}

void
shoes_native_surface_hide(SHOES_SURFACE_REF ref)
{
  shoes_native_control_hide(ref);
}

void
shoes_native_surface_show(SHOES_SURFACE_REF ref)
{
  shoes_native_control_show(ref);
}

void
shoes_native_surface_remove(shoes_canvas *canvas, SHOES_SURFACE_REF ref)
{
  gtk_container_remove(GTK_CONTAINER(canvas->slot->canvas), ref);
}

static gboolean
shoes_button_gtk_clicked(GtkButton *button, gpointer data)
{ 
  VALUE self = (VALUE)data;
  shoes_control_send(self, s_click);
  return TRUE;
}

SHOES_CONTROL_REF
shoes_native_button(VALUE self, shoes_canvas *canvas, shoes_place *place, char *msg)
{
  SHOES_CONTROL_REF ref = gtk_button_new_with_label(_(msg));
  g_signal_connect(G_OBJECT(ref), "clicked",
                   G_CALLBACK(shoes_button_gtk_clicked),
                   (gpointer)self);
  return ref;
}

void
shoes_native_secrecy(SHOES_CONTROL_REF ref)
{
  gtk_entry_set_visibility(GTK_ENTRY(ref), FALSE);
  gtk_entry_set_invisible_char(GTK_ENTRY(ref), SHOES_GTK_INVISIBLE_CHAR);
}

SHOES_CONTROL_REF
shoes_native_edit_line(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  SHOES_CONTROL_REF ref = gtk_entry_new();
  if (RTEST(ATTR(attr, secret))) shoes_native_secrecy(ref);
  gtk_entry_set_text(GTK_ENTRY(ref), _(msg));
  g_signal_connect(G_OBJECT(ref), "changed",
                   G_CALLBACK(shoes_widget_changed),
                   (gpointer)self);
  return ref;
}

VALUE
shoes_native_edit_line_get_text(SHOES_CONTROL_REF ref)
{
  return rb_str_new2(gtk_entry_get_text(GTK_ENTRY(ref)));
}

void
shoes_native_edit_line_set_text(SHOES_CONTROL_REF ref, char *msg)
{
  gtk_entry_set_text(GTK_ENTRY(ref), _(msg));
}

SHOES_CONTROL_REF
shoes_native_edit_box(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  GtkTextBuffer *buffer;
  GtkWidget* textview = gtk_text_view_new();
  SHOES_CONTROL_REF ref = gtk_scrolled_window_new(NULL, NULL);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  gtk_text_buffer_set_text(buffer, _(msg), -1);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ref),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(ref), GTK_SHADOW_IN);
  gtk_container_add(GTK_CONTAINER(ref), textview);
  g_signal_connect(G_OBJECT(buffer), "changed",
                   G_CALLBACK(shoes_widget_changed),
                   (gpointer)self);
  return ref;
}

VALUE
shoes_native_edit_box_get_text(SHOES_CONTROL_REF ref)
{
  GtkWidget *textview;
  GTK_CHILD(textview, ref);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  GtkTextIter begin, end;
  gtk_text_buffer_get_bounds(buffer, &begin, &end);
  return rb_str_new2(gtk_text_buffer_get_text(buffer, &begin, &end, TRUE));
}

void
shoes_native_edit_box_set_text(SHOES_CONTROL_REF ref, char *msg)
{
  GtkWidget *textview;
  GTK_CHILD(textview, ref);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
  gtk_text_buffer_set_text(buffer, _(msg), -1);
}

SHOES_CONTROL_REF
shoes_native_list_box(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
   SHOES_CONTROL_REF ref = gtk_combo_box_new_text();
   g_signal_connect(G_OBJECT(ref), "changed",
                    G_CALLBACK(shoes_widget_changed),
                    (gpointer)self);
   return ref;
}

void
shoes_native_list_box_update(SHOES_CONTROL_REF combo, VALUE ary)
{
  long i;
  gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo))));
  for (i = 0; i < RARRAY_LEN(ary); i++)
  {
    VALUE msg = shoes_native_to_s(rb_ary_entry(ary, i));
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _(RSTRING_PTR(msg)));
  }
}

VALUE
shoes_native_list_box_get_active(SHOES_CONTROL_REF ref, VALUE items)
{
  int sel = gtk_combo_box_get_active(GTK_COMBO_BOX(ref));
  if (sel >= 0)
    return rb_ary_entry(items, sel);
  return Qnil;
}

void
shoes_native_list_box_set_active(SHOES_CONTROL_REF combo, VALUE ary, VALUE item)
{
  int idx = rb_ary_index_of(ary, item);
  if (idx < 0) return;
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), idx);
}

SHOES_CONTROL_REF
shoes_native_progress(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  SHOES_CONTROL_REF ref = gtk_progress_bar_new();
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ref), _(msg));
  return ref;
}

double
shoes_native_progress_get_fraction(SHOES_CONTROL_REF ref)
{
  return gtk_progress_bar_get_fraction(GTK_PROGRESS_BAR(ref));
}

void
shoes_native_progress_set_fraction(SHOES_CONTROL_REF ref, double perc)
{
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ref), perc);
}

SHOES_CONTROL_REF
shoes_native_slider(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  SHOES_CONTROL_REF ref = gtk_hscale_new_with_range(0., 1., 0.01);
  gtk_scale_set_draw_value(GTK_SCALE(ref), FALSE);
  g_signal_connect(G_OBJECT(ref), "value-changed",
                   G_CALLBACK(shoes_widget_changed), (gpointer)self);
  return ref;
}

double
shoes_native_slider_get_fraction(SHOES_CONTROL_REF ref)
{
  return gtk_range_get_value(GTK_RANGE(ref));
}

void
shoes_native_slider_set_fraction(SHOES_CONTROL_REF ref, double perc)
{
  gtk_range_set_value(GTK_RANGE(ref), perc);
}

SHOES_CONTROL_REF
shoes_native_check(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  SHOES_CONTROL_REF ref = gtk_check_button_new();
  g_signal_connect(G_OBJECT(ref), "clicked",
                   G_CALLBACK(shoes_button_gtk_clicked),
                   (gpointer)self);
  return ref;
}

VALUE
shoes_native_check_get(SHOES_CONTROL_REF ref)
{
  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ref)) ? Qtrue : Qfalse;
}

void
shoes_native_check_set(SHOES_CONTROL_REF ref, int on)
{
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ref), on ? TRUE : FALSE);
}

SHOES_CONTROL_REF
shoes_native_radio(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, VALUE group)
{
  SHOES_CONTROL_REF ref;
  GSList *list = NULL;
  if (!NIL_P(group))
  {
    shoes_control *lctrl;
    VALUE leader = rb_ary_entry(group, 0);
    Data_Get_Struct(leader, shoes_control, lctrl);
    list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(lctrl->ref));
  }
  ref = gtk_radio_button_new(list);
  g_signal_connect(G_OBJECT(ref), "clicked",
                   G_CALLBACK(shoes_button_gtk_clicked),
                   (gpointer)self);
  return ref;
}

static gboolean
shoes_gtk_animate(gpointer data)
{
  VALUE timer = (VALUE)data;
  shoes_timer *self_t;
  Data_Get_Struct(timer, shoes_timer, self_t);
  if (self_t->started == ANIM_STARTED)
    shoes_timer_call(timer);
  return self_t->started == ANIM_STARTED;
}

void
shoes_native_timer_remove(shoes_canvas *canvas, SHOES_TIMER_REF ref)
{
  g_source_remove(ref);
}

SHOES_TIMER_REF
shoes_native_timer_start(VALUE self, shoes_canvas *canvas, unsigned int interval)
{
  return g_timeout_add(interval, shoes_gtk_animate, (gpointer)self);
}

VALUE
shoes_native_clipboard_get(shoes_app *app)
{
  GtkClipboard *primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  if (gtk_clipboard_wait_is_text_available(primary))
  {
    gchar *string = gtk_clipboard_wait_for_text(primary);
    return rb_str_new2(string);
  }
  return Qnil;
}

void
shoes_native_clipboard_set(shoes_app *app, VALUE string)
{
  GtkClipboard *primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(primary, RSTRING_PTR(string), RSTRING_LEN(string));
}

VALUE
shoes_native_to_s(VALUE text)
{
  text = rb_funcall(text, s_to_s, 0);
  return text;
}

VALUE
shoes_native_window_color(shoes_app *app)
{
  GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(APP_WINDOW(app)));
  GdkColor bg = style->bg[GTK_STATE_NORMAL];
  return shoes_color_new(bg.red / 257, bg.green / 257, bg.blue / 257 , SHOES_COLOR_OPAQUE);
}

VALUE
shoes_native_dialog_color(shoes_app *app)
{
  GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(APP_WINDOW(app)));
  GdkColor bg = style->bg[GTK_STATE_NORMAL];
  return shoes_color_new(bg.red / 257, bg.green / 257, bg.blue / 257 , SHOES_COLOR_OPAQUE);
}

VALUE
shoes_dialog_alert(VALUE self, VALUE msg)
{
  GLOBAL_APP(app);
  msg = shoes_native_to_s(msg);
  GtkWidget *dialog = gtk_message_dialog_new_with_markup(
    APP_WINDOW(app), GTK_DIALOG_MODAL,
    GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "<span size='larger'>%s</span>\n\n%s",
    _(dialog_title_says), RSTRING_PTR(msg));
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  return Qnil;
}

VALUE
shoes_dialog_ask(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  VALUE answer = Qnil;
  rb_parse_args(argc, argv, "s|h", &args);
  GLOBAL_APP(app);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_(dialog_title),
    APP_WINDOW(app), GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
  gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);
  GtkWidget *question = gtk_label_new(RSTRING_PTR(args.a[0]));
  gtk_misc_set_alignment(GTK_MISC(question), 0, 0);
  GtkWidget *_answer = gtk_entry_new();
  if (RTEST(ATTR(args.a[1], secret))) shoes_native_secrecy(_answer);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), question, FALSE, FALSE, 3);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), _answer, FALSE, TRUE, 3);
  gtk_widget_show_all(dialog);
  gint result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_OK)
  {
    const gchar *txt = gtk_entry_get_text(GTK_ENTRY(_answer));
    answer = rb_str_new2(txt);
  }
  gtk_widget_destroy(dialog);
  return answer;
}

VALUE
shoes_dialog_confirm(VALUE self, VALUE quiz)
{
  VALUE answer = Qfalse;
  GLOBAL_APP(app);
  quiz = shoes_native_to_s(quiz);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(_(dialog_title),
    APP_WINDOW(app), GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 6);
  gtk_container_set_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 6);
  GtkWidget *question = gtk_label_new(RSTRING_PTR(quiz));
  gtk_misc_set_alignment(GTK_MISC(question), 0, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), question, FALSE, FALSE, 3);
  gtk_widget_show_all(dialog);
  gint result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_OK)
    answer = Qtrue;
  gtk_widget_destroy(dialog);
  return answer;
}

VALUE
shoes_dialog_color(VALUE self, VALUE title)
{
  VALUE color = Qnil;
  GLOBAL_APP(app);
  title = shoes_native_to_s(title);
  GtkWidget *dialog = gtk_color_selection_dialog_new(RSTRING_PTR(title));
  gint result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_OK)
  {
    GdkColor _color;
    gtk_color_selection_get_current_color(
      GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(dialog)->colorsel),
      &_color);
    color = shoes_color_new(_color.red/256, _color.green/256, _color.blue/256, SHOES_COLOR_OPAQUE);
  }
  gtk_widget_destroy(dialog);
  return color;
}

static VALUE
shoes_dialog_chooser(VALUE self, char *title, GtkFileChooserAction act, const gchar *button, VALUE attr)
{
  VALUE path = Qnil;
  GLOBAL_APP(app);
  GtkWidget *dialog = gtk_file_chooser_dialog_new(title, APP_WINDOW(app),
    act, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, button, GTK_RESPONSE_ACCEPT, NULL);
  if (act == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
  if(RTEST(shoes_hash_get(attr, rb_intern("save"))))
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
            RSTRING_PTR(shoes_hash_get(attr, rb_intern("save"))));
  if(RTEST(shoes_hash_get(attr, rb_intern("types"))) && TYPE(shoes_hash_get(attr, rb_intern("types"))) == T_HASH) {
    VALUE hsh = shoes_hash_get(attr, rb_intern("types"));
    VALUE keys = rb_funcall(hsh, s_keys, 0);
    int i;
    for(i = 0; i < RARRAY_LEN(keys); i++) {
      VALUE key = rb_ary_entry(keys, i);
      VALUE val = rb_hash_aref(hsh, key);
      GtkFileFilter *ff = gtk_file_filter_new();
      gtk_file_filter_set_name(ff, RSTRING_PTR(key));
      gtk_file_filter_add_pattern(ff, RSTRING_PTR(val));
      gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), ff);
    }
  }
  gint result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    path = rb_str_new2(filename);
  }
  gtk_widget_destroy(dialog);
  return path;
}

VALUE
shoes_dialog_open(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Open file...", GTK_FILE_CHOOSER_ACTION_OPEN,
    GTK_STOCK_OPEN, args.a[0]);
}

VALUE
shoes_dialog_save(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Save file...", GTK_FILE_CHOOSER_ACTION_SAVE,
    GTK_STOCK_SAVE, args.a[0]);
}

VALUE
shoes_dialog_open_folder(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Open folder...", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
    GTK_STOCK_OPEN, args.a[0]);
}

VALUE
shoes_dialog_save_folder(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Save folder...", GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
    GTK_STOCK_SAVE, args.a[0]);
}
