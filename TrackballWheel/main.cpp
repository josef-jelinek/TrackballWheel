#include <windows.h>
#include "ui.h"

const int tray_icon_id = 1;
const int tray_message_id = WM_APP;
const int button_filter_delay_ms = 100;

HHOOK mouse_hook;
bool is_deactivated = false;
bool is_pressed = false;
bool is_scrolling = false;
POINT origin;
UINT_PTR start_scroll_timer_id = 0;
UINT_PTR end_scroll_timer_id = 0;
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
  SendInput(2, input, sizeof INPUT);
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
  SendInput(2, input, sizeof INPUT);
}

void cancel_end_scroll() {
  if (end_scroll_timer_id != 0 && KillTimer(nullptr, end_scroll_timer_id)) {
    end_scroll_timer_id = 0;
  }
}

void CALLBACK handle_end_scroll(HWND, UINT, UINT_PTR, DWORD) {
  cancel_end_scroll();
  if (end_scroll_timer_id == 0) {
    if (is_pressed && !is_scrolling) {
      // if the middle button was not used for scrolling, make it click
      send_mouse_click();
    }
    is_pressed = is_scrolling = false;
  }
}

void schedule_end_scroll() {
  cancel_end_scroll();
  if (end_scroll_timer_id == 0) {
    end_scroll_timer_id = SetTimer(nullptr, 0, button_filter_delay_ms, handle_end_scroll);
  }
}

LRESULT CALLBACK mouse_handler(int message_id, WPARAM event_id, LPARAM data) {
  if (!is_deactivated && message_id == HC_ACTION) {
    if (event_id == WM_MBUTTONDOWN) {
      if (is_pressed) {
        // continue scrolling like there is no tomorrow
        cancel_end_scroll();
      } else {
        // fresh middle button down 
        is_pressed = true;
        is_scrolling = false;
        origin = reinterpret_cast<const MSLLHOOKSTRUCT *>(data)->pt;
      }
      return 1;
    }
    if (event_id == WM_MBUTTONUP) {
      // tentatively end scrolling to filter accidental fast middle button ups and downs
      schedule_end_scroll();
      return 1;
    }
    if (event_id == WM_MOUSEMOVE && is_pressed) {
      is_scrolling = true;
      SetCursorPos(origin.x, origin.y);
      send_mouse_scroll(reinterpret_cast<const MSLLHOOKSTRUCT *>(data)->pt);
      return 1;
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
  mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, mouse_handler, instance_handle, 0);
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
