#pragma once
#include "DxCompat.hpp"
namespace directx::portable {
HRESULT LoadEffect(IDirect3DDevice9* device,const std::wstring& path,const std::string& source,ID3DXEffect** effect,ID3DXBuffer** error);
HRESULT CreateEffect(IDirect3DDevice9* device,const std::string& source,ID3DXEffect** effect,ID3DXBuffer** error);
}
