#include "../../source/GcLib/gstd/SmartPointer.hpp"
#include "../../source/GcLib/portable/Utf16.hpp"
#include "../../source/GcLib/portable/Encoding.hpp"
#include "../../source/GcLib/portable/Path.hpp"
#include "../../source/GcLib/portable/TriangleTopology.hpp"
#include "../../source/GcLib/portable/StateTable.hpp"
#include "../../source/GcLib/portable/VertexStream.hpp"
#include "../../source/GcLib/gstd/BudgetLru.hpp"
#include "screen-vertex-regression.hpp"
#include "fixed-modulate-regression.hpp"
#include "transform-cache-regression.hpp"
#include "render-node-pool-regression.hpp"
#include "prewarm-cache-regression.hpp"
#include "effect-uniform-regression.hpp"
#include <cassert>
#include <clocale>
#include <fstream>
#include <stdexcept>

struct Base { virtual ~Base() = default; };
struct UnwindGuard {
    int& count;
    ~UnwindGuard() { ++count; }
};
__attribute__((noinline)) void throwAcrossCall(int& destroyed) {
    UnwindGuard guard{destroyed};
    throw std::runtime_error("indirect exception");
}
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
    TestScreenVertices();
    TestFixedModulatePolicy();
    TestDrawTransforms();
    TestRenderNodePool();
    TestPrewarmCacheScope();
    TestEffectUniformValues();
    gstd::BudgetLru<int> lru;
    std::vector<int> removed;
    auto remove=[&](int key){removed.push_back(key);};
    lru.Remember(1,10);lru.Remember(2,10);lru.Remember(3,10);lru.Touch(1);
    assert(lru.Trim(20,10,[](int){return true;},remove)==1&&removed.back()==2);
    assert(lru.Bytes()==20&&lru.Size()==2);
    lru.Reweight(3,50);
    assert(lru.Trim(10,10,[](int key){return key!=3;},remove)==1&&removed.back()==1);
    assert(lru.Bytes()==50&&lru.Size()==1); // active working set is protected
    lru.Forget(3);assert(lru.Bytes()==0&&lru.Size()==0); // explicit pin, no deletion
    for(int i=0;i<100;++i)lru.Remember(i,0);
    assert(lru.Trim(0,8,[](int){return true;},remove)==92&&lru.Size()==8);
    lru.Clear();assert(lru.Size()==0&&lru.Bytes()==0);
    directx::portable::VertexStream<12> stream;
    auto slot=stream.Reserve(3);assert(slot.first==0&&slot.orphan&&!slot.direct);
    slot=stream.Reserve(6);assert(slot.first==3&&!slot.orphan&&!slot.direct);
    slot=stream.Reserve(3);assert(slot.first==9&&!slot.orphan&&!slot.direct);
    slot=stream.Reserve(1);assert(slot.first==0&&slot.orphan&&!slot.direct);
    slot=stream.Reserve(13);assert(slot.first==0&&!slot.orphan&&slot.direct);
    slot=stream.Reserve(3);assert(slot.first==0&&slot.orphan&&!slot.direct);
    stream.BeginFrame();slot=stream.Reserve(3);assert(slot.first==0&&slot.orphan&&!slot.direct);
    directx::portable::StateTable<64> states{{1,9},{63,17},{64,21},{0xffffffffu,33}};
    assert(states.Get(0)==0&&states.Get(1)==9&&states.Get(63)==17);
    assert(states.Get(64)==21&&states.Get(1000)==0&&states.Get(0xffffffffu)==33);
    states[1]=0;states[63]=100;states[1000]=5;
    assert(states.Get(1)==0&&states.Get(63)==100&&states.Get(1000)==5);
    states={{2,7}};
    assert(states.Get(1)==0&&states.Get(2)==7&&states.Get(63)==0&&states.Get(1000)==0);
    int destroyed=0;
    void (*volatile throwingCall)(int&)=throwAcrossCall;
    try {
        try { throwingCall(destroyed); assert(false); }
        catch(const std::runtime_error& e) {
            assert(std::string(e.what())=="indirect exception");
            assert(destroyed==1);
            throw;
        }
    } catch(const std::exception& e) {
        assert(std::string(e.what())=="indirect exception");
    }
    assert(destroyed==1);
    // Two independent strips must not produce bridging triangles; odd strip
    // primitives reverse their first two indices to retain front-face winding.
    std::vector<int> triangles;
    directx::portable::ExpandTriangles(std::vector<int>{0,1,2,3,4},triangles,3,true);
    assert((triangles==std::vector<int>{0,1,2,2,1,3,2,3,4}));
    directx::portable::ExpandTriangles(std::vector<int>{5,6,7,8},triangles,2,true);
    assert((triangles==std::vector<int>{5,6,7,7,6,8}));
    directx::portable::ExpandTriangles(std::vector<int>{0,1,2,3,4},triangles,3,false);
    assert((triangles==std::vector<int>{0,1,2,0,2,3,0,3,4}));
    directx::portable::ExpandTriangles(std::vector<int>{},triangles,0,true);
    assert(triangles.empty());
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
