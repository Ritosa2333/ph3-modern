#ifndef __GCLIB_PORTABLE_WINDOW__
#define __GCLIB_PORTABLE_WINDOW__

#include "../gstd/GstdConstant.hpp"
#include "../gstd/GstdUtility.hpp"
#include "../gstd/Thread.hpp"

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#endif

namespace gstd {

class WindowBase {
public:
  class Style;
  WindowBase();
  virtual ~WindowBase();
  HWND GetWindowHandle() { return hWnd_; }
#ifdef __EMSCRIPTEN__
  SDL_Window* GetSDLWindow() { return static_cast<SDL_Window*>(hWnd_); }
#endif
  bool IsDialog();
  bool Attach(HWND hWnd);
  bool Detach();
  int GetWindowId() { return idWindow_; }
  virtual void SetBounds(int x, int y, int width, int height);
  RECT GetClientRect();
  int GetClientX() { return GetClientRect().left; }
  int GetClientY() { return GetClientRect().top; }
  int GetClientWidth() { RECT rect = GetClientRect(); return rect.right - rect.left; }
  int GetClientHeight() { RECT rect = GetClientRect(); return rect.bottom - rect.top; }
  void SetWindowEnable(bool bEnable);
  void SetWindowVisible(bool bVisible);
  bool IsWindowVisible();
  void SetParentWindow(HWND hParentWnd);
  DWORD SetWindowStyle(DWORD style);
  DWORD RemoveWindowStyle(DWORD style);
  DWORD GetCurrentWindowStyle();
  virtual void LocateParts() {}
  void MoveWindowCenter();
  static HWND GetTopParentWindow(HWND hWnd);
#ifdef __EMSCRIPTEN__
  virtual void SetTitle(const std::wstring& title);
  virtual bool IsWindowFocused();
#endif
protected:
  HWND hWnd_;
  void* oldWndProc_;
  int idWindow_;
  HWND parent_ = nullptr;
  RECT bounds_{0, 0, 0, 0};
  DWORD style_ = 0;
  bool enabled_ = true, visible_ = false;
  std::wstring title_;
  static LRESULT CALLBACK _StaticWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  LRESULT _CallPreviousWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual LRESULT _WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class WindowBase::Style {
public:
  Style() : style_(0), styleEx_(0) {}
  virtual ~Style() {}
  DWORD GetStyle() { return style_; }
  DWORD SetStyle(DWORD style) { style_ |= style; return style_; }
  DWORD RemoveStyle(DWORD style) { style_ &= ~style; return style_; }
  DWORD GetStyleEx() { return styleEx_; }
  DWORD SetStyleEx(DWORD style) { styleEx_ |= style; return styleEx_; }
  DWORD RemoveStyleEx(DWORD style) { styleEx_ &= ~style; return styleEx_; }
protected:
  DWORD style_;
  DWORD styleEx_;
};

class ModalDialog : public WindowBase {
public:
  ModalDialog() : hParent_(nullptr), bEndDialog_(false) {}
  void Create(HWND hParent, LPCWSTR resource);
protected:
  HWND hParent_;
  bool bEndDialog_;
  void _RunMessageLoop();
  void _FinishMessageLoop();
};

class WPanel : public WindowBase {
public:
  void Create(HWND hWndParent);
protected:
  virtual LRESULT _WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class WButton : public WindowBase {
public:
  class Style;
  void Create(HWND hWndParent);
  void Create(HWND hWndParent, Style& style);
  void SetText(std::wstring text);
  bool IsChecked();
private:
  bool checked_ = false;
};
class WButton::Style : public WindowBase::Style {};

class WEditBox : public WindowBase {
public:
  class Style;
  void Create(HWND hWndParent, Style& style);
  void SetText(std::wstring text);
  std::wstring GetText();
  int GetTextLength();
  int GetMaxTextLength();
  void SetMaxTextLength(int length);
private:
  std::wstring text_;
  int maxLength_ = 1 << 20;
};
class WEditBox::Style : public WindowBase::Style {};

class WListView : public WindowBase {
public:
  class Style;
  void Create(HWND hWndParent, Style& style);
  void Clear();
  void AddColumn(int cx, int sub, DWORD fmt, std::wstring text);
  void AddColumn(int cx, int sub, std::wstring text);
  void SetColumnText(int cx, std::wstring text);
  void AddRow(std::wstring text);
  void SetText(int row, int column, std::wstring text);
  void DeleteRow(int row);
  int GetRowCount();
  std::wstring GetText(int row, int column);
  bool IsExistsInColumn(std::wstring value, int column);
  int GetIndexInColumn(std::wstring value, int column);
  void SetSelectedRow(int index);
  int GetSelectedRow();
  void ClearSelection();
private:
  std::vector<std::wstring> columns_;
  std::vector<std::vector<std::wstring>> rows_;
  int selected_ = -1;
};
class WListView::Style : public WindowBase::Style {
public:
  Style() : styleListViewEx_(0) {}
  DWORD GetListViewStyleEx() { return styleListViewEx_; }
  DWORD SetListViewStyleEx(DWORD style) { styleListViewEx_ |= style; return styleListViewEx_; }
  DWORD RemoveListViewStyleEx(DWORD style) { styleListViewEx_ &= ~style; return styleListViewEx_; }
protected:
  DWORD styleListViewEx_;
};

class WTabControll : public WindowBase {
public:
  ~WTabControll();
  void Create(HWND hWndParent);
  void AddTab(std::wstring text);
  void AddTab(std::wstring text, ref_count_ptr<WPanel> panel);
  void ShowPage();
  int GetCurrentPage();
  void SetCurrentPage(int page);
  ref_count_ptr<WPanel> GetPanel(int page) { return vectPanel_[page]; }
  int GetPageCount() { return static_cast<int>(vectPanel_.size()); }
  virtual void LocateParts();
protected:
  virtual LRESULT _WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
  std::vector<ref_count_ptr<WPanel>> vectPanel_;
  std::vector<std::wstring> titles_;
  int currentPage_ = 0;
};

class WStatusBar : public WindowBase {
public:
  using WindowBase::SetBounds;
  void Create(HWND hWndParent);
  void SetPartsSize(std::vector<int> parts);
  void SetText(int pos, std::wstring text);
  void SetBounds(WPARAM wParam, LPARAM lParam);
private:
  std::vector<int> sizes_;
  std::map<int, std::wstring> parts_;
};

class WTreeView : public WindowBase {
public:
  class Item;
  class ItemStyle;
  class Style;
  WTreeView();
  ~WTreeView();
  void Create(HWND hWndParent, Style& style);
  void Clear();
  void CreateRootItem(ItemStyle& style);
  ref_count_ptr<Item> GetRootItem() { return itemRoot_; }
  ref_count_ptr<Item> GetSelectedItem();
private:
  ref_count_ptr<Item> itemRoot_;
};
class WTreeView::Item {
public:
  Item();
  virtual ~Item();
  ref_count_ptr<Item> CreateChild(WTreeView::ItemStyle& style);
  void Delete();
  void SetText(std::wstring text);
  std::wstring GetText();
  void SetParam(LPARAM param);
  LPARAM GetParam();
  std::list<ref_count_ptr<Item>> GetChildList();
private:
  Item* parent_ = nullptr;
  std::wstring text_;
  LPARAM parameter_ = 0;
  std::list<ref_count_ptr<Item>> children_;
};
class WTreeView::ItemStyle {
public:
  ItemStyle() {}
  void SetMask(UINT mask) { mask_ |= mask; }
private:
  UINT mask_ = 0;
};
class WTreeView::Style : public WindowBase::Style {};

class WSplitter : public WindowBase {
public:
  enum SplitType { TYPE_VERTICAL, TYPE_HORIZONTAL };
  WSplitter();
  ~WSplitter();
  void Create(HWND hWndParent, SplitType type);
  void SetRatioX(float ratio) { ratioX_ = ratio; }
  float GetRatioX() { return ratioX_; }
  void SetRatioY(float ratio) { ratioY_ = ratio; }
  float GetRatioY() { return ratioY_; }
protected:
  SplitType type_;
  bool bCapture_;
  float ratioX_;
  float ratioY_;
  virtual LRESULT _WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class WindowUtility {
public:
  static void SetMouseVisible(bool bVisible);
};

#ifdef __EMSCRIPTEN__
inline void WindowUtility::SetMouseVisible(bool bVisible) {
  if (bVisible) SDL_ShowCursor(); else SDL_HideCursor();
}
#endif

} // namespace gstd

#endif
