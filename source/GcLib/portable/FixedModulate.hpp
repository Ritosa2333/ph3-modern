#pragma once
#include "DxCompat.hpp"
#include <array>
#include <string>

namespace directx::portable {
// Four bounded specializations of the existing fixed-function fragment
// program. No authored effect or uncommon fixed-function state uses them.
enum class ModulateProgram : unsigned {
    Generic=0, Product=1, ProductAlphaTest=2, TextureAlpha=3, TextureAlphaTest=4
};
inline ModulateProgram SelectModulateProgram(const std::array<DWORD,6>& stage,
    DWORD alphaTest,DWORD alphaFunction,DWORD alphaReference,DWORD fog) {
    if(fog || stage[0]!=D3DTOP_MODULATE || stage[1]!=D3DTA_TEXTURE ||
       stage[2]!=D3DTA_DIFFUSE || stage[4]!=D3DTA_TEXTURE)
        return ModulateProgram::Generic;
    const bool product=stage[3]==D3DTOP_MODULATE&&stage[5]==D3DTA_DIFFUSE;
    const bool texture=stage[3]==D3DTOP_SELECTARG1;
    if(!product&&!texture)return ModulateProgram::Generic;
    if(alphaTest&&(alphaFunction!=D3DCMP_GREATER||alphaReference!=0))
        return ModulateProgram::Generic;
    return static_cast<ModulateProgram>((product?1u:3u)+(alphaTest?1u:0u));
}
inline std::string ModulateFragmentSource(ModulateProgram mode) {
    std::string source="#version 300 es\n";
    if(mode==ModulateProgram::TextureAlpha||mode==ModulateProgram::TextureAlphaTest)
        source+="#define WOO_TEXTURE_ALPHA\n";
    if(mode==ModulateProgram::ProductAlphaTest||mode==ModulateProgram::TextureAlphaTest)
        source+="#define WOO_POSITIVE_ALPHA\n";
    source+=R"GLSL(precision highp float;
in highp vec2 texcoord;
in highp vec4 tint;
uniform sampler2D wooTexture;
out vec4 outputColor;
void main(){
 vec4 image=texture(wooTexture,texcoord);
 vec4 color=image*tint;
#ifdef WOO_TEXTURE_ALPHA
 color.a=image.a;
#endif
 color=clamp(color,0.0,1.0);
#ifdef WOO_POSITIVE_ALPHA
 if(!(color.a>0.0))discard;
#endif
 outputColor=color;
}
)GLSL";
    return source;
}
}
