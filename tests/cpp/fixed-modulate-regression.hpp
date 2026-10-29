#pragma once
#include "../../source/GcLib/portable/FixedModulate.hpp"
#include <cassert>

inline void TestFixedModulatePolicy() {
    using namespace directx::portable;
    std::array<DWORD,6> state{D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,
        D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE};
    const auto choose=[&](DWORD test=0,DWORD function=D3DCMP_GREATER,DWORD reference=0,DWORD fog=0){
        return SelectModulateProgram(state,test,function,reference,fog);
    };
    assert(choose()==ModulateProgram::Product);
    assert(choose(1)==ModulateProgram::ProductAlphaTest);
    assert(choose(9)==ModulateProgram::ProductAlphaTest);
    for(DWORD fog:{1u,5u})assert(choose(0,D3DCMP_GREATER,0,fog)==ModulateProgram::Generic);
    for(DWORD reference:{1u,127u,255u,256u})assert(choose(1,D3DCMP_GREATER,reference)==ModulateProgram::Generic);
    for(DWORD function:{D3DCMP_NEVER,D3DCMP_LESS,D3DCMP_EQUAL,D3DCMP_LESSEQUAL,
        D3DCMP_NOTEQUAL,D3DCMP_GREATEREQUAL,D3DCMP_ALWAYS}){
        assert(choose(1,function)==ModulateProgram::Generic);
        assert(choose(0,function,127)==ModulateProgram::Product);
    }
    for(unsigned i=0;i<state.size();++i){
        const DWORD original=state[i];state[i]=0xffffffffu;
        assert(choose()==ModulateProgram::Generic);state[i]=original;
    }
    for(unsigned i:{1u,2u,4u,5u}){
        const DWORD original=state[i];
        for(DWORD modifier:{D3DTA_COMPLEMENT,D3DTA_ALPHAREPLICATE}){
            state[i]=original|modifier;assert(choose()==ModulateProgram::Generic);
        }
        state[i]=original;
    }
    state[3]=D3DTOP_SELECTARG1;
    assert(choose()==ModulateProgram::TextureAlpha);
    assert(choose(1)==ModulateProgram::TextureAlphaTest);
    state[5]=0xffffffffu; // SELECTARG1 never reads its second argument.
    assert(choose()==ModulateProgram::TextureAlpha);
    for(unsigned i=1;i<5;++i){
        const auto shader=ModulateFragmentSource(static_cast<ModulateProgram>(i));
        assert(shader.find("#version 300 es\n")==0);
        assert((shader.find("#define WOO_TEXTURE_ALPHA")!=std::string::npos)==(i>=3));
        assert((shader.find("#define WOO_POSITIVE_ALPHA")!=std::string::npos)==(i==2||i==4));
    }
}
