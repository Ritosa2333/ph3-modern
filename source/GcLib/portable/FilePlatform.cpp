// Included at the File owner in gstd/File.cpp. All archive, buffer and manager
// implementations remain the original Woo code, independent of this OS layer.
namespace {
std::string FilePathUtf8(std::wstring path) {
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return gstd::portable::ResolveFilePath(gstd::StringUtility::ConvertWideToMulti(path, CP_UTF8));
}
std::vector<std::wstring> DirectoryContents(const std::wstring& path, bool directories) {
    std::vector<std::wstring> result;
    std::error_code error;
    auto root = std::filesystem::u8path(FilePathUtf8(path));
    for (std::filesystem::directory_iterator iterator(root, error), end; !error && iterator != end; iterator.increment(error)) {
        const auto& entry = *iterator;
        if (entry.is_directory(error) != directories || error) continue;
        std::wstring name = gstd::StringUtility::ConvertMultiToWide(entry.path().generic_u8string(), CP_UTF8);
        if (directories) name += L'/';
        result.push_back(std::move(name));
    }
    std::sort(result.begin(), result.end());
    return result;
}
}

File::File() : hFile_(NULL) {}
File::File(std::wstring path) : hFile_(NULL), path_(std::move(path)) {}
File::~File() { Close(); }
bool File::CreateDirectory() {
    const auto directory = std::filesystem::u8path(FilePathUtf8(PathProperty::GetFileDirectory(path_)));
    if (directory.empty()) return true;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    return !error && std::filesystem::is_directory(directory, error);
}
void File::Delete() { Close(); std::remove(FilePathUtf8(path_).c_str()); }
bool File::IsExists() { return hFile_ != NULL || IsExists(path_); }
bool File::IsExists(std::wstring path) {
    struct stat info;
    return stat(FilePathUtf8(path).c_str(), &info) == 0;
}
bool File::IsDirectory() {
    struct stat info;
    return stat(FilePathUtf8(path_).c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}
int File::GetSize() {
    struct stat info;
    int status = hFile_ ? fstat(fileno(static_cast<FILE*>(hFile_)), &info) : stat(FilePathUtf8(path_).c_str(), &info);
    if (status != 0) return 0;
    if (info.st_size > std::numeric_limits<int>::max()) throw wexception(L"File exceeds Woo's 32-bit size limit");
    return static_cast<int>(info.st_size);
}
bool File::Open() { return Open(READ); }
bool File::Open(AccessType access) {
    Close();
    hFile_ = std::fopen(FilePathUtf8(path_).c_str(), access == READ ? "rb" : "r+b");
    return hFile_ != NULL;
}
bool File::Create() {
    Close(); hFile_ = std::fopen(FilePathUtf8(path_).c_str(), "w+b");
    return hFile_ != NULL;
}
void File::Close() {
    if (hFile_) std::fclose(static_cast<FILE*>(hFile_));
    hFile_ = NULL;
}
DWORD File::Read(LPVOID buffer, DWORD size) {
    return hFile_ ? static_cast<DWORD>(std::fread(buffer, 1, size, static_cast<FILE*>(hFile_))) : 0;
}
DWORD File::Write(LPVOID buffer, DWORD size) {
    return hFile_ ? static_cast<DWORD>(std::fwrite(buffer, 1, size, static_cast<FILE*>(hFile_))) : 0;
}
BOOL File::Seek(LONG offset, DWORD method) {
    if (!hFile_ || method > FILE_END) return FALSE;
    return std::fseek(static_cast<FILE*>(hFile_), offset,
        method == FILE_BEGIN ? SEEK_SET : method == FILE_CURRENT ? SEEK_CUR : SEEK_END) == 0;
}
LONG File::GetFilePointer() {
    if (!hFile_) return -1;
    return static_cast<LONG>(std::ftell(static_cast<FILE*>(hFile_)));
}
bool File::IsEqualsPath(std::wstring first, std::wstring second) {
    return PathProperty::GetUnique(first) == PathProperty::GetUnique(second);
}
std::vector<std::wstring> File::GetFilePathList(std::wstring path) { return DirectoryContents(path, false); }
std::vector<std::wstring> File::GetDirectoryPathList(std::wstring path) { return DirectoryContents(path, true); }
