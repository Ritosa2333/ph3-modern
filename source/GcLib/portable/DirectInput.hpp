#ifndef __GCLIB_PORTABLE_DIRECTINPUT__
#define __GCLIB_PORTABLE_DIRECTINPUT__

#include "../directx/DxConstant.hpp"

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#endif

enum {
  DIK_ESCAPE=0x01, DIK_1=0x02, DIK_2=0x03, DIK_3=0x04, DIK_4=0x05,
  DIK_5=0x06, DIK_6=0x07, DIK_7=0x08, DIK_8=0x09, DIK_9=0x0A,
  DIK_0=0x0B, DIK_MINUS=0x0C, DIK_EQUALS=0x0D, DIK_BACK=0x0E, DIK_TAB=0x0F,
  DIK_Q=0x10, DIK_W=0x11, DIK_E=0x12, DIK_R=0x13, DIK_T=0x14,
  DIK_Y=0x15, DIK_U=0x16, DIK_I=0x17, DIK_O=0x18, DIK_P=0x19,
  DIK_LBRACKET=0x1A, DIK_RBRACKET=0x1B, DIK_RETURN=0x1C, DIK_LCONTROL=0x1D,
  DIK_A=0x1E, DIK_S=0x1F, DIK_D=0x20, DIK_F=0x21, DIK_G=0x22,
  DIK_H=0x23, DIK_J=0x24, DIK_K=0x25, DIK_L=0x26, DIK_SEMICOLON=0x27,
  DIK_APOSTROPHE=0x28, DIK_GRAVE=0x29, DIK_LSHIFT=0x2A, DIK_BACKSLASH=0x2B,
  DIK_Z=0x2C, DIK_X=0x2D, DIK_C=0x2E, DIK_V=0x2F, DIK_B=0x30,
  DIK_N=0x31, DIK_M=0x32, DIK_COMMA=0x33, DIK_PERIOD=0x34, DIK_SLASH=0x35,
  DIK_RSHIFT=0x36, DIK_MULTIPLY=0x37, DIK_LMENU=0x38, DIK_SPACE=0x39,
  DIK_CAPITAL=0x3A, DIK_F1=0x3B, DIK_F2=0x3C, DIK_F3=0x3D, DIK_F4=0x3E,
  DIK_F5=0x3F, DIK_F6=0x40, DIK_F7=0x41, DIK_F8=0x42, DIK_F9=0x43,
  DIK_F10=0x44, DIK_NUMLOCK=0x45, DIK_SCROLL=0x46, DIK_NUMPAD7=0x47,
  DIK_NUMPAD8=0x48, DIK_NUMPAD9=0x49, DIK_SUBTRACT=0x4A, DIK_NUMPAD4=0x4B,
  DIK_NUMPAD5=0x4C, DIK_NUMPAD6=0x4D, DIK_ADD=0x4E, DIK_NUMPAD1=0x4F,
  DIK_NUMPAD2=0x50, DIK_NUMPAD3=0x51, DIK_NUMPAD0=0x52, DIK_DECIMAL=0x53,
  DIK_F11=0x57, DIK_F12=0x58, DIK_F13=0x64, DIK_F14=0x65, DIK_F15=0x66,
  DIK_KANA=0x70, DIK_CONVERT=0x79, DIK_NOCONVERT=0x7B, DIK_YEN=0x7D,
  DIK_NUMPADEQUALS=0x8D, DIK_CIRCUMFLEX=0x90, DIK_AT=0x91, DIK_COLON=0x92,
  DIK_UNDERLINE=0x93, DIK_KANJI=0x94, DIK_STOP=0x95, DIK_AX=0x96,
  DIK_UNLABELED=0x97, DIK_NUMPADENTER=0x9C, DIK_RCONTROL=0x9D,
  DIK_NUMPADCOMMA=0xB3, DIK_DIVIDE=0xB5, DIK_SYSRQ=0xB7, DIK_RMENU=0xB8,
  DIK_PAUSE=0xC5, DIK_HOME=0xC7, DIK_UP=0xC8, DIK_PRIOR=0xC9,
  DIK_LEFT=0xCB, DIK_RIGHT=0xCD, DIK_END=0xCF, DIK_DOWN=0xD0,
  DIK_NEXT=0xD1, DIK_INSERT=0xD2, DIK_DELETE=0xD3, DIK_LWIN=0xDB,
  DIK_RWIN=0xDC, DIK_APPS=0xDD, DIK_POWER=0xDE, DIK_SLEEP=0xDF,
};

namespace directx {

enum { DI_MOUSE_LEFT=0, DI_MOUSE_RIGHT=1, DI_MOUSE_MIDDLE=2 };
enum { KEY_FREE=0, KEY_PUSH=1, KEY_PULL=2, KEY_HOLD=3 };

class DirectInput {
  static DirectInput* thisBase_;
public:
  enum { MAX_JOYPAD=4, MAX_KEY=256, MAX_MOUSE_BUTTON=4, MAX_PAD_BUTTON=16 };
  DirectInput();
  virtual ~DirectInput();
  static DirectInput* GetBase() { return thisBase_; }
  virtual bool Initialize(HWND hWnd);
  virtual void Update();
  int GetKeyState(int key);
  int GetMouseState(int button);
  int GetPadState(int padNo, int button);
  int GetMouseMoveX() { return mouseMoveX_; }
  int GetMouseMoveY() { return mouseMoveY_; }
  int GetMouseMoveZ() { return mouseMoveZ_; }
  POINT GetMousePosition();
  void ResetInputState();
  void ResetMouseState();
  void ResetKeyState();
  void ResetPadState();
  int GetPadDeviceCount() { return static_cast<int>(bufPad_.size()); }
#ifdef __EMSCRIPTEN__
  void SetInjectedKeyState(int key, bool down);
  void SetDirectTouchKeyState(int key, bool down);
  void PulseInjectedKey(int key);
  void ClearInjectedKeyState();
  void ClearDirectTouchKeyState();
  bool IsNonTouchKeyDown(int key) const { return key >= 0 && key < MAX_KEY && bNonTouchKeyState_[key]; }
  bool IsWindowFocused();
  bool _IdleKeyboard();
  bool _IdleMouse();
  bool _IdleJoypad();
  int _GetKey(UINT code, int state);
  int _GetMouseButton(int button, int state);
#endif
protected:
  HWND hWnd_;
  int mouseMoveX_ = 0;
  int mouseMoveY_ = 0;
  int mouseMoveZ_ = 0;
  int bufKey_[MAX_KEY]{};
  int bufMouse_[MAX_MOUSE_BUTTON]{};
  std::vector<std::vector<int>> bufPad_;
  int _GetStateSub(bool flag, int state);
#ifdef __EMSCRIPTEN__
  bool bKeyState_[MAX_KEY]{};
  bool bKeyPressed_[MAX_KEY]{};
  bool bInjectedKeyState_[MAX_KEY]{};
  bool bDirectTouchKeyState_[MAX_KEY]{};
  bool bPulseKeyState_[MAX_KEY]{};
  bool bNonTouchKeyState_[MAX_KEY]{};
  BYTE injectedPulseFrames_[MAX_KEY]{};
  bool bMouseButton_[MAX_MOUSE_BUTTON]{};
  Uint32 dwFrameTick_ = 0;
  static int SDLScancodeToDIK(SDL_Scancode sc);
  static SDL_Scancode DIKToSDLScancode(int dik);
#endif
};

class VirtualKeyManager;
class VirtualKey {
  friend VirtualKeyManager;
public:
  VirtualKey(int keyboard, int padIndex, int padButton);
  virtual ~VirtualKey();
  int GetKeyState() { return state_; }
  void SetKeyState(int state) { state_ = state; }
  int GetKeyCode() { return keyboard_; }
  void SetKeyCode(int code) { keyboard_ = code; }
  int GetPadIndex() { return padIndex_; }
  void SetPadIndex(int index) { padIndex_ = index; }
  int GetPadButton() { return padButton_; }
  void SetPadButton(int button) { padButton_ = button; }
private:
  int keyboard_;
  int padIndex_;
  int padButton_;
  int state_;
};

class VirtualKeyManager : public DirectInput {
public:
  VirtualKeyManager();
  ~VirtualKeyManager();
  virtual void Update();
  void ClearKeyState();
  void AddKeyMap(int id, gstd::ref_count_ptr<VirtualKey> key) { mapKey_[id] = key; }
  void DeleteKeyMap(int id) { mapKey_.erase(id); }
  void ClearKeyMap() { mapKey_.clear(); }
  int GetVirtualKeyState(int id);
  gstd::ref_count_ptr<VirtualKey> GetVirtualKey(int id);
  bool IsTargetKeyCode(int key);
protected:
  std::map<int, gstd::ref_count_ptr<VirtualKey>> mapKey_;
  int _GetVirtualKeyState(int id);
};

class KeyReplayManager {
#ifdef WOO_REPLAY_VERIFY
  friend struct ::woo::verify::Access;
#endif
public:
  enum { STATE_RECORD, STATE_REPLAY };
  KeyReplayManager(VirtualKeyManager* input);
  virtual ~KeyReplayManager();
  void SetManageState(int state) { state_ = state; }
  void AddTarget(int key);
  bool IsTargetKeyCode(int key);
  void Update();
  void ReadRecord(gstd::RecordBuffer& record);
  void WriteRecord(gstd::RecordBuffer& record);
protected:
  struct ReplayData { int id_; int frame_; int state_; };
  int state_;
  int frame_;
  std::map<int, int> mapLastKeyState_;
  std::list<int> listTarget_;
  std::list<ReplayData> listReplayData_;
  VirtualKeyManager* input_;
};

} // namespace directx

#endif

