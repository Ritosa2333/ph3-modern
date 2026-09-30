#include "DirectInput.hpp"

using namespace gstd;
using namespace directx;

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#include <cmath>
#endif

/**********************************************************
//DirectInput
**********************************************************/
DirectInput* DirectInput::thisBase_ = NULL;

#ifdef __EMSCRIPTEN__

DirectInput::DirectInput()
{
	hWnd_ = NULL;
	mouseMoveX_ = 0;
	mouseMoveY_ = 0;
	mouseMoveZ_ = 0;
	dwFrameTick_ = 0;
	for (int i = 0; i < MAX_KEY; i++) {
		bufKey_[i] = KEY_FREE;
		bKeyState_[i] = false;
		bKeyPressed_[i] = false;
	}
	for (int i = 0; i < MAX_MOUSE_BUTTON; i++) {
		bufMouse_[i] = KEY_FREE;
		bMouseButton_[i] = false;
	}
}

DirectInput::~DirectInput()
{
	Logger::WriteTop(L"DirectInput：終了開始");
	thisBase_ = NULL;
	Logger::WriteTop(L"DirectInput：終了完了");
}

bool DirectInput::Initialize(HWND hWnd)
{
	if (thisBase_ != NULL)
		return false;
	Logger::WriteTop(L"DirectInput：初期化");
	hWnd_ = hWnd;
	thisBase_ = this;
	Logger::WriteTop(L"DirectInput：初期化完了");
	return true;
}

int DirectInput::SDLScancodeToDIK(SDL_Scancode sc)
{
	switch (sc) {
	case SDL_SCANCODE_A: return DIK_A;
	case SDL_SCANCODE_B: return DIK_B;
	case SDL_SCANCODE_C: return DIK_C;
	case SDL_SCANCODE_D: return DIK_D;
	case SDL_SCANCODE_E: return DIK_E;
	case SDL_SCANCODE_F: return DIK_F;
	case SDL_SCANCODE_G: return DIK_G;
	case SDL_SCANCODE_H: return DIK_H;
	case SDL_SCANCODE_I: return DIK_I;
	case SDL_SCANCODE_J: return DIK_J;
	case SDL_SCANCODE_K: return DIK_K;
	case SDL_SCANCODE_L: return DIK_L;
	case SDL_SCANCODE_M: return DIK_M;
	case SDL_SCANCODE_N: return DIK_N;
	case SDL_SCANCODE_O: return DIK_O;
	case SDL_SCANCODE_P: return DIK_P;
	case SDL_SCANCODE_Q: return DIK_Q;
	case SDL_SCANCODE_R: return DIK_R;
	case SDL_SCANCODE_S: return DIK_S;
	case SDL_SCANCODE_T: return DIK_T;
	case SDL_SCANCODE_U: return DIK_U;
	case SDL_SCANCODE_V: return DIK_V;
	case SDL_SCANCODE_W: return DIK_W;
	case SDL_SCANCODE_X: return DIK_X;
	case SDL_SCANCODE_Y: return DIK_Y;
	case SDL_SCANCODE_Z: return DIK_Z;
	case SDL_SCANCODE_1: return DIK_1;
	case SDL_SCANCODE_2: return DIK_2;
	case SDL_SCANCODE_3: return DIK_3;
	case SDL_SCANCODE_4: return DIK_4;
	case SDL_SCANCODE_5: return DIK_5;
	case SDL_SCANCODE_6: return DIK_6;
	case SDL_SCANCODE_7: return DIK_7;
	case SDL_SCANCODE_8: return DIK_8;
	case SDL_SCANCODE_9: return DIK_9;
	case SDL_SCANCODE_0: return DIK_0;
	case SDL_SCANCODE_ESCAPE: return DIK_ESCAPE;
	case SDL_SCANCODE_RETURN: return DIK_RETURN;
	case SDL_SCANCODE_SPACE: return DIK_SPACE;
	case SDL_SCANCODE_BACKSPACE: return DIK_BACK;
	case SDL_SCANCODE_TAB: return DIK_TAB;
	case SDL_SCANCODE_LEFT: return DIK_LEFT;
	case SDL_SCANCODE_RIGHT: return DIK_RIGHT;
	case SDL_SCANCODE_UP: return DIK_UP;
	case SDL_SCANCODE_DOWN: return DIK_DOWN;
	case SDL_SCANCODE_LCTRL: return DIK_LCONTROL;
	case SDL_SCANCODE_RCTRL: return DIK_RCONTROL;
	case SDL_SCANCODE_LSHIFT: return DIK_LSHIFT;
	case SDL_SCANCODE_RSHIFT: return DIK_RSHIFT;
	case SDL_SCANCODE_LALT: return DIK_LMENU;
	case SDL_SCANCODE_RALT: return DIK_RMENU;
	case SDL_SCANCODE_F1: return DIK_F1;
	case SDL_SCANCODE_F2: return DIK_F2;
	case SDL_SCANCODE_F3: return DIK_F3;
	case SDL_SCANCODE_F4: return DIK_F4;
	case SDL_SCANCODE_F5: return DIK_F5;
	case SDL_SCANCODE_F6: return DIK_F6;
	case SDL_SCANCODE_F7: return DIK_F7;
	case SDL_SCANCODE_F8: return DIK_F8;
	case SDL_SCANCODE_F9: return DIK_F9;
	case SDL_SCANCODE_F10: return DIK_F10;
	case SDL_SCANCODE_F11: return DIK_F11;
	case SDL_SCANCODE_F12: return DIK_F12;
	case SDL_SCANCODE_NUMLOCKCLEAR: return DIK_NUMLOCK;
	case SDL_SCANCODE_SCROLLLOCK: return DIK_SCROLL;
	case SDL_SCANCODE_HOME: return DIK_HOME;
	case SDL_SCANCODE_END: return DIK_END;
	case SDL_SCANCODE_PAGEUP: return DIK_PRIOR;
	case SDL_SCANCODE_PAGEDOWN: return DIK_NEXT;
	case SDL_SCANCODE_INSERT: return DIK_INSERT;
	case SDL_SCANCODE_DELETE: return DIK_DELETE;
	case SDL_SCANCODE_MINUS: return DIK_MINUS;
	case SDL_SCANCODE_EQUALS: return DIK_EQUALS;
	case SDL_SCANCODE_LEFTBRACKET: return DIK_LBRACKET;
	case SDL_SCANCODE_RIGHTBRACKET: return DIK_RBRACKET;
	case SDL_SCANCODE_BACKSLASH: return DIK_BACKSLASH;
	case SDL_SCANCODE_SEMICOLON: return DIK_SEMICOLON;
	case SDL_SCANCODE_APOSTROPHE: return DIK_APOSTROPHE;
	case SDL_SCANCODE_GRAVE: return DIK_GRAVE;
	case SDL_SCANCODE_SLASH: return DIK_SLASH;
	case SDL_SCANCODE_PERIOD: return DIK_PERIOD;
	case SDL_SCANCODE_COMMA: return DIK_COMMA;
	case SDL_SCANCODE_APPLICATION: return DIK_APPS;
	case SDL_SCANCODE_KP_0: return DIK_NUMPAD0;
	case SDL_SCANCODE_KP_1: return DIK_NUMPAD1;
	case SDL_SCANCODE_KP_2: return DIK_NUMPAD2;
	case SDL_SCANCODE_KP_3: return DIK_NUMPAD3;
	case SDL_SCANCODE_KP_4: return DIK_NUMPAD4;
	case SDL_SCANCODE_KP_5: return DIK_NUMPAD5;
	case SDL_SCANCODE_KP_6: return DIK_NUMPAD6;
	case SDL_SCANCODE_KP_7: return DIK_NUMPAD7;
	case SDL_SCANCODE_KP_8: return DIK_NUMPAD8;
	case SDL_SCANCODE_KP_9: return DIK_NUMPAD9;
	case SDL_SCANCODE_KP_ENTER: return DIK_NUMPADENTER;
	case SDL_SCANCODE_KP_MINUS: return DIK_SUBTRACT;
	case SDL_SCANCODE_KP_PLUS: return DIK_ADD;
	case SDL_SCANCODE_KP_PERIOD: return DIK_DECIMAL;
	case SDL_SCANCODE_KP_DIVIDE: return DIK_DIVIDE;
	case SDL_SCANCODE_KP_MULTIPLY: return DIK_MULTIPLY;
	case SDL_SCANCODE_PAUSE: return DIK_PAUSE;
	case SDL_SCANCODE_CAPSLOCK: return DIK_CAPITAL;
	default: return -1;
	}
}

SDL_Scancode DirectInput::DIKToSDLScancode(int dik)
{
	switch (dik) {
	case DIK_A: return SDL_SCANCODE_A;
	case DIK_B: return SDL_SCANCODE_B;
	case DIK_C: return SDL_SCANCODE_C;
	case DIK_D: return SDL_SCANCODE_D;
	case DIK_E: return SDL_SCANCODE_E;
	case DIK_F: return SDL_SCANCODE_F;
	case DIK_G: return SDL_SCANCODE_G;
	case DIK_H: return SDL_SCANCODE_H;
	case DIK_I: return SDL_SCANCODE_I;
	case DIK_J: return SDL_SCANCODE_J;
	case DIK_K: return SDL_SCANCODE_K;
	case DIK_L: return SDL_SCANCODE_L;
	case DIK_M: return SDL_SCANCODE_M;
	case DIK_N: return SDL_SCANCODE_N;
	case DIK_O: return SDL_SCANCODE_O;
	case DIK_P: return SDL_SCANCODE_P;
	case DIK_Q: return SDL_SCANCODE_Q;
	case DIK_R: return SDL_SCANCODE_R;
	case DIK_S: return SDL_SCANCODE_S;
	case DIK_T: return SDL_SCANCODE_T;
	case DIK_U: return SDL_SCANCODE_U;
	case DIK_V: return SDL_SCANCODE_V;
	case DIK_W: return SDL_SCANCODE_W;
	case DIK_X: return SDL_SCANCODE_X;
	case DIK_Y: return SDL_SCANCODE_Y;
	case DIK_Z: return SDL_SCANCODE_Z;
	case DIK_1: return SDL_SCANCODE_1;
	case DIK_2: return SDL_SCANCODE_2;
	case DIK_3: return SDL_SCANCODE_3;
	case DIK_4: return SDL_SCANCODE_4;
	case DIK_5: return SDL_SCANCODE_5;
	case DIK_6: return SDL_SCANCODE_6;
	case DIK_7: return SDL_SCANCODE_7;
	case DIK_8: return SDL_SCANCODE_8;
	case DIK_9: return SDL_SCANCODE_9;
	case DIK_0: return SDL_SCANCODE_0;
	case DIK_ESCAPE: return SDL_SCANCODE_ESCAPE;
	case DIK_RETURN: return SDL_SCANCODE_RETURN;
	case DIK_SPACE: return SDL_SCANCODE_SPACE;
	case DIK_BACK: return SDL_SCANCODE_BACKSPACE;
	case DIK_TAB: return SDL_SCANCODE_TAB;
	case DIK_LEFT: return SDL_SCANCODE_LEFT;
	case DIK_RIGHT: return SDL_SCANCODE_RIGHT;
	case DIK_UP: return SDL_SCANCODE_UP;
	case DIK_DOWN: return SDL_SCANCODE_DOWN;
	case DIK_LCONTROL: return SDL_SCANCODE_LCTRL;
	case DIK_RCONTROL: return SDL_SCANCODE_RCTRL;
	case DIK_LSHIFT: return SDL_SCANCODE_LSHIFT;
	case DIK_RSHIFT: return SDL_SCANCODE_RSHIFT;
	case DIK_LMENU: return SDL_SCANCODE_LALT;
	case DIK_RMENU: return SDL_SCANCODE_RALT;
	case DIK_F1: return SDL_SCANCODE_F1;
	case DIK_F2: return SDL_SCANCODE_F2;
	case DIK_F3: return SDL_SCANCODE_F3;
	case DIK_F4: return SDL_SCANCODE_F4;
	case DIK_F5: return SDL_SCANCODE_F5;
	case DIK_F6: return SDL_SCANCODE_F6;
	case DIK_F7: return SDL_SCANCODE_F7;
	case DIK_F8: return SDL_SCANCODE_F8;
	case DIK_F9: return SDL_SCANCODE_F9;
	case DIK_F10: return SDL_SCANCODE_F10;
	case DIK_F11: return SDL_SCANCODE_F11;
	case DIK_F12: return SDL_SCANCODE_F12;
	case DIK_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
	case DIK_SCROLL: return SDL_SCANCODE_SCROLLLOCK;
	case DIK_HOME: return SDL_SCANCODE_HOME;
	case DIK_END: return SDL_SCANCODE_END;
	case DIK_PRIOR: return SDL_SCANCODE_PAGEUP;
	case DIK_NEXT: return SDL_SCANCODE_PAGEDOWN;
	case DIK_INSERT: return SDL_SCANCODE_INSERT;
	case DIK_DELETE: return SDL_SCANCODE_DELETE;
	case DIK_MINUS: return SDL_SCANCODE_MINUS;
	case DIK_EQUALS: return SDL_SCANCODE_EQUALS;
	case DIK_LBRACKET: return SDL_SCANCODE_LEFTBRACKET;
	case DIK_RBRACKET: return SDL_SCANCODE_RIGHTBRACKET;
	case DIK_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
	case DIK_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
	case DIK_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
	case DIK_GRAVE: return SDL_SCANCODE_GRAVE;
	case DIK_SLASH: return SDL_SCANCODE_SLASH;
	case DIK_PERIOD: return SDL_SCANCODE_PERIOD;
	case DIK_COMMA: return SDL_SCANCODE_COMMA;
	case DIK_APPS: return SDL_SCANCODE_APPLICATION;
	case DIK_NUMPAD0: return SDL_SCANCODE_KP_0;
	case DIK_NUMPAD1: return SDL_SCANCODE_KP_1;
	case DIK_NUMPAD2: return SDL_SCANCODE_KP_2;
	case DIK_NUMPAD3: return SDL_SCANCODE_KP_3;
	case DIK_NUMPAD4: return SDL_SCANCODE_KP_4;
	case DIK_NUMPAD5: return SDL_SCANCODE_KP_5;
	case DIK_NUMPAD6: return SDL_SCANCODE_KP_6;
	case DIK_NUMPAD7: return SDL_SCANCODE_KP_7;
	case DIK_NUMPAD8: return SDL_SCANCODE_KP_8;
	case DIK_NUMPAD9: return SDL_SCANCODE_KP_9;
	case DIK_NUMPADENTER: return SDL_SCANCODE_KP_ENTER;
	case DIK_SUBTRACT: return SDL_SCANCODE_KP_MINUS;
	case DIK_ADD: return SDL_SCANCODE_KP_PLUS;
	case DIK_DECIMAL: return SDL_SCANCODE_KP_PERIOD;
	case DIK_DIVIDE: return SDL_SCANCODE_KP_DIVIDE;
	case DIK_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
	case DIK_PAUSE: return SDL_SCANCODE_PAUSE;
	case DIK_CAPITAL: return SDL_SCANCODE_CAPSLOCK;
	default: return SDL_SCANCODE_UNKNOWN;
	}
}

bool DirectInput::_IdleKeyboard()
{
	if (hWnd_ == NULL) return false;
	const bool* keyState = SDL_GetKeyboardState(NULL);
	if (!keyState) return false;
	for (int i = 0; i < MAX_KEY; i++) {
		SDL_Scancode sc = DIKToSDLScancode(i);
		if (sc == SDL_SCANCODE_UNKNOWN) continue;
		bool pressed = keyState[sc] != 0;
		if (pressed && !bKeyState_[i]) {
			bKeyPressed_[i] = true;
		} else if (!pressed && bKeyState_[i]) {
			bKeyPressed_[i] = false;
		}
		bKeyState_[i] = pressed;
	}
	return true;
}

bool DirectInput::_IdleMouse()
{
	if (hWnd_ == NULL) return false;
	float x, y;
	SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
	for (int i = 0; i < 3; i++) {
		bool pressed = false;
		switch (i) {
		case 0: pressed = (buttons & SDL_BUTTON_LMASK) != 0; break;
		case 1: pressed = (buttons & SDL_BUTTON_RMASK) != 0; break;
		case 2: pressed = (buttons & SDL_BUTTON_MMASK) != 0; break;
		}
		if (pressed && !bMouseButton_[i]) {
			bKeyPressed_[i] = true;
		} else if (!pressed && bMouseButton_[i]) {
			bKeyPressed_[i] = false;
		}
		bMouseButton_[i] = pressed;
	}
	return true;
}

bool DirectInput::_IdleJoypad()
{
	return true;
}

int DirectInput::_GetKey(UINT code, int state)
{
	if (code < 0 || code >= MAX_KEY) return KEY_FREE;
	return _GetStateSub(bKeyState_[code], state);
}

int DirectInput::_GetMouseButton(int button, int state)
{
	if (button < 0 || button >= MAX_MOUSE_BUTTON) return KEY_FREE;
	return _GetStateSub(bMouseButton_[button], state);
}

int DirectInput::_GetStateSub(bool flag, int state)
{
	int res = KEY_FREE;
	if (flag) {
		if (state == KEY_FREE)
			res = KEY_PUSH;
		else
			res = KEY_HOLD;
	} else {
		if (state == KEY_PUSH || state == KEY_HOLD)
			res = KEY_PULL;
		else
			res = KEY_FREE;
	}
	return res;
}

POINT DirectInput::GetMousePosition()
{
	POINT res = { 0, 0 };
	float x, y;
	SDL_GetMouseState(&x, &y);
	res.x = (int)x;
	res.y = (int)y;
	return res;
}

void DirectInput::Update()
{
	// DirectInput's DIMOUSESTATE lX/lY/lZ are per-update deltas, not the
	// absolute cursor position. Accumulate SDL events for this logical input
	// update and reset them on every call, matching the native polling owner.
	float deltaX = 0.0f;
	float deltaY = 0.0f;
	float deltaWheel = 0.0f;
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) ResetInputState();
		else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			deltaX += event.motion.xrel;
			deltaY += event.motion.yrel;
		} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			float wheel = event.wheel.y;
			if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) wheel = -wheel;
			deltaWheel += wheel;
		}
	}
	mouseMoveX_ = static_cast<int>(std::lround(deltaX));
	mouseMoveY_ = static_cast<int>(std::lround(deltaY));
	// Win32 DirectInput reports the conventional wheel detent as 120 units.
	mouseMoveZ_ = static_cast<int>(std::lround(deltaWheel * 120.0f));
	this->_IdleKeyboard();
	this->_IdleJoypad();
	this->_IdleMouse();

	for (int iKey = 0; iKey < 255; iKey++)
		bufKey_[iKey] = _GetKey(iKey, bufKey_[iKey]);

	for (int iButton = 0; iButton < 3; iButton++)
		bufMouse_[iButton] = _GetMouseButton(iButton, bufMouse_[iButton]);
}

void DirectInput::ResetInputState()
{
	ResetMouseState();
	ResetKeyState();
	ResetPadState();
}
void DirectInput::ResetMouseState()
{
	for (int iButton = 0; iButton < 3; iButton++)
		bufMouse_[iButton] = KEY_FREE;
	ZeroMemory(bMouseButton_, sizeof(bMouseButton_));
}
void DirectInput::ResetKeyState()
{
	for (int iKey = 0; iKey < MAX_KEY; iKey++)
		bufKey_[iKey] = KEY_FREE;
	ZeroMemory(bKeyState_, sizeof(bKeyState_));
}
void DirectInput::ResetPadState()
{
	for (int iPad = 0; iPad < bufPad_.size(); iPad++) {
		for (int iKey = 0; iKey < bufPad_.size(); iKey++)
			bufPad_[iPad][iKey] = KEY_FREE;
	}
}

bool DirectInput::IsWindowFocused()
{
	if (hWnd_ == NULL) return false;
	return SDL_GetWindowFlags(static_cast<SDL_Window*>(hWnd_)) & SDL_WINDOW_INPUT_FOCUS;
}
int DirectInput::GetKeyState(int key) { return key >= 0 && key < MAX_KEY ? bufKey_[key] : KEY_FREE; }
int DirectInput::GetMouseState(int button) { return button >= 0 && button < MAX_MOUSE_BUTTON ? bufMouse_[button] : KEY_FREE; }
int DirectInput::GetPadState(int pad,int button) {
	return pad >= 0 && pad < static_cast<int>(bufPad_.size()) && button >= 0 && button < static_cast<int>(bufPad_[pad].size()) ? bufPad_[pad][button] : KEY_FREE;
}

#else

DirectInput::DirectInput()
{
	hWnd_ = NULL;
	pInput_ = NULL;
	pKeyboard_ = NULL;
	pMouse_ = NULL;
}
DirectInput::~DirectInput()
{
	Logger::WriteTop(L"DirectInput：終了開始");
	for (int iPad = 0; iPad < pJoypad_.size(); iPad++) {
		if (pJoypad_[iPad] == NULL)
			continue;
		pJoypad_[iPad]->Unacquire();
		if (pJoypad_[iPad] != NULL)
			pJoypad_[iPad]->Release();
	}

	if (pMouse_ != NULL) {
		pMouse_->Unacquire();
		pMouse_->Release();
	}

	if (pKeyboard_ != NULL) {
		pKeyboard_->Unacquire();
		pKeyboard_->Release();
	}

	if (pInput_ != NULL)
		pInput_->Release();
	thisBase_ = NULL;
	Logger::WriteTop(L"DirectInput：終了完了");
}

bool DirectInput::Initialize(HWND hWnd)
{
	if (thisBase_ != NULL)
		return false;
	Logger::WriteTop(L"DirectInput：初期化");
	hWnd_ = hWnd;

	HINSTANCE hInst = ::GetModuleHandle(NULL);
	HRESULT hrInput = DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&pInput_, NULL);
	if (FAILED(hrInput)) {
		Logger::WriteTop(L"DirectInput：DirectInput8Create失敗");
		return false;
	}

	_InitializeKeyBoard();
	_InitializeMouse();
	_InitializeJoypad();

	bufPad_.resize(pJoypad_.size());
	for (int iPad = 0; iPad < pJoypad_.size(); iPad++) {
		bufPad_[iPad].resize(32);
	}

	thisBase_ = this;
	Logger::WriteTop(L"DirectInput：初期化完了");
	return true;
}

bool DirectInput::_InitializeKeyBoard()
{
	Logger::WriteTop(L"DirectIuput：キーボード初期化");

	HRESULT hrDevice = pInput_->CreateDevice(GUID_SysKeyboard, &pKeyboard_, NULL);
	if (FAILED(hrDevice)) {
		Logger::WriteTop(L"DirectInput：キーボードのデバイスオブジェクト作成失敗");
		return false;
	}

	HRESULT hrFormat = pKeyboard_->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hrFormat)) {
		Logger::WriteTop(L"DirectInput：キーボードのデータフォーマット設定失敗");
		return false;
	}

	HRESULT hrCoop = pKeyboard_->SetCooperativeLevel(hWnd_, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
	if (FAILED(hrCoop)) {
		Logger::WriteTop(L"DirectInput：キーボードの動作設定失敗");
		return false;
	}

	pKeyboard_->Acquire();

	Logger::WriteTop(L"DirectIuput：キーボード初期化完了");

	return true;
}
bool DirectInput::_InitializeMouse()
{
	Logger::WriteTop(L"DirectIuput：マウス初期化");

	HRESULT hrDevice = pInput_->CreateDevice(GUID_SysMouse, &pMouse_, NULL);
	if (FAILED(hrDevice)) {
		Logger::WriteTop(L"DirectInput：マウスのデバイスオブジェクト作成失敗");
		return false;
	}

	HRESULT hrFormat = pMouse_->SetDataFormat(&c_dfDIMouse);
	if (FAILED(hrFormat)) {
		Logger::WriteTop(L"DirectInput：マウスのデータフォーマット設定失敗");
		return false;
	}

	HRESULT hrCoop = pMouse_->SetCooperativeLevel(hWnd_, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
	if (FAILED(hrCoop)) {
		Logger::WriteTop(L"DirectInput：マウスの動作設定失敗");
		return false;
	}

	pMouse_->Acquire();

	Logger::WriteTop(L"DirectIuput：マウス初期化完了");
	return true;
}
bool DirectInput::_InitializeJoypad()
{
	Logger::WriteTop(L"DirectIuput：ジョイパッド初期化");
	pInput_->EnumDevices(DI8DEVCLASS_GAMECTRL, (LPDIENUMDEVICESCALLBACK)_GetJoypadStaticCallback, this, DIEDFL_ATTACHEDONLY);
	int count = pJoypad_.size();
	if (count == 0) {
		Logger::WriteTop(L"DirectIuput：ジョイパッドは見つかりませんでした");
		return false;
	}

	statePad_.resize(count);
	padRes_.resize(count);
	for (int iPad = 0; iPad < count; iPad++)
		padRes_[iPad] = 500;

	Logger::WriteTop(L"DirectIuput：ジョイパッド初期化完了");

	return true;
}
BOOL CALLBACK DirectInput::_GetJoypadStaticCallback(LPDIDEVICEINSTANCE lpddi, LPVOID pvRef)
{
	DirectInput* input = (DirectInput*)pvRef;
	return input->_GetJoypadCallback(lpddi);
}
BOOL DirectInput::_GetJoypadCallback(LPDIDEVICEINSTANCE lpddi)
{
	Logger::WriteTop(L"DirectInput：ジョイパッドを見つけました");
	LPDIRECTINPUTDEVICE8 pJoypad = NULL;
	HRESULT hrDevice = pInput_->CreateDevice(lpddi->guidInstance, &pJoypad, NULL);
	if (FAILED(hrDevice)) {
		Logger::WriteTop(L"DirectInput：入力装置のデバイスオブジェクト作成失敗");
		return DIENUM_CONTINUE;
	}

	{
		DIDEVICEINSTANCE State;
		ZeroMemory(&State, sizeof(State));
		State.dwSize = sizeof(State);
		pJoypad->GetDeviceInfo(&State);

		Logger::WriteTop(StringUtility::Format(L"デバイスの登録名:%s", State.tszInstanceName));
		Logger::WriteTop(StringUtility::Format(L"デバイスの製品登録名:%s", State.tszProductName));
	}

	HRESULT hrFormat = pJoypad->SetDataFormat(&c_dfDIJoystick);
	if (FAILED(hrFormat)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドのデータフォーマット設定失敗");
		return DIENUM_CONTINUE;
	}

	HRESULT hrCoop = pJoypad->SetCooperativeLevel(hWnd_, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
	if (FAILED(hrCoop)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドの動作設定失敗");
		return DIENUM_CONTINUE;
	}

	DIPROPRANGE diprg;
	diprg.diph.dwSize = sizeof(diprg);
	diprg.diph.dwHeaderSize = sizeof(diprg.diph);
	diprg.diph.dwObj = DIJOFS_X;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = -1000;
	diprg.lMax = +1000;
	HRESULT hrRangeX = pJoypad->SetProperty(DIPROP_RANGE, &diprg.diph);
	if (FAILED(hrRangeX)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのx軸関係の設定に失敗しました");
		return DIENUM_CONTINUE;
	}

	DIPROPRANGE diprgY;
	diprgY.diph.dwObj = DIJOFS_Y;
	HRESULT hrRangeY = pJoypad->SetProperty(DIPROP_RANGE, &diprgY.diph);
	if (FAILED(hrRangeY)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのy軸関係の設定に失敗しました");
		return DIENUM_CONTINUE;
	}

	DIPROPRANGE diprgZ;
	diprgZ.diph.dwObj = DIJOFS_Z;
	HRESULT hrRangeZ = pJoypad->SetProperty(DIPROP_RANGE, &diprgZ.diph);
	if (FAILED(hrRangeZ)) {
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのz軸関係の設定に失敗しました");
	}

	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof(dipdw);
	dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
	dipdw.diph.dwObj = DIJOFS_X;
	dipdw.diph.dwHow = DIPH_BYOFFSET;
	dipdw.dwData = 2500;
	HRESULT hrDeadX = pJoypad->SetProperty(DIPROP_DEADZONE, &dipdw.diph);
	if (FAILED(hrDeadX)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのx軸の無効ゾーンの設定に失敗しました");
		return DIENUM_CONTINUE;
	}

	DIPROPDWORD dipdwY;
	dipdwY.diph.dwObj = DIJOFS_Y;
	HRESULT hrDeadY = pJoypad->SetProperty(DIPROP_DEADZONE, &dipdwY.diph);
	if (FAILED(hrDeadY)) {
		if (pJoypad != NULL)
			pJoypad->Release();
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのy軸の無効ゾーンの設定に失敗しました");
		return DIENUM_CONTINUE;
	}

	DIPROPDWORD dipdwZ;
	dipdwZ.diph.dwObj = DIJOFS_Z;
	HRESULT hrDeadZ = pJoypad->SetProperty(DIPROP_DEADZONE, &dipdwZ.diph);
	if (FAILED(hrDeadZ)) {
		Logger::WriteTop(L"DirectInput：ジョイパッドデバイスのz軸の無効ゾーンの設定に失敗しました");
	}

	pJoypad->Acquire();

	pJoypad_.push_back(pJoypad);

	return DIENUM_CONTINUE;
}

int DirectInput::_GetKey(UINT code, int state)
{
	return _GetStateSub((stateKey_[code] & 0x80) == 0x80, state);
}
int DirectInput::_GetMouseButton(int button, int state)
{
	return _GetStateSub((stateMouse_.rgbButtons[button] & 0x80) == 0x80, state);
}
int DirectInput::_GetPadDirection(int index, UINT code, int state)
{
	if (index >= pJoypad_.size())
		return KEY_FREE;
	int response = padRes_[index];

	int res = KEY_FREE;

	switch (code) {
	case DIK_UP:
		res = _GetStateSub(statePad_[index].lY < -response, state);
		break;
	case DIK_DOWN:
		res = _GetStateSub(statePad_[index].lY > response, state);
		break;
	case DIK_LEFT:
		res = _GetStateSub(statePad_[index].lX < -response, state);
		break;
	case DIK_RIGHT:
		res = _GetStateSub(statePad_[index].lX > response, state);
		break;
	}

	return res;
}
int DirectInput::_GetPadButton(int index, int buttonNo, int state)
{
	return _GetStateSub((statePad_[index].rgbButtons[buttonNo] & 0x80) == 0x80, state);
}
int DirectInput::_GetStateSub(bool flag, int state)
{
	int res = KEY_FREE;
	if (flag) {
		if (state == KEY_FREE)
			res = KEY_PUSH;
		else
			res = KEY_HOLD;
	} else {
		if (state == KEY_PUSH || state == KEY_HOLD)
			res = KEY_PULL;
		else
			res = KEY_FREE;
	}
	return res;
}

bool DirectInput::_IdleKeyboard()
{
	if (!pInput_ || !pKeyboard_)
		return false;

	HRESULT hr = pKeyboard_->GetDeviceState(MAX_KEY, stateKey_);
	if (SUCCEEDED(hr)) {

	} else if (hr == DIERR_INPUTLOST) {
		pKeyboard_->Acquire();
	}
	return true;
}
bool DirectInput::_IdleJoypad()
{
	if (pJoypad_.size() == 0)
		return false;
	for (int iPad = 0; iPad < pJoypad_.size(); iPad++) {
		if (!pInput_ || !pJoypad_[iPad])
			return false;

		pJoypad_[iPad]->Poll();
		HRESULT hr = pJoypad_[iPad]->GetDeviceState(sizeof(DIJOYSTATE), &statePad_[iPad]);
		if (SUCCEEDED(hr)) {

		} else if (hr == DIERR_INPUTLOST) {
			pJoypad_[iPad]->Acquire();
		}
	}
	return true;
}
bool DirectInput::_IdleMouse()
{
	if (!pInput_ || !pMouse_)
		return false;

	HRESULT hr = pMouse_->GetDeviceState(sizeof(DIMOUSESTATE), &stateMouse_);
	if (SUCCEEDED(hr)) {

	} else if (hr == DIERR_INPUTLOST) {
		pMouse_->Acquire();
	}

	return true;
}

void DirectInput::Update()
{
	this->_IdleKeyboard();
	this->_IdleJoypad();
	this->_IdleMouse();

	for (int iKey = 0; iKey < 255; iKey++)
		bufKey_[iKey] = _GetKey(iKey, bufKey_[iKey]);

	for (int iButton = 0; iButton < 3; iButton++)
		bufMouse_[iButton] = _GetMouseButton(iButton, bufMouse_[iButton]);

	for (int iPad = 0; iPad < pJoypad_.size(); iPad++) {
		bufPad_[iPad][0] = _GetPadDirection(iPad, DIK_LEFT, bufPad_[iPad][0]);
		bufPad_[iPad][1] = _GetPadDirection(iPad, DIK_RIGHT, bufPad_[iPad][1]);
		bufPad_[iPad][2] = _GetPadDirection(iPad, DIK_UP, bufPad_[iPad][2]);
		bufPad_[iPad][3] = _GetPadDirection(iPad, DIK_DOWN, bufPad_[iPad][3]);

		for (int iButton = 0; iButton < MAX_PAD_BUTTON; iButton++)
			bufPad_[iPad][iButton + 4] = _GetPadButton(iPad, iButton, bufPad_[iPad][iButton + 4]);
	}
}
int DirectInput::GetKeyState(int key)
{
	if (key < 0 || key >= MAX_KEY)
		return KEY_FREE;
	return bufKey_[key];
}
int DirectInput::GetMouseState(int button)
{
	if (button < 0 || button >= MAX_MOUSE_BUTTON)
		return KEY_FREE;
	return bufMouse_[button];
}
int DirectInput::GetPadState(int padNo, int button)
{
	int res = KEY_FREE;
	if (padNo < bufPad_.size())
		res = bufPad_[padNo][button];
	return res;
}

POINT DirectInput::GetMousePosition()
{
	POINT res = { 0, 0 };
	GetCursorPos(&res);
	ScreenToClient(hWnd_, &res);
	return res;
}

void DirectInput::ResetInputState()
{
	ResetMouseState();
	ResetKeyState();
	ResetPadState();
}
void DirectInput::ResetMouseState()
{
	for (int iButton = 0; iButton < 3; iButton++)
		bufMouse_[iButton] = KEY_FREE;
	ZeroMemory(&stateMouse_, sizeof(stateMouse_));
}
void DirectInput::ResetKeyState()
{
	for (int iKey = 0; iKey < MAX_KEY; iKey++)
		bufKey_[iKey] = KEY_FREE;
	ZeroMemory(&stateKey_, sizeof(stateKey_));
}
void DirectInput::ResetPadState()
{
	for (int iPad = 0; iPad < bufPad_.size(); iPad++) {
		for (int iKey = 0; iKey < bufPad_.size(); iKey++)
			bufPad_[iPad][iKey] = KEY_FREE;
		statePad_[iPad].lX = 0;
		statePad_[iPad].lY = 0;
		for (int iButton = 0; iButton < MAX_PAD_BUTTON; iButton++) {
			statePad_[iPad].rgbButtons[iButton] = KEY_FREE;
		}
	}
}
DIDEVICEINSTANCE DirectInput::GetPadDeviceInformation(int padIndex)
{
	DIDEVICEINSTANCE state;
	ZeroMemory(&state, sizeof(state));
	state.dwSize = sizeof(state);
	pJoypad_[padIndex]->GetDeviceInfo(&state);
	return state;
}

#endif // __EMSCRIPTEN__

/**********************************************************
//VirtualKeyManager
**********************************************************/
//VirtualKey
VirtualKey::VirtualKey(int keyboard, int padIndex, int padButton)
{
	keyboard_ = keyboard;
	padIndex_ = padIndex;
	padButton_ = padButton;
	state_ = KEY_FREE;
}
VirtualKey::~VirtualKey()
{
}

//VirtualKeyManager
VirtualKeyManager::VirtualKeyManager()
{
}
VirtualKeyManager::~VirtualKeyManager()
{
}

void VirtualKeyManager::Update()
{
	DirectInput::Update();

	std::map<int, gstd::ref_count_ptr<VirtualKey>>::iterator itr = mapKey_.begin();
	for (; itr != mapKey_.end(); itr++) {
		int id = itr->first;
		gstd::ref_count_ptr<VirtualKey> key = itr->second;

		int state = _GetVirtualKeyState(id);
		key->SetKeyState(state);
	}
}
void VirtualKeyManager::ClearKeyState()
{
	DirectInput::ResetInputState();
	std::map<int, gstd::ref_count_ptr<VirtualKey>>::iterator itr = mapKey_.begin();
	for (; itr != mapKey_.end(); itr++) {
		gstd::ref_count_ptr<VirtualKey> key = itr->second;
		key->SetKeyState(KEY_FREE);
	}
}
int VirtualKeyManager::_GetVirtualKeyState(int id)
{
	if (mapKey_.find(id) == mapKey_.end())
		return KEY_FREE;

	gstd::ref_count_ptr<VirtualKey> key = mapKey_[id];

	int res = KEY_FREE;
	if (key->keyboard_ >= 0 && key->keyboard_ < MAX_KEY)
		res = bufKey_[key->keyboard_];
	if (res == KEY_FREE) {
		int indexPad = key->padIndex_;
		if (indexPad >= 0 && indexPad < bufPad_.size()) {
			if (key->padButton_ >= 0 && key->padButton_ < bufPad_[indexPad].size())
				res = bufPad_[indexPad][key->padButton_];
		}
	}

	return res;
}

int VirtualKeyManager::GetVirtualKeyState(int id)
{
	if (mapKey_.find(id) == mapKey_.end())
		return KEY_FREE;
	gstd::ref_count_ptr<VirtualKey> key = mapKey_[id];
	return key->GetKeyState();
}

gstd::ref_count_ptr<VirtualKey> VirtualKeyManager::GetVirtualKey(int id)
{
	if (mapKey_.find(id) == mapKey_.end())
		return NULL;
	return mapKey_[id];
}
bool VirtualKeyManager::IsTargetKeyCode(int key)
{
	bool res = false;
	std::map<int, gstd::ref_count_ptr<VirtualKey>>::iterator itr = mapKey_.begin();
	for (; itr != mapKey_.end(); itr++) {
		gstd::ref_count_ptr<VirtualKey> vKey = itr->second;
		int keyCode = vKey->GetKeyCode();
		if (key == keyCode) {
			res = true;
			break;
		}
	}
	return res;
}

/**********************************************************
//KeyReplayManager
**********************************************************/
KeyReplayManager::KeyReplayManager(VirtualKeyManager* input)
{
	frame_ = 0;
	input_ = input;
	state_ = STATE_RECORD;
}
KeyReplayManager::~KeyReplayManager()
{
}
void KeyReplayManager::AddTarget(int key)
{
	listTarget_.push_back(key);
	mapLastKeyState_[key] = KEY_FREE;
}
void KeyReplayManager::Update()
{
	if (state_ == STATE_RECORD) {
		std::list<int>::iterator itrTarget = listTarget_.begin();
		for (; itrTarget != listTarget_.end(); itrTarget++) {
			int idKey = *itrTarget;
			int keyState = input_->GetVirtualKeyState(idKey);
			bool bInsert = (frame_ == 0 || mapLastKeyState_[idKey] != keyState);
			if (bInsert) {
				ReplayData data;
				data.id_ = idKey;
				data.frame_ = frame_;
				data.state_ = keyState;
				listReplayData_.push_back(data);
			}
			mapLastKeyState_[idKey] = keyState;
		}
	} else if (state_ == STATE_REPLAY) {
		std::list<int>::iterator itrTarget = listTarget_.begin();
		for (; itrTarget != listTarget_.end(); itrTarget++) {
			int idKey = *itrTarget;
			std::list<ReplayData>::iterator itrData = listReplayData_.begin();
			for (; itrData != listReplayData_.end();) {
				ReplayData data = *itrData;
				if (data.frame_ > frame_)
					break;

				if (idKey == data.id_ && data.frame_ == frame_) {
					mapLastKeyState_[idKey] = data.state_;
					itrData = listReplayData_.erase(itrData);
				} else {
					itrData++;
				}
			}

			gstd::ref_count_ptr<VirtualKey> key = input_->GetVirtualKey(idKey);
			int lastKeyState = mapLastKeyState_[idKey];
			key->SetKeyState(lastKeyState);
		}
	}
	frame_++;
}
bool KeyReplayManager::IsTargetKeyCode(int key)
{
	bool res = false;
	std::list<int>::iterator itrTarget = listTarget_.begin();
	for (; itrTarget != listTarget_.end(); itrTarget++) {
		int idKey = *itrTarget;
		gstd::ref_count_ptr<VirtualKey> vKey = input_->GetVirtualKey(idKey);
		int keyCode = vKey->GetKeyCode();
		if (key == keyCode) {
			res = true;
			break;
		}
	}
	return res;
}
void KeyReplayManager::ReadRecord(gstd::RecordBuffer& record)
{
	int countReplayData = record.GetRecordAsInteger("count");

	ByteBuffer buffer;
	buffer.SetSize(sizeof(ReplayData) * countReplayData);
	record.GetRecord("data", buffer.GetPointer(), buffer.GetSize());

	for (int iRec = 0; iRec < countReplayData; iRec++) {
		ReplayData data;
		buffer.Read(&data, sizeof(ReplayData));
		listReplayData_.push_back(data);
	}
}
void KeyReplayManager::WriteRecord(gstd::RecordBuffer& record)
{
	int countReplayData = listReplayData_.size();
	record.SetRecordAsInteger("count", countReplayData);

	ByteBuffer buffer;
	std::list<ReplayData>::iterator itrData = listReplayData_.begin();
	for (; itrData != listReplayData_.end(); itrData++) {
		ReplayData data = *itrData;
		buffer.Write(&data, sizeof(ReplayData));
	}

	record.SetRecord("data", buffer.GetPointer(), buffer.GetSize());
}
