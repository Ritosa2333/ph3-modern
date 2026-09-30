#include "../../source/GcLib/gstd/SmartPointer.hpp"
#include "../../source/GcLib/portable/Utf16.hpp"
#include "../../source/GcLib/portable/Encoding.hpp"
#include "../../source/GcLib/portable/Path.hpp"
#include <cassert>
#include <clocale>
#include <fstream>

struct Base { virtual ~Base() = default; };
struct Tracked final : Base {
    inline static int alive = 0;
    std::vector<int> payload = std::vector<int>(256, 42);
    Tracked() { ++alive; }
    ~Tracked() override { --alive; }
};
template<bool Sync> void ownership() {
    gstd::ref_count_weak_ptr<Tracked, Sync> weak;
    {
        gstd::ref_count_ptr<Tracked, Sync> owner = new Tracked();
        assert(Tracked::alive == 1);
        weak = owner;
        {
            gstd::ref_count_ptr<Base, Sync> base = owner;
            auto recovered = gstd::ref_count_ptr<Tracked, Sync>::DownCast(base);
            assert(recovered.GetPointer() == owner.GetPointer());
            assert(recovered->payload.back() == 42);
        }
        assert(owner.GetReferenceCount() == 1);
    }
    assert(Tracked::alive == 0);
    assert(!weak.IsExists());
}
int main() {
    assert(std::setlocale(LC_CTYPE, "C.UTF-8"));
    ownership<true>(); ownership<false>();
    std::wstring text = L"日本語 / Alpha";
    auto bytes = gstd::portable::EncodeUtf16(text);
    assert(gstd::portable::DecodeUtf16Units(bytes.data(), bytes.size()) == text);
    assert(gstd::portable::DecodeText(gstd::portable::EncodeText(text, 932), 932) == text);
    wchar_t output[128]{};
    auto format = gstd::portable::WidePrintfFormat(L"name=%s value=%d");
    assert(std::swprintf(output, 128, format.c_str(), text.c_str(), 7) > 0);
    assert(std::wstring(output) == L"name=日本語 / Alpha value=7");
    auto scoreFormat = gstd::portable::WidePrintfFormat(L"%012I64d");
    assert(scoreFormat == L"%012lld");
    wchar_t score[128]{};
    assert(std::swprintf(score, 128, scoreFormat.c_str(), static_cast<long long>(147368070)) > 0);
    assert(std::wstring(score) == L"000147368070");
    std::filesystem::create_directories("/game/lib");
    std::ofstream("/game/lib/lib_Result.dnh") << "fixture";
    assert(gstd::portable::ResolveFilePath("/GAME/LIB/lib_result.dnh") == "/game/lib/lib_Result.dnh");
    assert(gstd::portable::ResolveFilePath("/GAME/LIB/new-save.dat") == "/game/lib/new-save.dat");
    assert(gstd::portable::NormalizeFilePath("/GAME/LIB/../LIB/lib_result.dnh") == "/GAME/LIB/lib_result.dnh");
    assert(gstd::portable::NormalizeFilePath("/missing/archive/../img/foo.png") == "/missing/img/foo.png");
    assert(gstd::portable::NormalizeFilePath("/game\\script\\../img/") == "/game/img/");
    std::puts("PASS: scalar ownership, weak references, UTF-16, CP932, Japanese formatting, Windows path casing");
}
