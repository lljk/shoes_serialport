//
// shoes/native/windows.c
// Windows-specific code for Shoes.
//
#include "shoes/app.h"
#include "shoes/ruby.h"
#include "shoes/config.h"
#include "shoes/world.h"
#include "shoes/native.h"
#include "shoes/internal.h"
#include "shoes/appwin32.h"
#include <commdlg.h>
#include <shlobj.h>
#include <ctype.h>

#define HEIGHT_PAD 6

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

#ifndef BIF_NONEWFOLDERBUTTON
#define BIF_NONEWFOLDERBUTTON 0x200
#endif

static WNDPROC shoes_original_edit_line_proc = NULL;
HHOOK hhook;
VALUE kh_up_v = (VALUE)NULL;
VALUE kh_down_v = (VALUE)NULL;

shoes_code shoes_classex_init();
LRESULT CALLBACK shoes_app_win32proc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK shoes_slot_win32proc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK shoes_edit_line_win32proc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK shoes_keyhook(int, WPARAM, LPARAM);

#define KEYUPDOWN \
  if (vk == VK_LCONTROL) \
    v = ID2SYM(rb_intern("left_ctrl")); \
  else if (vk == VK_RCONTROL) \
    v = ID2SYM(rb_intern("right_ctrl")); \
  else if (vk == VK_LMENU) \
    v = ID2SYM(rb_intern("left_alt")); \
  else if (vk == VK_RMENU) \
    v = ID2SYM(rb_intern("right_alt")); \
  else if (vk == VK_LSHIFT) \
    v = ID2SYM(rb_intern("left_shift")); \
  else if (vk == VK_RSHIFT) \
    v = ID2SYM(rb_intern("right_shift")); \
  else if (vk == VK_ESCAPE) \
    v = ID2SYM(rb_intern("escape")); \
  else if (vk == VK_INSERT) \
    v = ID2SYM(rb_intern("insert")); \
  else if (vk == VK_DELETE) \
    v = ID2SYM(rb_intern("delete")); \
  else if (vk == VK_PRIOR) \
    v = ID2SYM(rb_intern("page_up")); \
  else if (vk == VK_NEXT) \
    v = ID2SYM(rb_intern("page_down")); \
  else if (vk == VK_HOME) \
    v = ID2SYM(rb_intern("home")); \
  else if (vk == VK_END) \
    v = ID2SYM(rb_intern("end")); \
  else if (vk == VK_LEFT) \
    v = ID2SYM(rb_intern("left")); \
  else if (vk == VK_UP) \
    v = ID2SYM(rb_intern("up")); \
  else if (vk == VK_RIGHT) \
    v = ID2SYM(rb_intern("right")); \
  else if (vk == VK_DOWN) \
    v = ID2SYM(rb_intern("down")); \
  else if (vk == VK_F1) \
    v = ID2SYM(rb_intern("f1")); \
  else if (vk == VK_F2) \
    v = ID2SYM(rb_intern("f2")); \
  else if (vk == VK_F2) \
    v = ID2SYM(rb_intern("f2")); \
  else if (vk == VK_F3) \
    v = ID2SYM(rb_intern("f3")); \
  else if (vk == VK_F4) \
    v = ID2SYM(rb_intern("f4")); \
  else if (vk == VK_F5) \
    v = ID2SYM(rb_intern("f5")); \
  else if (vk == VK_F6) \
    v = ID2SYM(rb_intern("f6")); \
  else if (vk == VK_F7) \
    v = ID2SYM(rb_intern("f7")); \
  else if (vk == VK_F8) \
    v = ID2SYM(rb_intern("f8")); \
  else if (vk == VK_F9) \
    v = ID2SYM(rb_intern("f9")); \
  else if (vk == VK_F10) \
    v = ID2SYM(rb_intern("f10")); \
  else if (vk == VK_F11) \
    v = ID2SYM(rb_intern("f11")); \
  else if (vk == VK_F12) \
    v = ID2SYM(rb_intern("f12")); \
  else if (vk == 186) \
  { \
    letter = ':'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 187) \
  { \
    letter = ';'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 188) \
  { \
    letter = ','; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 189) \
  { \
    letter = '-'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 190) \
  { \
    letter = '.'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 191) \
  { \
    letter = '/'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 192) \
  { \
    letter = '@'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 219) \
  { \
    letter = '['; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 220) \
  { \
    letter = '_'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 221) \
  { \
    letter = ']'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 222) \
  { \
    letter = '^'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 226) \
  { \
    letter = '\\'; \
    v = rb_str_new(&letter, 1); \
  } \
  else if (vk == 0x08) \
    v = ID2SYM(rb_intern("backspace")); \
  else if (vk == 0x09) \
    v = ID2SYM(rb_intern("tab")); \
  else if (vk == 0x0D) \
    v = ID2SYM(rb_intern("enter")); \
  else \
  { \
    letter = tolower(letter); \
    v = rb_str_new(&letter, 1); \
  }

static WCHAR *
shoes_wchar(char *utf8)
{
  WCHAR *buffer = NULL;
  LONG wlen = 0;
  if (utf8 == NULL) return NULL;
  wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
  if (!wlen) return NULL;
  buffer = SHOE_ALLOC_N(WCHAR, wlen);
  if (!buffer) return NULL;
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, wlen);
  return buffer;
}

static char *
shoes_utf8(WCHAR *buffer)
{
  char *utf8 = NULL;
  LONG i8 = 0;
  if (buffer == NULL) return NULL;
  i8 = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
  if (!i8) return NULL;
  utf8 = SHOE_ALLOC_N(char, i8);
  if (!utf8) return NULL;
  WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8, i8, NULL, NULL);
  return utf8;
}

static void
shoes_win32_center(HWND hwnd)
{
  RECT rc;
  
  GetWindowRect(hwnd, &rc);
  
  SetWindowPos(hwnd, 0, 
    (GetSystemMetrics(SM_CXSCREEN) - rc.right)/2,
    (GetSystemMetrics(SM_CYSCREEN) - rc.bottom)/2,
     0, 0, SWP_NOZORDER|SWP_NOSIZE );
}

int
shoes_win32_cmdvector(const char *cmdline, char ***argv)
{
  return rb_w32_cmdvector(cmdline, argv);
}

VALUE
shoes_load_font(const char *filename)
{
  VALUE allfonts, newfonts, oldfonts;
  int fonts = AddFontResourceEx(filename, FR_PRIVATE, 0);
  if (!fonts) return Qnil;
  allfonts = shoes_font_list();
  oldfonts = rb_const_get(cShoes, rb_intern("FONTS"));
  newfonts = rb_funcall(allfonts, rb_intern("-"), 1, oldfonts);
  shoes_update_fonts(allfonts);
  return newfonts;
}

static int CALLBACK
shoes_font_list_iter(const ENUMLOGFONTEX *font, const NEWTEXTMETRICA *pfont, DWORD type, LPARAM l)
{
  rb_ary_push(l, rb_str_new2(font->elfLogFont.lfFaceName));
  return TRUE;
}

VALUE
shoes_font_list()
{
  LOGFONT font = {0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, ""};
  VALUE ary = rb_ary_new();
  HDC dc = GetDC(NULL);
  EnumFontFamiliesEx(dc, &font, (FONTENUMPROC)shoes_font_list_iter, (LPARAM)ary, 0);
  ReleaseDC(NULL, dc);
  rb_funcall(ary, rb_intern("uniq!"), 0);
  rb_funcall(ary, rb_intern("sort!"), 0);
  return ary;
}

void shoes_native_init()
{
  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC = ICC_PROGRESS_CLASS;
  InitCommonControlsEx(&InitCtrlEx);
  shoes_classex_init();
  shoes_world->os.hidden = CreateWindow(SHOES_HIDDENCLS, SHOES_HIDDENCLS, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, shoes_world->os.instance, NULL);

  // Detect Windows Vista
  OSVERSIONINFOEXW osvi = { 0 };
  BOOL ret;

  // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
  // If that fails, try using the OSVERSIONINFO structure.

  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
  ret = GetVersionExW ((OSVERSIONINFOW*)&osvi);

  shoes_world->os.doublebuffer = FALSE;

  // Fallback to old version info struct if necessary.
  if (!ret)
  {
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    ret = GetVersionExW((OSVERSIONINFOW*)&osvi);
  }

  if (!ret)
  {
    // Error. Cannot detect version. Assume the worst: it is Vista.
    shoes_world->os.doublebuffer = TRUE;
  }
  else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0) {
    // Detected Windows "6.0", that is, Vista
    shoes_world->os.doublebuffer = TRUE;
  }

  hhook = SetWindowsHookEx(WH_KEYBOARD_LL, shoes_keyhook, shoes_world->os.instance, 0);
}

LRESULT CALLBACK shoes_keyhook(int n, WPARAM w, LPARAM l)
{
  VALUE v;
  WPARAM vk;

  kh_up_v = (VALUE)NULL;
  kh_down_v = (VALUE)NULL;
  
  if (n == HC_ACTION)
  {
    KBDLLHOOKSTRUCT *kbdHS = (KBDLLHOOKSTRUCT *)l;
    switch (w)
    {
      case WM_SYSKEYUP:
      case WM_KEYUP:
      {
        vk = kbdHS->vkCode;
        char letter = vk;
        KEYUPDOWN
        kh_up_v = v;
      }
      break;
      
      case WM_SYSKEYDOWN:
      case WM_KEYDOWN:
      {
        vk = kbdHS->vkCode;
        char letter = vk;
        KEYUPDOWN
        kh_down_v = v;
      }
      break;      
    }
  }
  
  return CallNextHookEx(hhook, n, w, l);
}

void shoes_native_cleanup(shoes_world_t *world)
{
  UnhookWindowsHookEx(hhook);
}

void shoes_native_quit()
{
  PostQuitMessage(0);
}

int shoes_throw_message(unsigned int name, VALUE obj, void *data)
{
  return SendMessage(shoes_world->os.hidden, SHOES_WM_MESSAGE + name, obj, (LPARAM)data);
}

void shoes_native_slot_mark(SHOES_SLOT_OS *slot)
{
  rb_gc_mark_maybe(slot->controls);
  rb_gc_mark_maybe(slot->focus);
}

void shoes_native_slot_reset(SHOES_SLOT_OS *slot)
{
  slot->controls = rb_ary_new();
  rb_gc_register_address(&slot->controls);
}

void shoes_native_slot_clear(shoes_canvas *canvas)
{
  rb_ary_clear(canvas->slot->controls);
}

void shoes_native_slot_paint(SHOES_SLOT_OS *slot)
{
  if (shoes_world->os.doublebuffer && slot->parent != NULL)
  {
    HWND parentWindow = GetWindow(slot->window, GW_OWNER);

    if (parentWindow != HWND_DESKTOP && parentWindow != NULL)
    {
      VALUE c = (VALUE)GetWindowLong(slot->window, GWL_USERDATA);

      if (c != (VALUE)NULL)
      {
        shoes_canvas *canvas;
        Data_Get_Struct(c, shoes_canvas, canvas);
        RedrawWindow(canvas->slot->window, NULL, NULL, RDW_INVALIDATE|RDW_ALLCHILDREN);
        return;
      }
    }
  }

  RedrawWindow(slot->window, NULL, NULL, RDW_INVALIDATE|RDW_ALLCHILDREN);
}

void shoes_native_slot_lengthen(SHOES_SLOT_OS *slot, int height, int endy)
{
  SCROLLINFO si;
  si.cbSize = sizeof(SCROLLINFO);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = endy - 1; 
  si.nPage = height;
  si.nPos = slot->scrolly;
  INFO("SetScrollInfo(%d, nMin: %d, nMax: %d, nPage: %d)\n", 
    si.nPos, si.nMin, si.nMax, si.nPage);
  SetScrollInfo(slot->window, SB_VERT, &si, TRUE);
}

void shoes_native_slot_scroll_top(SHOES_SLOT_OS *slot)
{
  SetScrollPos(slot->window, SB_VERT, slot->scrolly, TRUE);
}

int shoes_native_slot_gutter(SHOES_SLOT_OS *slot)
{
  return GetSystemMetrics(SM_CXVSCROLL);
}

void shoes_native_remove_item(SHOES_SLOT_OS *slot, VALUE item, char c)
{
  if (c)
  {
    long i = rb_ary_index_of(slot->controls, item);
    if (i >= 0)
      rb_ary_insert_at(slot->controls, i, 1, Qnil);
    if (item == slot->focus)
      slot->focus = Qnil;
  }
}

//
// Window-level events
//
#define WINDOW_STYLE WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
#define WINDOW_STYLE_FULLSCREEN WS_POPUP | WS_VSCROLL | ES_AUTOVSCROLL

#define WM_POINTS() \
  POINT p; \
  p.x = LOWORD(l); \
  p.y = HIWORD(l)
#define WM_POINTS2() \
  WM_POINTS(); \
  ClientToScreen(win, &p); \
  ScreenToClient(canvas->app->slot->window, &p); \

#define KEY_SYM(sym)  shoes_app_keypress(app, ID2SYM(rb_intern("" # sym)))
#define KEYPRESS(name, sym) \
  else if (w == VK_##name) { \
    VALUE v = ID2SYM(rb_intern("" # sym)); \
    if (app->os.altkey) \
      KEY_STATE(alt); \
    if (app->os.shiftkey) \
      KEY_STATE(shift); \
    if (app->os.ctrlkey) \
      KEY_STATE(control); \
    shoes_app_keypress(app, v); \
  }

static void
shoes_canvas_win32_vscroll(shoes_canvas *canvas, int code, int pos)
{
  SCROLLINFO si;
  SHOE_MEMZERO(&si, SCROLLINFO, 1);
  si.cbSize = sizeof(SCROLLINFO);
  si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
  GetScrollInfo(canvas->slot->window, SB_VERT, &si);

  switch (code)
  {
    case SB_LINEUP:
      si.nPos -= 16;
    break;
    case SB_LINEDOWN:
      si.nPos += 16;
    break;
    case SB_PAGEUP:
      si.nPos -= si.nPage - 32;
    break;
    case SB_PAGEDOWN:
      si.nPos += si.nPage - 32;
    break;
    case SB_THUMBTRACK:
      si.nPos = pos;
    break;
    default:
      return;
  }

  if (si.nPos < 0)
    si.nPos = 0;
  else if (si.nPos > (si.nMax - si.nPage))
    si.nPos = si.nMax - si.nPage;

  SetScrollInfo(canvas->slot->window, SB_VERT, &si, TRUE);
  canvas->slot->scrolly = si.nPos;
  if (canvas->app->slot->dc == canvas->slot->dc) canvas->app->slot->scrolly = si.nPos;
  shoes_native_slot_paint(canvas->slot);
}

LRESULT CALLBACK
shoes_slot_win32proc(
  HWND win,
  UINT msg,
  WPARAM w,
  LPARAM l)
{
  shoes_canvas *canvas;
  VALUE c = (VALUE)GetWindowLong(win, GWL_USERDATA);

  if (c != (VALUE)NULL)
  {
    Data_Get_Struct(c, shoes_canvas, canvas);

    switch (msg)
    {
      case WM_ERASEBKGND:
        return 1;

      case WM_SIZE:
        if (shoes_world->os.doublebuffer)
        {
          // On Vista, flickering still happens due to a bug in Vista's compositing
          // engine. We circumvent using some old school software double buffering.
          // Yes. It sucks. Like shifting gears in an automatic... poorly.

          RECT rc;
          GetClientRect(win, &rc);

          DeleteDC(canvas->slot->dc);

          HDC windowDC = GetDC(canvas->slot->window);
          canvas->slot->dc = CreateCompatibleDC(windowDC);

          HBITMAP hbmp = CreateCompatibleBitmap(windowDC, rc.right, rc.bottom);
          SelectObject(canvas->slot->dc, hbmp);
          DeleteObject(hbmp);

          ReleaseDC(canvas->slot->window, windowDC);

          HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
          FillRect(canvas->slot->dc, &rc, bg);
          DeleteObject(bg);
        }
        break;

      case WM_PAINT:
        INFO("WM_PAINT(slot, %lu)\n", win);
        if (c != canvas->app->canvas)
          shoes_canvas_paint(c);
      return 1;

      case WM_VSCROLL:
        shoes_canvas_win32_vscroll(canvas, LOWORD(w), HIWORD(w));
      break;

      case WM_LBUTTONDOWN:
      {
        WM_POINTS2();
        shoes_app_click(canvas->app, 1, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_RBUTTONDOWN:
      {
        WM_POINTS2();
        shoes_app_click(canvas->app, 2, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_MBUTTONDOWN:
      {
        WM_POINTS2();
        shoes_app_click(canvas->app, 3, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_LBUTTONUP:
      {
        WM_POINTS2();
        shoes_app_release(canvas->app, 1, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_RBUTTONUP:
      {
        WM_POINTS2();
        shoes_app_release(canvas->app, 2, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_MBUTTONUP:
      {
        WM_POINTS2();
        shoes_app_release(canvas->app, 3, p.x, p.y + canvas->app->slot->scrolly);
      }
      break;

      case WM_MOUSEMOVE:
      {
        WM_POINTS2();
        shoes_app_motion(canvas->app, p.x, p.y + canvas->app->slot->scrolly);
      }
      return 1;

      case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE)
        {
          int i;
          HWND newFocus = GetFocus();
          for (i = 0; i < RARRAY_LEN(canvas->slot->controls); i++)
          {
            VALUE ctrl = rb_ary_entry(canvas->slot->controls, i);
            if (rb_obj_is_kind_of(ctrl, cNative))
            {
              shoes_control *self_t;
              Data_Get_Struct(ctrl, shoes_control, self_t);
              if (self_t->ref == newFocus)
              {
                canvas->slot->focus = ctrl;
                break;
              }
            }
          }
        }
      break;

      case WM_SETFOCUS:
        if (!NIL_P(canvas->slot->focus) && TYPE(canvas->slot->focus) != T_FALSE)
        {
          shoes_control_focus(canvas->slot->focus);
        }
      break;

      /* TODO: use to make controls clear
      case WM_CTLCOLOREDIT:
      case WM_CTLCOLORSTATIC:
        {
          HDC hdc = (HDC) w;
          HBRUSH hbrBkcolor = (HBRUSH)GetStockObject(NULL_BRUSH);

          SetBkMode(hdc, TRANSPARENT);
          return (LRESULT) hbrBkcolor;
        }
      break;
      */

      case WM_HSCROLL:
      {
        if (LOWORD(w) == TB_THUMBTRACK)
        {
          int id = GetDlgCtrlID((HWND)l);
          VALUE control = rb_ary_entry(canvas->slot->controls, id - SHOES_CONTROL1);
          if (!NIL_P(control))
            shoes_control_send(control, s_change);
        }
      }
      break;

      case WM_COMMAND:
        if ((HWND)l)
        {
          switch (HIWORD(w))
          {
            case BN_CLICKED:
            {
              int id = LOWORD(w);
              VALUE control = rb_ary_entry(canvas->slot->controls, id - SHOES_CONTROL1);
              if (!NIL_P(control))
                shoes_button_send_click(control);
            }
            break;

            case CBN_SELCHANGE:
            case EN_CHANGE:
            {
              int id = LOWORD(w);
              VALUE control = rb_ary_entry(canvas->slot->controls, id - SHOES_CONTROL1);
              if (!NIL_P(control))
                shoes_control_send(control, s_change);
            }
            break;
          }
        }
      break;
    }
  }
  return DefWindowProc(win, msg, w, l);
}

LRESULT CALLBACK
shoes_hidden_win32proc(HWND win, UINT msg, WPARAM w, LPARAM l)
{
  if (msg > SHOES_WM_MESSAGE && msg < SHOES_WM_MESSAGE + SHOES_MAX_MESSAGE)
    return shoes_catch_message(msg - SHOES_WM_MESSAGE, (VALUE)w, (void *)l);
  return DefWindowProc(win, msg, w, l);
}

static void
shoes_app_decor(HWND win, int *width, int *height)
{
   RECT rect, wrect;
   GetClientRect(win, &rect);
   GetWindowRect(win, &wrect);
   *width = ((wrect.right - wrect.left) - (rect.right - rect.left)) - 2;
   *height = ((wrect.bottom - wrect.top) - (rect.bottom - rect.top)) - 2;
}

LRESULT CALLBACK
shoes_app_win32proc(
  HWND win,
  UINT msg,
  WPARAM w,
  LPARAM l)
{
  shoes_app *app = (shoes_app *)GetWindowLong(win, GWL_USERDATA);

  switch (msg)
  {
    case WM_DESTROY:
      if (shoes_app_remove(app))
        PostQuitMessage(0);
    return 0;

    case WM_ERASEBKGND:
      return 1;

    case WM_SIZE:
      if (shoes_world->os.doublebuffer)
      {
        // On Vista, flickering still happens due to a bug in Vista's compositing
        // engine. We circumvent using some old school software double buffering.
        // Yes. It sucks. Like shifting gears in an automatic... poorly.

        RECT rc;
        GetClientRect(win, &rc);

        if (app->slot->dc != NULL)
          DeleteDC(app->slot->dc);

        HDC windowDC = GetDC(app->slot->window);
        app->slot->dc = CreateCompatibleDC(windowDC);

        HBITMAP hbmp = CreateCompatibleBitmap(windowDC, rc.right, rc.bottom);
        SelectObject(app->slot->dc, hbmp);
        DeleteObject(hbmp);

        ReleaseDC(app->slot->window, windowDC);

        HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        FillRect(app->slot->dc, &rc, bg);
        DeleteObject(bg);
      }
    break;

    //
    // On Windows, I have to ensure the scrollbar's width is added
    // to the client area width.  In Shoes, the toplevel slot size is
    // always obscured by the scrollbar when it appears, rather than
    // resizing the width of the slot->
    //
    case WM_PAINT:
    {
      RECT rect;
      int edgew, edgeh;
      int scrollwidth = GetSystemMetrics(SM_CXVSCROLL);
      GetClientRect(win, &rect);

      shoes_app_decor(win, &edgew, &edgeh);
      if (edgew > scrollwidth)
        rect.right += scrollwidth;
      app->width = rect.right;
      app->height = rect.bottom;
      shoes_canvas_size(app->canvas, app->width, app->height);
      INFO("WM_PAINT(app, %lu)\n", win);

	  shoes_app_paint(app);
    }
    break;

    case WM_GETMINMAXINFO:
      if (app != NULL)
      {
        int edgew, edgeh;
        int scrollwidth = GetSystemMetrics(SM_CXVSCROLL);
        MINMAXINFO *size = (MINMAXINFO *)l;
        shoes_app_decor(win, &edgew, &edgeh);
        if (edgew > scrollwidth)
          edgew -= scrollwidth;
        size->ptMinTrackSize.x = app->minwidth + edgew;
        size->ptMinTrackSize.y = app->minheight + edgeh;
      }
    return 0;

    case WM_LBUTTONDOWN:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_click(app, 1, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_RBUTTONDOWN:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_click(app, 2, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_MBUTTONDOWN:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_click(app, 3, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_LBUTTONUP:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_release(app, 1, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_RBUTTONUP:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_release(app, 2, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_MBUTTONUP:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_release(app, 3, p.x, p.y + canvas->slot->scrolly);
    }
    break;

    case WM_MOUSEMOVE:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      WM_POINTS();
      shoes_app_motion(app, p.x, p.y + canvas->slot->scrolly);
    }
    return 1;

    case WM_CHAR:
      switch(w)
      {
        case 0x08:
          KEY_SYM(backspace);
        break;

        case 0x09:
          KEY_SYM(tab);
        break;

        case 0x0D:
          shoes_app_keypress(app, rb_str_new2("\n"));
        break;

        default:
        {
          VALUE v;
          WCHAR _str = w;
          CHAR str[10];
          DWORD len = WideCharToMultiByte(CP_UTF8, 0, &_str, 1, (LPSTR)str, 10, NULL, NULL);
          str[len] = '\0';
          v = rb_str_new(str, len);
          shoes_app_keypress(app, v);
        }
      }
    break;

    case WM_KEYDOWN:
      app->os.altkey = FALSE;
    case WM_SYSKEYDOWN:
      shoes_app_keydown(app, kh_down_v);
      if (w == VK_CONTROL)
        app->os.ctrlkey = TRUE;
      else if (w == VK_MENU)
        app->os.altkey = TRUE;
      else if (w == VK_SHIFT)
        app->os.shiftkey = TRUE;
      KEYPRESS(ESCAPE, escape)
      KEYPRESS(INSERT, insert)
      KEYPRESS(DELETE, delete)
      KEYPRESS(PRIOR, page_up)
      KEYPRESS(NEXT, page_down)
      KEYPRESS(HOME, home)
      KEYPRESS(END, end)
      KEYPRESS(LEFT, left)
      KEYPRESS(UP, up)
      KEYPRESS(RIGHT, right)
      KEYPRESS(DOWN, down)
      KEYPRESS(F1, f1)
      KEYPRESS(F2, f2)
      KEYPRESS(F3, f3)
      KEYPRESS(F4, f4)
      KEYPRESS(F5, f5)
      KEYPRESS(F6, f6)
      KEYPRESS(F7, f7)
      KEYPRESS(F8, f8)
      KEYPRESS(F9, f9)
      KEYPRESS(F10, f10)
      KEYPRESS(F11, f11)
      KEYPRESS(F12, f12)
      else if ((w >= 'A' && w <= 'Z') || w == 191 || w == 190) {
        VALUE v;
        char letter = w;
        if (w == 191)
        {
          if (app->os.shiftkey)
            letter = '?';
          else
            letter = '/';
        }
        else if (w == 190)
        {
          if (app->os.shiftkey)
            letter = '>';
          else
            letter = '.';
        }
        else
        {
          if (!app->os.shiftkey)
            letter += 32;
        }
        v = rb_str_new(&letter, 1);
        if (app->os.altkey) {
          KEY_STATE(alt);
          shoes_app_keypress(app, v);
        }
      }
    break;

    case WM_SYSKEYUP:
    case WM_KEYUP:
      shoes_app_keyup(app, kh_up_v);
      if (w == VK_CONTROL)
        app->os.ctrlkey = FALSE;
      else if (w == VK_MENU)
        app->os.altkey = FALSE;
      else if (w == VK_SHIFT)
        app->os.shiftkey = FALSE;
    break;

    case WM_MOUSEWHEEL:
    {
      shoes_canvas *canvas;
      int lines = 0, scode = 0;
      int notch = ((int)w >> 16) / WHEEL_DELTA;
      SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
      if (lines == WHEEL_PAGESCROLL)
        scode = (int)w < 0 ? SB_PAGEDOWN : SB_PAGEUP;
      else
      {
        scode = (int)w < 0 ? SB_LINEDOWN : SB_LINEUP;
        notch *= lines;
      }

      INFO("WM_MOUSEWHEEL: %d (%d, %d) %lu\n", w, scode, notch, lines);
      notch = abs(notch);
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      while (notch--)
        shoes_canvas_win32_vscroll(canvas, scode, 0);
    }
    break;

    case WM_VSCROLL:
    {
      shoes_canvas *canvas;
      Data_Get_Struct(app->canvas, shoes_canvas, canvas);
      shoes_canvas_win32_vscroll(canvas, LOWORD(w), HIWORD(w));
    }
    break;

    case WM_TIMER:
    {
      int id = LOWORD(w);
      VALUE timer = rb_ary_entry(app->extras, id - SHOES_CONTROL1);
      if (!NIL_P(timer))
      {
        if (rb_obj_is_kind_of(timer, cTimer))
          KillTimer(win, id);
        shoes_timer_call(timer);
      }
    }
    break;

    case WM_ACTIVATE:
      if (LOWORD(w) == WA_INACTIVE)
      {
        int i;
        HWND newFocus = GetFocus();
        for (i = 0; i < RARRAY_LEN(app->slot->controls); i++)
        {
          VALUE ctrl = rb_ary_entry(app->slot->controls, i);
          if (rb_obj_is_kind_of(ctrl, cNative))
          {
            shoes_control *self_t;
            Data_Get_Struct(ctrl, shoes_control, self_t);
            if (self_t->ref == newFocus)
            {
              app->slot->focus = ctrl;
              break;
            }
          }
        }
      }
    break;

    case WM_SETFOCUS:
      if (!NIL_P(app->slot->focus))
      {
        shoes_control_focus(app->slot->focus);
      }
    break;

    case WM_HSCROLL:
    {
      if (LOWORD(w) == TB_THUMBTRACK)
      {
        int id = GetDlgCtrlID((HWND)l);
        VALUE control = rb_ary_entry(app->slot->controls, id - SHOES_CONTROL1);
        if (!NIL_P(control))
          shoes_control_send(control, s_change);
      }
    }
    break;

    case WM_COMMAND:
      if ((HWND)l)
      {
        switch (HIWORD(w))
        {
          case BN_CLICKED:
          {
            int id = LOWORD(w);
            VALUE control = rb_ary_entry(app->slot->controls, id - SHOES_CONTROL1);
            if (!NIL_P(control))
              shoes_button_send_click(control);
          }
          break;

          case CBN_SELCHANGE:
          case EN_CHANGE:
          {
            int id = LOWORD(w);
            VALUE control = rb_ary_entry(app->slot->controls, id - SHOES_CONTROL1);
            if (!NIL_P(control))
              shoes_control_send(control, s_change);
          }
          break;
        }
      }
    break;
  }

  return DefWindowProc(win, msg, w, l);
}

shoes_code
shoes_app_cursor(shoes_app *app, ID cursor)
{
  HCURSOR c;
  if (app->slot == NULL || app->slot->window == NULL)
    goto done;

  if (cursor == s_hand || cursor == s_link)
  {
    c = LoadCursor(NULL, IDC_HAND);
  }
  else if (cursor == s_arrow)
  {
    c = LoadCursor(NULL, IDC_ARROW);
  }
  else if (cursor == s_text)
  {
    c = LoadCursor(NULL, IDC_IBEAM);
  }
  else
    goto done;

  SetCursor(c);

  app->cursor = cursor;

done:
  return SHOES_OK;
}

shoes_code
shoes_classex_init()
{
  shoes_code code = SHOES_OK;

  shoes_world->os.hiddenex.cbSize = sizeof(WNDCLASSEX);
  shoes_world->os.hiddenex.style = 0;
  shoes_world->os.hiddenex.lpfnWndProc = (WNDPROC)shoes_hidden_win32proc;
  shoes_world->os.hiddenex.cbClsExtra = 0;
  shoes_world->os.hiddenex.cbWndExtra = 0;
  shoes_world->os.hiddenex.hInstance = shoes_world->os.instance;
  shoes_world->os.hiddenex.hIcon = NULL;
  shoes_world->os.hiddenex.hCursor = NULL;
  shoes_world->os.hiddenex.hbrBackground = NULL;
  shoes_world->os.hiddenex.lpszMenuName = NULL;
  shoes_world->os.hiddenex.lpszClassName = SHOES_HIDDENCLS;
  shoes_world->os.hiddenex.hIconSm = NULL;

  if (!RegisterClassEx(&shoes_world->os.hiddenex))
  {
    QUIT("Couldn't register Shoes hidden window class.");
  }


  shoes_world->os.classex.hInstance = shoes_world->os.instance;
  shoes_world->os.classex.lpszClassName = SHOES_SHORTNAME;
  shoes_world->os.classex.lpfnWndProc = shoes_app_win32proc;
  shoes_world->os.classex.style = 0;
  shoes_world->os.classex.cbSize = sizeof(WNDCLASSEX);
  shoes_world->os.classex.hIcon = LoadIcon(shoes_world->os.instance, IDI_APPLICATION);
  shoes_world->os.classex.hIconSm = LoadIcon(shoes_world->os.instance, IDI_APPLICATION);
  shoes_world->os.classex.hCursor = LoadCursor(NULL, IDC_ARROW);
  shoes_world->os.classex.lpszMenuName = NULL;
  shoes_world->os.classex.cbClsExtra = 0;
  shoes_world->os.classex.cbWndExtra = 0;
  shoes_world->os.classex.hbrBackground = (HBRUSH)COLOR_WINDOW;

  shoes_world->os.classatom = RegisterClassEx(&shoes_world->os.classex);
  if (!shoes_world->os.classatom)
  {
    QUIT("Couldn't register WIN32 window class.");
  }

  shoes_world->os.vlclassex.hInstance = shoes_world->os.slotex.hInstance = shoes_world->os.instance;
  shoes_world->os.vlclassex.lpszClassName = SHOES_VLCLASS;
  shoes_world->os.slotex.lpszClassName = SHOES_SLOTCLASS;
  shoes_world->os.vlclassex.style = shoes_world->os.slotex.style = CS_NOCLOSE;
  shoes_world->os.vlclassex.lpfnWndProc = DefWindowProc;
  shoes_world->os.slotex.lpfnWndProc = shoes_slot_win32proc;
  shoes_world->os.vlclassex.cbSize = shoes_world->os.slotex.cbSize = sizeof(WNDCLASSEX);
  shoes_world->os.vlclassex.hIcon = shoes_world->os.slotex.hIcon = NULL;
  shoes_world->os.vlclassex.hIconSm = shoes_world->os.slotex.hIconSm = NULL;
  shoes_world->os.vlclassex.hCursor = shoes_world->os.slotex.hCursor = LoadCursor(NULL, IDC_ARROW);
  shoes_world->os.vlclassex.lpszMenuName = shoes_world->os.slotex.lpszMenuName = NULL;
  shoes_world->os.vlclassex.cbClsExtra = shoes_world->os.slotex.cbClsExtra = 0;
  shoes_world->os.vlclassex.cbWndExtra = shoes_world->os.slotex.cbWndExtra = 0;
  shoes_world->os.vlclassex.hbrBackground = shoes_world->os.slotex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

  if (!RegisterClassEx(&shoes_world->os.slotex) || !RegisterClassEx(&shoes_world->os.vlclassex))
  {
    QUIT("Couldn't register VLC window class.");
  }

quit:
  return code;
}

void
shoes_native_app_resized(shoes_app *app)
{
  if (app->slot->window != NULL)
  {
    RECT r;
    GetWindowRect(app->slot->window, &r);
    r.right = r.left + app->width;
    r.bottom = r.top + app->height;
    AdjustWindowRect(&r, WINDOW_STYLE, FALSE);

    MoveWindow(app->slot->window, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
  }
}

void
shoes_native_app_title(shoes_app *app, char *msg)
{
  WCHAR *buffer = shoes_wchar(msg);
  if (buffer != NULL)
  {
    SetWindowTextW(app->slot->window, buffer);
    SHOE_FREE(buffer);
  }
}

void
shoes_native_app_fullscreen(shoes_app *app, char yn)
{
  if (yn)
    GetWindowRect(app->slot->window, &app->os.normal);
  SetWindowLong(app->slot->window, GWL_STYLE, yn ? WINDOW_STYLE_FULLSCREEN : app->os.style);
  ShowWindow(FindWindow("Shell_TrayWnd", NULL), yn ? SW_HIDE : SW_SHOW);
  if (!yn)
    MoveWindow(app->slot->window, app->os.normal.left, app->os.normal.top,
      app->os.normal.right - app->os.normal.left, app->os.normal.bottom - app->os.normal.top, TRUE);
  ShowWindow(app->slot->window, yn ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
}

shoes_code
shoes_native_app_open(shoes_app *app, char *path, int dialog)
{
  shoes_code code = SHOES_OK;
  LONG style;
  DWORD exStyle = dialog ? WS_EX_WINDOWEDGE : WS_EX_CLIENTEDGE;

  app->slot->controls = Qnil;
  app->slot->focus = Qnil;
  app->slot->parent = NULL;
  app->os.ctrlkey = FALSE;
  app->os.altkey = FALSE;
  app->os.shiftkey = FALSE;

  // remove the menu
  app->os.normal.left = 0;
  app->os.normal.top = 0;
  app->os.normal.right = app->width;
  app->os.normal.bottom = app->height;
  AdjustWindowRectEx(&app->os.normal, WINDOW_STYLE, FALSE, exStyle);

  if (!shoes_world->os.doublebuffer)
	{
    if (rb_eval_string("$NOLAYERED == 1"))
      exStyle |= WS_EX_COMPOSITED;
    else
      exStyle |= WS_EX_COMPOSITED|WS_EX_LAYERED;
	}
  else
    exStyle |= WS_EX_COMPOSITED;

  style = app->os.style = WINDOW_STYLE |
    (app->resizable ? (WS_THICKFRAME | WS_MAXIMIZEBOX) : WS_DLGFRAME) |
    WS_VSCROLL | ES_AUTOVSCROLL;
  if (app->fullscreen)
    style = WINDOW_STYLE_FULLSCREEN;

  if (shoes_world->os.doublebuffer)
    style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  app->slot->window = CreateWindowEx(
    exStyle, SHOES_SHORTNAME, SHOES_APPNAME, style,
    CW_USEDEFAULT, CW_USEDEFAULT,
    app->os.normal.right-app->os.normal.left, app->os.normal.bottom-app->os.normal.top,
    HWND_DESKTOP,
    NULL,
    shoes_world->os.instance,
    NULL);

  SetWindowLong(app->slot->window, GWL_USERDATA, (long)app);
  shoes_win32_center(app->slot->window);

  SCROLLINFO si;
  si.cbSize = sizeof(SCROLLINFO);
  si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  si.nMin = 0;
  si.nMax = 0;
  si.nPage = 0;
  si.nPos = 0;
  SetScrollInfo(app->slot->window, SB_VERT, &si, TRUE);

  return code;
}

void
shoes_native_app_show(shoes_app *app)
{
  // TODO: disable parent windows of dialogs
  // if (dialog && !NIL_P(app->owner))
  // {
  //   shoes_app *owner;
  //   Data_Get_Struct(app->owner, shoes_app, owner);
  //   EnableWindow(owner->slot->window, FALSE);
  // }
  ShowWindow(app->slot->window, SW_SHOWNORMAL);
}

void shoes_tab_focus(shoes_app *app)
{
  int i, j, n;
  HWND newFocus = GetFocus();
  n = RARRAY_LEN(app->slot->controls);
  
  for (i = 0; i < n; i++)
  {
    if (app->os.shiftkey)
    {
      if (i == 0)
        j = n - 1;
      else
        j = i - 1;
    }
    else
    {
      if (i == n - 1)
        j = 0;
      else
        j = i + 1;
    }
    
    VALUE ctrl = rb_ary_entry(app->slot->controls, i);
    VALUE nctrl = rb_ary_entry(app->slot->controls, j);
    
    if (rb_obj_is_kind_of(ctrl, cNative))
    {
      shoes_control *self_t;
      Data_Get_Struct(ctrl, shoes_control, self_t);
      if (self_t->ref == newFocus)
      {
        app->slot->focus = nctrl;
        shoes_control_focus(app->slot->focus);
        break;
      }
      else
      {
        if (i == n - 1)
        {
          if (app->os.shiftkey)
            nctrl = rb_ary_entry(app->slot->controls, n - 1);
          else
            nctrl = rb_ary_entry(app->slot->controls, 0);
          app->slot->focus = nctrl;
          shoes_control_focus(app->slot->focus);
          break;
        }
      }
    }
  }
}

void
shoes_native_loop()
{
  MSG msgs;
  while (msgs.message != WM_QUIT)
  {
    BOOL msg = PeekMessage(&msgs, NULL, 0, 0, PM_REMOVE);
    if (msg)
    {
      HWND focused = GetForegroundWindow();
      shoes_app *appk = (shoes_app *)GetWindowLong(focused, GWL_USERDATA);
      if (msgs.message == WM_KEYDOWN || msgs.message == WM_KEYUP)
      {
        ATOM wndatom = GetClassLong(focused, GCW_ATOM);
        if (appk != NULL && wndatom == shoes_world->os.classatom && RARRAY_LEN(appk->slot->controls) > 0)
        {
          switch (msgs.wParam)
          {
            case VK_SHIFT:
              if (msgs.message == WM_KEYDOWN)
                appk->os.shiftkey = TRUE;
              else
                appk->os.shiftkey = FALSE;
              msg = FALSE;
              break;
            case VK_TAB:
              if (msgs.message == WM_KEYUP) shoes_tab_focus(appk);
              msg = FALSE;
              break;
            case VK_UP: case VK_LEFT: case VK_DOWN:
            case VK_RIGHT: case VK_PRIOR: case VK_NEXT:
              break;
            default:
              msg = FALSE;
          }
        }
        else msg = FALSE;
      }
      else if (msgs.message == WM_SYSCHAR || msgs.message == WM_CHAR)
        msg = FALSE;
      else if (msgs.message == WM_MOUSEMOVE && focused == GetActiveWindow())
        shoes_app_cursor(appk, appk->cursor);

      if (msg)
        msg = IsDialogMessage(focused, &msgs);

      if (!msg)
      {
        TranslateMessage(&msgs);
        DispatchMessage(&msgs);
      }
    }
    else
    {
      rb_eval_string("sleep(0.001)");
    }
  }
}

void
shoes_native_app_close(shoes_app *app)
{
  SendMessage(APP_WINDOW(app), WM_CLOSE, 0, 0);
}

void
shoes_browser_open(char *url)
{
  ShellExecute(0, "open", url, 0, 0, 0);
}

void
shoes_slot_init(VALUE c, SHOES_SLOT_OS *parent, int x, int y, int width, int height, int scrolls, int toplevel)
{
  shoes_canvas *canvas;
  SHOES_SLOT_OS *slot;
  Data_Get_Struct(c, shoes_canvas, canvas);
  slot = shoes_slot_alloc(canvas, parent, toplevel);
  slot->parent = parent;
  slot->vscroll = scrolls;

  if (toplevel)
  {
    slot->dc = parent->dc;
    slot->window = parent->window;
    slot->controls = parent->controls;
  }
  else
  {
    slot->controls = rb_ary_new();
    slot->dc = NULL;

    DWORD exStyle = WS_EX_TRANSPARENT;

    if (shoes_world->os.doublebuffer)
      exStyle = WS_EX_COMPOSITED;

    slot->window = CreateWindowEx(exStyle, SHOES_SLOTCLASS, "Shoes Slot Window",
      WS_CHILD | WS_TABSTOP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
      x, y, width, height, parent->window, NULL,
      (HINSTANCE)GetWindowLong(parent->window, GWL_HINSTANCE), NULL);

    SetWindowPos(slot->window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
    SetWindowLong(slot->window, GWL_USERDATA, (long)c);
  }
  if (toplevel) shoes_canvas_size(c, width, height);
}

void
shoes_slot_destroy(shoes_canvas *canvas, shoes_canvas *pc)
{
  if (shoes_world->os.doublebuffer)
    DeleteDC(canvas->slot->dc);

  DestroyWindow(canvas->slot->window);
}

cairo_t *
shoes_cairo_create(shoes_canvas *canvas)
{
  if (canvas->slot->surface != NULL)
    return NULL;

  RECT rc;
  GetClientRect(canvas->slot->window, &rc);

  if (shoes_world->os.doublebuffer)
  {
    if (canvas->slot->window == canvas->app->slot->window)
    {
      canvas->slot->dc = canvas->app->slot->dc;
    }

    // So that siblings see the changes, we must copy from siblings beneath us
    if (canvas->slot->window != canvas->app->slot->window)
    {
      HANDLE current = canvas->slot->window;
      current = GetWindow(current, GW_HWNDLAST);

      RECT canvas_rcw;
      GetWindowRect(canvas->slot->window, &canvas_rcw);
      while(current != canvas->slot->window)
      {
        shoes_canvas *sibling_canvas;
        VALUE c = (VALUE)GetWindowLong(current, GWL_USERDATA);

        if (c == (VALUE)NULL)
        {
          break;
        }

        Data_Get_Struct(c, shoes_canvas, sibling_canvas);

        RECT sibling_rc;
        RECT sibling_rcw;
        GetClientRect(sibling_canvas->slot->window, &sibling_rc);
        GetWindowRect(sibling_canvas->slot->window, &sibling_rcw);

        BitBlt(canvas->slot->dc, sibling_rcw.left-canvas_rcw.left, sibling_rcw.top-canvas_rcw.top, sibling_rc.right, sibling_rc.bottom, sibling_canvas->slot->dc, 0, 0, SRCCOPY);
        current = GetWindow(current, GW_HWNDPREV);
      }
    }
  }
  else
  {
    canvas->slot->dc = BeginPaint(canvas->slot->window, &canvas->slot->ps);
  }

  if (canvas->slot->window == canvas->app->slot->window)
  {
    HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    FillRect(canvas->slot->dc, &rc, bg);
    DeleteObject(bg);
  }

  canvas->slot->surface = cairo_win32_surface_create(canvas->slot->dc);

  cairo_t *cr = cairo_create(canvas->slot->surface);
  cairo_translate(cr, 0, -canvas->slot->scrolly);
  return cr;
}

void shoes_cairo_destroy(shoes_canvas *canvas)
{
  RECT rc;
  GetClientRect(canvas->slot->window, &rc);

  cairo_surface_destroy(canvas->slot->surface);
  canvas->slot->surface = NULL;

  if (shoes_world->os.doublebuffer)
  {
    // We are manually double buffering in Vista, so we need to copy the
    // image over to the actual display.
    BeginPaint(canvas->slot->window, &canvas->slot->ps);

    if (canvas->slot->window == canvas->app->slot->window)
    {
      canvas->slot->dc = canvas->app->slot->dc;
    }

    BitBlt(canvas->slot->ps.hdc,
	  0, 0, rc.right, rc.bottom,
	  canvas->slot->dc,
	  0, 0, SRCCOPY);

    RECT rcw;
    GetWindowRect(canvas->slot->window, &rcw);

    // So children see the changes, we copy to each child
    HANDLE current;

    current = canvas->slot->window;
    current = GetWindow(current, GW_CHILD);

    while(current != NULL)
    {
      shoes_canvas *child_canvas;
      VALUE c = (VALUE)GetWindowLong(current, GWL_USERDATA);

      if (c == (VALUE)NULL)
        break;

      Data_Get_Struct(c, shoes_canvas, child_canvas);

      RECT child_rc;
      RECT child_rcw;
      GetClientRect(child_canvas->slot->window, &child_rc);
      GetWindowRect(child_canvas->slot->window, &child_rcw);

      if (canvas->slot->window == canvas->app->slot->window)
      {
        POINT p = {0};
        ClientToScreen(child_canvas->app->slot->window, &p);
        BitBlt(child_canvas->slot->dc, 0, 0, child_rc.right, child_rc.bottom, canvas->slot->dc, child_rcw.left-p.x, child_rcw.top-p.y, SRCCOPY);
      }
      else
      {
        BitBlt(child_canvas->slot->dc, 0, 0, child_rc.right, child_rc.bottom, canvas->slot->dc, child_rcw.left-rcw.left, child_rcw.top-rcw.top, SRCCOPY);
      }
      current = GetWindow(current, GW_HWNDNEXT);
    }

    // update parents
    SHOES_SLOT_OS *child = canvas->slot;

    while (child->window != canvas->app->slot->window)
    {
      SHOES_SLOT_OS *parent;
      parent = (SHOES_SLOT_OS*)child->parent;

      RECT parent_rcw;
      RECT canvas_rc;
      RECT canvas_rcw;

      GetWindowRect(parent->window, &parent_rcw);

	  GetClientRect(child->window, &canvas_rc);
      GetWindowRect(child->window, &canvas_rcw);

      BitBlt(parent->dc, canvas_rcw.left-parent_rcw.left, canvas_rcw.top-parent_rcw.top, canvas_rc.right, canvas_rc.bottom, child->dc, 0, 0, SRCCOPY);
      child = parent;
    }
  }

  EndPaint(canvas->slot->window, &canvas->slot->ps);
}

void
shoes_group_clear(SHOES_GROUP_OS *group)
{
}

void
shoes_native_canvas_place(shoes_canvas *self_t, shoes_canvas *pc)
{
  RECT r;
  GetWindowRect(self_t->slot->window, &r);
  if (r.left != self_t->place.ix + self_t->place.dx ||
      r.top != (self_t->place.iy + self_t->place.dy) - pc->slot->scrolly ||
      r.right - r.left != self_t->place.iw ||
      r.bottom - r.top != self_t->place.ih)
  {
    MoveWindow(self_t->slot->window, self_t->place.ix + self_t->place.dx,
      (self_t->place.iy + self_t->place.dy) - pc->slot->scrolly, self_t->place.iw,
      self_t->place.ih, TRUE);
  }
}

void
shoes_native_canvas_resize(shoes_canvas *canvas)
{
  if (shoes_world->os.doublebuffer)
  {
    RECT rc;
    GetClientRect(canvas->slot->window, &rc);

    HDC windowDC = GetDC(canvas->slot->window);
    canvas->slot->dc = CreateCompatibleDC(windowDC);

    HBITMAP hbmp = CreateCompatibleBitmap(windowDC, rc.right, rc.bottom);
    SelectObject(canvas->slot->dc, hbmp);
    DeleteObject(hbmp);

    ReleaseDC(canvas->slot->window, windowDC);
  }
}

void
shoes_native_control_hide(SHOES_CONTROL_REF ref)
{
  ShowWindow(ref, SW_HIDE);
}

void
shoes_native_control_show(SHOES_CONTROL_REF ref)
{
  ShowWindow(ref, SW_SHOW);
}

void
shoes_native_control_position(SHOES_CONTROL_REF ref, shoes_place *p1, VALUE self,
  shoes_canvas *canvas, shoes_place *p2)
{
  PLACE_COORDS();
  MoveWindow(ref, p2->ix + p2->dx, p2->iy + p2->dy, p2->iw, p2->ih, TRUE);
  if (shoes_world->os.doublebuffer)
  {
    RECT rc;
    GetClientRect(canvas->slot->window, &rc);

//    HDC windowDC = GetDC(canvas->slot->window);
//    canvas->slot->dc = CreateCompatibleDC(windowDC);

    HBITMAP hbmp = CreateCompatibleBitmap(canvas->slot->dc, rc.right, rc.bottom);
    SelectObject(canvas->slot->dc, hbmp);
    DeleteObject(hbmp);

//    ReleaseDC(canvas->slot->window, windowDC);
  }
}

void
shoes_native_control_position_no_pad(SHOES_CONTROL_REF ref, shoes_place *p1, VALUE self,
  shoes_canvas *canvas, shoes_place *p2)
{
  PLACE_COORDS_NO_PAD();
  MoveWindow(ref, p2->ix + p2->dx, p2->iy + p2->dy, p2->iw, p2->ih, TRUE);
  if (shoes_world->os.doublebuffer)
  {
    RECT rc;
    GetClientRect(canvas->slot->window, &rc);

//    HDC windowDC = GetDC(canvas->slot->window);
//    canvas->slot->dc = CreateCompatibleDC(windowDC);

    HBITMAP hbmp = CreateCompatibleBitmap(canvas->slot->dc, rc.right, rc.bottom);
    SelectObject(canvas->slot->dc, hbmp);
    DeleteObject(hbmp);

//    ReleaseDC(canvas->slot->window, windowDC);
  }
}

void
shoes_native_control_repaint(SHOES_CONTROL_REF ref, shoes_place *p1,
  shoes_canvas *canvas, shoes_place *p2)
{
  p2->iy -= canvas->slot->scrolly;
  if (CHANGED_COORDS())
    shoes_native_control_position(ref, p1, Qnil, canvas, p2);
  p2->iy += canvas->slot->scrolly;
}

void
shoes_native_control_repaint_no_pad(SHOES_CONTROL_REF ref, shoes_place *p1,
  shoes_canvas *canvas, shoes_place *p2)
{
  p2->iy -= canvas->slot->scrolly;
  if (CHANGED_COORDS_NO_PAD())
    shoes_native_control_position_no_pad(ref, p1, Qnil, canvas, p2);
  p2->iy += canvas->slot->scrolly;
}

void
shoes_native_control_state(SHOES_CONTROL_REF ref, BOOL sensitive, BOOL setting)
{
  EnableWindow(ref, sensitive);
  SendMessage(ref, EM_SETREADONLY, !setting, 0);
}

void
shoes_native_control_focus(SHOES_CONTROL_REF ref)
{
  SetFocus(ref);
}

void
shoes_native_control_remove(SHOES_CONTROL_REF ref, shoes_canvas *canvas)
{
  if (GetFocus() == ref) SetFocus(canvas->app->slot->window);
  DestroyWindow(ref);
}

void
shoes_native_control_free(SHOES_CONTROL_REF ref)
{
}

inline void shoes_win32_control_font(int id, HWND hwnd)
{
  SendDlgItemMessage(hwnd, id, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
}

SHOES_SURFACE_REF
shoes_native_surface_new(shoes_canvas *canvas, VALUE self, shoes_place *place)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  SHOES_SURFACE_REF ref = CreateWindowEx(WS_EX_TRANSPARENT, SHOES_VLCLASS, "Shoes VLC Window",
      WS_CHILD | WS_TABSTOP | WS_VISIBLE,
      place->ix + place->dx, place->iy + place->dy,
      place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE), NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
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
  DestroyWindow(ref);
}

SHOES_CONTROL_REF
shoes_native_button(VALUE self, shoes_canvas *canvas, shoes_place *place, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  WCHAR *buffer = shoes_wchar(msg);
  SHOES_CONTROL_REF ref = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"BUTTON", buffer,
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  if (buffer != NULL) SHOE_FREE(buffer);
  shoes_win32_control_font(cid, canvas->slot->window);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

SHOES_CONTROL_REF
shoes_native_edit_line(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  SHOES_CONTROL_REF ref = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_TRANSPARENT, TEXT("EDIT"), NULL,
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL |
      (RTEST(ATTR(attr, secret)) ? ES_PASSWORD : 0),
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  
  shoes_original_edit_line_proc = (WNDPROC)GetWindowLong(ref, GWL_WNDPROC);
  SetWindowLong(ref, GWL_WNDPROC, (LONG)shoes_edit_line_win32proc); 
  
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  shoes_win32_control_font(cid, canvas->slot->window);
  shoes_native_edit_line_set_text(ref, msg);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

LRESULT CALLBACK shoes_edit_line_win32proc(HWND win, UINT msg, WPARAM w, LPARAM l)
{
  if (msg == WM_KEYDOWN && w == VK_RETURN) rb_eval_string("Shoes.hook");
  return CallWindowProc(shoes_original_edit_line_proc, win, msg, w, l);
}

VALUE
shoes_native_edit_line_get_text(SHOES_CONTROL_REF ref)
{
  VALUE text;
  LONG i;
  char *utf8 = NULL;
  WCHAR *buffer = NULL;
  i = (LONG)SendMessageW(ref, WM_GETTEXTLENGTH, 0, 0) + 1;
  if (!i) goto empty;
  buffer = SHOE_ALLOC_N(WCHAR, i);
  if (!buffer) goto empty;
  SendMessageW(ref, WM_GETTEXT, i, (LPARAM)buffer);

  utf8 = shoes_utf8(buffer);
  text = rb_str_new2(utf8);
  SHOE_FREE(utf8);
  SHOE_FREE(buffer);
  return text;
empty:
  if (buffer != NULL) SHOE_FREE(buffer);
  return rb_str_new2("");
}

void
shoes_native_edit_line_set_text(SHOES_CONTROL_REF ref, char *msg)
{
  WCHAR *buffer = shoes_wchar(msg);
  if (buffer != NULL)
  {
    SendMessageW(ref, WM_SETTEXT, 0, (LPARAM)buffer);
    SHOE_FREE(buffer);
  }
}

SHOES_CONTROL_REF
shoes_native_edit_box(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  DWORD exStyle = WS_EX_CLIENTEDGE;
  if (!shoes_world->os.doublebuffer)
    exStyle |= WS_EX_TRANSPARENT;

  SHOES_CONTROL_REF ref = CreateWindowEx(exStyle, TEXT("EDIT"), NULL,
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT |
    ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
    place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
    canvas->slot->window, (HMENU)cid,
    (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
    NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  shoes_win32_control_font(cid, canvas->slot->window);
  shoes_native_edit_line_set_text(ref, msg);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

VALUE
shoes_native_edit_box_get_text(SHOES_CONTROL_REF ref)
{
  return shoes_native_edit_line_get_text(ref);
}

void
shoes_native_edit_box_set_text(SHOES_CONTROL_REF ref, char *msg)
{
  shoes_native_edit_line_set_text(ref, msg);
}

SHOES_CONTROL_REF
shoes_native_list_box(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  SHOES_CONTROL_REF ref = CreateWindowEx(WS_EX_TRANSPARENT, TEXT("COMBOBOX"), NULL,
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  shoes_win32_control_font(cid, canvas->slot->window);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

void
shoes_native_list_box_update(SHOES_CONTROL_REF box, VALUE ary)
{
  long i;
  SendMessage(box, CB_RESETCONTENT, 0, 0);
  for (i = 0; i < RARRAY_LEN(ary); i++)
  {
    VALUE msg = shoes_native_to_s(rb_ary_entry(ary, i));
    WCHAR *buffer = shoes_wchar(RSTRING_PTR(msg));
    if (buffer != NULL)
    {
      SendMessageW(box, CB_ADDSTRING, 0, (LPARAM)buffer);
      SHOE_FREE(buffer);
    }
  }
}

VALUE
shoes_native_list_box_get_active(SHOES_CONTROL_REF ref, VALUE items)
{
  int sel = SendMessage(ref, CB_GETCURSEL, 0, 0);
  if (sel >= 0)
    return rb_ary_entry(items, sel);
  return Qnil;
}

void
shoes_native_list_box_set_active(SHOES_CONTROL_REF box, VALUE ary, VALUE item)
{
  int idx = rb_ary_index_of(ary, item);
  if (idx < 0) return;
  SendMessage(box, CB_SETCURSEL, idx, 0);
}

SHOES_CONTROL_REF
shoes_native_progress(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  HWND ref = CreateWindowEx(WS_EX_TRANSPARENT, PROGRESS_CLASS, msg,
      WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, NULL, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  return ref;
}

double
shoes_native_progress_get_fraction(SHOES_CONTROL_REF ref)
{
  return SendMessage(ref, PBM_GETPOS, 0, 0) * 0.01;
}

void
shoes_native_progress_set_fraction(SHOES_CONTROL_REF ref, double perc)
{
  SendMessage(ref, PBM_SETPOS, (int)(perc * 100), 0L);
}

SHOES_CONTROL_REF
shoes_native_slider(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  HWND ref = CreateWindowEx(WS_EX_TRANSPARENT, TRACKBAR_CLASS, msg,
      WS_VISIBLE | WS_CHILD,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid,
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

double
shoes_native_slider_get_fraction(SHOES_CONTROL_REF ref)
{
  return SendMessage(ref, TBM_GETPOS, 0, 0) * 0.01;
}

void
shoes_native_slider_set_fraction(SHOES_CONTROL_REF ref, double perc)
{
  SendMessage(ref, TBM_SETPOS, (int)(perc * 100), 0L);
}

SHOES_CONTROL_REF
shoes_native_check(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, char *msg)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  SHOES_CONTROL_REF ref = CreateWindowEx(WS_EX_TRANSPARENT, TEXT("BUTTON"), NULL,
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  shoes_win32_control_font(cid, canvas->slot->window);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

VALUE
shoes_native_check_get(SHOES_CONTROL_REF ref)
{
  return SendMessage(ref, BM_GETCHECK, 0, 0) == BST_CHECKED ? Qtrue : Qfalse;
}

void
shoes_native_check_set(SHOES_CONTROL_REF ref, int on)
{
  SendMessage(ref, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0L);
}

SHOES_CONTROL_REF
shoes_native_radio(VALUE self, shoes_canvas *canvas, shoes_place *place, VALUE attr, VALUE group)
{
  int cid = SHOES_CONTROL1 + RARRAY_LEN(canvas->slot->controls);
  SHOES_CONTROL_REF ref = CreateWindowEx(WS_EX_TRANSPARENT, TEXT("BUTTON"), NULL,
      WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_RADIOBUTTON,
      place->ix + place->dx, place->iy + place->dy, place->iw, place->ih,
      canvas->slot->window, (HMENU)cid, 
      (HINSTANCE)GetWindowLong(canvas->slot->window, GWL_HINSTANCE),
      NULL);
  SetWindowPos(ref, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW);
  shoes_win32_control_font(cid, canvas->slot->window);
  rb_ary_push(canvas->slot->controls, self);
  return ref;
}

void
shoes_native_timer_remove(shoes_canvas *canvas, SHOES_TIMER_REF ref)
{
  KillTimer(canvas->slot->window, ref);
}

SHOES_TIMER_REF
shoes_native_timer_start(VALUE self, shoes_canvas *canvas, unsigned int interval)
{
  long nid = rb_ary_index_of(canvas->app->extras, self);
  SetTimer(canvas->app->slot->window, SHOES_CONTROL1 + nid, interval, NULL);
  return SHOES_CONTROL1 + nid;
}

VALUE
shoes_native_clipboard_get(shoes_app *app)
{
  VALUE paste = Qnil;
  if (OpenClipboard(app->slot->window))
  {
    HANDLE hclip = GetClipboardData(CF_UNICODETEXT);
    WCHAR *buffer = (WCHAR *)GlobalLock(hclip);
    char *utf8 = shoes_utf8(buffer);
    paste = rb_str_new2(utf8);
    GlobalUnlock(hclip);
    CloseClipboard();
    SHOE_FREE(utf8);
  }
  return paste;
}

void
shoes_native_clipboard_set(shoes_app *app, VALUE string)
{
  if (OpenClipboard(app->slot->window))
  {
    WCHAR *buffer = shoes_wchar(RSTRING_PTR(string));
    LONG buflen = wcslen(buffer);
    HGLOBAL hclip;
    EmptyClipboard();
    hclip = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, (buflen + 1) * sizeof(WCHAR));
    wcsncpy((WCHAR *)GlobalLock(hclip), buffer, buflen + 1);
    GlobalUnlock(hclip);
    SetClipboardData(CF_UNICODETEXT, hclip);
    CloseClipboard();
    SHOE_FREE(buffer);
  }
}

VALUE
shoes_native_to_s(VALUE text)
{
  text = rb_funcall(text, s_to_s, 0);
  text = rb_funcall(text, s_gsub, 2, reLF, rb_str_new2("\r\n"));
  return text;
}

VALUE
shoes_native_window_color(shoes_app *app)
{
  DWORD winc = GetSysColor(COLOR_WINDOW);
  return shoes_color_new(GetRValue(winc), GetGValue(winc), GetBValue(winc), SHOES_COLOR_OPAQUE);
}

VALUE
shoes_native_dialog_color(shoes_app *app)
{
  DWORD winc = GetSysColor(COLOR_3DFACE);
  return shoes_color_new(GetRValue(winc), GetGValue(winc), GetBValue(winc), SHOES_COLOR_OPAQUE);
}

LPWSTR win32_dialog_label = NULL;
LPWSTR win32_dialog_answer = NULL;

BOOL CALLBACK
shoes_ask_win32proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_INITDIALOG:
      SetDlgItemTextW(hwnd, IDQUIZ, win32_dialog_label);
      if (win32_dialog_label != NULL)
      {
        SHOE_FREE(win32_dialog_label);
        win32_dialog_label = NULL;
      }
      if (win32_dialog_answer != NULL)
      {
        GlobalFree((HANDLE)win32_dialog_answer);
        win32_dialog_answer = NULL;
      }
      return TRUE;

    case WM_COMMAND:
      switch (LOWORD(wParam))
      {
        case IDOK:
        {
          int len = GetWindowTextLength(GetDlgItem(hwnd, IDQUED));
          if(len > 0)
          {
            win32_dialog_answer = (LPWSTR)GlobalAlloc(GPTR, (len + 1) * sizeof(WCHAR));
            GetDlgItemTextW(hwnd, IDQUED, win32_dialog_answer, len + 1);
          }
        }
        case IDCANCEL:
          EndDialog(hwnd, LOWORD(wParam));
          return TRUE;
      }
      break;

    case WM_CLOSE:
      EndDialog(hwnd, 0);
      return FALSE;
  }
  return FALSE;
}

VALUE
shoes_dialog_alert(VALUE self, VALUE msg)
{
  WCHAR *buffer;
  GLOBAL_APP(app);
  msg = shoes_native_to_s(msg);
  buffer = shoes_wchar(RSTRING_PTR(msg));
  MessageBoxW(APP_WINDOW(app), buffer, (LPCWSTR)dialog_title_says, MB_OK);
  if (buffer != NULL) SHOE_FREE(buffer);
  return Qnil;
}

VALUE
shoes_dialog_ask(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  VALUE answer = Qnil;
  rb_parse_args(argc, argv, "s|h", &args);
  GLOBAL_APP(app);
  win32_dialog_label = shoes_wchar(RSTRING_PTR(args.a[0]));
  int confirm = DialogBox(shoes_world->os.instance,
    MAKEINTRESOURCE(RTEST(ATTR(args.a[1], secret)) ? ASKSECRETDLG : ASKDLG),
    APP_WINDOW(app), shoes_ask_win32proc);
  if (confirm == IDOK)
  {
    if (win32_dialog_answer != NULL)
    {
      char *ans8 = shoes_utf8(win32_dialog_answer);
      answer = rb_str_new2(ans8);
      SHOE_FREE(ans8);
      GlobalFree((HANDLE)win32_dialog_answer);
      win32_dialog_answer = NULL;
    }
  }
  return answer;
}

VALUE
shoes_dialog_confirm(VALUE self, VALUE quiz)
{
  WCHAR *buffer;
  VALUE answer = Qfalse;
  GLOBAL_APP(app);
  quiz = shoes_native_to_s(quiz);
  buffer = shoes_wchar(RSTRING_PTR(quiz));
  int confirm = MessageBoxW(APP_WINDOW(app), buffer, (LPCWSTR)dialog_title, MB_OKCANCEL);
  if (confirm == IDOK)
    answer = Qtrue;
  if (buffer != NULL) SHOE_FREE(buffer);
  return answer;
}

VALUE
shoes_dialog_color(VALUE self, VALUE title)
{
  VALUE color = Qnil;
  GLOBAL_APP(app);
  CHOOSECOLOR cc;
  static COLORREF acrCustClr[16];
  static DWORD rgbCurrent;

  // Initialize CHOOSECOLOR 
  ZeroMemory(&cc, sizeof(cc));
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = APP_WINDOW(app);
  cc.lpCustColors = (LPDWORD) acrCustClr;
  cc.rgbResult = rgbCurrent;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;

  if (ChooseColor(&cc)) {
    color = shoes_color_new(GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult),
      SHOES_COLOR_OPAQUE);
  }
  return color;
}

static char *
shoes_fix_slashes(char *path)
{
  char *p;
  for (p = path; *p != '\0'; p++)
    if (*p == '\\')
      *p = '/';
  return path;
}

static void
shoes_append_filter_token(char **filters, char *token, int token_len) {
  memcpy(*filters, token, token_len);
  (*filters) += token_len;
  *(*filters)++ = '\0';
}

static VALUE
shoes_dialog_chooser(VALUE self, char *title, DWORD flags, VALUE attr)
{
  BOOL ok;
  VALUE path = Qnil;
  GLOBAL_APP(app);
  char _path[MAX_PATH+1];
  OPENFILENAME ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  int filter_len = 64;
  int val_len, key_len = val_len = 0;
  char *filters;

  if((filters = malloc(filter_len)) == NULL)
    return Qnil;
  *filters = '\0';

  if(RTEST(shoes_hash_get(attr, rb_intern("types"))) && 
           TYPE(shoes_hash_get(attr, rb_intern("types"))) == T_HASH) {
    char *filters_buff = filters;
    int i;
    VALUE hsh = shoes_hash_get(attr, rb_intern("types"));
    VALUE keys = rb_funcall(hsh, s_keys, 0);

    for(i = 0; i < RARRAY_LEN(keys); i++) {
      VALUE key = rb_ary_entry(keys, i);
      char *key_name = RSTRING_PTR(key);
      char *val = RSTRING_PTR(rb_hash_aref(hsh, key));

      key_len = strlen(key_name);
      val_len = strlen(val);
      int pos = filters_buff - filters;

      // ensure there's room for both tokens, thier NULL terminators
      // and the NULL terminator for the whole filter.
      if((pos + key_len + val_len + 3) >= filter_len) {
	filter_len *= 2;
	if((filters = realloc(filters, filter_len)) == NULL)
	  return Qnil;
	filters_buff = filters + pos;
      }

      // add the tokens on to the filter string
      shoes_append_filter_token(&filters_buff, key_name, key_len);
      shoes_append_filter_token(&filters_buff, val, val_len);
    }

    // the whole thing needs a final terminator
    *filters_buff = '\0';
  } else
    // old shoes apps under windows may expect this
    memcpy(filters, "All\0*.*\0Text\0*.TXT\0\0", 20);

  ofn.lpstrFilter     = (LPCTSTR)filters;
  ofn.lStructSize     = sizeof(ofn);
  ofn.hwndOwner       = APP_WINDOW(app);
  ofn.hInstance       = shoes_world->os.instance;
  ofn.lpstrFile       = _path;
  ofn.lpstrTitle      = title;
  ofn.nMaxFile        = sizeof(_path);
  ofn.lpstrFile[0]    = '\0';
  ofn.nFilterIndex    = 1;
  ofn.lpstrFileTitle  = NULL;
  ofn.nMaxFileTitle   = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags           = OFN_EXPLORER | flags;
  VALUE save          = ID2SYM(rb_intern("save"));

  if (RTEST(ATTR(attr, save)))
    ofn.lpstrFile = (LPSTR)RSTRING_PTR(rb_hash_aref(attr, save));
  if (flags & OFN_OVERWRITEPROMPT)
    ok = GetSaveFileName(&ofn);
  else
    ok = GetOpenFileName(&ofn);

  free(filters);

  if (ok)
    path = rb_str_new2(shoes_fix_slashes(ofn.lpstrFile));

  return path;
}

static VALUE
shoes_dialog_chooser2(VALUE self, char *title, UINT flags, VALUE attr)
{
  VALUE path = Qnil;
  BROWSEINFO bi = {0};
  bi.lpszTitle = title;
  bi.ulFlags = BIF_USENEWUI | flags;
  LPITEMIDLIST pidl = SHBrowseForFolder (&bi);
  if (pidl != 0)
  {
    char _path[MAX_PATH+1];
    if (SHGetPathFromIDList(pidl, _path))
      path = rb_str_new2(shoes_fix_slashes(_path));

    IMalloc *imalloc = 0;
    if (SUCCEEDED(SHGetMalloc(&imalloc)))
    {
      IMalloc_Free(imalloc, pidl);
      IMalloc_Release(imalloc);
    }
  }
  return path;
}

VALUE
shoes_dialog_open(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Open file...", OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, args.a[0]);
}

VALUE
shoes_dialog_save(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser(self, "Save file...", OFN_OVERWRITEPROMPT, args.a[0]);
}

VALUE
shoes_dialog_open_folder(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser2(self, "Open folder...", BIF_NONEWFOLDERBUTTON | BIF_RETURNONLYFSDIRS, args.a[0]);
}

VALUE
shoes_dialog_save_folder(int argc, VALUE *argv, VALUE self)
{
  rb_arg_list args;
  rb_parse_args(argc, argv, "|h", &args);
  return shoes_dialog_chooser2(self, "Save folder...", BIF_RETURNONLYFSDIRS, args.a[0]);
}
