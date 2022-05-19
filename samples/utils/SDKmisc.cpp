//--------------------------------------------------------------------------------------
// File: SDKmisc.cpp
//
// Various helper functionality that is shared between SDK samples
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=320437
//--------------------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#include "SDKmisc.h"

#include <string.h>
#include <stdio.h>

#define DXUTERR_MEDIANOTFOUND MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0903)

//--------------------------------------------------------------------------------------
// Internal functions forward declarations
//--------------------------------------------------------------------------------------
static bool DXUTFindMediaSearchTypicalDirs(char *strSearchPath, int cchSearch, char const *strLeaf, char const *strExePath, char const *strExeName);
static bool DXUTFindMediaSearchParentDirs(char *strSearchPath, int cchSearch, char const *strStartAt, char const *strLeafName);

//--------------------------------------------------------------------------------------
// Returns pointer to static media search buffer
//--------------------------------------------------------------------------------------
char *DXUTMediaSearchPath()
{
    static char s_strMediaSearchPath[MAX_PATH] = {0};
    return s_strMediaSearchPath;
}

//--------------------------------------------------------------------------------------
char const *DXUTGetMediaSearchPath()
{
    return DXUTMediaSearchPath();
}

//--------------------------------------------------------------------------------------
HRESULT DXUTSetMediaSearchPath(char const *strPath)
{
    HRESULT hr;

    char *s_strSearchPath = DXUTMediaSearchPath();

    hr = strcpy_s(s_strSearchPath, MAX_PATH, strPath);
    if (SUCCEEDED(hr))
    {
        // append slash if needed
        size_t ch = 0;
        ch = strnlen(s_strSearchPath, MAX_PATH);
        if (SUCCEEDED(hr) && s_strSearchPath[ch - 1] != '\\')
        {
            hr = strcat_s(s_strSearchPath, MAX_PATH, "\\");
        }
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Tries to find the location of a SDK media file
//       cchDest is the size in WCHARs of strDestPath.  Be careful not to
//       pass in sizeof(strDest) on UNICODE builds.
//--------------------------------------------------------------------------------------
HRESULT DXUTFindDXSDKMediaFileCch(char *strDestPath, int cchDest, char const *strFilename)
{
    bool bFound;
    char strSearchFor[MAX_PATH];

    if (!strFilename || strFilename[0] == 0 || !strDestPath || cchDest < 10)
        return E_INVALIDARG;

    // Get the exe name, and exe path
    char strExePath[MAX_PATH] = {0};
    char strExeName[MAX_PATH] = {0};
    char *strLastSlash = nullptr;
    GetModuleFileNameA(nullptr, strExePath, MAX_PATH);
    strExePath[MAX_PATH - 1] = 0;
    strLastSlash = strrchr(strExePath, TEXT('\\'));
    if (strLastSlash)
    {
        strcpy_s(strExeName, MAX_PATH, &strLastSlash[1]);

        // Chop the exe name from the exe path
        *strLastSlash = 0;

        // Chop the .exe from the exe name
        strLastSlash = strrchr(strExeName, TEXT('.'));
        if (strLastSlash)
            *strLastSlash = 0;
    }

    // Typical directories:
    //      .\
    //      ..\
    //      ..\..\
    //      %EXE_DIR%\
    //      %EXE_DIR%\..\
    //      %EXE_DIR%\..\..\
    //      %EXE_DIR%\..\%EXE_NAME%
    //      %EXE_DIR%\..\..\%EXE_NAME%

    // Typical directory search
    bFound = DXUTFindMediaSearchTypicalDirs(strDestPath, cchDest, strFilename, strExePath, strExeName);
    if (bFound)
        return S_OK;

    // Typical directory search again, but also look in a subdir called "\media\"
    sprintf_s(strSearchFor, MAX_PATH, "media\\%s", strFilename);
    bFound = DXUTFindMediaSearchTypicalDirs(strDestPath, cchDest, strSearchFor, strExePath, strExeName);
    if (bFound)
        return S_OK;

    char strLeafName[MAX_PATH] = {0};

    // Search all parent directories starting at .\ and using strFilename as the leaf name
    strcpy_s(strLeafName, MAX_PATH, strFilename);
    bFound = DXUTFindMediaSearchParentDirs(strDestPath, cchDest, ".", strLeafName);
    if (bFound)
        return S_OK;

    // Search all parent directories starting at the exe's dir and using strFilename as the leaf name
    bFound = DXUTFindMediaSearchParentDirs(strDestPath, cchDest, strExePath, strLeafName);
    if (bFound)
        return S_OK;

    // Search all parent directories starting at .\ and using "media\strFilename" as the leaf name
    sprintf_s(strLeafName, MAX_PATH, "media\\%s", strFilename);
    bFound = DXUTFindMediaSearchParentDirs(strDestPath, cchDest, ".", strLeafName);
    if (bFound)
        return S_OK;

    // Search all parent directories starting at the exe's dir and using "media\strFilename" as the leaf name
    bFound = DXUTFindMediaSearchParentDirs(strDestPath, cchDest, strExePath, strLeafName);
    if (bFound)
        return S_OK;

    // On failure, return the file as the path but also return an error code
    strcpy_s(strDestPath, cchDest, strFilename);

    return DXUTERR_MEDIANOTFOUND;
}

//--------------------------------------------------------------------------------------
// Search a set of typical directories
//--------------------------------------------------------------------------------------
static bool DXUTFindMediaSearchTypicalDirs(char *strSearchPath, int cchSearch, char const *strLeaf, char const *strExePath, char const *strExeName)
{
    // Typical directories:
    //      .\
    //      ..\
    //      ..\..\
    //      %EXE_DIR%\
    //      %EXE_DIR%\..\
    //      %EXE_DIR%\..\..\
    //      %EXE_DIR%\..\%EXE_NAME%
    //      %EXE_DIR%\..\..\%EXE_NAME%
    //      DXSDK media path

    // Search in .\  
    strcpy_s(strSearchPath, cchSearch, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in ..\  
    sprintf_s(strSearchPath, cchSearch, "..\\%s", strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in ..\..\ 
    sprintf_s(strSearchPath, cchSearch, "..\\..\\%s", strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in ..\..\ 
    sprintf_s(strSearchPath, cchSearch, "..\\..\\%s", strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in the %EXE_DIR%\ 
    sprintf_s(strSearchPath, cchSearch, "%s\\%s", strExePath, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in the %EXE_DIR%\..\ 
    sprintf_s(strSearchPath, cchSearch, "%s\\..\\%s", strExePath, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in the %EXE_DIR%\..\..\ 
    sprintf_s(strSearchPath, cchSearch, "%s\\..\\..\\%s", strExePath, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in "%EXE_DIR%\..\%EXE_NAME%\".  This matches the DirectX SDK layout
    sprintf_s(strSearchPath, cchSearch, "%s\\..\\%s\\%s", strExePath, strExeName, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in "%EXE_DIR%\..\..\%EXE_NAME%\".  This matches the DirectX SDK layout
    sprintf_s(strSearchPath, cchSearch, "%s\\..\\..\\%s\\%s", strExePath, strExeName, strLeaf);
    if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
        return true;

    // Search in media search dir
    char *s_strSearchPath = DXUTMediaSearchPath();
    if (s_strSearchPath[0] != 0)
    {
        sprintf_s(strSearchPath, cchSearch, "%s%s", s_strSearchPath, strLeaf);
        if (GetFileAttributesA(strSearchPath) != 0xFFFFFFFF)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------
// Search parent directories starting at strStartAt, and appending strLeafName
// at each parent directory.  It stops at the root directory.
//--------------------------------------------------------------------------------------
static bool DXUTFindMediaSearchParentDirs(char *strSearchPath, int cchSearch, char const *strStartAt, char const *strLeafName)
{
    char strFullPath[MAX_PATH] = {0};
    char strFullFileName[MAX_PATH] = {0};
    char strSearch[MAX_PATH] = {0};
    char *strFilePart = nullptr;

    if (!GetFullPathNameA(strStartAt, MAX_PATH, strFullPath, &strFilePart))
        return false;

#pragma warning(disable : 6102)
    while (strFilePart && *strFilePart != '\0')
    {
        sprintf_s(strFullFileName, MAX_PATH, "%s\\%s", strFullPath, strLeafName);
        if (GetFileAttributesA(strFullFileName) != 0xFFFFFFFF)
        {
            strcpy_s(strSearchPath, cchSearch, strFullFileName);
            return true;
        }

        sprintf_s(strSearch, MAX_PATH, "%s\\..", strFullPath);
        if (!GetFullPathNameA(strSearch, MAX_PATH, strFullPath, &strFilePart))
            return false;
    }

    return false;
}
