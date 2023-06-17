#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3dcompiler.h>

static HRESULT(WINAPI *g_pFn_Original_D3DCompile)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, CONST D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs) = NULL;
static HRESULT(WINAPI *g_pFn_Original_D3DReflect)(LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void **ppReflector) = NULL;

static struct Initialize_Original_Function_Pointers
{
    Initialize_Original_Function_Pointers()
    {
        HMODULE hModule = LoadLibraryExW(D3DCOMPILER_DLL_W, NULL, 0U);
        g_pFn_Original_D3DCompile = reinterpret_cast<HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR, CONST D3D_SHADER_MACRO *, ID3DInclude *, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob **, ID3DBlob **)>(GetProcAddress(hModule, "D3DCompile"));
        g_pFn_Original_D3DReflect = reinterpret_cast<HRESULT(WINAPI *)(LPCVOID, SIZE_T, REFIID, void **)>(GetProcAddress(hModule, "D3DReflect"));
    };
} Instance_Initialize_Original_Function_Pointers;

extern "C" HRESULT WINAPI D3DCompile(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, CONST D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs)
{
#ifndef NDEBUG
    Flags1 = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    Flags1 = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    return g_pFn_Original_D3DCompile(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}

extern "C" HRESULT WINAPI D3DReflect(LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void **ppReflector)
{
    return g_pFn_Original_D3DReflect(pSrcData, SrcDataSize, pInterface, ppReflector);
}

extern "C" HRESULT WINAPI D3DStripShader(LPCVOID pShaderBytecode, SIZE_T BytecodeLength, UINT uStripFlags, ID3DBlob **ppStrippedBlob)
{
    return E_FAIL;
}
