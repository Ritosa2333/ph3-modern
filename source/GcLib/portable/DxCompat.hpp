#pragma once

#include "Win32Compat.hpp"
#include "DxMath.hpp"
#include "DxConstants.inc"
#include <memory>
#include <string>
#include <vector>

// This is Woo's used D3D9 interface surface, not a Windows device emulator.
// Resource and drawing implementations live in DxDevice.cpp and perform actual
// GLES3 work. Numeric states retain their SDK values, including script-visible
// constants; unused device capabilities are not fabricated.
using D3DCOLOR=DWORD;
using D3DFORMAT=DWORD;
using D3DPOOL=DWORD;
using D3DPRIMITIVETYPE=DWORD;
using D3DCMPFUNC=DWORD;
using D3DTRANSFORMSTATETYPE=DWORD;
using D3DRENDERSTATETYPE=DWORD;
using D3DTEXTURESTAGESTATETYPE=DWORD;
using D3DSAMPLERSTATETYPE=DWORD;
using D3DMULTISAMPLE_TYPE=DWORD;
using D3DSWAPEFFECT=DWORD;
using D3DDEVTYPE=DWORD;
using D3DLIGHTTYPE=DWORD;
using D3DRESOURCETYPE=DWORD;
using D3DXIMAGE_FILEFORMAT=DWORD;
using D3DXHANDLE=const char*;

// Woo retains this legacy misspelling in its original primitive switch.
constexpr DWORD D3DPT_TRIANGESTRIP=D3DPT_TRIANGLESTRIP;

constexpr D3DCOLOR D3DCOLOR_ARGB(DWORD a,DWORD r,DWORD g,DWORD b){return ((a&255)<<24)|((r&255)<<16)|((g&255)<<8)|(b&255);}
constexpr D3DCOLOR D3DCOLOR_XRGB(DWORD r,DWORD g,DWORD b){return D3DCOLOR_ARGB(255,r,g,b);}
constexpr D3DCOLOR D3DCOLOR_RGBA(DWORD r,DWORD g,DWORD b,DWORD a){return D3DCOLOR_ARGB(a,r,g,b);}
constexpr DWORD D3DVS_VERSION(DWORD major,DWORD minor){return 0xfffe0000u|(major<<8)|minor;}
constexpr DWORD D3DPS_VERSION(DWORD major,DWORD minor){return 0xffff0000u|(major<<8)|minor;}
constexpr D3DTRANSFORMSTATETYPE D3DTS_WORLDMATRIX(UINT index){return D3DTS_WORLD+index;}

struct D3DCOLORVALUE { float r=0,g=0,b=0,a=0; };
struct D3DMATERIAL9 { D3DCOLORVALUE Diffuse,Ambient,Specular,Emissive; float Power=0; };
struct D3DRECT { LONG x1,y1,x2,y2; };
struct D3DLOCKED_RECT { INT Pitch; void* pBits; };
struct D3DVIEWPORT9 { DWORD X,Y,Width,Height; float MinZ,MaxZ; };
struct D3DPRESENT_PARAMETERS {
    UINT BackBufferWidth,BackBufferHeight; D3DFORMAT BackBufferFormat;
    UINT BackBufferCount; D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality;
    D3DSWAPEFFECT SwapEffect; HWND hDeviceWindow; BOOL Windowed,EnableAutoDepthStencil;
    D3DFORMAT AutoDepthStencilFormat; DWORD Flags; UINT FullScreen_RefreshRateInHz,PresentationInterval;
};
struct D3DSURFACE_DESC {
    D3DFORMAT Format; D3DRESOURCETYPE Type; DWORD Usage; D3DPOOL Pool;
    D3DMULTISAMPLE_TYPE MultiSampleType; DWORD MultiSampleQuality; UINT Width,Height;
};
struct D3DCAPS9 {
    DWORD DeviceType=0,AdapterOrdinal=0,DevCaps=0,PrimitiveMiscCaps=0,TextureCaps=0;
    DWORD MaxTextureWidth=0,MaxTextureHeight=0,VertexShaderVersion=0,PixelShaderVersion=0;
};
struct D3DDISPLAYMODE { UINT Width,Height,RefreshRate; D3DFORMAT Format; };
struct D3DLIGHT9 {
    D3DLIGHTTYPE Type=0; D3DCOLORVALUE Diffuse,Specular,Ambient;
    D3DVECTOR Position,Direction; float Range=0,Falloff=0,Attenuation0=0,Attenuation1=0,Attenuation2=0,Theta=0,Phi=0;
};
struct D3DVERTEXELEMENT9 { WORD Stream,Offset; BYTE Type,Method,Usage,UsageIndex; };
constexpr D3DVERTEXELEMENT9 D3DDECL_END(){return {0xff,0,static_cast<BYTE>(D3DDECLTYPE_UNUSED),0,0,0};}
struct D3DXIMAGE_INFO { UINT Width,Height,Depth,MipLevels; D3DFORMAT Format; D3DRESOURCETYPE ResourceType; D3DXIMAGE_FILEFORMAT ImageFileFormat; };
struct D3DXPASS_DESC { const char* Name=nullptr; UINT Annotations=0; const DWORD* pVertexShaderFunction=nullptr; const DWORD* pPixelShaderFunction=nullptr; };
struct RGNDATA;
struct SDL_Window;
class IDirect3D9;
class IDirect3DDevice9;
class IDirect3DTexture9;
class IDirect3DSurface9;
class IDirect3DVertexBuffer9;
class IDirect3DIndexBuffer9;
class IDirect3DVertexDeclaration9;
class IDirect3DVertexShader9;
class IDirect3DPixelShader9;
class ID3DXEffect;
class ID3DXBuffer;
struct IDirectInput8;
struct IDirectInputDevice8;
using LPDIRECTINPUT8=IDirectInput8*;
using LPDIRECTINPUTDEVICE8=IDirectInputDevice8*;
struct DIDEVICEINSTANCE { DWORD dwSize; WCHAR tszInstanceName[MAX_PATH],tszProductName[MAX_PATH]; };
using LPDIDEVICEINSTANCE=DIDEVICEINSTANCE*;
struct DIMOUSESTATE { LONG lX,lY,lZ; BYTE rgbButtons[4]; };
struct DIJOYSTATE { LONG lX,lY,lZ,lRx,lRy,lRz,rglSlider[2]; DWORD rgdwPOV[4]; BYTE rgbButtons[32]; };

class WooDxResource {
public:
    virtual ~WooDxResource()=default;
    ULONG AddRef(){return ++references_;}
    ULONG Release(){const ULONG left=--references_;if(!left)delete this;return left;}
private:
    ULONG references_=1;
};

namespace directx::portable { struct TextureStorage; struct DeviceState; }

class IDirect3DBaseTexture9 : public WooDxResource {
public:
    virtual UINT GetGLTexture()=0;
    virtual UINT GetWidth() const=0;
    virtual UINT GetHeight() const=0;
};
class IDirect3DTexture9 : public IDirect3DBaseTexture9 {
public:
    explicit IDirect3DTexture9(std::shared_ptr<directx::portable::TextureStorage> storage);
    HRESULT GetLevelDesc(UINT level,D3DSURFACE_DESC* description);
    HRESULT GetSurfaceLevel(UINT level,IDirect3DSurface9** surface);
    HRESULT LockRect(UINT level,D3DLOCKED_RECT* locked,const RECT* rect,DWORD flags);
    HRESULT UnlockRect(UINT level);
    UINT GetGLTexture() override;
    UINT GetWidth() const override;
    UINT GetHeight() const override;
    std::shared_ptr<directx::portable::TextureStorage> storage;
};
class IDirect3DSurface9 : public WooDxResource {
public:
    explicit IDirect3DSurface9(std::shared_ptr<directx::portable::TextureStorage> storage);
    HRESULT GetDesc(D3DSURFACE_DESC* description);
    HRESULT LockRect(D3DLOCKED_RECT* locked,const RECT* rect,DWORD flags);
    HRESULT UnlockRect();
    std::shared_ptr<directx::portable::TextureStorage> storage;
};
class IDirect3DVertexBuffer9 : public WooDxResource {
public:
    explicit IDirect3DVertexBuffer9(UINT size):bytes(size){}
    HRESULT Lock(UINT offset,UINT size,void** pointer,DWORD flags);
    HRESULT Unlock();
    std::vector<BYTE> bytes;
    bool locked=false;
};
class IDirect3DIndexBuffer9 : public IDirect3DVertexBuffer9 {
public:
    IDirect3DIndexBuffer9(UINT size,D3DFORMAT format):IDirect3DVertexBuffer9(size),format(format){}
    D3DFORMAT format;
};
class IDirect3DVertexDeclaration9 : public WooDxResource {
public:
    std::vector<D3DVERTEXELEMENT9> elements;
};

class IDirect3DDevice9 : public WooDxResource {
public:
    // Creates one real SDL canvas/context. Existing managers keep owning their
    // resources and rendering semantics above this platform boundary.
    explicit IDirect3DDevice9(UINT width,UINT height,SDL_Window* window=nullptr);
    ~IDirect3DDevice9() override;
    SDL_Window* GetWindow() const;
    HRESULT GetDeviceCaps(D3DCAPS9* caps);
    HRESULT TestCooperativeLevel();
    HRESULT Reset(D3DPRESENT_PARAMETERS* parameters);
    HRESULT BeginScene();
    void BeginBrowserFrame();
    HRESULT EndScene();
    HRESULT Present(const RECT*,const RECT*,HWND,const RGNDATA*);
    HRESULT Clear(DWORD count,const D3DRECT* rects,DWORD flags,D3DCOLOR color,float depth,DWORD stencil);
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE type,const D3DMATRIX* matrix);
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE type,D3DMATRIX* matrix);
    HRESULT SetViewport(const D3DVIEWPORT9* viewport);
    HRESULT GetViewport(D3DVIEWPORT9* viewport);
    HRESULT SetRenderState(D3DRENDERSTATETYPE type,DWORD value);
    HRESULT GetRenderState(D3DRENDERSTATETYPE type,DWORD* value);
    HRESULT SetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD value);
    HRESULT GetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE type,DWORD* value);
    HRESULT SetSamplerState(DWORD stage,D3DSAMPLERSTATETYPE type,DWORD value);
    HRESULT GetSamplerState(DWORD stage,D3DSAMPLERSTATETYPE type,DWORD* value);
    HRESULT SetTexture(DWORD stage,IDirect3DBaseTexture9* texture);
    HRESULT GetTexture(DWORD stage,IDirect3DBaseTexture9** texture);
    HRESULT SetLight(DWORD index,const D3DLIGHT9* light);
    HRESULT GetLight(DWORD index,D3DLIGHT9* light);
    HRESULT LightEnable(DWORD index,BOOL enable);
    HRESULT SetMaterial(const D3DMATERIAL9* material);
    HRESULT GetMaterial(D3DMATERIAL9* material);
    HRESULT CreateTexture(UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** texture,HANDLE*);
    HRESULT CreateDepthStencilSurface(UINT width,UINT height,D3DFORMAT format,D3DMULTISAMPLE_TYPE samples,DWORD quality,BOOL discard,IDirect3DSurface9** surface,HANDLE*);
    HRESULT GetRenderTarget(DWORD index,IDirect3DSurface9** surface);
    HRESULT SetRenderTarget(DWORD index,IDirect3DSurface9* surface);
    HRESULT GetDepthStencilSurface(IDirect3DSurface9** surface);
    HRESULT SetDepthStencilSurface(IDirect3DSurface9* surface);
    HRESULT CreateVertexBuffer(UINT length,DWORD usage,DWORD fvf,D3DPOOL pool,IDirect3DVertexBuffer9** buffer,HANDLE*);
    HRESULT CreateIndexBuffer(UINT length,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DIndexBuffer9** buffer,HANDLE*);
    HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9* elements,IDirect3DVertexDeclaration9** declaration);
    HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9* declaration);
    HRESULT SetStreamSource(UINT stream,IDirect3DVertexBuffer9* buffer,UINT offset,UINT stride);
    HRESULT SetIndices(IDirect3DIndexBuffer9* indices);
    HRESULT SetFVF(DWORD fvf);
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE type,UINT start,UINT count);
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE type,UINT count,const void* vertices,UINT stride);
    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE type,INT baseVertex,UINT minVertex,UINT numVertices,UINT startIndex,UINT count);
    HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE type,UINT minVertex,UINT numVertices,UINT count,const void* indices,D3DFORMAT indexFormat,const void* vertices,UINT stride);

    // Effect bridge: supplied state overrides use the PH3FX002 ordering;
    // 0xffffffff inherits the original D3D sampler slot, rather than forcing a
    // filter/wrap mode. Actual GLES sampler objects keep texture units separate.
    void BindTextureUnit(UINT unit,IDirect3DBaseTexture9* texture,UINT sourceSlot,const DWORD* overrides=nullptr);
    IDirect3DBaseTexture9* PeekTexture(UINT stage) const;
    std::unique_ptr<directx::portable::DeviceState> state;
};

class ID3DXBuffer : public WooDxResource {
public:
    std::string text;
    void* GetBufferPointer(){return text.data();}
    DWORD GetBufferSize(){return static_cast<DWORD>(text.size());}
};

// Effect.cpp supplies a real implementation. No default method pretends that an
// unsupported effect rendered successfully.
class ID3DXEffect : public WooDxResource {
public:
    virtual HRESULT SetTechnique(D3DXHANDLE)=0;
    virtual D3DXHANDLE GetCurrentTechnique()=0;
    virtual D3DXHANDLE GetPass(D3DXHANDLE,UINT)=0;
    virtual HRESULT GetPassDesc(D3DXHANDLE,D3DXPASS_DESC*)=0;
    virtual HRESULT SetMatrix(D3DXHANDLE,const D3DXMATRIX*)=0;
    virtual HRESULT SetMatrixArray(D3DXHANDLE,const D3DXMATRIX*,UINT)=0;
    virtual HRESULT SetVector(D3DXHANDLE,const D3DXVECTOR4*)=0;
    virtual HRESULT SetFloat(D3DXHANDLE,float)=0;
    virtual HRESULT SetFloatArray(D3DXHANDLE,const float*,UINT)=0;
    virtual HRESULT SetTexture(D3DXHANDLE,IDirect3DBaseTexture9*)=0;
    virtual HRESULT Begin(UINT*,DWORD)=0;
    virtual HRESULT BeginPass(UINT)=0;
    virtual HRESULT EndPass()=0;
    virtual HRESULT End()=0;
    virtual HRESULT OnLostDevice()=0;
    virtual HRESULT OnResetDevice()=0;
    virtual UINT GetProgram(const char* vertexSource)=0;
    virtual void ApplyUniforms(IDirect3DDevice9* device)=0;
    virtual const std::vector<D3DXMATRIX>* WorldMatrices() const {return nullptr;}
    static ID3DXEffect* Active(){return active_;}
    static void SetActive(ID3DXEffect* effect){active_=effect;}
private:
    inline static ID3DXEffect* active_=nullptr;
};

HRESULT D3DXGetImageInfoFromFileInMemory(const void* bytes,UINT size,D3DXIMAGE_INFO* info);
HRESULT D3DXCreateTextureFromFileInMemoryEx(IDirect3DDevice9* device,const void* bytes,UINT size,
    UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,
    DWORD mipFilter,D3DCOLOR colorKey,D3DXIMAGE_INFO* info,void* palette,IDirect3DTexture9** texture);
HRESULT D3DXSaveSurfaceToFile(const wchar_t* path,D3DXIMAGE_FILEFORMAT format,IDirect3DSurface9* surface,
    const void* palette,const RECT* rect);
