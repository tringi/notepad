#ifndef NOTEPAD_IPC_HPP
#define NOTEPAD_IPC_HPP

#include "Notepad_File.hpp"
#include <map>

// AskInstancesForOpenFile
//  - iterates over running Notepad instances and queries if 'file' is already open, activating that window
//  - parameters:
//     - hAbove - if non-NULL, 
//     - file - file ID is passed to the instances to check, if it's alreadyopen
//     - instances - if non-NULL, the function fills it with PID/HWND pairs of visited running instances
//  - returns:
//     - true if the file was already open
//     - false otherwise
//
bool AskInstancesForOpenFile (HWND hAbove, File * file, std::map <DWORD, HWND> * instances);

// AskInstanceToOpenFile
//  - requests one particular instance pid/hWnd to open new window with 'hFile' in it
//
bool AskInstanceToOpenFile (DWORD pid, HWND hWnd, HANDLE hFile, int nCmdShow);

// AskInstancesToOpenWindow
//  - requests random running instance to open new window and show it in 'nCmdShow' manner
//
bool AskInstancesToOpenWindow (int nCmdShow);

#endif
