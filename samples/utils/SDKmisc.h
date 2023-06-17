//--------------------------------------------------------------------------------------
// File: SDKMisc.h
//
// Various helper functionality that is shared between SDK samples
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=320437
//--------------------------------------------------------------------------------------
#pragma once

//--------------------------------------------------------------------------------------
// Tries to finds a media file by searching in common locations
//--------------------------------------------------------------------------------------
HRESULT DXUTFindDXSDKMediaFileCch(char *strDestPath, int cchDest, char const *strFilename);
HRESULT DXUTSetMediaSearchPath(char const *strPath);
char const *DXUTGetMediaSearchPath();