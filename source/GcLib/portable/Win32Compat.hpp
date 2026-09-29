#ifndef __GCLIB_PORTABLE_WIN32COMPAT__
#define __GCLIB_PORTABLE_WIN32COMPAT__

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <cwchar>
#include <cwctype>
#include <cstdio>

using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using LONG = std::int32_t;
using ULONG = std::uint32_t;
using UINT = std::uint32_t;
using INT = std::int32_t;
using SHORT = std::int16_t;
using USHORT = std::uint16_t;
using _int64 = std::int64_t;
using BOOL = int;
using boolean = bool;
using FLOAT = float;
using WCHAR = wchar_t;
using CHAR = char;
using HRESULT = long;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using ULONG_PTR = std::uintptr_t;
using SIZE_T = std::size_t;

using HANDLE = void*;
using HWND = void*;
using HINSTANCE = void*;
using HMODULE = void*;
using HDC = void*;
using HFONT = void*;
using HBITMAP = void*;
using HBRUSH = void*;
using HPEN = void*;
using HICON = void*;
using HCURSOR = void*;
using HMENU = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using LPWSTR = WCHAR*;
using LPCWSTR = const WCHAR*;
using LPSTR = CHAR*;
using LPCSTR = const CHAR*;
using LPCTSTR = const WCHAR*;

struct POINT { LONG x; LONG y; };
struct SIZE { LONG cx; LONG cy; };
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
struct WIN32_FIND_DATA {
  DWORD dwFileAttributes;
  DWORD nFileSizeLow = 0, nFileSizeHigh = 0;
  WCHAR cFileName[260];
};
struct LARGE_INTEGER { long long QuadPart; };
struct CRITICAL_SECTION { void* native; };
struct SYSTEMTIME {
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
};

constexpr std::size_t LF_FACESIZE = 32;
struct LOGFONT {
  LONG lfHeight;
  LONG lfWidth;
  LONG lfEscapement;
  LONG lfOrientation;
  LONG lfWeight;
  BYTE lfItalic;
  BYTE lfUnderline;
  BYTE lfStrikeOut;
  BYTE lfCharSet;
  BYTE lfOutPrecision;
  BYTE lfClipPrecision;
  BYTE lfQuality;
  BYTE lfPitchAndFamily;
  WCHAR lfFaceName[LF_FACESIZE];
};

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef _MAX_PATH
#define _MAX_PATH MAX_PATH
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(std::intptr_t)-1)
#endif
#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr) ((HRESULT)(hr) < 0)
#endif
#ifndef ZeroMemory
#define ZeroMemory(destination, length) std::memset((destination), 0, (length))
#endif
#ifndef _vsnprintf
#define _vsnprintf std::vsnprintf
#endif

constexpr DWORD FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
constexpr DWORD INFINITE = 0xffffffffu;
constexpr DWORD WAIT_OBJECT_0 = 0, WAIT_TIMEOUT = 258;
constexpr DWORD INVALID_FILE_SIZE = 0xffffffffu, INVALID_SET_FILE_POINTER = 0xffffffffu;
constexpr DWORD FILE_BEGIN = 0;
constexpr DWORD FILE_CURRENT = 1;
constexpr DWORD FILE_END = 2;
constexpr UINT WM_APP = 0x8000;
constexpr UINT WM_DESTROY = 0x0002;
constexpr UINT WM_CLOSE = 0x0010;
constexpr UINT WM_KEYDOWN = 0x0100;
constexpr UINT WM_COMMAND = 0x0111;
constexpr UINT MB_OK = 0, MB_ICONERROR = 0x10;
constexpr DWORD RGB(BYTE r, BYTE g, BYTE b) { return DWORD(r) | (DWORD(g) << 8) | (DWORD(b) << 16); }
extern "C" int MessageBox(HWND window,LPCWSTR text,LPCWSTR title,UINT type);
constexpr WPARAM VK_RETURN = 0x0d;
constexpr LONG FW_NORMAL = 400;
constexpr LONG FW_BOLD = 700;
constexpr BYTE ANSI_CHARSET = 0;
constexpr BYTE DEFAULT_CHARSET = 1;
constexpr BYTE SHIFTJIS_CHARSET = 128;
inline BOOL IsCharAlphaNumeric(wchar_t character) { return std::iswalnum(character) != 0; }
constexpr UINT CP_ACP = 0;
constexpr UINT CP_UTF8 = 65001;
constexpr DWORD LOCALE_SYSTEM_DEFAULT = 0x0800;
constexpr DWORD NORM_IGNORECASE = 0x00000001;
constexpr int CSTR_LESS_THAN = 1;
constexpr int CSTR_EQUAL = 2;
constexpr int CSTR_GREATER_THAN = 3;

constexpr DWORD WS_POPUP = 0x80000000u;
constexpr DWORD WS_CHILD = 0x40000000u;
constexpr DWORD WS_VISIBLE = 0x10000000u;
constexpr DWORD WS_CLIPSIBLINGS = 0x04000000u;
constexpr DWORD WS_CLIPCHILDREN = 0x02000000u;
constexpr DWORD WS_CAPTION = 0x00c00000u;
constexpr DWORD WS_BORDER = 0x00800000u;
constexpr DWORD WS_VSCROLL = 0x00200000u;
constexpr DWORD WS_HSCROLL = 0x00100000u;
constexpr DWORD WS_SYSMENU = 0x00080000u;
constexpr DWORD WS_THICKFRAME = 0x00040000u;
constexpr DWORD WS_SIZEBOX = WS_THICKFRAME;
constexpr DWORD WS_MINIMIZEBOX = 0x00020000u;
constexpr DWORD WS_MAXIMIZEBOX = 0x00010000u;
constexpr DWORD WS_TABSTOP = 0x00010000u;
constexpr DWORD WS_OVERLAPPEDWINDOW = 0x00cf0000u;

constexpr DWORD WS_EX_DLGMODALFRAME = 0x00000001u;
constexpr DWORD WS_EX_TOOLWINDOW = 0x00000080u;
constexpr DWORD WS_EX_CLIENTEDGE = 0x00000200u;
constexpr DWORD WS_EX_STATICEDGE = 0x00020000u;

constexpr DWORD TVS_HASBUTTONS = 0x0001u;
constexpr DWORD TVS_HASLINES = 0x0002u;
constexpr DWORD TVS_LINESATROOT = 0x0004u;
constexpr UINT TVIF_TEXT = 0x0001u;
constexpr UINT TVIF_PARAM = 0x0004u;

constexpr DWORD LVS_REPORT = 0x0001u;
constexpr DWORD LVS_SINGLESEL = 0x0004u;
constexpr DWORD LVS_SHOWSELALWAYS = 0x0008u;
constexpr DWORD LVS_NOCOLUMNHEADER = 0x4000u;
constexpr DWORD LVS_NOSORTHEADER = 0x8000u;
constexpr DWORD LVS_EX_GRIDLINES = 0x0001u;
constexpr DWORD LVS_EX_CHECKBOXES = 0x0004u;
constexpr DWORD LVS_EX_FULLROWSELECT = 0x0020u;
constexpr UINT LVIF_TEXT = 0x0001u;
constexpr UINT LVIF_PARAM = 0x0004u;

constexpr DWORD ES_MULTILINE = 0x0004u;
constexpr DWORD ES_AUTOVSCROLL = 0x0040u;
constexpr DWORD ES_AUTOHSCROLL = 0x0080u;
constexpr DWORD ES_READONLY = 0x0800u;
constexpr DWORD BS_PUSHBUTTON = 0x0000u;
constexpr DWORD BS_AUTOCHECKBOX = 0x0003u;
constexpr DWORD BS_GROUPBOX = 0x0007u;
constexpr DWORD BS_AUTORADIOBUTTON = 0x0009u;
constexpr DWORD BS_PUSHLIKE = 0x1000u;
constexpr DWORD SS_NOTIFY = 0x0100u;

extern "C" {
void _wsplitpath(const wchar_t* path, wchar_t* drive, wchar_t* directory, wchar_t* filename, wchar_t* extension);
DWORD GetModuleFileName(HMODULE module, LPWSTR filename, DWORD size);
BOOL PathRelativePathTo(LPWSTR output, LPCWSTR from, DWORD fromAttributes, LPCWSTR to, DWORD toAttributes);
BOOL PathCanonicalize(LPWSTR output, LPCWSTR input);
HMODULE GetModuleHandle(LPCWSTR moduleName);
DWORD SetFilePointer(HANDLE file, LONG distance, LONG* distanceHigh, DWORD method);
HANDLE FindFirstFile(LPCWSTR pattern, WIN32_FIND_DATA* data);
BOOL FindNextFile(HANDLE find, WIN32_FIND_DATA* data);
BOOL FindClose(HANDLE find);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR wide, int wideCount,
  LPSTR multi, int multiCount, LPCSTR defaultChar, BOOL* usedDefaultChar);
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR multi, int multiCount,
  LPWSTR wide, int wideCount);
DWORD GetCurrentThreadId();
void GetLocalTime(SYSTEMTIME* time);
void Sleep(DWORD milliseconds);
void InitializeCriticalSection(CRITICAL_SECTION* section);
void DeleteCriticalSection(CRITICAL_SECTION* section);
void EnterCriticalSection(CRITICAL_SECTION* section);
void LeaveCriticalSection(CRITICAL_SECTION* section);
}

inline LONG InterlockedIncrement(LONG* value) { return ++(*value); }
inline LONG InterlockedDecrement(LONG* value) { return --(*value); }
inline long InterlockedIncrement(long* value) { return ++(*value); }
inline long InterlockedDecrement(long* value) { return --(*value); }

inline int CompareString(DWORD, DWORD flags, LPCWSTR left, int leftCount, LPCWSTR right, int rightCount)
{
  const auto length = [](LPCWSTR text, int count) -> std::size_t {
    return count < 0 ? std::wcslen(text) : static_cast<std::size_t>(count);
  };
  const std::size_t leftLength = length(left, leftCount);
  const std::size_t rightLength = length(right, rightCount);
  const std::size_t common = leftLength < rightLength ? leftLength : rightLength;
  for (std::size_t i = 0; i < common; ++i) {
    wchar_t a = left[i];
    wchar_t b = right[i];
    if (flags & NORM_IGNORECASE) {
      a = std::towlower(a);
      b = std::towlower(b);
    }
    if (a < b) return CSTR_LESS_THAN;
    if (a > b) return CSTR_GREATER_THAN;
  }
  if (leftLength < rightLength) return CSTR_LESS_THAN;
  if (leftLength > rightLength) return CSTR_GREATER_THAN;
  return CSTR_EQUAL;
}

inline DWORD timeGetTime()
{
  using namespace std::chrono;
  return static_cast<DWORD>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

inline BOOL IsDBCSLeadByte(BYTE value)
{
  return ((value >= 0x81 && value <= 0x9f) || (value >= 0xe0 && value <= 0xfc)) ? TRUE : FALSE;
}

inline BOOL IsDBCSLeadByteEx(UINT, BYTE value)
{
  return IsDBCSLeadByte(value);
}

inline BOOL SetRect(RECT* rect, int left, int top, int right, int bottom)
{
  if (rect == NULL) return FALSE;
  rect->left = left;
  rect->top = top;
  rect->right = right;
  rect->bottom = bottom;
  return TRUE;
}

#endif
