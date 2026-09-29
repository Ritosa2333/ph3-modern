#include "../gstd/Logger.hpp"
#include "Encoding.hpp"
#include <emscripten.h>

using namespace gstd;
WindowLogger::WindowLogger():bEnable_(false){}
WindowLogger::~WindowLogger()=default;
bool WindowLogger::Initialize(bool enabled){
    bEnable_=enabled;
    wndTab_=new WTabControll();wndStatus_=new WStatusBar();
    wndInfoPanel_=new InfoPanel();wndLogPanel_=new LogPanel();
    AddPanel(wndLogPanel_,L"Log");AddPanel(wndInfoPanel_,L"Info");return true;
}
void WindowLogger::SaveState(){if(!bEnable_)return;RecordBuffer record;record.SetRecordAsInteger("panelIndex",wndTab_->GetCurrentPage());record.WriteToFile(PathProperty::GetModuleDirectory()+L"LogWindow.dat");}
void WindowLogger::LoadState(){if(!bEnable_)return;RecordBuffer record;if(record.ReadFromFile(PathProperty::GetModuleDirectory()+L"LogWindow.dat"))wndTab_->SetCurrentPage(record.GetRecordAsInteger("panelIndex"));}
void WindowLogger::SetInfo(int row,std::wstring label,std::wstring data){if(bEnable_&&wndInfoPanel_!=NULL)wndInfoPanel_->SetInfo(row,std::move(label),std::move(data));}
bool WindowLogger::AddPanel(ref_count_ptr<Panel> panel,std::wstring name){if(!bEnable_||panel==NULL)return false;panel->_AddedLogger(nullptr);wndTab_->AddTab(std::move(name),panel);return true;}
void WindowLogger::ShowLogWindow(){EM_ASM({const e=document.getElementById('log');if(e){const details=e.closest('details');if(details)details.open=true;}});}
void WindowLogger::InsertOpenCommandInSystemMenu(HWND){/* Browser shell owns the diagnostics button. */}
void WindowLogger::_CreateWindow(){throw std::runtime_error("Native logger window is replaced by the browser console");}
void WindowLogger::_Run(){throw std::runtime_error("Native logger thread cannot run in the browser");}
void WindowLogger::_Write(SYSTEMTIME&,std::wstring text){auto utf8=portable::EncodeText(text,CP_UTF8);std::puts(utf8.c_str());}
LRESULT WindowLogger::_WindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
WindowLogger::WindowThread::WindowThread(WindowLogger* logger){_SetOuter(logger);}
void WindowLogger::WindowThread::_Run(){_GetOuter()->_Run();}
WindowLogger::LogPanel::LogPanel()=default;
WindowLogger::LogPanel::~LogPanel()=default;
bool WindowLogger::LogPanel::_AddedLogger(HWND parent){Create(parent);WEditBox::Style style;wndEdit_.Create(parent,style);return true;}
void WindowLogger::LogPanel::LocateParts(){auto r=GetClientRect();wndEdit_.SetBounds(0,0,r.right,r.bottom);}
void WindowLogger::LogPanel::AddText(std::wstring text){auto previous=wndEdit_.GetText();previous+=text;if(previous.size()>65536)previous.erase(0,previous.size()-65536);wndEdit_.SetText(std::move(previous));}
WindowLogger::InfoPanel::InfoPanel()=default;
WindowLogger::InfoPanel::~InfoPanel()=default;
bool WindowLogger::InfoPanel::_AddedLogger(HWND parent){Create(parent);WListView::Style style;wndListView_.Create(parent,style);return true;}
void WindowLogger::InfoPanel::LocateParts(){auto r=GetClientRect();wndListView_.SetBounds(0,0,r.right,r.bottom);}
void WindowLogger::InfoPanel::SetInfo(int row,std::wstring label,std::wstring data){wndListView_.SetText(row,ROW_INFO,std::move(label));wndListView_.SetText(row,ROW_DATA,std::move(data));}
WindowLogger::InfoCollectThread::InfoCollectThread(ref_count_ptr<WStatusBar> status):wndStatus_(status){}
WindowLogger::InfoCollectThread::~InfoCollectThread()=default;
void WindowLogger::InfoCollectThread::_Run(){throw std::runtime_error("Native CPU polling is not available in a browser");}
WindowLogger::InfoCollectThread::CpuInfo WindowLogger::InfoCollectThread::_GetCpuInformation(){CpuInfo info{};info.cpuName="WebAssembly";return info;}
double WindowLogger::InfoCollectThread::_GetCpuPerformance(){return std::numeric_limits<double>::quiet_NaN();}
