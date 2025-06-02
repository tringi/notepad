#ifndef WINDOWS_ISWINDOWSBUILDORGREATER_HPP
#define WINDOWS_ISWINDOWSBUILDORGREATER_HPP

/* Windows IsWindowsBuildOrGreater version helper addition
// Windows_IsWindowsBuildOrGreater.hpp
//
// Author: Jan Ringos, http://Tringi.MX-3.cz, jan@ringos.cz
// Version: 1.0
//
// Changelog:
//      23.11.2017 - initial version
*/

#include <windows.h>

namespace Windows {
    
    // IsWindowsBuildOrGreater
    //  - determines, if current OS matches or is newer than the specified version
    
    bool IsWindowsBuildOrGreater (WORD wMajorVersion, WORD wMinorVersion, DWORD dwBuildNumber);
    bool IsWindowsBuildOrGreater (DWORD dwBuildNumber);
};

#endif

