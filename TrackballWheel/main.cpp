#include <windows.h>
#include "ui.h"

const int tray_icon_id = 1;
const int tray_message_id = WM_APP;

HHOOK mouse_hook;
bool is_deactivated = false;
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

LRESULT CALLBACK mouse_handler(int message_id, WPARAM event_id, LPARAM data) {
  static bool is_pressed = false;
  static bool is_scrolling = false;
  static POINT origin;
  if (!is_deactivated && message_id == HC_ACTION) {
    const MSLLHOOKSTRUCT * event_data = reinterpret_cast<const MSLLHOOKSTRUCT *>(data);
    if (event_id == WM_MBUTTONDOWN) {
      is_pressed = true;
      is_scrolling = false;
      origin = event_data->pt;
      return 1;
    }
    if (event_id == WM_MBUTTONUP) {
      if (is_pressed && !is_scrolling) {
        INPUT input[2];
        input[0].type = input[1].type = INPUT_MOUSE;
        input[0].mi.dx = input[1].mi.dx = event_data->pt.x;
        input[0].mi.dy = input[1].mi.dy = event_data->pt.y;
        input[0].mi.mouseData = input[1].mi.mouseData = 0;
        input[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
        input[0].mi.time = input[1].mi.time = 0;
        input[0].mi.dwExtraInfo = input[1].mi.dwExtraInfo = 0;
        SendInput(2, input, sizeof INPUT);
      }
      is_pressed = is_scrolling = false;
      return 1;
    }
    if (event_id == WM_MOUSEMOVE && is_pressed) {
      is_scrolling = true;
      SetCursorPos(origin.x, origin.y);
      int dx = origin.x - event_data->pt.x;
      int dy = origin.y - event_data->pt.y;
      // prefer vertical scrolling
      if (abs(dy) > abs(dx)) {
        dx = 0;
      }
      INPUT input[2];
      input[0].type = input[1].type = INPUT_MOUSE;
      input[0].mi.dx = input[1].mi.dx = event_data->pt.x;
      input[0].mi.dy = input[1].mi.dy = event_data->pt.y;
      input[0].mi.mouseData = 10 * dx;
      input[1].mi.mouseData = 10 * dy;
      input[0].mi.dwFlags = MOUSEEVENTF_HWHEEL;
      input[1].mi.dwFlags = MOUSEEVENTF_WHEEL;
      input[0].mi.time = input[1].mi.time = 0;
      input[0].mi.dwExtraInfo = input[1].mi.dwExtraInfo = 0;
      SendInput(2, input, sizeof INPUT);
      return 1;
    }
  }
  return CallNextHookEx(mouse_hook, message_id, event_id, data);
}

INT_PTR CALLBACK dialog_handler(HWND window_handle, UINT message_id, WPARAM, LPARAM lParam) {
  if (message_id == tray_message_id) {
    if (lParam == WM_MBUTTONDBLCLK) {
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

int APIENTRY wWinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR pCmdLine, int nCmdShow) {
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
  return msg.wParam;
}
