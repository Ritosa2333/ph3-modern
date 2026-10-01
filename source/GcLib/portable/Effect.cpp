#include "Effect.hpp"
#include "UniformValueCache.hpp"
#include "../gstd/File.hpp"
#include "../directx/HLSL.hpp"
#include <GLES3/gl3.h>
#include <array>
#include <map>
#include <stdexcept>
#ifdef PH3_PERF_SCAN
#include <emscripten/emscripten.h>
#endif

namespace {
constexpr HRESULT Invalid=static_cast<HRESULT>(0x8876086c);
struct Binding {std::string uniform,parameter;DWORD slot;std::array<DWORD,5> states;};
struct Binary {
    const std::string& bytes;size_t offset=8;
    DWORD integer(){if(bytes.size()-offset<4)throw std::runtime_error("Truncated effect header");DWORD result=0;for(int i=0;i<4;++i)result|=DWORD(static_cast<unsigned char>(bytes[offset++]))<<(i*8);return result;}
    std::string blob(){DWORD length=integer();if(length>bytes.size()-offset)throw std::runtime_error("Truncated effect payload");auto result=bytes.substr(offset,length);offset+=length;return result;}
};
GLuint compile(GLenum type,const char* source){
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(log);}return shader;
}
GLuint link(const char* vertex,const char* fragment){
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=0,p=0;
    try{fs=compile(GL_FRAGMENT_SHADER,fragment);p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);throw std::runtime_error(log);}}
    catch(...){if(p)glDeleteProgram(p);if(fs)glDeleteShader(fs);glDeleteShader(vs);throw;}
    glDeleteShader(vs);glDeleteShader(fs);return p;
}
class Effect final:public ID3DXEffect {
public:
    std::string technique,fragment,prefix,path;
    std::vector<Binding> bindings;
    bool builtin=false;
    ~Effect()override{if(Active()==this)SetActive(nullptr);if(program)glDeleteProgram(program);for(auto& [name,texture]:textures)if(texture)texture->Release();if(black)black->Release();}
    HRESULT SetTechnique(D3DXHANDLE name)override{return name&&technique==name?S_OK:Invalid;}
    D3DXHANDLE GetCurrentTechnique()override{return technique.c_str();}
    D3DXHANDLE GetPass(D3DXHANDLE name,UINT index)override{return name&&name==technique&&index==0?"P0":nullptr;}
    HRESULT GetPassDesc(D3DXHANDLE pass,D3DXPASS_DESC* desc)override{if(!pass||std::string(pass)!="P0"||!desc)return Invalid;*desc={};desc->Name="P0";return S_OK;}
    HRESULT SetMatrix(D3DXHANDLE name,const D3DXMATRIX* value)override{return SetMatrixArray(name,value,1);}
    HRESULT SetMatrixArray(D3DXHANDLE name,const D3DXMATRIX* value,UINT count)override{if(!name||(!value&&count))return Invalid;auto& dest=matrices[name];dest.clear();if(count)dest.assign(value,value+count);return S_OK;}
    HRESULT SetVector(D3DXHANDLE name,const D3DXVECTOR4* value)override{if(!name||!value)return Invalid;values[name]={value->x,value->y,value->z,value->w};return S_OK;}
    HRESULT SetFloat(D3DXHANDLE name,float value)override{return SetFloatArray(name,&value,1);}
    HRESULT SetFloatArray(D3DXHANDLE name,const float* value,UINT count)override{if(!name||(!value&&count))return Invalid;auto& dest=values[name];dest.clear();if(count)dest.assign(value,value+count);return S_OK;}
    HRESULT SetTexture(D3DXHANDLE name,IDirect3DBaseTexture9* texture)override{if(!name)return Invalid;auto& old=textures[name];if(texture)texture->AddRef();if(old)old->Release();old=texture;return S_OK;}
    HRESULT Begin(UINT* passes,DWORD)override{if(!passes)return Invalid;*passes=1;return S_OK;}
    HRESULT BeginPass(UINT pass)override{if(pass!=0)throw std::runtime_error("Effect pass index is out of range");SetActive(this);return S_OK;}
    HRESULT EndPass()override{if(Active()==this)SetActive(nullptr);return S_OK;}
    HRESULT End()override{return EndPass();}
    HRESULT OnLostDevice()override{if(program)glDeleteProgram(program);program=0;uniformValues.Clear();return S_OK;}
    HRESULT OnResetDevice()override{return S_OK;}
    const std::vector<D3DXMATRIX>* WorldMatrices()const override{auto it=matrices.find("mWorldMatrixArray");return builtin&&it!=matrices.end()?&it->second:nullptr;}
    UINT GetProgram(const char* vertex)override{
        if(!program){
#ifdef PH3_PERF_SCAN
            const double compileBegin=emscripten_get_now();
#endif
            program=link(vertex,fragment.c_str());GLint count=0;glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&count);uniforms.clear();locations.clear();uniformValues.Clear();
            for(int i=0;i<count;++i){char name[1024];GLint size;GLenum type;glGetActiveUniform(program,i,sizeof(name),nullptr,&size,&type,name);std::string key=name;if(key.size()>3&&key.substr(key.size()-3)=="[0]")key.resize(key.size()-3);uniforms[key]={type,size};}
#ifdef PH3_PERF_SCAN
            const double compileMs=emscripten_get_now()-compileBegin;
            EM_ASM({const d=globalThis.ph3Diagnostics;if(d){const list=d.effectCompiles||(d.effectCompiles=[]);list.push(({path:UTF8ToString($0),ms:$1}));}},path.c_str(),compileMs);
#endif
        }
        return program;
    }
    bool ApplyUniforms(IDirect3DDevice9* device)override{
        for(const auto& [name,value]:values){
            auto key=prefix+name;auto found=uniforms.find(key);if(found==uniforms.end()||value.empty())continue;
            GLint location=Location(key);auto [type,size]=found->second;
            switch(type){
                case GL_FLOAT:FloatUniform(location,UniformKind::Float1,1,std::min<int>(size,value.size()),value.data(),glUniform1fv);break;
                case GL_FLOAT_VEC2:if(value.size()<2)throw std::runtime_error("Effect vec2 value is short");FloatUniform(location,UniformKind::Float2,2,std::min<int>(size,value.size()/2),value.data(),glUniform2fv);break;
                case GL_FLOAT_VEC3:if(value.size()<3)throw std::runtime_error("Effect vec3 value is short");FloatUniform(location,UniformKind::Float3,3,std::min<int>(size,value.size()/3),value.data(),glUniform3fv);break;
                case GL_FLOAT_VEC4:if(value.size()<4)throw std::runtime_error("Effect vec4 value is short");FloatUniform(location,UniformKind::Float4,4,std::min<int>(size,value.size()/4),value.data(),glUniform4fv);break;
                case GL_INT:case GL_BOOL:IntUniform(location,static_cast<int>(value[0]));break;
                default:throw std::runtime_error("Unsupported effect numeric uniform: "+key);
            }
        }
        for(const auto& [name,value]:matrices){auto key=prefix+name;auto found=uniforms.find(key);if(found==uniforms.end()||value.empty())continue;if(found->second.first!=GL_FLOAT_MAT4)throw std::runtime_error("Unsupported effect matrix layout");const auto location=Location(key);const int count=std::min<int>(found->second.second,value.size());Upload(location,UniformKind::Matrix4,count,value[0].m[0],sizeof(float)*16*count,[&](){glUniformMatrix4fv(location,count,GL_FALSE,value[0].m[0]);});}
        unsigned unit=0;
        for(const auto& binding:bindings){
            IDirect3DBaseTexture9* texture=device->PeekTexture(binding.slot);
            if(!binding.parameter.empty()){auto found=textures.find(binding.parameter);texture=found==textures.end()?nullptr:found->second;
                if(!texture){if(!black){if(FAILED(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&black,nullptr)))throw std::runtime_error("Cannot create null sampler");D3DLOCKED_RECT rect;black->LockRect(0,&rect,nullptr,0);*static_cast<DWORD*>(rect.pBits)=0xff000000;black->UnlockRect(0);}texture=black;}}
            if(!device->BindTextureUnit(unit,texture,binding.slot,binding.states.data())){glActiveTexture(GL_TEXTURE0);return false;}
            IntUniform(Location(binding.uniform),unit++);
        }
        glActiveTexture(GL_TEXTURE0);
        return true;
    }
    void ApplyAlphaTest(DWORD enabled,DWORD comparison,DWORD reference)override{
        // D3D's alpha test follows the custom pixel shader. Parameter positions
        // belong to this linked program, not to the current draw. GetProgram()
        // clears Location's cache whenever the program is recreated.
        // Keep uploading the values after user uniforms, including unusual
        // sidecars whose parameters overlap these reserved names.
        const GLint test=Location("ph3AlphaTest");
        const GLint function=Location("ph3AlphaFunc");
        const GLint threshold=Location("ph3AlphaRef");
        if(test>=0)IntUniform(test,int(enabled));
        if(function>=0)IntUniform(function,int(comparison));
        if(threshold>=0){const float value=float(reference)/255.0f;Upload(threshold,UniformKind::Float1,1,&value,sizeof value,[&](){glUniform1f(threshold,value);});}
    }
private:
    using UniformKind=directx::portable::UniformValueCache::Kind;
    directx::portable::UniformValueCache uniformValues;
    template<class Submit> void Upload(GLint location,UniformKind kind,std::size_t count,
                                       const void* bytes,std::size_t length,Submit submit){
#ifdef PH3_LEGACY_EFFECT_UNIFORMS
        submit();
#else
        uniformValues.Apply(location,kind,count,bytes,length,submit);
#endif
    }
    void IntUniform(GLint location,int value){
        Upload(location,UniformKind::Int1,1,&value,sizeof value,[&](){glUniform1i(location,value);});
    }
    template<class Function> void FloatUniform(GLint location,UniformKind kind,int width,
                                               int count,const float* values,Function function){
        Upload(location,kind,count,values,sizeof(float)*width*count,[&](){function(location,count,values);});
    }
    GLuint program=0;
    std::map<std::string,GLint> locations;
    GLint Location(const std::string& name){auto it=locations.find(name);if(it!=locations.end())return it->second;return locations.emplace(name,glGetUniformLocation(program,name.c_str())).first->second;}
    IDirect3DTexture9* black=nullptr;
    std::map<std::string,std::pair<GLenum,GLint>> uniforms;
    std::map<std::string,std::vector<float>> values;
    std::map<std::string,std::vector<D3DXMATRIX>> matrices;
    std::map<std::string,IDirect3DBaseTexture9*> textures;
};
HRESULT error(const std::exception& failure,ID3DXBuffer** output){if(output){auto buffer=new ID3DXBuffer();buffer->text=failure.what();*output=buffer;}return E_FAIL;}
}
HRESULT directx::portable::LoadEffect(IDirect3DDevice9*,const std::wstring& path,const std::string& source,ID3DXEffect** output,ID3DXBuffer** errors){
    if(!output)return Invalid;*output=nullptr;if(errors)*errors=nullptr;
    try{
        auto reader=gstd::FileManager::GetBase()->GetFileReader(path+L".pfx");if(reader==NULL||!reader->Open())throw std::runtime_error("Missing precompiled effect: "+gstd::StringUtility::ConvertWideToMulti(path,CP_UTF8));
        auto bytes=reader->ReadAllString();if(bytes.size()<8||bytes.substr(0,8)!="PH3FX002")throw std::runtime_error("Unsupported effect bundle version");
        Binary data{bytes};if(data.blob()!=source)throw std::runtime_error("Effect sidecar does not match its source");
        auto effect=std::make_unique<Effect>();effect->path=gstd::StringUtility::ConvertWideToMulti(path,CP_UTF8);effect->technique=data.blob();effect->fragment=data.blob();effect->prefix=data.blob();DWORD count=data.integer();if(count>16)throw std::runtime_error("Too many effect samplers");
        for(DWORD i=0;i<count;++i){Binding binding;binding.uniform=data.blob();binding.parameter=data.blob();binding.slot=data.integer();for(auto& state:binding.states)state=data.integer();if(binding.slot>=16)throw std::runtime_error("Invalid effect sampler slot");effect->bindings.push_back(std::move(binding));}
        if(data.offset!=bytes.size())throw std::runtime_error("Unexpected trailing effect data");*output=effect.release();return S_OK;
    }catch(const std::exception& failure){return error(failure,errors);}
}
HRESULT directx::portable::CreateEffect(IDirect3DDevice9*,const std::string& source,ID3DXEffect** output,ID3DXBuffer** errors){
    if(!output)return Invalid;*output=nullptr;if(errors)*errors=nullptr;
    try{
        if(source!=directx::HLSL_DEFAULT_SKINED_MESH)throw std::runtime_error("Dynamic HLSL requires a compiled sidecar; source is not the built-in skin effect");
        auto effect=std::make_unique<Effect>();effect->builtin=true;effect->technique="BasicTec";
        effect->fragment=R"(#version 300 es
precision highp float;
in highp vec2 texcoord;in highp vec4 tint;in highp float fogFactor;
uniform sampler2D wooTexture;out vec4 color;
uniform bool ph3AlphaTest;uniform int ph3AlphaFunc;uniform float ph3AlphaRef;
bool ph3_alpha_pass(float a){
    if(ph3AlphaFunc==1)return false;
    if(ph3AlphaFunc==2)return a<ph3AlphaRef;
    if(ph3AlphaFunc==3)return a==ph3AlphaRef;
    if(ph3AlphaFunc==4)return a<=ph3AlphaRef;
    if(ph3AlphaFunc==5)return a>ph3AlphaRef;
    if(ph3AlphaFunc==6)return a!=ph3AlphaRef;
    if(ph3AlphaFunc==7)return a>=ph3AlphaRef;
    return true;
}
void main(){color=clamp(texture(wooTexture,texcoord)*tint,0.0,1.0);if(ph3AlphaTest&&!ph3_alpha_pass(color.a))discard;}
)";
        effect->bindings.push_back({"wooTexture","",0,{0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu,0xffffffffu}});
        *output=effect.release();return S_OK;
    }catch(const std::exception& failure){return error(failure,errors);}
}
