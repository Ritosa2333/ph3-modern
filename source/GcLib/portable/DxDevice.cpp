#include "DxCompat.hpp"
#include "TriangleTopology.hpp"
#include "StateTable.hpp"
#include "ScreenVertex.hpp"
#include "FixedModulate.hpp"
#include "DrawTransforms.hpp"
#include "../gstd/GstdUtility.hpp"
#include <SDL3/SDL.h>
#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <png.h>
#include <array>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace directx::portable {
namespace {
constexpr HRESULT InvalidCall=static_cast<HRESULT>(0x8876086cu);
constexpr unsigned MaxSlots=16;
constexpr unsigned MaxElements=32;
template<class T> void AssignReference(T*& destination,T* source) {
    if(source)source->AddRef();
    if(destination)destination->Release();
    destination=source;
}

void Check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}

GLuint CompileShader(GLenum type,const char* source) {
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){GLint length=0;glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&length);std::string log(std::max(1,length),'\0');glGetShaderInfoLog(shader,length,nullptr,log.data());glDeleteShader(shader);throw std::runtime_error("Woo GLES shader: "+log);}
    return shader;
}

GLuint LinkProgram(const char* vertex,const char* fragment) {
    GLuint vs=CompileShader(GL_VERTEX_SHADER,vertex),fs=0,program=0;
    try{fs=CompileShader(GL_FRAGMENT_SHADER,fragment);program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
        GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){GLint length=0;glGetProgramiv(program,GL_INFO_LOG_LENGTH,&length);std::string log(std::max(1,length),'\0');glGetProgramInfoLog(program,length,nullptr,log.data());throw std::runtime_error("Woo GLES program: "+log);}
    }catch(...){if(program)glDeleteProgram(program);if(fs)glDeleteShader(fs);glDeleteShader(vs);throw;}
    glDeleteShader(vs);glDeleteShader(fs);return program;
}

constexpr const char* VertexSource=R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec4 wooPosition;
layout(location=1) in vec2 wooTexcoord;
layout(location=2) in vec4 wooTint;
layout(location=3) in vec2 wooPixel;
layout(location=4) in float wooFog;
out highp vec2 texcoord;
out highp vec4 tint;
out highp vec2 pixelPosition;
out highp float fogFactor;
void main(){gl_Position=wooPosition;texcoord=wooTexcoord;tint=wooTint;pixelPosition=wooPixel;fogFactor=wooFog;gl_PointSize=1.0;}
)GLSL";

constexpr const char* FragmentSource=R"GLSL(#version 300 es
precision highp float;
in highp vec2 texcoord;
in highp vec4 tint;
in highp float fogFactor;
uniform sampler2D wooTexture;
uniform ivec3 wooColorStage;
uniform ivec3 wooAlphaStage;
uniform vec4 wooTextureFactor;
uniform bool wooAlphaTest;
uniform int wooAlphaFunc;
uniform float wooAlphaRef;
uniform vec4 wooFogColor;
uniform bool wooFogEnable;
out vec4 outputColor;
vec4 argument(int code,vec4 sampleColor){
 int base=code&15;vec4 value=tint;
 if(base==2)value=sampleColor;else if(base==3)value=wooTextureFactor;
 if((code&32)!=0)value=vec4(value.a);if((code&16)!=0)value=vec4(1.0)-value;return value;
}
vec4 combine(int op,vec4 a,vec4 b){
 if(op==2)return a;if(op==3)return b;if(op==4)return a*b;
 if(op==5)return 2.0*a*b;if(op==6)return 4.0*a*b;
 if(op==7)return a+b;if(op==10)return a-b;return a*b;
}
bool alphaPass(float a){
 if(wooAlphaFunc==1)return false;if(wooAlphaFunc==2)return a<wooAlphaRef;
 if(wooAlphaFunc==3)return a==wooAlphaRef;if(wooAlphaFunc==4)return a<=wooAlphaRef;
 if(wooAlphaFunc==5)return a>wooAlphaRef;if(wooAlphaFunc==6)return a!=wooAlphaRef;
 if(wooAlphaFunc==7)return a>=wooAlphaRef;return true;
}
void main(){
 vec4 image=texture(wooTexture,texcoord);
 vec4 color=combine(wooColorStage.x,argument(wooColorStage.y,image),argument(wooColorStage.z,image));
 color.a=combine(wooAlphaStage.x,argument(wooAlphaStage.y,image),argument(wooAlphaStage.z,image)).a;
 color=clamp(color,0.0,1.0);if(wooAlphaTest&&!alphaPass(color.a))discard;
 if(wooFogEnable)color.rgb=mix(wooFogColor.rgb,color.rgb,clamp(fogFactor,0.0,1.0));outputColor=color;
}
)GLSL";

struct Vertex {
    D3DXVECTOR4 position;
    D3DXVECTOR2 uv;
    D3DXVECTOR4 tint;
    D3DXVECTOR2 pixel;
    float fog=1;
};

D3DXVECTOR4 Color(DWORD color) {
    return {float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,float((color>>24)&255)/255};
}

DWORD FloatBits(float value){DWORD result;std::memcpy(&result,&value,4);return result;}
float BitsFloat(DWORD value){float result;std::memcpy(&result,&value,4);return result;}

GLenum Compare(DWORD value) {
    switch(value){case D3DCMP_NEVER:return GL_NEVER;case D3DCMP_LESS:return GL_LESS;case D3DCMP_EQUAL:return GL_EQUAL;case D3DCMP_LESSEQUAL:return GL_LEQUAL;case D3DCMP_GREATER:return GL_GREATER;case D3DCMP_NOTEQUAL:return GL_NOTEQUAL;case D3DCMP_GREATEREQUAL:return GL_GEQUAL;default:return GL_ALWAYS;}
}
GLenum Blend(DWORD value) {
    switch(value){case D3DBLEND_ZERO:return GL_ZERO;case D3DBLEND_ONE:return GL_ONE;case D3DBLEND_SRCCOLOR:return GL_SRC_COLOR;case D3DBLEND_INVSRCCOLOR:return GL_ONE_MINUS_SRC_COLOR;case D3DBLEND_SRCALPHA:return GL_SRC_ALPHA;case D3DBLEND_INVSRCALPHA:return GL_ONE_MINUS_SRC_ALPHA;case D3DBLEND_DESTALPHA:return GL_DST_ALPHA;case D3DBLEND_INVDESTALPHA:return GL_ONE_MINUS_DST_ALPHA;case D3DBLEND_DESTCOLOR:return GL_DST_COLOR;case D3DBLEND_INVDESTCOLOR:return GL_ONE_MINUS_DST_COLOR;case D3DBLEND_SRCALPHASAT:return GL_SRC_ALPHA_SATURATE;default:throw std::runtime_error("Unsupported D3D blend factor");}
}
GLenum BlendOperation(DWORD value) {
    switch(value){case D3DBLENDOP_ADD:return GL_FUNC_ADD;case D3DBLENDOP_SUBTRACT:return GL_FUNC_SUBTRACT;case D3DBLENDOP_REVSUBTRACT:return GL_FUNC_REVERSE_SUBTRACT;case D3DBLENDOP_MIN:return GL_MIN;case D3DBLENDOP_MAX:return GL_MAX;default:throw std::runtime_error("Unsupported D3D blend operation");}
}
GLenum Address(DWORD value) {
    switch(value){case D3DTADDRESS_WRAP:return GL_REPEAT;case D3DTADDRESS_MIRROR:return GL_MIRRORED_REPEAT;case D3DTADDRESS_CLAMP:return GL_CLAMP_TO_EDGE;default:throw std::runtime_error("Unsupported D3D texture addressing");}
}
GLenum Primitive(DWORD value) {
    switch(value){case D3DPT_POINTLIST:return GL_POINTS;case D3DPT_LINELIST:return GL_LINES;case D3DPT_LINESTRIP:return GL_LINE_STRIP;case D3DPT_TRIANGLELIST:return GL_TRIANGLES;case D3DPT_TRIANGLESTRIP:return GL_TRIANGLE_STRIP;case D3DPT_TRIANGLEFAN:return GL_TRIANGLE_FAN;default:throw std::runtime_error("Unsupported D3D primitive");}
}
std::size_t VertexCount(DWORD type,UINT count) {
    if(!count)return 0;
    switch(type){case D3DPT_POINTLIST:return count;case D3DPT_LINELIST:return std::size_t(count)*2;case D3DPT_LINESTRIP:return std::size_t(count)+1;case D3DPT_TRIANGLELIST:return std::size_t(count)*3;case D3DPT_TRIANGLESTRIP:case D3DPT_TRIANGLEFAN:return std::size_t(count)+2;default:throw std::runtime_error("Invalid primitive type");}
}

}

struct TextureStorage {
    UINT width=0,height=0,levels=1;
    D3DFORMAT format=D3DFMT_A8R8G8B8;
    D3DPOOL pool=D3DPOOL_DEFAULT;
    DWORD usage=0;
    GLuint texture=0,framebuffer=0,depthBuffer=0;
    bool framebufferConfigured=false;
    // Retain the attachment: a deleted/reused GL name must not make the cached
    // completeness result appear valid for a different depth buffer.
    std::shared_ptr<TextureStorage> framebufferDepth;
    bool back=false,depthOnly=false,locked=false,readOnly=false,cpuDirty=false,cpuFresh=true,gpuDirty=false;
    std::vector<DWORD> pixels;

    ~TextureStorage(){if(framebuffer)glDeleteFramebuffers(1,&framebuffer);if(depthBuffer)glDeleteRenderbuffers(1,&depthBuffer);if(texture)glDeleteTextures(1,&texture);}
    D3DSURFACE_DESC Description() const {return {format,D3DRTYPE_SURFACE,usage,pool,D3DMULTISAMPLE_NONE,0,width,height};}
    void Upload(){
        if(!texture||depthOnly)return;
        // GetGLTexture() is also queried for unchanged textures at every
        // draw. Binding belongs to BindTextureUnit unless data/mips need work.
        if(!cpuDirty&&(!gpuDirty||levels<=1))return;
        glBindTexture(GL_TEXTURE_2D,texture);
        if(cpuDirty){
            std::vector<BYTE> rgba(pixels.size()*4);
            for(std::size_t i=0;i<pixels.size();++i){DWORD value=pixels[i];rgba[i*4]=BYTE(value>>16);rgba[i*4+1]=BYTE(value>>8);rgba[i*4+2]=BYTE(value);rgba[i*4+3]=format==D3DFMT_X8R8G8B8?255:BYTE(value>>24);}
            glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());cpuDirty=false;gpuDirty=true;
        }
        if(gpuDirty&&levels>1){glGenerateMipmap(GL_TEXTURE_2D);gpuDirty=false;}
    }
    void Readback(){
        if(cpuFresh)return;
        Check(!depthOnly,"Cannot lock a depth surface as color");
        GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
        if(!framebuffer)glGenFramebuffers(1,&framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        std::vector<BYTE> rgba(std::size_t(width)*height*4);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        pixels.resize(std::size_t(width)*height);
        for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){const BYTE* p=rgba.data()+(std::size_t(back?height-1-y:y)*width+x)*4;pixels[std::size_t(y)*width+x]=D3DCOLOR_ARGB(p[3],p[0],p[1],p[2]);}
        glBindFramebuffer(GL_FRAMEBUFFER,previous);cpuFresh=true;
    }
    HRESULT Lock(D3DLOCKED_RECT* result,const RECT* rectangle,DWORD flags){
        if(!result||locked||depthOnly)return InvalidCall;
        RECT rect=rectangle?*rectangle:RECT{0,0,LONG(width),LONG(height)};
        if(rect.left<0||rect.top<0||rect.right>LONG(width)||rect.bottom>LONG(height)||rect.left>=rect.right||rect.top>=rect.bottom)return InvalidCall;
        if(!(flags&D3DLOCK_DISCARD))Readback();
        pixels.resize(std::size_t(width)*height);
        result->Pitch=INT(width*4);result->pBits=pixels.data()+std::size_t(rect.top)*width+rect.left;
        locked=true;readOnly=(flags&D3DLOCK_READONLY)!=0;return S_OK;
    }
    HRESULT Unlock(){if(!locked)return InvalidCall;locked=false;if(!readOnly){cpuDirty=true;cpuFresh=true;Upload();}return S_OK;}
};

struct DeviceState {
    SDL_Window* window=nullptr;
    SDL_GLContext context=nullptr;
    bool ownWindow=false,inScene=false;
    GLuint program=0,vao=0,vbo=0,white=0,feedbackTexture=0;
    std::array<GLuint,5> modulatePrograms{};
    UINT modulateSubmissions=0;
    UINT feedbackWidth=0,feedbackHeight=0;
    bool feedbackSnapshotReady=false;
    std::map<std::string,GLint> uniformLocations;
    std::array<GLuint,MaxSlots> glSamplers{};
    struct SamplerCache { bool valid=false; GLint minFilter=0,magFilter=0; GLenum wrapS=0,wrapT=0; };
    std::array<SamplerCache,MaxSlots> samplerCache{};
    std::map<DWORD,D3DXMATRIX> transforms;
    StateTable<256> render;
    std::array<StateTable<64>,MaxSlots> samplers,stages;
    std::array<IDirect3DBaseTexture9*,MaxSlots> textures{};
    std::map<DWORD,D3DLIGHT9> lights;
    std::map<DWORD,bool> lightEnabled;
    D3DMATERIAL9 material;
    D3DVIEWPORT9 viewport{};
    IDirect3DSurface9 *back=nullptr,*backDepth=nullptr,*target=nullptr,*depth=nullptr;
    IDirect3DVertexBuffer9* stream=nullptr;
    IDirect3DIndexBuffer9* indices=nullptr;
    IDirect3DVertexDeclaration9* declaration=nullptr;
    UINT offset=0,stride=0;
    DWORD fvf=0;
    UINT presented=0,draws=0,actualDraws=0,verticesDrawn=0,feedbackResolved=0;
    UINT feedbackFixed=0,feedbackEffect=0;
    UINT feedbackBack=0,feedbackOffscreen=0;
    std::array<UINT,MaxSlots> feedbackByUnit{};
    std::array<UINT,MaxSlots> feedbackBySourceSlot{};
    std::vector<Vertex> converted,wireframe;
    struct FixedBatchKey {
        TextureStorage* target=nullptr;
        TextureStorage* depth=nullptr;
        IDirect3DBaseTexture9* texture=nullptr;
        D3DVIEWPORT9 viewport{};
        std::array<DWORD,20> render{};
        std::array<DWORD,6> stage{};
        std::array<DWORD,5> sampler{};
        GLenum primitive=0;
        bool operator==(const FixedBatchKey& other) const {
            return target==other.target&&depth==other.depth&&texture==other.texture&&primitive==other.primitive&&
                viewport.X==other.viewport.X&&viewport.Y==other.viewport.Y&&viewport.Width==other.viewport.Width&&viewport.Height==other.viewport.Height&&
                viewport.MinZ==other.viewport.MinZ&&viewport.MaxZ==other.viewport.MaxZ&&
                render==other.render&&stage==other.stage&&sampler==other.sampler;
        }
    };
    FixedBatchKey batchKey{};
    std::vector<Vertex> batchVertices;
    bool batchActive=false;
    // Only this backend changes GL raster state. Clear invalidates the cache
    // because it forces write masks. No GL queries are needed in the hot path.
    bool rasterValid=false;
    bool targetBindingValid=false,viewportBindingValid=false,vertexBindingValid=false;
    GLuint boundFramebuffer=0,boundProgram=0;
    D3DVIEWPORT9 boundViewport{};
    GLint boundViewportY=0;
    std::array<DWORD,15> rasterState{};
    bool rasterBack=false;
    bool fixedUniformsValid=false;
    std::array<DWORD,6> fixedStageState{};
    std::array<DWORD,6> fixedRenderState{};

    ~DeviceState(){
        if(context)SDL_GL_MakeCurrent(window,context);
        for(auto*& texture:textures)AssignReference(texture,static_cast<IDirect3DBaseTexture9*>(nullptr));
        AssignReference(stream,static_cast<IDirect3DVertexBuffer9*>(nullptr));AssignReference(indices,static_cast<IDirect3DIndexBuffer9*>(nullptr));AssignReference(declaration,static_cast<IDirect3DVertexDeclaration9*>(nullptr));
        AssignReference(target,static_cast<IDirect3DSurface9*>(nullptr));AssignReference(depth,static_cast<IDirect3DSurface9*>(nullptr));
        if(back)back->Release();if(backDepth)backDepth->Release();
        glDeleteSamplers(MaxSlots,glSamplers.data());if(white)glDeleteTextures(1,&white);if(feedbackTexture)glDeleteTextures(1,&feedbackTexture);if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);if(program)glDeleteProgram(program);
        for(GLuint specialized:modulatePrograms)if(specialized)glDeleteProgram(specialized);
        if(context)SDL_GL_DestroyContext(context);if(ownWindow&&window)SDL_DestroyWindow(window);
    }
    DWORD Render(DWORD type)const{return render.Get(type);}
    GLint Uniform(const char* name){auto found=uniformLocations.find(name);if(found!=uniformLocations.end())return found->second;return uniformLocations.emplace(name,glGetUniformLocation(program,name)).first->second;}
    D3DXMATRIX Transform(DWORD type)const{auto found=transforms.find(type);if(found!=transforms.end())return found->second;D3DXMATRIX identity;D3DXMatrixIdentity(&identity);return identity;}
    FixedBatchKey MakeFixedBatchKey(GLenum primitive) const {
        FixedBatchKey key;key.target=target?target->storage.get():nullptr;key.depth=depth?depth->storage.get():nullptr;key.texture=textures[0];key.viewport=viewport;key.primitive=primitive;
        const DWORD renderKeys[]={D3DRS_ZENABLE,D3DRS_ZFUNC,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,D3DRS_SEPARATEALPHABLENDENABLE,
            D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_SRCBLENDALPHA,D3DRS_DESTBLENDALPHA,D3DRS_BLENDOP,D3DRS_BLENDOPALPHA,D3DRS_CULLMODE,
            D3DRS_FILLMODE,D3DRS_COLORWRITEENABLE,D3DRS_TEXTUREFACTOR,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,D3DRS_FOGENABLE,D3DRS_FOGCOLOR};
        for(std::size_t i=0;i<key.render.size();++i)key.render[i]=Render(renderKeys[i]);
        const DWORD stageKeys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2};
        for(std::size_t i=0;i<key.stage.size();++i)key.stage[i]=stages[0].Get(stageKeys[i]);
        const DWORD samplerKeys[]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV};
        for(std::size_t i=0;i<key.sampler.size();++i)key.sampler[i]=samplers[0].Get(samplerKeys[i]);
        return key;
    }
    void ApplyRasterState(){
#ifdef PH3_LEGACY_GL_STATE
        rasterValid=false;
#endif
        const DWORD keys[]={D3DRS_ZENABLE,D3DRS_ZFUNC,D3DRS_ZWRITEENABLE,D3DRS_ALPHABLENDENABLE,
            D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_SRCBLENDALPHA,
            D3DRS_DESTBLENDALPHA,D3DRS_BLENDOP,D3DRS_BLENDOPALPHA,D3DRS_CULLMODE,
            D3DRS_COLORWRITEENABLE,D3DRS_FILLMODE,D3DRS_SHADEMODE};
        std::array<DWORD,15> next{};
        for(std::size_t i=0;i<next.size();++i)next[i]=Render(keys[i]);
        const auto changed=[&](std::size_t i){return !rasterValid||next[i]!=rasterState[i];};
        if(changed(0))(next[0]?glEnable:glDisable)(GL_DEPTH_TEST);
        if(changed(1))glDepthFunc(Compare(next[1]));
        if(changed(2))glDepthMask(next[2]?GL_TRUE:GL_FALSE);
        if(changed(3))(next[3]?glEnable:glDisable)(GL_BLEND);
        if(changed(4)||changed(5)||changed(6)||changed(7)||changed(8)){
            if(next[4])glBlendFuncSeparate(Blend(next[5]),Blend(next[6]),Blend(next[7]),Blend(next[8]));
            else glBlendFunc(Blend(next[5]),Blend(next[6]));
        }
        if(changed(4)||changed(9)||changed(10)){
            if(next[4])glBlendEquationSeparate(BlendOperation(next[9]),BlendOperation(next[10]));
            else glBlendEquation(BlendOperation(next[9]));
        }
        const bool back=target->storage->back;
        if(changed(11)||!rasterValid||back!=rasterBack){
            if(next[11]==D3DCULL_NONE)glDisable(GL_CULL_FACE);
            else{glEnable(GL_CULL_FACE);glFrontFace(back?GL_CCW:GL_CW);glCullFace(next[11]==D3DCULL_CCW?GL_BACK:GL_FRONT);}
        }
        if(changed(12)){const DWORD mask=next[12];glColorMask(mask&1,mask&2,mask&4,mask&8);}
        if(!rasterValid)glDisable(GL_SCISSOR_TEST);
        rasterState=next;rasterBack=back;rasterValid=true;
    }
    void ApplyFixedUniforms(){
#ifdef PH3_LEGACY_GL_STATE
        fixedUniformsValid=false;
#endif
        const DWORD stageKeys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,
            D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2};
        const DWORD renderKeys[]={D3DRS_TEXTUREFACTOR,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,
            D3DRS_ALPHAREF,D3DRS_FOGENABLE,D3DRS_FOGCOLOR};
        std::array<DWORD,6> nextStage{},nextRender{};
        for(std::size_t i=0;i<6;++i){nextStage[i]=stages[0].Get(stageKeys[i]);nextRender[i]=Render(renderKeys[i]);}
        if(!fixedUniformsValid)glUniform1i(Uniform("wooTexture"),0);
        if(!fixedUniformsValid||!std::equal(nextStage.begin(),nextStage.begin()+3,fixedStageState.begin()))
            glUniform3i(Uniform("wooColorStage"),nextStage[0],nextStage[1],nextStage[2]);
        if(!fixedUniformsValid||!std::equal(nextStage.begin()+3,nextStage.end(),fixedStageState.begin()+3))
            glUniform3i(Uniform("wooAlphaStage"),nextStage[3],nextStage[4],nextStage[5]);
        const auto changed=[&](std::size_t i){return !fixedUniformsValid||nextRender[i]!=fixedRenderState[i];};
        if(changed(0)){auto c=Color(nextRender[0]);glUniform4f(Uniform("wooTextureFactor"),c.x,c.y,c.z,c.w);}
        if(changed(1))glUniform1i(Uniform("wooAlphaTest"),nextRender[1]);
        if(changed(2))glUniform1i(Uniform("wooAlphaFunc"),nextRender[2]);
        if(changed(3))glUniform1f(Uniform("wooAlphaRef"),float(nextRender[3])/255);
        if(changed(4))glUniform1i(Uniform("wooFogEnable"),nextRender[4]);
        if(changed(5)){auto c=Color(nextRender[5]);glUniform4f(Uniform("wooFogColor"),c.x,c.y,c.z,c.w);}
        fixedStageState=nextStage;fixedRenderState=nextRender;fixedUniformsValid=true;
    }
    void SubmitVertices(GLenum primitive,const std::vector<Vertex>& vertices){
        if(!vertexBindingValid){
            glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
#ifndef PH3_LEGACY_GL_BINDINGS
            vertexBindingValid=true;
#endif
        }
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
        glDrawArrays(primitive,0,static_cast<GLsizei>(vertices.size()));
        ++actualDraws;
    }
    void FlushBatch(){
        if(!batchActive)return;
        if(!batchVertices.empty()){
            SubmitVertices(batchKey.primitive,batchVertices);
        }
        batchVertices.clear();batchActive=false;
    }
    void BindTarget(){
        Check(target!=nullptr,"No Woo render target");auto& surface=*target->storage;
        {
            if(!surface.framebuffer)glGenFramebuffers(1,&surface.framebuffer);
            if(!targetBindingValid||boundFramebuffer!=surface.framebuffer){
                glBindFramebuffer(GL_FRAMEBUFFER,surface.framebuffer);
                boundFramebuffer=surface.framebuffer;
#ifndef PH3_LEGACY_GL_BINDINGS
                targetBindingValid=true;
#endif
            }
            const auto attachment=depth?depth->storage:std::shared_ptr<TextureStorage>();
            if(!surface.framebufferConfigured||surface.framebufferDepth!=attachment){
                glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,surface.texture,0);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,attachment?attachment->depthBuffer:0);
                Check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete Woo render target");
                surface.framebufferDepth=attachment;
                surface.framebufferConfigured=true;
            }
        }
        const GLint y=surface.back?surface.height-viewport.Y-viewport.Height:viewport.Y;
        if(!viewportBindingValid||boundViewport.X!=viewport.X||boundViewportY!=y||
           boundViewport.Width!=viewport.Width||boundViewport.Height!=viewport.Height)
            glViewport(viewport.X,y,viewport.Width,viewport.Height);
        if(!viewportBindingValid||boundViewport.MinZ!=viewport.MinZ||boundViewport.MaxZ!=viewport.MaxZ)
            glDepthRangef(viewport.MinZ,viewport.MaxZ);
        boundViewport=viewport;boundViewportY=y;
#ifndef PH3_LEGACY_GL_BINDINGS
        viewportBindingValid=true;
#endif
    }
    void UseProgram(GLuint selected){
#ifdef PH3_LEGACY_GL_BINDINGS
        glUseProgram(selected);
#else
        if(boundProgram!=selected){glUseProgram(selected);boundProgram=selected;}
#endif
    }
    GLuint SnapshotTarget(){
        Check(target!=nullptr&&!target->storage->depthOnly,"Cannot snapshot an invalid render target");
        auto& surface=*target->storage;
        if(!feedbackTexture)glGenTextures(1,&feedbackTexture);
        glBindTexture(GL_TEXTURE_2D,feedbackTexture);
        if(feedbackWidth!=surface.width||feedbackHeight!=surface.height){
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,surface.width,surface.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            feedbackWidth=surface.width;feedbackHeight=surface.height;
        }
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,surface.width,surface.height);
        feedbackSnapshotReady=true;
        return feedbackTexture;
    }
};

}

using namespace directx::portable;

IDirect3DTexture9::IDirect3DTexture9(std::shared_ptr<TextureStorage> value):storage(std::move(value)){}
IDirect3DSurface9::IDirect3DSurface9(std::shared_ptr<TextureStorage> value):storage(std::move(value)){}
UINT IDirect3DTexture9::GetWidth()const{return storage->width;}
UINT IDirect3DTexture9::GetHeight()const{return storage->height;}
UINT IDirect3DTexture9::GetGLTexture(){storage->Upload();return storage->texture;}
HRESULT IDirect3DTexture9::GetLevelDesc(UINT level,D3DSURFACE_DESC* result){if(level||!result)return InvalidCall;*result=storage->Description();return S_OK;}
HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT level,IDirect3DSurface9** result){if(level||!result)return InvalidCall;*result=new IDirect3DSurface9(storage);return S_OK;}
HRESULT IDirect3DTexture9::LockRect(UINT level,D3DLOCKED_RECT* result,const RECT* rect,DWORD flags){return level?InvalidCall:storage->Lock(result,rect,flags);}
HRESULT IDirect3DTexture9::UnlockRect(UINT level){return level?InvalidCall:storage->Unlock();}
HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC* result){if(!result)return InvalidCall;*result=storage->Description();return S_OK;}
HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT* result,const RECT* rect,DWORD flags){return storage->Lock(result,rect,flags);}
HRESULT IDirect3DSurface9::UnlockRect(){return storage->Unlock();}
HRESULT IDirect3DVertexBuffer9::Lock(UINT offset,UINT size,void** pointer,DWORD){if(!pointer||locked||offset>bytes.size()||(size&&size>bytes.size()-offset))return InvalidCall;*pointer=bytes.data()+offset;locked=true;return S_OK;}
HRESULT IDirect3DVertexBuffer9::Unlock(){if(!locked)return InvalidCall;locked=false;return S_OK;}

IDirect3DDevice9::IDirect3DDevice9(UINT width,UINT height,SDL_Window* existing):state(std::make_unique<DeviceState>()){
    Check(width&&height,"Invalid Woo canvas dimensions");auto& s=*state;
    Check(SDL_InitSubSystem(SDL_INIT_VIDEO),SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE,8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0);SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,0);
    // SDL owns CSS-size/device-pixel-size synchronization. Keep the authored
    // D3D backbuffer dimensions below independent from display pixel density.
    s.window=existing?existing:SDL_CreateWindow("Hollow Song of Birds - Woo Web",width,height,SDL_WINDOW_OPENGL|SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE);
    s.ownWindow=!existing;Check(s.window!=nullptr,SDL_GetError());
    s.context=SDL_GL_CreateContext(s.window);Check(s.context!=nullptr,SDL_GetError());
    Check(SDL_GL_MakeCurrent(s.window,s.context),SDL_GetError());SDL_GL_SetSwapInterval(0);
    s.program=LinkProgram(VertexSource,FragmentSource);glGenVertexArrays(1,&s.vao);glGenBuffers(1,&s.vbo);glGenSamplers(MaxSlots,s.glSamplers.data());
#ifndef PH3_LEGACY_MODULATE_SHADER
    // Compile the small bounded family during initialization, never on Boss
    // entry. Unsupported states still use the unchanged generic program.
    for(unsigned i=1;i<s.modulatePrograms.size();++i){
        const auto fragment=ModulateFragmentSource(static_cast<ModulateProgram>(i));
        s.modulatePrograms[i]=LinkProgram(VertexSource,fragment.c_str());
        glUseProgram(s.modulatePrograms[i]);
        glUniform1i(glGetUniformLocation(s.modulatePrograms[i],"wooTexture"),0);
    }
#endif
    glBindVertexArray(s.vao);glBindBuffer(GL_ARRAY_BUFFER,s.vbo);
    const GLint sizes[]={4,2,4,2,1};const std::size_t offsets[]={offsetof(Vertex,position),offsetof(Vertex,uv),offsetof(Vertex,tint),offsetof(Vertex,pixel),offsetof(Vertex,fog)};
    for(unsigned i=0;i<5;++i){glEnableVertexAttribArray(i);glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsets[i]));}
    glGenTextures(1,&s.white);glBindTexture(GL_TEXTURE_2D,s.white);const BYTE white[]={255,255,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    auto back=std::make_shared<TextureStorage>();back->width=width;back->height=height;back->back=true;back->cpuFresh=false;
    // Keep the original logical backbuffer independent of responsive CSS/DPI.
    // Screenshots and render-target copies still observe exactly width x height.
    glGenTextures(1,&back->texture);glBindTexture(GL_TEXTURE_2D,back->texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
    s.back=new IDirect3DSurface9(back);AssignReference(s.target,s.back);
    auto depth=std::make_shared<TextureStorage>();depth->width=width;depth->height=height;depth->back=true;depth->depthOnly=true;depth->format=D3DFMT_D16;
    glGenRenderbuffers(1,&depth->depthBuffer);glBindRenderbuffer(GL_RENDERBUFFER,depth->depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,width,height);
    s.backDepth=new IDirect3DSurface9(depth);AssignReference(s.depth,s.backDepth);
    s.viewport={0,0,width,height,0,1};
    s.render={{D3DRS_ZENABLE,TRUE},{D3DRS_ZWRITEENABLE,TRUE},{D3DRS_ZFUNC,D3DCMP_LESSEQUAL},
        {D3DRS_CULLMODE,D3DCULL_CCW},{D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_SHADEMODE,D3DSHADE_GOURAUD},
        {D3DRS_SRCBLEND,D3DBLEND_ONE},{D3DRS_DESTBLEND,D3DBLEND_ZERO},{D3DRS_BLENDOP,D3DBLENDOP_ADD},
        {D3DRS_SRCBLENDALPHA,D3DBLEND_ONE},{D3DRS_DESTBLENDALPHA,D3DBLEND_ZERO},{D3DRS_BLENDOPALPHA,D3DBLENDOP_ADD},
        {D3DRS_ALPHAFUNC,D3DCMP_ALWAYS},{D3DRS_TEXTUREFACTOR,0xffffffff},{D3DRS_COLORWRITEENABLE,15},
        {D3DRS_LIGHTING,TRUE},{D3DRS_FOGSTART,FloatBits(0)},{D3DRS_FOGEND,FloatBits(1)},{D3DRS_FOGDENSITY,FloatBits(1)}};
    s.material.Diffuse={1,1,1,1};s.material.Ambient={1,1,1,1};
    for(unsigned i=0;i<MaxSlots;++i){s.samplers[i]={{D3DSAMP_MINFILTER,D3DTEXF_POINT},{D3DSAMP_MAGFILTER,D3DTEXF_POINT},{D3DSAMP_MIPFILTER,D3DTEXF_NONE},{D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP},{D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP}};
        s.stages[i]={{D3DTSS_COLOROP,i?D3DTOP_DISABLE:D3DTOP_MODULATE},{D3DTSS_COLORARG1,D3DTA_TEXTURE},{D3DTSS_COLORARG2,D3DTA_CURRENT},{D3DTSS_ALPHAOP,i?D3DTOP_DISABLE:D3DTOP_SELECTARG1},{D3DTSS_ALPHAARG1,D3DTA_TEXTURE},{D3DTSS_ALPHAARG2,D3DTA_CURRENT}};}
    s.BindTarget();glDisable(GL_DITHER);
    std::printf("[Woo GLES] %s; canvas %ux%u\n",glGetString(GL_VERSION),width,height);
}
IDirect3DDevice9::~IDirect3DDevice9()=default;
SDL_Window* IDirect3DDevice9::GetWindow()const{return state->window;}
HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* caps){if(!caps)return InvalidCall;*caps={};GLint size=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&size);caps->MaxTextureWidth=caps->MaxTextureHeight=size;caps->VertexShaderVersion=D3DVS_VERSION(3,0);caps->PixelShaderVersion=D3DPS_VERSION(3,0);return S_OK;}
HRESULT IDirect3DDevice9::TestCooperativeLevel(){return SDL_GL_GetCurrentContext()==state->context?S_OK:static_cast<HRESULT>(D3DERR_DEVICELOST);}
HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* params){if(!params||params->BackBufferWidth!=state->back->storage->width||params->BackBufferHeight!=state->back->storage->height)return InvalidCall;return TestCooperativeLevel();}
void IDirect3DDevice9::BeginBrowserFrame(){state->draws=state->actualDraws=state->verticesDrawn=state->modulateSubmissions=0;state->targetBindingValid=state->viewportBindingValid=state->vertexBindingValid=false;state->boundProgram=0;}
HRESULT IDirect3DDevice9::BeginScene(){if(state->inScene)return InvalidCall;state->inScene=true;return S_OK;}
HRESULT IDirect3DDevice9::EndScene(){if(!state->inScene)return InvalidCall;state->FlushBatch();state->inScene=false;return S_OK;}
HRESULT IDirect3DDevice9::Present(const RECT*,const RECT*,HWND,const RGNDATA*){
    state->FlushBatch();
    auto& back=*state->back->storage;
    int width=0,height=0;emscripten_get_canvas_element_size("#canvas",&width,&height);
    if(width<=0||height<=0)return E_FAIL;
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,back.framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    state->targetBindingValid=false;
    glBlitFramebuffer(0,0,back.width,back.height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_LINEAR);
#ifdef PH3_WEB_GL_ERROR_CHECK
    const GLenum error=glGetError();
    if(error!=GL_NO_ERROR)throw std::runtime_error("WebGL rendering error: "+std::to_string(error));
#endif
    glFlush();if(!SDL_GL_SwapWindow(state->window))return E_FAIL;
    ++state->presented;
    EM_ASM({const d=globalThis.ph3Diagnostics;if(d){d.presentedFrames=$0;d.drawCalls=$1;d.actualDrawCalls=$2;d.drawVertices=$3;d.rejectedFeedbackDraws=0;d.resolvedFeedbackDraws=$4;
        d.feedbackFixed=$5;d.feedbackEffect=$6;d.feedbackBack=$7;d.feedbackOffscreen=$8;d.modulateSubmissions=$9;}},
        state->presented,state->draws,state->actualDraws,state->verticesDrawn,state->feedbackResolved,
        state->feedbackFixed,state->feedbackEffect,state->feedbackBack,state->feedbackOffscreen,state->modulateSubmissions);
    EM_ASM({const d=globalThis.ph3Diagnostics;if(d){d.feedbackUnit0=$0;d.feedbackUnit1=$1;d.feedbackUnit2=$2;d.feedbackUnit3=$3;
        d.feedbackSlot0=$4;d.feedbackSlot1=$5;d.feedbackSlot2=$6;d.feedbackSlot3=$7;}},
        state->feedbackByUnit[0],state->feedbackByUnit[1],state->feedbackByUnit[2],state->feedbackByUnit[3],
        state->feedbackBySourceSlot[0],state->feedbackBySourceSlot[1],state->feedbackBySourceSlot[2],state->feedbackBySourceSlot[3]);
    return S_OK;
}
HRESULT IDirect3DDevice9::Clear(DWORD count,const D3DRECT* rects,DWORD flags,D3DCOLOR color,float depth,DWORD stencil){
    if(count&&!rects)return InvalidCall;auto& s=*state;s.FlushBatch();s.BindTarget();s.rasterValid=false;auto rgba=Color(color);glClearColor(rgba.x,rgba.y,rgba.z,rgba.w);glClearDepthf(depth);glClearStencil(stencil);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);
    GLbitfield bits=((flags&D3DCLEAR_TARGET)?GL_COLOR_BUFFER_BIT:0)|((flags&D3DCLEAR_ZBUFFER)?GL_DEPTH_BUFFER_BIT:0)|((flags&D3DCLEAR_STENCIL)?GL_STENCIL_BUFFER_BIT:0);
    if(!count){glDisable(GL_SCISSOR_TEST);glClear(bits);}else{glEnable(GL_SCISSOR_TEST);for(UINT i=0;i<count;++i){const auto& r=rects[i];glScissor(r.x1,s.target->storage->back?LONG(s.target->storage->height)-r.y2:r.y1,std::max(0,r.x2-r.x1),std::max(0,r.y2-r.y1));glClear(bits);}glDisable(GL_SCISSOR_TEST);}
    if(flags&D3DCLEAR_TARGET){s.target->storage->cpuFresh=false;s.target->storage->gpuDirty=true;}return S_OK;
}
HRESULT IDirect3DDevice9::SetTransform(DWORD type,const D3DMATRIX* matrix){if(!matrix)return InvalidCall;state->transforms[type]=D3DXMATRIX(*matrix);return S_OK;}
HRESULT IDirect3DDevice9::GetTransform(DWORD type,D3DMATRIX* matrix){if(!matrix)return InvalidCall;*matrix=state->Transform(type);return S_OK;}
HRESULT IDirect3DDevice9::SetViewport(const D3DVIEWPORT9* viewport){if(!viewport||!viewport->Width||!viewport->Height)return InvalidCall;state->viewport=*viewport;return S_OK;}
HRESULT IDirect3DDevice9::GetViewport(D3DVIEWPORT9* viewport){if(!viewport)return InvalidCall;*viewport=state->viewport;return S_OK;}
HRESULT IDirect3DDevice9::SetRenderState(DWORD type,DWORD value){state->render[type]=value;return S_OK;}
HRESULT IDirect3DDevice9::GetRenderState(DWORD type,DWORD* value){if(!value)return InvalidCall;*value=state->Render(type);return S_OK;}
HRESULT IDirect3DDevice9::SetTextureStageState(DWORD stage,DWORD type,DWORD value){if(stage>=MaxSlots)return InvalidCall;state->stages[stage][type]=value;return S_OK;}
HRESULT IDirect3DDevice9::GetTextureStageState(DWORD stage,DWORD type,DWORD* value){if(stage>=MaxSlots||!value)return InvalidCall;*value=state->stages[stage][type];return S_OK;}
HRESULT IDirect3DDevice9::SetSamplerState(DWORD stage,DWORD type,DWORD value){if(stage>=MaxSlots)return InvalidCall;state->samplers[stage][type]=value;return S_OK;}
HRESULT IDirect3DDevice9::GetSamplerState(DWORD stage,DWORD type,DWORD* value){if(stage>=MaxSlots||!value)return InvalidCall;*value=state->samplers[stage][type];return S_OK;}
HRESULT IDirect3DDevice9::SetTexture(DWORD stage,IDirect3DBaseTexture9* texture){if(stage>=MaxSlots)return InvalidCall;AssignReference(state->textures[stage],texture);return S_OK;}
HRESULT IDirect3DDevice9::GetTexture(DWORD stage,IDirect3DBaseTexture9** texture){if(stage>=MaxSlots||!texture)return InvalidCall;*texture=state->textures[stage];if(*texture)(*texture)->AddRef();return S_OK;}
IDirect3DBaseTexture9* IDirect3DDevice9::PeekTexture(UINT stage)const{return stage<MaxSlots?state->textures[stage]:nullptr;}
HRESULT IDirect3DDevice9::SetLight(DWORD index,const D3DLIGHT9* light){if(!light)return InvalidCall;state->lights[index]=*light;return S_OK;}
HRESULT IDirect3DDevice9::GetLight(DWORD index,D3DLIGHT9* light){if(!light||!state->lights.count(index))return InvalidCall;*light=state->lights[index];return S_OK;}
HRESULT IDirect3DDevice9::LightEnable(DWORD index,BOOL enable){state->lightEnabled[index]=enable!=FALSE;return S_OK;}
HRESULT IDirect3DDevice9::SetMaterial(const D3DMATERIAL9* material){if(!material)return InvalidCall;state->material=*material;return S_OK;}
HRESULT IDirect3DDevice9::GetMaterial(D3DMATERIAL9* material){if(!material)return InvalidCall;*material=state->material;return S_OK;}

HRESULT IDirect3DDevice9::CreateTexture(UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** output,HANDLE*){
    if(!output)return InvalidCall;*output=nullptr;GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if(!width||!height||width>UINT(maximum)||height>UINT(maximum)||(format!=D3DFMT_A8R8G8B8&&format!=D3DFMT_X8R8G8B8))return InvalidCall;
    auto data=std::make_shared<TextureStorage>();data->width=width;data->height=height;data->usage=usage;data->format=format;data->pool=pool;
    UINT fullLevels=1;for(UINT n=std::max(width,height);n>1;n>>=1)++fullLevels;data->levels=levels==0||levels==D3DX_DEFAULT?fullLevels:std::min(levels,fullLevels);
    glGenTextures(1,&data->texture);glBindTexture(GL_TEXTURE_2D,data->texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,data->levels-1);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    if(data->levels>1)glGenerateMipmap(GL_TEXTURE_2D);*output=new IDirect3DTexture9(data);return S_OK;
}
HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT width,UINT height,D3DFORMAT format,D3DMULTISAMPLE_TYPE samples,DWORD quality,BOOL,IDirect3DSurface9** output,HANDLE*){
    if(!output)return InvalidCall;*output=nullptr;if(!width||!height||samples!=D3DMULTISAMPLE_NONE||quality||(format!=D3DFMT_D16&&format!=D3DFMT_D24S8))return InvalidCall;
    auto data=std::make_shared<TextureStorage>();data->width=width;data->height=height;data->format=format;data->depthOnly=true;data->usage=D3DUSAGE_DEPTHSTENCIL;
    glGenRenderbuffers(1,&data->depthBuffer);glBindRenderbuffer(GL_RENDERBUFFER,data->depthBuffer);glRenderbufferStorage(GL_RENDERBUFFER,format==D3DFMT_D16?GL_DEPTH_COMPONENT16:GL_DEPTH24_STENCIL8,width,height);*output=new IDirect3DSurface9(data);return S_OK;
}
HRESULT IDirect3DDevice9::GetRenderTarget(DWORD index,IDirect3DSurface9** surface){if(index||!surface)return InvalidCall;*surface=state->target;if(*surface)(*surface)->AddRef();return S_OK;}
HRESULT IDirect3DDevice9::SetRenderTarget(DWORD index,IDirect3DSurface9* surface){if(index||!surface||surface->storage->depthOnly)return InvalidCall;if(state->target!=surface)state->targetBindingValid=false;AssignReference(state->target,surface);state->viewport={0,0,surface->storage->width,surface->storage->height,0,1};return S_OK;}
HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** surface){if(!surface)return InvalidCall;*surface=state->depth;if(*surface)(*surface)->AddRef();return S_OK;}
HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* surface){if(surface&&!surface->storage->depthOnly)return InvalidCall;AssignReference(state->depth,surface);return S_OK;}
HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT length,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** buffer,HANDLE*){if(!buffer||!length)return InvalidCall;*buffer=new IDirect3DVertexBuffer9(length);return S_OK;}
HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT length,DWORD,D3DFORMAT format,D3DPOOL,IDirect3DIndexBuffer9** buffer,HANDLE*){if(!buffer||!length||(format!=D3DFMT_INDEX16&&format!=D3DFMT_INDEX32))return InvalidCall;*buffer=new IDirect3DIndexBuffer9(length,format);return S_OK;}
HRESULT IDirect3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9* elements,IDirect3DVertexDeclaration9** output){
    if(!elements||!output)return InvalidCall;*output=nullptr;auto result=std::make_unique<IDirect3DVertexDeclaration9>();
    for(unsigned i=0;i<MaxElements;++i){if(elements[i].Stream==0xff){*output=result.release();return S_OK;}if(elements[i].Stream!=0||elements[i].Method!=D3DDECLMETHOD_DEFAULT)return InvalidCall;result->elements.push_back(elements[i]);}return InvalidCall;
}
HRESULT IDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* declaration){AssignReference(state->declaration,declaration);return S_OK;}
HRESULT IDirect3DDevice9::SetStreamSource(UINT stream,IDirect3DVertexBuffer9* buffer,UINT offset,UINT stride){if(stream||(!stride&&buffer))return InvalidCall;AssignReference(state->stream,buffer);state->offset=offset;state->stride=stride;return S_OK;}
HRESULT IDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* indices){AssignReference(state->indices,indices);return S_OK;}
HRESULT IDirect3DDevice9::SetFVF(DWORD fvf){state->fvf=fvf;AssignReference(state->declaration,static_cast<IDirect3DVertexDeclaration9*>(nullptr));return S_OK;}
HRESULT IDirect3DDevice9::WarmEffectProgram(ID3DXEffect* effect){
    if(!effect)return InvalidCall;
    return effect->GetProgram(VertexSource)?S_OK:E_FAIL;
}

bool IDirect3DDevice9::BindTextureUnit(UINT unit,IDirect3DBaseTexture9* texture,UINT sourceSlot,const DWORD* overrides){
    Check(unit<MaxSlots&&sourceSlot<MaxSlots,"Effect texture slot is out of range");
    const DWORD keys[]={D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV};
    DWORD values[5];for(unsigned i=0;i<5;++i)values[i]=overrides&&overrides[i]!=0xffffffffu?overrides[i]:state->samplers[sourceSlot][keys[i]];
    glActiveTexture(GL_TEXTURE0+unit);GLuint id=texture?texture->GetGLTexture():state->white;
    if(texture&&id==state->target->storage->texture){
        id=state->feedbackSnapshotReady?state->feedbackTexture:state->SnapshotTarget();
        ++state->feedbackResolved;
        if(ID3DXEffect::Active())++state->feedbackEffect;else ++state->feedbackFixed;
        if(state->target&&state->target->storage->back)++state->feedbackBack;else ++state->feedbackOffscreen;
        if(unit<MaxSlots)++state->feedbackByUnit[unit];
        if(sourceSlot<MaxSlots)++state->feedbackBySourceSlot[sourceSlot];
    }
    glBindTexture(GL_TEXTURE_2D,id);glBindSampler(unit,state->glSamplers[unit]);
    const bool linear=values[0]==D3DTEXF_LINEAR||values[0]==D3DTEXF_ANISOTROPIC;
    GLint minFilter=linear?GL_LINEAR:GL_NEAREST;
    if(values[2]==D3DTEXF_POINT)minFilter=linear?GL_LINEAR_MIPMAP_NEAREST:GL_NEAREST_MIPMAP_NEAREST;
    else if(values[2]==D3DTEXF_LINEAR)minFilter=linear?GL_LINEAR_MIPMAP_LINEAR:GL_NEAREST_MIPMAP_LINEAR;
    const GLint magFilter=values[1]==D3DTEXF_LINEAR||values[1]==D3DTEXF_ANISOTROPIC?GL_LINEAR:GL_NEAREST;
    const GLenum wrapS=Address(values[3]),wrapT=Address(values[4]);
    auto& cache=state->samplerCache[unit];
    if(!cache.valid||cache.minFilter!=minFilter)glSamplerParameteri(state->glSamplers[unit],GL_TEXTURE_MIN_FILTER,minFilter);
    if(!cache.valid||cache.magFilter!=magFilter)glSamplerParameteri(state->glSamplers[unit],GL_TEXTURE_MAG_FILTER,magFilter);
    if(!cache.valid||cache.wrapS!=wrapS)glSamplerParameteri(state->glSamplers[unit],GL_TEXTURE_WRAP_S,wrapS);
    if(!cache.valid||cache.wrapT!=wrapT)glSamplerParameteri(state->glSamplers[unit],GL_TEXTURE_WRAP_T,wrapT);
    cache={true,minFilter,magFilter,wrapS,wrapT};
    return true;
}

namespace directx::portable {
namespace {

struct DecodedVertex {
    D3DXVECTOR4 position{0,0,0,1};D3DXVECTOR3 normal;D3DXVECTOR2 uv;D3DXVECTOR4 color{1,1,1,1};
    std::array<float,4> weights{{1,0,0,0}};std::array<int,4> bones{};
    bool transformed=false,hasNormal=false,skinned=false;
};

template<class T>T Read(const BYTE* bytes,UINT offset,UINT stride){Check(offset<=stride&&sizeof(T)<=stride-offset,"D3D vertex element exceeds its stride");T value;std::memcpy(&value,bytes+offset,sizeof(T));return value;}

DecodedVertex Decode(DeviceState& state,const BYTE* bytes,UINT stride){
    DecodedVertex v;
    if(state.declaration){
        for(const auto& e:state.declaration->elements){
            const UINT offset=e.Offset;
            if(e.Usage==D3DDECLUSAGE_POSITION||e.Usage==D3DDECLUSAGE_POSITIONT){
                if(e.Type==D3DDECLTYPE_FLOAT3){auto p=Read<D3DXVECTOR3>(bytes,offset,stride);v.position={p,1};}
                else if(e.Type==D3DDECLTYPE_FLOAT4)v.position=Read<D3DXVECTOR4>(bytes,offset,stride);
                else throw std::runtime_error("Unsupported position declaration");v.transformed=e.Usage==D3DDECLUSAGE_POSITIONT;
            }else if(e.Usage==D3DDECLUSAGE_NORMAL){v.normal=Read<D3DXVECTOR3>(bytes,offset,stride);v.hasNormal=true;}
            else if(e.Usage==D3DDECLUSAGE_TEXCOORD&&e.UsageIndex==0)v.uv=Read<D3DXVECTOR2>(bytes,offset,stride);
            else if(e.Usage==D3DDECLUSAGE_COLOR&&e.UsageIndex==0){if(e.Type==D3DDECLTYPE_D3DCOLOR)v.color=Color(Read<DWORD>(bytes,offset,stride));else if(e.Type==D3DDECLTYPE_FLOAT4)v.color=Read<D3DXVECTOR4>(bytes,offset,stride);else throw std::runtime_error("Unsupported color declaration");}
            else if(e.Usage==D3DDECLUSAGE_BLENDWEIGHT){Check(e.Type==D3DDECLTYPE_FLOAT4,"Unsupported weight declaration");auto weights=Read<D3DXVECTOR4>(bytes,offset,stride);std::copy(&weights.x,&weights.x+4,v.weights.begin());v.skinned=true;}
            else if(e.Usage==D3DDECLUSAGE_BLENDINDICES){Check(e.Type==D3DDECLTYPE_FLOAT4,"Unsupported bone declaration");auto bones=Read<D3DXVECTOR4>(bytes,offset,stride);for(unsigned i=0;i<4;++i)v.bones[i]=int((&bones.x)[i]);v.skinned=true;}
            else throw std::runtime_error("Unsupported D3D vertex declaration semantic");
        }
        return v;
    }
    UINT offset=0;DWORD position=state.fvf&D3DFVF_POSITION_MASK;
    if(position==D3DFVF_XYZRHW){v.position=Read<D3DXVECTOR4>(bytes,0,stride);offset=16;v.transformed=true;}
    else{auto p=Read<D3DXVECTOR3>(bytes,0,stride);v.position={p,1};offset=12;
        if(position>=D3DFVF_XYZB1&&position<=D3DFVF_XYZB5){unsigned count=(position-D3DFVF_XYZB1)/2+1;
            unsigned weights=count-((state.fvf&D3DFVF_LASTBETA_UBYTE4)?1:0);Check(weights<=3,"Unsupported FVF skin weights");float remaining=1;
            for(unsigned i=0;i<weights;++i){v.weights[i]=Read<float>(bytes,offset,stride);offset+=4;remaining-=v.weights[i];}v.weights[weights]=remaining;
            if(state.fvf&D3DFVF_LASTBETA_UBYTE4){DWORD packed=Read<DWORD>(bytes,offset,stride);offset+=4;for(unsigned i=0;i<4;++i)v.bones[i]=(packed>>(8*i))&255;}
            else for(unsigned i=0;i<4;++i)v.bones[i]=i;v.skinned=true;
        }else Check(position==D3DFVF_XYZ,"Unsupported FVF position format");
    }
    if(state.fvf&D3DFVF_NORMAL){v.normal=Read<D3DXVECTOR3>(bytes,offset,stride);offset+=12;v.hasNormal=true;}
    if(state.fvf&D3DFVF_PSIZE)offset+=4;
    if(state.fvf&D3DFVF_DIFFUSE){v.color=Color(Read<DWORD>(bytes,offset,stride));offset+=4;}
    if(state.fvf&D3DFVF_SPECULAR)offset+=4;
    if(state.fvf&D3DFVF_TEXCOUNT_MASK)v.uv=Read<D3DXVECTOR2>(bytes,offset,stride);
    return v;
}

Vertex Convert(DeviceState& s,const DecodedVertex& source,DrawTransforms& transforms){
    Vertex result;result.uv=source.uv;result.tint=source.color;
    const bool back=s.target->storage->back;
    const auto& viewport=s.viewport;
    if(source.transformed){
        float w=source.position.w!=0?1/source.position.w:1;
        const float x=source.position.x+.5f,y=source.position.y+.5f;
        // D3D9 pre-transformed XYZRHW vertices can still be rasterized when
        // Z testing is disabled even if CPU-side 2D transforms push z outside
        // the nominal [0,1] depth interval (for example a 180-degree Y flip).
        // WebGL always performs homogeneous clip rejection before depth test,
        // so keep disabled-Z vertices inside the clip volume explicitly.
        const float depth=s.Render(D3DRS_ZENABLE)?source.position.z:std::clamp(source.position.z,0.f,1.f);
        result.position={(2*(x-viewport.X)/viewport.Width-1)*w,(back?-1.f:1.f)*(2*(y-viewport.Y)/viewport.Height-1)*w,(2*depth-1)*w,w};
        result.pixel={source.position.x,source.position.y};
    }else{
        transforms.Prepare(s,D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION);
        D3DXVECTOR4 worldPosition;D3DXVECTOR3 worldNormal;
        const bool lighting=source.hasNormal&&s.Render(D3DRS_LIGHTING);
#ifdef PH3_LEGACY_VERTEX_CONSTANTS
        const bool transformNormal=true;
#else
        const bool transformNormal=lighting;
#endif
        const auto* bones=ID3DXEffect::Active()?ID3DXEffect::Active()->WorldMatrices():nullptr;
        if(source.skinned){
            for(unsigned i=0;i<4;++i){if(source.weights[i]==0)continue;D3DXMATRIX matrix;
                if(bones){Check(source.bones[i]>=0&&std::size_t(source.bones[i])<bones->size(),"Bone index exceeds effect matrices");matrix=(*bones)[source.bones[i]];}
                else matrix=s.Transform(D3DTS_WORLDMATRIX(source.bones[i]));
                D3DXVECTOR4 position=source.position;D3DXVec4Transform(&position,&position,&matrix);worldPosition=worldPosition+position*source.weights[i];
                if(transformNormal){D3DXVECTOR3 normal=source.normal;D3DXVec3TransformNormal(&normal,&normal,&matrix);worldNormal+=normal*source.weights[i];}
            }
        }else{worldPosition=source.position;D3DXVec4Transform(&worldPosition,&worldPosition,&transforms.world);if(transformNormal){worldNormal=source.normal;D3DXVec3TransformNormal(&worldNormal,&worldNormal,&transforms.world);}}
        // Preserve three separate transformations and their float operation
        // order. Combining them into an MVP matrix changes rasterized pixels.
        D3DXVECTOR4 viewed=worldPosition;D3DXVec4Transform(&viewed,&viewed,&transforms.view);D3DXVECTOR4 clip=viewed;D3DXVec4Transform(&clip,&clip,&transforms.projection);
        if(clip.w!=0)result.pixel={(clip.x/clip.w+1)*viewport.Width*.5f+viewport.X,(1-clip.y/clip.w)*viewport.Height*.5f+viewport.Y};
        clip.x+=clip.w/viewport.Width;clip.y-=clip.w/viewport.Height;if(!back)clip.y=-clip.y;clip.z=2*clip.z-clip.w;result.position=clip;
        if(lighting){
            D3DXVec3Normalize(&worldNormal,&worldNormal);
            auto ambient=Color(s.Render(D3DRS_AMBIENT));D3DXVECTOR4 illumination{ambient.x*s.material.Ambient.r+s.material.Emissive.r,ambient.y*s.material.Ambient.g+s.material.Emissive.g,ambient.z*s.material.Ambient.b+s.material.Emissive.b,s.material.Diffuse.a};
            for(const auto& [index,light]:s.lights)if(s.lightEnabled[index]){
                Check(light.Type==D3DLIGHT_DIRECTIONAL,"Unsupported non-directional fixed-function light");D3DXVECTOR3 direction(light.Direction);D3DXVec3Normalize(&direction,&direction);float diffuse=std::max(0.f,-D3DXVec3Dot(&worldNormal,&direction));
                illumination.x+=light.Ambient.r*s.material.Ambient.r+light.Diffuse.r*s.material.Diffuse.r*diffuse;
                illumination.y+=light.Ambient.g*s.material.Ambient.g+light.Diffuse.g*s.material.Diffuse.g*diffuse;
                illumination.z+=light.Ambient.b*s.material.Ambient.b+light.Diffuse.b*s.material.Diffuse.b*diffuse;
            }
            result.tint={std::clamp(illumination.x,0.f,1.f),std::clamp(illumination.y,0.f,1.f),std::clamp(illumination.z,0.f,1.f),illumination.w};
        }
        if(s.Render(D3DRS_FOGENABLE)){
            float z=std::abs(viewed.z),start=BitsFloat(s.Render(D3DRS_FOGSTART)),end=BitsFloat(s.Render(D3DRS_FOGEND)),density=BitsFloat(s.Render(D3DRS_FOGDENSITY));
            switch(s.Render(D3DRS_FOGVERTEXMODE)){case D3DFOG_EXP:result.fog=std::exp(-density*z);break;case D3DFOG_EXP2:result.fog=std::exp(-density*density*z*z);break;case D3DFOG_LINEAR:result.fog=end!=start?(end-z)/(end-start):0;break;default:result.fog=1;}
        }
    }
    return result;
}

void SetUniform(DeviceState& state,const char* name,int value){GLint location=state.Uniform(name);if(location>=0)glUniform1i(location,value);}
void SetUniform(DeviceState& state,const char* name,float value){GLint location=state.Uniform(name);if(location>=0)glUniform1f(location,value);}
void SetUniform(DeviceState& state,const char* name,const D3DXVECTOR4& value){GLint location=state.Uniform(name);if(location>=0)glUniform4f(location,value.x,value.y,value.z,value.w);}

HRESULT Draw(IDirect3DDevice9& device,DWORD type,UINT primitives,const BYTE* bytes,UINT stride,
             std::size_t availableVertices,const BYTE* indices,DWORD indexFormat,INT baseVertex){
    auto& s=*device.state;const std::size_t count=VertexCount(type,primitives);
    if(!count)return S_OK;if(!bytes||!stride||count>10000000)return InvalidCall;
    ID3DXEffect* effect=ID3DXEffect::Active();
    DWORD fill=s.Render(D3DRS_FILLMODE);GLenum primitive=Primitive(type);
    const bool wireframe=fill==D3DFILL_WIREFRAME&&type>=D3DPT_TRIANGLELIST;
    if(fill==D3DFILL_POINT)primitive=GL_POINTS;else if(wireframe)primitive=GL_LINES;
    const bool expandTriangles=!effect&&!wireframe&&fill!=D3DFILL_POINT&&
        (type==D3DPT_TRIANGLESTRIP||type==D3DPT_TRIANGLEFAN);
    if(expandTriangles)primitive=GL_TRIANGLES;
    const bool listPrimitive=primitive==GL_POINTS||primitive==GL_LINES||primitive==GL_TRIANGLES;
    const bool originalList=type==D3DPT_POINTLIST||type==D3DPT_LINELIST||type==D3DPT_TRIANGLELIST;
    const bool batchable=!effect&&(originalList||expandTriangles||wireframe||fill==D3DFILL_POINT)&&listPrimitive;
    DeviceState::FixedBatchKey batchKey{};
    if(batchable)batchKey=s.MakeFixedBatchKey(primitive);
    if(s.batchActive&&(!batchable||!(s.batchKey==batchKey)))s.FlushBatch();
    const bool appendBatch=batchable&&s.batchActive;

    s.converted.resize(count);
#ifndef PH3_LEGACY_VERTEX_DECODE
    const DWORD screenFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    const bool screenStream = !indices && !s.declaration &&
        (s.fvf == screenFVF || s.fvf == (screenFVF | D3DFVF_TEX1));
    if(screenStream){
        const bool textured = (s.fvf & D3DFVF_TEXCOUNT_MASK) != 0;
        Check(stride >= (textured ? 28u : 20u), "D3D vertex element exceeds its stride");
        if(baseVertex < 0 || std::uint64_t(baseVertex) > availableVertices ||
           count > availableVertices - std::size_t(baseVertex))return InvalidCall;
        const auto& v=s.viewport;
        const ScreenVertexConverter converter{v.X,v.Y,v.Width,v.Height,
            s.target->storage->back,s.Render(D3DRS_ZENABLE)!=0};
        const BYTE* source=bytes+std::size_t(baseVertex)*stride;
        for(std::size_t i=0;i<count;++i,source+=stride)
            converter.Convert(s.converted[i],source,textured);
    }else
#endif
    {
    DrawTransforms transforms;
    for(std::size_t i=0;i<count;++i){
        std::int64_t index=static_cast<std::int64_t>(i);
        if(indices){if(indexFormat==D3DFMT_INDEX16){WORD value;std::memcpy(&value,indices+i*2,2);index=value;}else if(indexFormat==D3DFMT_INDEX32){DWORD value;std::memcpy(&value,indices+i*4,4);index=value;}else return InvalidCall;}
        index+=baseVertex;if(index<0||static_cast<std::uint64_t>(index)>=availableVertices)return InvalidCall;
        s.converted[i]=Convert(s,Decode(s,bytes+std::size_t(index)*stride,stride),transforms);
    }
    }
    const std::vector<Vertex>* vertices=&s.converted;
    if(expandTriangles){
        ExpandTriangles(s.converted,s.wireframe,primitives,type==D3DPT_TRIANGLESTRIP);
        vertices=&s.wireframe;
    }
    if(wireframe){
        s.wireframe.clear();s.wireframe.reserve(std::size_t(primitives)*6);
        for(UINT i=0;i<primitives;++i){UINT a,b,c;if(type==D3DPT_TRIANGLELIST){a=i*3;b=a+1;c=a+2;}else if(type==D3DPT_TRIANGLESTRIP){a=i;b=i+1;c=i+2;}else{a=0;b=i+1;c=i+2;}
            for(UINT index:{a,b,b,c,c,a})s.wireframe.push_back(s.converted[index]);}
        vertices=&s.wireframe;
    }
    if(appendBatch){
        s.batchVertices.insert(s.batchVertices.end(),vertices->begin(),vertices->end());
        ++s.draws;s.verticesDrawn+=vertices->size();s.target->storage->cpuFresh=false;s.target->storage->gpuDirty=true;return S_OK;
    }

    s.BindTarget();
    s.ApplyRasterState();
    ModulateProgram modulate=ModulateProgram::Generic;
#ifndef PH3_LEGACY_MODULATE_SHADER
    if(!effect){
        const DWORD keys[]={D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,
            D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2};
        std::array<DWORD,6> stage;
        for(unsigned i=0;i<stage.size();++i)stage[i]=s.stages[0].Get(keys[i]);
        modulate=SelectModulateProgram(stage,s.Render(D3DRS_ALPHATESTENABLE),
            s.Render(D3DRS_ALPHAFUNC),s.Render(D3DRS_ALPHAREF),s.Render(D3DRS_FOGENABLE));
    }
#endif
    const bool specialized=modulate!=ModulateProgram::Generic;
    GLuint program=effect?effect->GetProgram(VertexSource):
        specialized?s.modulatePrograms[static_cast<unsigned>(modulate)]:s.program;
    Check(program!=0,"An active effect did not provide a GLES program");s.UseProgram(program);
    s.feedbackSnapshotReady=false;
    bool uniformsReady=true;
    if(effect){
        uniformsReady=effect->ApplyUniforms(&device);
        if(uniformsReady){
            // Alpha test is a D3D9 fixed-function stage after the pixel shader.
            // Custom FX therefore observe the same state as fixed-function draws.
            effect->ApplyAlphaTest(s.Render(D3DRS_ALPHATESTENABLE),
                s.Render(D3DRS_ALPHAFUNC),s.Render(D3DRS_ALPHAREF));
        }
    }
    else{
        uniformsReady=device.BindTextureUnit(0,s.textures[0],0);
        if(uniformsReady){
            if(specialized)++s.modulateSubmissions;
            else s.ApplyFixedUniforms();
        }
    }
    if(!uniformsReady){
        // D3D rejects this one invalid draw; Woo ignores its HRESULT and keeps
        // rendering the remaining objects. Do not turn it into a fatal frame.
        glActiveTexture(GL_TEXTURE0);return InvalidCall;
    }
    if(batchable){
        s.batchKey=batchKey;s.batchVertices.assign(vertices->begin(),vertices->end());s.batchActive=true;
        ++s.draws;s.verticesDrawn+=vertices->size();s.target->storage->cpuFresh=false;s.target->storage->gpuDirty=true;return S_OK;
    }
    s.SubmitVertices(primitive,*vertices);
    ++s.draws;s.verticesDrawn+=vertices->size();s.target->storage->cpuFresh=false;s.target->storage->gpuDirty=true;return S_OK;
}

}
}

HRESULT IDirect3DDevice9::DrawPrimitive(DWORD type,UINT start,UINT count){auto& s=*state;if(!s.stream||!s.stride||s.offset>s.stream->bytes.size())return InvalidCall;std::size_t available=(s.stream->bytes.size()-s.offset)/s.stride;if(start>available)return InvalidCall;return Draw(*this,type,count,s.stream->bytes.data()+s.offset+std::size_t(start)*s.stride,s.stride,available-start,nullptr,0,0);}
HRESULT IDirect3DDevice9::DrawPrimitiveUP(DWORD type,UINT count,const void* vertices,UINT stride){return Draw(*this,type,count,static_cast<const BYTE*>(vertices),stride,VertexCount(type,count),nullptr,0,0);}
HRESULT IDirect3DDevice9::DrawIndexedPrimitive(DWORD type,INT base,UINT,UINT,UINT start,UINT count){auto& s=*state;if(!s.stream||!s.indices||!s.stride||s.offset>s.stream->bytes.size())return InvalidCall;std::size_t bytes=s.indices->format==D3DFMT_INDEX16?2:4;std::size_t required=(std::size_t(start)+VertexCount(type,count))*bytes;if(required>s.indices->bytes.size())return InvalidCall;return Draw(*this,type,count,s.stream->bytes.data()+s.offset,s.stride,(s.stream->bytes.size()-s.offset)/s.stride,s.indices->bytes.data()+std::size_t(start)*bytes,s.indices->format,base);}
HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(DWORD type,UINT minimum,UINT number,UINT count,const void* indices,DWORD format,const void* vertices,UINT stride){if(!indices)return InvalidCall;return Draw(*this,type,count,static_cast<const BYTE*>(vertices),stride,std::size_t(minimum)+number,static_cast<const BYTE*>(indices),format,0);}

namespace {
struct Image {UINT width=0,height=0;D3DXIMAGE_FILEFORMAT format=D3DXIFF_PNG;std::vector<BYTE> rgba;std::vector<DWORD> pixels;};
Image DecodeImage(const void* bytes,UINT size,bool pixels){
    Check(bytes&&size>=8,"Image file is truncated");Image image;
    if(png_sig_cmp(static_cast<png_const_bytep>(bytes),0,8)==0){
        png_image png{};png.version=PNG_IMAGE_VERSION;Check(png_image_begin_read_from_memory(&png,bytes,size)!=0,"Invalid PNG header");
        image.width=png.width;image.height=png.height;Check(image.width&&image.height&&std::uint64_t(image.width)*image.height<=268435456,"PNG dimensions are too large");
        if(pixels){png.format=PNG_FORMAT_RGBA;image.rgba.resize(PNG_IMAGE_SIZE(png));if(!png_image_finish_read(&png,nullptr,image.rgba.data(),0,nullptr)){std::string error=png.message;png_image_free(&png);throw std::runtime_error(error);}}
        png_image_free(&png);
    }else if(static_cast<const BYTE*>(bytes)[0]=='B'&&static_cast<const BYTE*>(bytes)[1]=='M'){
        SDL_Surface* bitmap=SDL_LoadBMP_IO(SDL_IOFromConstMem(bytes,size),true);Check(bitmap!=nullptr,SDL_GetError());
        image.width=bitmap->w;image.height=bitmap->h;image.format=D3DXIFF_BMP;
        if(pixels){SDL_Surface* rgba=SDL_ConvertSurface(bitmap,SDL_PIXELFORMAT_RGBA32);if(!rgba){SDL_DestroySurface(bitmap);throw std::runtime_error(SDL_GetError());}image.rgba.resize(std::size_t(image.width)*image.height*4);for(UINT y=0;y<image.height;++y)std::memcpy(image.rgba.data()+std::size_t(y)*image.width*4,static_cast<BYTE*>(rgba->pixels)+std::size_t(y)*rgba->pitch,std::size_t(image.width)*4);SDL_DestroySurface(rgba);}SDL_DestroySurface(bitmap);
    }else throw std::runtime_error("This Woo Web build supports PNG and BMP images; unsupported image format");
    return image;
}
}

HRESULT D3DXGetImageInfoFromFileInMemory(const void* bytes,UINT size,D3DXIMAGE_INFO* info){if(!info)return InvalidCall;try{auto image=DecodeImage(bytes,size,false);*info={image.width,image.height,1,1,D3DFMT_A8R8G8B8,D3DRTYPE_TEXTURE,image.format};return S_OK;}catch(const std::exception& error){std::fprintf(stderr,"Woo image info: %s\n",error.what());return E_FAIL;}}
HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9* device,const void* bytes,UINT size,UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD,D3DCOLOR colorKey,D3DXIMAGE_INFO* info,void*,IDirect3DTexture9** texture){
    if(!device||!texture)return InvalidCall;*texture=nullptr;
    try{
        auto image=DecodeImage(bytes,size,true);if(info)*info={image.width,image.height,1,1,D3DFMT_A8R8G8B8,D3DRTYPE_TEXTURE,image.format};
        if(width==D3DX_DEFAULT||!width)width=image.width;if(height==D3DX_DEFAULT||!height)height=image.height;
        if(format==D3DFMT_UNKNOWN||format==D3DX_DEFAULT)format=D3DFMT_A8R8G8B8;
        // The browser renderer consumes RGBA directly. The common PNG path uses
        // native dimensions, A8R8G8B8 and no color key, so upload libpng's RGBA
        // result without materializing a D3D DWORD image only to convert it back
        // to RGBA in TextureStorage::Upload(). LockRect can still lazily read the
        // texture back into D3D pixel storage if a caller actually needs it.
        if(width==image.width&&height==image.height&&!colorKey&&format==D3DFMT_A8R8G8B8){
            HRESULT result=device->CreateTexture(width,height,levels,usage,format,pool,texture,nullptr);if(FAILED(result))return result;
            auto& storage=*(*texture)->storage;glBindTexture(GL_TEXTURE_2D,storage.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,image.rgba.data());
            storage.cpuFresh=false;storage.cpuDirty=false;storage.gpuDirty=true;storage.Upload();return S_OK;
        }
        image.pixels.resize(std::size_t(image.width)*image.height);
        for(std::size_t i=0;i<image.pixels.size();++i)image.pixels[i]=D3DCOLOR_ARGB(image.rgba[i*4+3],image.rgba[i*4],image.rgba[i*4+1],image.rgba[i*4+2]);
        image.rgba.clear();image.rgba.shrink_to_fit();
        if(width!=image.width||height!=image.height){
            // Original callers use native dimensions; reject a filter operation
            // until implemented rather than silently selecting a different one.
            Check(filter==D3DX_FILTER_POINT||filter==D3DX_FILTER_NONE,"Filtered image resizing is not implemented");
            std::vector<DWORD> resized(std::size_t(width)*height);for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x)resized[std::size_t(y)*width+x]=image.pixels[std::size_t(std::uint64_t(y)*image.height/height)*image.width+std::uint64_t(x)*image.width/width];image.pixels=std::move(resized);
        }
        if(colorKey)for(DWORD& pixel:image.pixels)if((pixel&0xffffff)==(colorKey&0xffffff))pixel=0;
        HRESULT result=device->CreateTexture(width,height,levels,usage,format,pool,texture,nullptr);if(FAILED(result))return result;
        (*texture)->storage->pixels=std::move(image.pixels);(*texture)->storage->cpuDirty=true;(*texture)->storage->Upload();return S_OK;
    }catch(const std::exception& error){std::fprintf(stderr,"Woo texture load: %s\n",error.what());if(*texture){(*texture)->Release();*texture=nullptr;}return E_FAIL;}
}
HRESULT D3DXSaveSurfaceToFile(const wchar_t* path,D3DXIMAGE_FILEFORMAT format,IDirect3DSurface9* surface,const void*,const RECT* rectangle){
    if(!path||!surface||(format!=D3DXIFF_PNG&&format!=D3DXIFF_BMP))return InvalidCall;
    try{auto& data=*surface->storage;data.Readback();RECT rect=rectangle?*rectangle:RECT{0,0,LONG(data.width),LONG(data.height)};if(rect.left<0||rect.top<0||rect.right>LONG(data.width)||rect.bottom>LONG(data.height)||rect.left>=rect.right||rect.top>=rect.bottom)return InvalidCall;
        UINT width=rect.right-rect.left,height=rect.bottom-rect.top;std::vector<BYTE> rgba(std::size_t(width)*height*4);for(UINT y=0;y<height;++y)for(UINT x=0;x<width;++x){DWORD pixel=data.pixels[std::size_t(y+rect.top)*data.width+x+rect.left];BYTE* p=rgba.data()+(std::size_t(y)*width+x)*4;p[0]=BYTE(pixel>>16);p[1]=BYTE(pixel>>8);p[2]=BYTE(pixel);p[3]=BYTE(pixel>>24);}
        std::string file=gstd::StringUtility::ConvertWideToMulti(std::wstring(path),65001);
        if(format==D3DXIFF_PNG){png_image image{};image.version=PNG_IMAGE_VERSION;image.width=width;image.height=height;image.format=PNG_FORMAT_RGBA;Check(png_image_write_to_file(&image,file.c_str(),0,rgba.data(),0,nullptr)!=0,"PNG screenshot write failed");return S_OK;}
        // D3DX's BMP path is used by ph3's public SaveRenderedTexture/Snapshot
        // APIs. Write a conventional bottom-up 32-bit BI_RGB bitmap so the
        // Web backend exposes the same script-visible file format.
        auto u16=[](std::ostream& out,std::uint16_t value){char bytes[2]={char(value),char(value>>8)};out.write(bytes,2);};
        auto u32=[](std::ostream& out,std::uint32_t value){char bytes[4]={char(value),char(value>>8),char(value>>16),char(value>>24)};out.write(bytes,4);};
        const std::uint32_t pixelBytes=width*height*4u,fileBytes=14u+40u+pixelBytes;
        std::ofstream out(file,std::ios::binary);Check(out.good(),"BMP screenshot open failed");
        out.put('B');out.put('M');u32(out,fileBytes);u16(out,0);u16(out,0);u32(out,54);
        u32(out,40);u32(out,width);u32(out,height);u16(out,1);u16(out,32);u32(out,0);u32(out,pixelBytes);u32(out,2835);u32(out,2835);u32(out,0);u32(out,0);
        for(UINT y=height;y-- >0;)for(UINT x=0;x<width;++x){const BYTE* p=rgba.data()+(std::size_t(y)*width+x)*4;const char bgra[4]={char(p[2]),char(p[1]),char(p[0]),char(p[3])};out.write(bgra,4);}Check(out.good(),"BMP screenshot write failed");return S_OK;
    }catch(const std::exception& error){std::fprintf(stderr,"Woo screenshot: %s\n",error.what());return E_FAIL;}
}
