#include "Window.hpp"
#include "Encoding.hpp"
#include <stdexcept>

// Only the game window owns an SDL handle. Legacy diagnostic widgets retain
// their data models, not invented native handles or a second game renderer.
using namespace gstd;
WindowBase::WindowBase():hWnd_(nullptr),oldWndProc_(nullptr),idWindow_(0){}
WindowBase::~WindowBase(){Detach();}
bool WindowBase::IsDialog(){return dynamic_cast<ModalDialog*>(this)!=nullptr;}
bool WindowBase::Attach(HWND window){hWnd_=window;return window!=nullptr;}
bool WindowBase::Detach(){bool had=hWnd_!=nullptr;hWnd_=nullptr;return had;}
void WindowBase::SetBounds(int x,int y,int width,int height){bounds_={x,y,x+width,y+height};if(hWnd_)SDL_SetWindowSize(GetSDLWindow(),width,height);}
RECT WindowBase::GetClientRect(){int w=bounds_.right-bounds_.left,h=bounds_.bottom-bounds_.top;if(hWnd_)SDL_GetWindowSize(GetSDLWindow(),&w,&h);return {0,0,w,h};}
void WindowBase::SetWindowEnable(bool enabled){enabled_=enabled;}
void WindowBase::SetWindowVisible(bool visible){visible_=visible;if(hWnd_){if(visible)SDL_ShowWindow(GetSDLWindow());else SDL_HideWindow(GetSDLWindow());}}
bool WindowBase::IsWindowVisible(){return visible_;}
void WindowBase::SetParentWindow(HWND parent){parent_=parent;}
DWORD WindowBase::SetWindowStyle(DWORD style){return style_|=style;}
DWORD WindowBase::RemoveWindowStyle(DWORD style){return style_&=~style;}
DWORD WindowBase::GetCurrentWindowStyle(){return style_;}
void WindowBase::MoveWindowCenter(){if(hWnd_)SDL_SetWindowPosition(GetSDLWindow(),SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);}
HWND WindowBase::GetTopParentWindow(HWND window){return window;}
void WindowBase::SetTitle(const std::wstring& title){title_=title;if(hWnd_)SDL_SetWindowTitle(GetSDLWindow(),portable::EncodeText(title,CP_UTF8).c_str());}
bool WindowBase::IsWindowFocused(){return enabled_&&hWnd_&&SDL_GetKeyboardFocus()==GetSDLWindow();}
LRESULT WindowBase::_StaticWindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
LRESULT WindowBase::_CallPreviousWindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
LRESULT WindowBase::_WindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
void ModalDialog::Create(HWND parent,LPCWSTR){hParent_=parent;throw std::runtime_error("Native resource dialog has no browser view");}
void ModalDialog::_RunMessageLoop(){throw std::runtime_error("Blocking modal loop is not supported in a browser");}
void ModalDialog::_FinishMessageLoop(){bEndDialog_=true;}
void WPanel::Create(HWND parent){parent_=parent;}
LRESULT WPanel::_WindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
void WButton::Create(HWND parent){parent_=parent;}
void WButton::Create(HWND parent,Style& style){Create(parent);style_=style.GetStyle();}
void WButton::SetText(std::wstring text){title_=std::move(text);}
bool WButton::IsChecked(){return checked_;}
void WEditBox::Create(HWND parent,Style& style){parent_=parent;style_=style.GetStyle();}
void WEditBox::SetText(std::wstring text){text_=text.substr(0,maxLength_);}
std::wstring WEditBox::GetText(){return text_;}
int WEditBox::GetTextLength(){return static_cast<int>(text_.size());}
int WEditBox::GetMaxTextLength(){return maxLength_;}
void WEditBox::SetMaxTextLength(int length){maxLength_=std::max(0,length);if(text_.size()>static_cast<size_t>(maxLength_))text_.resize(maxLength_);}
void WListView::Create(HWND parent,Style& style){parent_=parent;style_=style.GetStyle();}
void WListView::Clear(){rows_.clear();selected_=-1;}
void WListView::AddColumn(int,int sub,DWORD,std::wstring text){if(sub<0)return;if(columns_.size()<=static_cast<size_t>(sub))columns_.resize(sub+1);columns_[sub]=std::move(text);}
void WListView::AddColumn(int width,int sub,std::wstring text){AddColumn(width,sub,0,std::move(text));}
void WListView::SetColumnText(int index,std::wstring text){AddColumn(0,index,0,std::move(text));}
void WListView::AddRow(std::wstring text){rows_.push_back({std::move(text)});}
void WListView::SetText(int row,int column,std::wstring text){if(row<0||column<0)return;if(rows_.size()<=static_cast<size_t>(row))rows_.resize(row+1);if(rows_[row].size()<=static_cast<size_t>(column))rows_[row].resize(column+1);rows_[row][column]=std::move(text);}
void WListView::DeleteRow(int row){if(row>=0&&row<GetRowCount())rows_.erase(rows_.begin()+row);if(selected_>=GetRowCount())selected_=-1;}
int WListView::GetRowCount(){return static_cast<int>(rows_.size());}
std::wstring WListView::GetText(int row,int column){return row>=0&&row<GetRowCount()&&column>=0&&column<static_cast<int>(rows_[row].size())?rows_[row][column]:L"";}
bool WListView::IsExistsInColumn(std::wstring value,int column){return GetIndexInColumn(value,column)>=0;}
int WListView::GetIndexInColumn(std::wstring value,int column){for(int i=0;i<GetRowCount();++i)if(GetText(i,column)==value)return i;return -1;}
void WListView::SetSelectedRow(int index){selected_=index>=0&&index<GetRowCount()?index:-1;}
int WListView::GetSelectedRow(){return selected_;}
void WListView::ClearSelection(){selected_=-1;}
WTabControll::~WTabControll()=default;
void WTabControll::Create(HWND parent){parent_=parent;}
void WTabControll::AddTab(std::wstring text){AddTab(std::move(text),ref_count_ptr<WPanel>());}
void WTabControll::AddTab(std::wstring text,ref_count_ptr<WPanel> panel){titles_.push_back(std::move(text));vectPanel_.push_back(panel);}
void WTabControll::ShowPage(){for(size_t i=0;i<vectPanel_.size();++i)if(vectPanel_[i]!=NULL)vectPanel_[i]->SetWindowVisible(static_cast<int>(i)==currentPage_);}
int WTabControll::GetCurrentPage(){return currentPage_;}
void WTabControll::SetCurrentPage(int page){if(page>=0&&page<GetPageCount())currentPage_=page;ShowPage();}
void WTabControll::LocateParts(){for(auto& panel:vectPanel_)if(panel!=NULL)panel->LocateParts();}
LRESULT WTabControll::_WindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
void WStatusBar::Create(HWND parent){parent_=parent;}
void WStatusBar::SetPartsSize(std::vector<int> parts){sizes_=std::move(parts);}
void WStatusBar::SetText(int position,std::wstring text){parts_[position]=std::move(text);}
void WStatusBar::SetBounds(WPARAM,LPARAM){if(parent_){int w,h;SDL_GetWindowSize(static_cast<SDL_Window*>(parent_),&w,&h);bounds_={0,h-24,w,h};}}
WTreeView::WTreeView()=default;
WTreeView::~WTreeView()=default;
void WTreeView::Create(HWND parent,Style& style){parent_=parent;style_=style.GetStyle();}
void WTreeView::Clear(){itemRoot_=NULL;}
void WTreeView::CreateRootItem(ItemStyle&){itemRoot_=new Item();}
ref_count_ptr<WTreeView::Item> WTreeView::GetSelectedItem(){return itemRoot_;}
WTreeView::Item::Item()=default;
WTreeView::Item::~Item(){for(auto& child:children_)child->parent_=nullptr;}
ref_count_ptr<WTreeView::Item> WTreeView::Item::CreateChild(ItemStyle&){ref_count_ptr<Item> item=new Item();item->parent_=this;children_.push_back(item);return item;}
void WTreeView::Item::Delete(){if(parent_){auto parent=parent_;parent_=nullptr;parent->children_.remove_if([this](const auto& item){return item.GetPointer()==this;});}}
void WTreeView::Item::SetText(std::wstring text){text_=std::move(text);}
std::wstring WTreeView::Item::GetText(){return text_;}
void WTreeView::Item::SetParam(LPARAM param){parameter_=param;}
LPARAM WTreeView::Item::GetParam(){return parameter_;}
std::list<ref_count_ptr<WTreeView::Item>> WTreeView::Item::GetChildList(){return children_;}
WSplitter::WSplitter():type_(TYPE_VERTICAL),bCapture_(false),ratioX_(0.5),ratioY_(0.5){}
WSplitter::~WSplitter()=default;
void WSplitter::Create(HWND parent,SplitType type){parent_=parent;type_=type;}
LRESULT WSplitter::_WindowProcedure(HWND,UINT,WPARAM,LPARAM){return 0;}
