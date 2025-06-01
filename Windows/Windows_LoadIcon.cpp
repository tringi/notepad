#include "Windows_LoadIcon.hpp"
#include "Windows_GetCurrentModuleHandle.hpp"
#include "Windows_Window.hpp"

#include <VersionHelpers.h>
#include <CommCtrl.h>
#include <shellapi.h>

HICON Windows::LoadBestIcon (LPCWSTR resource, SIZE size) {
    return LoadBestIcon (Windows::GetCurrentModuleHandle (), resource, size);
}

HICON Windows::LoadBestIcon (HMODULE hModule, LPCWSTR resource, SIZE size) {
    HICON hNewIcon = NULL;
    if (size.cx && size.cy) {
        if (LoadIconWithScaleDown (hModule, resource, size.cx, size.cy, &hNewIcon) == S_OK) {
            return hNewIcon;
        }
    }
    return (HICON) LoadImage (hModule, resource, IMAGE_ICON, size.cx, size.cy, LR_DEFAULTCOLOR);
}

extern "C" const IID IID_IImageList;

SIZE Windows::GetIconMetrics (Windows::IconSize size, const LONG * metrics, UINT dpi, UINT dpiSystem) {
    switch (size) {
        case IconSize::Small:
            return { metrics [SM_CXSMICON], metrics [SM_CYSMICON] };
        case IconSize::Start:
            return {
                (metrics [SM_CXICON] + metrics [SM_CXSMICON]) / 2,
                (metrics [SM_CYICON] + metrics [SM_CYSMICON]) / 2
            };
        case IconSize::Large:
        default:
            return { metrics [SM_CXICON], metrics [SM_CYICON] };

        case IconSize::Taskbar:
            if (IsWindows10OrGreater ()) {
                return GetIconMetrics (IconSize::Start, metrics, dpi, dpiSystem);
            } else {
                return GetIconMetrics (IconSize::Large, metrics, dpi, dpiSystem);
            }
    }
    return { 0, 0 };
}
