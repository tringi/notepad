#include "Windows_IsWindowsBuildOrGreater.hpp"
#include <VersionHelpers.h>

/* Windows IsWindowsBuildOrGreater version helper addition
// Windows_IsWindowsBuildOrGreater.cpp
//
// Author: Jan Ringos, http://Tringi.MX-3.cz, jan@ringos.cz
// Version: 1.0
//
// Changelog:
//      23.11.2017 - initial version
*/

bool Windows::IsWindowsBuildOrGreater (WORD wMajorVersion, WORD wMinorVersion, DWORD dwBuildNumber) {
    OSVERSIONINFOEXW osvi = { sizeof (osvi), 0, 0, 0, 0, { 0 }, 0, 0 };
    DWORDLONG mask = 0;

    mask = VerSetConditionMask (mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    mask = VerSetConditionMask (mask, VER_MINORVERSION, VER_GREATER_EQUAL);
    mask = VerSetConditionMask (mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.dwBuildNumber = dwBuildNumber;

    return VerifyVersionInfoW (&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, mask);
}

bool Windows::IsWindowsBuildOrGreater (DWORD dwBuildNumber) {
    OSVERSIONINFOEXW osvi = { sizeof (osvi), 0, 0, 0, 0, { 0 }, 0, 0 };
    osvi.dwBuildNumber = dwBuildNumber;

    return VerifyVersionInfoW (&osvi, VER_BUILDNUMBER,
                               VerSetConditionMask (0, VER_BUILDNUMBER, VER_GREATER_EQUAL));
}
