#include "Win32Compat.hpp"
#include "Encoding.hpp"
#include "Path.hpp"
#include <emscripten.h>
#include <chrono>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

namespace {
std::string Utf8(const wchar_t* source) {
    std::wstring path = source ? source : L"";
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return gstd::portable::ResolveFilePath(gstd::portable::EncodeText(path, CP_UTF8));
}
std::wstring Wide(const std::string& source) { return gstd::portable::DecodeText(source, CP_UTF8); }
void Copy(wchar_t* destination, const std::wstring& source, std::size_t capacity = MAX_PATH) {
    if (!destination || !capacity) return;
    if (source.size() >= capacity) throw std::runtime_error("Path exceeds the Woo filename limit");
    std::copy(source.begin(), source.end(), destination); destination[source.size()] = 0;
}
struct DirectorySearch { std::vector<std::filesystem::directory_entry> entries; std::size_t index=0; };
void Fill(WIN32_FIND_DATA& data, const std::filesystem::directory_entry& entry) {
    data = {};
    Copy(data.cFileName, Wide(entry.path().filename().generic_u8string()));
    std::error_code error;
    if (entry.is_directory(error)) data.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    else {
        const auto size=entry.file_size(error);
        if(!error){data.nFileSizeLow=static_cast<DWORD>(size);data.nFileSizeHigh=static_cast<DWORD>(size>>32);}
    }
}
}

extern "C" {
// This runtime deliberately has one cooperative execution thread and no
// pthread workers. The original owner uses this identity for main-thread tests.
DWORD GetCurrentThreadId(){return 1;}
BOOL QueryPerformanceCounter(LARGE_INTEGER* value){if(!value)return FALSE;value->QuadPart=static_cast<std::int64_t>(emscripten_get_now()*1000);return TRUE;}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* value){if(!value)return FALSE;value->QuadPart=1000000;return TRUE;}
void Sleep(DWORD milliseconds){if(milliseconds)throw std::runtime_error("Blocking Sleep reached the Web main thread; use the cooperative owner");}
void GetLocalTime(SYSTEMTIME* output){
    if(!output)return;
    auto now=std::chrono::system_clock::now();auto seconds=std::chrono::system_clock::to_time_t(now);std::tm local{};localtime_r(&seconds,&local);
    *output={WORD(local.tm_year+1900),WORD(local.tm_mon+1),WORD(local.tm_wday),WORD(local.tm_mday),WORD(local.tm_hour),WORD(local.tm_min),WORD(local.tm_sec),WORD(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000)};
}
void OutputDebugString(LPCWSTR message){std::fputs(gstd::portable::EncodeText(message?message:L"",CP_UTF8).c_str(),stderr);}
void _wsplitpath(LPCWSTR source,LPWSTR drive,LPWSTR directory,LPWSTR filename,LPWSTR extension){
    std::wstring path=source?source:L"";
    std::replace(path.begin(),path.end(),L'\\',L'/');
    std::wstring disk;
    if(path.size()>=2&&path[1]==L':'){disk=path.substr(0,2);path.erase(0,2);}
    auto slash=path.find_last_of(L'/');
    std::wstring folder=slash==std::wstring::npos?L"":path.substr(0,slash+1);
    std::wstring base=slash==std::wstring::npos?path:path.substr(slash+1);
    auto dot=base.find_last_of(L'.');
    std::wstring suffix=dot==std::wstring::npos?L"":base.substr(dot);
    if(dot!=std::wstring::npos)base.resize(dot);
    Copy(drive,disk);Copy(directory,folder);Copy(filename,base);Copy(extension,suffix);
}
BOOL PathCanonicalize(LPWSTR destination,LPCWSTR source){
    if(!destination||!source)return FALSE;
    auto normalized=gstd::portable::NormalizeFilePath(gstd::portable::EncodeText(source,CP_UTF8));
    Copy(destination,Wide(normalized));return TRUE;
}
BOOL PathRelativePathTo(LPWSTR destination,LPCWSTR from,DWORD,LPCWSTR to,DWORD){
    if(!destination||!from||!to)return FALSE;
    auto relative=std::filesystem::u8path(Utf8(to)).lexically_relative(std::filesystem::u8path(Utf8(from))).generic_u8string();
    if(relative.empty())return FALSE;
    if(relative[0]!='.')relative="./"+relative;
    if(relative.back()!='/')relative+='/';
    Copy(destination,Wide(relative));return TRUE;
}
DWORD GetModuleFileName(HMODULE,LPWSTR destination,DWORD capacity){
    // Explicit virtual identity of the original executor; no fabricated .exe is
    // loaded. Original config/save path calculations remain relative to /game.
    const std::wstring path=L"/game/th_dnh.exe";
    if(capacity<=path.size())return 0;
    Copy(destination,path,capacity);return path.size();
}
DWORD GetLongPathName(LPCWSTR source,LPWSTR destination,DWORD capacity){
    if(!source||!destination)return 0;
    std::filesystem::path requested=std::filesystem::u8path(Utf8(source));
    std::error_code error;
    if(!std::filesystem::exists(requested,error))return 0;
    auto path=Wide(requested.lexically_normal().generic_u8string());
    if(path.size()+1>capacity)return path.size()+1;
    Copy(destination,path,capacity);return path.size();
}
BOOL SetCurrentDirectory(LPCWSTR directory){return directory&&chdir(Utf8(directory).c_str())==0;}
BOOL PathFileExists(LPCWSTR path){struct stat info;return path&&stat(Utf8(path).c_str(),&info)==0;}
HANDLE FindFirstFile(LPCWSTR pattern,WIN32_FIND_DATA* data){
    if(!pattern||!data)return INVALID_HANDLE_VALUE;
    auto path=std::filesystem::u8path(Utf8(pattern));std::string name=path.filename().generic_u8string();
    auto search=std::make_unique<DirectorySearch>();std::error_code error;
    if(name=="*"||name=="*.*"){
        for(std::filesystem::directory_iterator it(path.parent_path(),error),end;!error&&it!=end;it.increment(error))search->entries.push_back(*it);
        std::sort(search->entries.begin(),search->entries.end(),[](const auto& a,const auto& b){return a.path()<b.path();});
    }else if(std::filesystem::exists(path,error))search->entries.emplace_back(path,error);
    if(error||search->entries.empty())return INVALID_HANDLE_VALUE;
    Fill(*data,search->entries.front());return search.release();
}
BOOL FindNextFile(HANDLE handle,WIN32_FIND_DATA* data){
    if(!handle||handle==INVALID_HANDLE_VALUE||!data)return FALSE;
    auto& search=*static_cast<DirectorySearch*>(handle);if(++search.index>=search.entries.size())return FALSE;Fill(*data,search.entries[search.index]);return TRUE;
}
BOOL FindClose(HANDLE handle){if(!handle||handle==INVALID_HANDLE_VALUE)return FALSE;delete static_cast<DirectorySearch*>(handle);return TRUE;}
BOOL CreateDirectory(LPCWSTR path,void*){std::error_code error;bool created=std::filesystem::create_directory(std::filesystem::u8path(Utf8(path)),error);return created&&!error;}
BOOL DeleteFile(LPCWSTR path){return path&&std::remove(Utf8(path).c_str())==0;}
DWORD GetFileSize(HANDLE handle,DWORD* high){if(high)*high=0;if(!handle)return INVALID_FILE_SIZE;struct stat info;if(fstat(fileno(static_cast<FILE*>(handle)),&info))return INVALID_FILE_SIZE;if(high)*high=static_cast<DWORD>(static_cast<std::uint64_t>(info.st_size)>>32);return static_cast<DWORD>(info.st_size);}
DWORD SetFilePointer(HANDLE handle,LONG offset,LONG*,DWORD method){if(!handle||method>FILE_END)return INVALID_SET_FILE_POINTER;FILE* file=static_cast<FILE*>(handle);if(std::fseek(file,offset,method==FILE_BEGIN?SEEK_SET:method==FILE_CURRENT?SEEK_CUR:SEEK_END))return INVALID_SET_FILE_POINTER;return static_cast<DWORD>(std::ftell(file));}
DWORD GetLastError(){return static_cast<DWORD>(errno);}
int MessageBox(HWND,LPCWSTR text,LPCWSTR title,UINT type){
    if((type&15)!=MB_OK)throw std::runtime_error("Interactive native MessageBox choices have no browser adapter");
    std::string message=gstd::portable::EncodeText(std::wstring(title?title:L"Woo")+L": "+(text?text:L""),CP_UTF8);
    std::fprintf(stderr,"%s\n",message.c_str());
    EM_ASM({const message=UTF8ToString($0);const d=globalThis.ph3Diagnostics;if(d){d.messages.push(message);if($1){d.errors.push(message);d.phase='error';}}const status=document.getElementById('status');if(status)status.textContent=message;},message.c_str(),(type&MB_ICONERROR)!=0);
    return 1;
}
}
