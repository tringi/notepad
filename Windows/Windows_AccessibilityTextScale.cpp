#include "Windows_AccessibilityTextScale.hpp"
#include <VersionHelpers.h>

bool Windows::TextScale::initialize () {
    if (IsWindows10OrGreater ()) { // TODO: which build?
        this->hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
        if (this->hEvent)
            if (this->ReOpenKeys ()) {
                RegisterWaitForSingleObject (&hThreadPoolWait, this->hEvent,
                                             [] (PVOID self, BOOLEAN) {
                                                 if (static_cast <Windows::TextScale *> (self)->OnEvent ()) {
                                                     // SendMessage ((HWND) hWnd, WM_SETTINGCHANGE, 0, 0);
                                                 }
                                             }, this, INFINITE, 0);
                return true;
            }
    }
    return false;
}

void Windows::TextScale::terminate () {
    if (this->hThreadPoolWait) {
        if (UnregisterWaitEx (this->hThreadPoolWait, INVALID_HANDLE_VALUE)) {
            CloseHandle (this->hEvent);
        }
    } else {
        CloseHandle (this->hEvent);
    }
    RegCloseKey (this->hKey);

    this->hThreadPoolWait = NULL;
    this->hEvent = NULL;
    this->hKey = NULL;
}

Windows::TextScale::~TextScale () {
    this->terminate ();
}

bool Windows::TextScale::OnEvent () {
    if (this->parent) {
        // "Accessibility" subkey might have been created, try to acces it again
        if (this->ReOpenKeys ()) {
            // if the subkey was created, we now have new scale factor, so report that as change
            return this->parent == false;
        } else
            return false;

    } else {
        bool changed = false;
        // some value inside "Accessibility" subkey has changed, see if it was "TextScaleFactor"
        auto updated = this->GetCurrentTextScaleFactor ();
        if (this->current != updated) {
            this->current = updated;
            changed = true;
        }
        // re-register for next event
        RegNotifyChangeKeyValue (this->hKey, FALSE, REG_NOTIFY_THREAD_AGNOSTIC | REG_NOTIFY_CHANGE_LAST_SET, this->hEvent, TRUE);
        return changed;
    }
}

bool Windows::TextScale::ReOpenKeys () {
    if (this->hKey) {
        RegCloseKey (this->hKey);
        this->hKey = NULL;
    }
    if (RegOpenKeyEx (HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Accessibility", 0, KEY_NOTIFY | KEY_QUERY_VALUE, &this->hKey) == ERROR_SUCCESS) {
        if (RegNotifyChangeKeyValue (this->hKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET, this->hEvent, TRUE) == ERROR_SUCCESS) {
            this->parent = false;
            this->current = this->GetCurrentTextScaleFactor ();
            return true;
        }
    } else {
        if (RegOpenKeyEx (HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft", 0, KEY_NOTIFY, &this->hKey) == ERROR_SUCCESS) {
            if (RegNotifyChangeKeyValue (this->hKey, FALSE, REG_NOTIFY_CHANGE_NAME, this->hEvent, TRUE) == ERROR_SUCCESS) {
                this->parent = true;
                return true;
            }
        }
    }
    return false;
}

DWORD Windows::TextScale::GetCurrentTextScaleFactor () const {
    DWORD scale;
    DWORD cb = sizeof scale;

    if ((this->parent == false) && (this->hKey != NULL) && (RegQueryValueEx (this->hKey, L"TextScaleFactor", NULL, NULL, (LPBYTE) &scale, &cb) == ERROR_SUCCESS)) {
        return scale;
    } else
        return 100;
}
