#include <windows.h>
#include "ui.h"

const int tray_icon_id = 1;
const int tray_message_id = WM_APP;
const int too_fast_click_ms = 30;
const int too_slow_click_ms = 300;

HHOOK mouse_hook;
bool is_deactivated = false;
bool is_pressed = false;
bool is_scrolling = false;
bool is_fresh = true;
POINT origin;
FILETIME last_button_down_time = { 0, 0 };
FILETIME last_button_up_time = { 0, 0 };
NOTIFYICONDATA tray_icon_data;
HICON active_icon, inactive_icon;

void show_tray_icon(HWND dialog) {
  ZeroMemory(&tray_icon_data, sizeof NOTIFYICONDATA);
  tray_icon_data.cbSize = sizeof NOTIFYICONDATA;
  tray_icon_data.uID = tray_icon_id;
  tray_icon_data.uFlags = NIF_ICON | NIF_MESSAGE;
  tray_icon_data.hIcon = active_icon;
  tray_icon_data.hWnd = dialog;
  tray_icon_data.uCallbackMessage = tray_message_id;
  Shell_NotifyIcon(NIM_ADD, &tray_icon_data);
}

void update_tray_icon(bool is_active) {
  tray_icon_data.hIcon = is_active ? active_icon : inactive_icon;
  Shell_NotifyIcon(NIM_MODIFY, &tray_icon_data);
}

void delete_tray_icon() {
  tray_icon_data.uFlags = 0;
  Shell_NotifyIcon(NIM_DELETE, &tray_icon_data);
}

void send_mouse_click() {
  INPUT input[2];
  input[0].type = input[1].type = INPUT_MOUSE;
  input[0].mi.dx = input[1].mi.dx = origin.x;
  input[0].mi.dy = input[1].mi.dy = origin.y;
  input[0].mi.mouseData = input[1].mi.mouseData = 0;
  input[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
  input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
  input[0].mi.time = input[1].mi.time = 0;
  input[0].mi.dwExtraInfo = input[1].mi.dwExtraInfo = 0;
  SendInput(2, &input[0], sizeof INPUT);
}

void send_mouse_scroll(POINT pt) {
  int dx = origin.x - pt.x;
  int dy = origin.y - pt.y;
  // prefer vertical scrolling
  if (abs(dy) > abs(dx)) {
    dx = 0;
  }
  INPUT input[2];
  input[0].type = input[1].type = INPUT_MOUSE;
  input[0].mi.dx = input[1].mi.dx = pt.x;
  input[0].mi.dy = input[1].mi.dy = pt.y;
  input[0].mi.mouseData = 10 * dx;
  input[1].mi.mouseData = 10 * dy;
  input[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
  input[1].mi.dwFlags = MOUSEEVENTF_WHEEL;
  input[0].mi.time = input[1].mi.time = 0;
  input[0].mi.dwExtraInfo = input[1].mi.dwExtraInfo = 0;
  SendInput(2, &input[0], sizeof INPUT);
}

int get_time(LPFILETIME new_time, FILETIME old_time) {
  GetSystemTimePreciseAsFileTime(new_time);
  ULARGE_INTEGER new_value, old_value;
  new_value.HighPart = new_time->dwHighDateTime;
  new_value.LowPart = new_time->dwLowDateTime;
  old_value.HighPart = old_time.dwHighDateTime;
  old_value.LowPart = old_time.dwLowDateTime;
  ULONGLONG d = (new_value.QuadPart - old_value.QuadPart) / 10000;
  return d > MAXINT ? MAXINT : (int)d;
}

LRESULT CALLBACK event_handler(int message_id, WPARAM event_id, LPARAM data) {
  if (!is_deactivated && message_id == HC_ACTION) {
    if (event_id == WM_MBUTTONDOWN) {
      if (is_fresh) {
        is_pressed = true;
        FILETIME time;
        int up_down_ms = get_time(&time, last_button_up_time);
        if (!is_scrolling || up_down_ms > too_fast_click_ms) {
          is_scrolling = false;
          origin = reinterpret_cast<const MSLLHOOKSTRUCT *>(data)->pt;
        }
        last_button_down_time = time;
        return 1;
      }
    }
    if (event_id == WM_MBUTTONUP) {
      if (is_fresh) {
        is_pressed = false;
        FILETIME time;
        int down_up_ms = get_time(&time, last_button_down_time);
        if (!is_scrolling && down_up_ms > too_fast_click_ms && down_up_ms < too_slow_click_ms) {
          is_fresh = false;
          send_mouse_click();
        }
        last_button_up_time = time;
        return 1;
      } else {
        is_fresh = true;
      }
    }
    if (event_id == WM_MOUSEMOVE) {
      if (is_pressed) {
        is_scrolling = true;
        SetCursorPos(origin.x, origin.y);
        send_mouse_scroll(reinterpret_cast<const MSLLHOOKSTRUCT *>(data)->pt);
        return 1;
      }
    }
  }
  return CallNextHookEx(mouse_hook, message_id, event_id, data);
}

INT_PTR CALLBACK dialog_handler(HWND window_handle, UINT message_id, WPARAM, LPARAM lParam) {
  if (message_id == tray_message_id) {
    if (lParam == WM_LBUTTONDBLCLK) {
      DestroyWindow(window_handle);
    } else if (lParam == WM_LBUTTONDOWN) {
      is_deactivated = !is_deactivated;
      update_tray_icon(!is_deactivated);
    }
  } else if (message_id == WM_DESTROY) {
    PostQuitMessage(0);
  }
  return 0;
}

HICON load_icon(HINSTANCE instance_handle, int resource_id) {
  return (HICON)LoadImage(
    instance_handle,
    MAKEINTRESOURCE(resource_id),
    IMAGE_ICON,
    GetSystemMetrics(SM_CXSMICON),
    GetSystemMetrics(SM_CYSMICON),
    LR_DEFAULTCOLOR);
}

int APIENTRY wWinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int) {
  mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, event_handler, instance_handle, 0);
  if (mouse_hook == nullptr) {
    return 1;
  }
  HWND dialog = CreateDialog(instance_handle, MAKEINTRESOURCE(DIALOG_RID), nullptr, dialog_handler);
  if (dialog == nullptr) {
    return 1;
  }
  active_icon = load_icon(instance_handle, ICON_ACTIVE_RID);
  inactive_icon = load_icon(instance_handle, ICON_INACTIVE_RID);
  show_tray_icon(dialog);
  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    DispatchMessage(&msg);
  }
  delete_tray_icon();
  UnhookWindowsHookEx(mouse_hook);
  return (int)msg.wParam;
}
