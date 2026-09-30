#include "GcLibImpl.hpp"
#include "StgScene.hpp"
#include "System.hpp"

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <vector>
#ifdef PH3_PERF_SCAN
#include <emscripten/emscripten.h>
namespace {
void ReportPerfPhase(const char* phase,double ms){
	if(ms < 1000.0 / 60)return;
	EM_ASM({
		const d=globalThis.ph3Diagnostics;if(!d)return;
		const all=d.perfPhases||(d.perfPhases=({}));
		const name=UTF8ToString($0);
		const p=all[name]||(all[name]=({slow16:0,slow33:0,slow50:0,maxMs:0}));
		++p.slow16;if($1>=1000/30)++p.slow33;if($1>=50)++p.slow50;if($1>p.maxMs)p.maxMs=$1;
	},phase,ms);
}
}
#endif
#ifndef PH3_PERF_SCAN
namespace {
#endif
void PrewarmWebEffects(EShaderManager* shaderManager, EDirectGraphics* graphics){
	std::vector<std::string> sources;
	const std::filesystem::path root("/game/script/th_hsob");
	if(!std::filesystem::exists(root))return;
	for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
		if(!entry.is_regular_file()||entry.path().extension()!=".pfx")continue;
		auto source=entry.path();
		source.replace_extension();
		sources.push_back(source.generic_string());
	}
	std::sort(sources.begin(),sources.end());
	auto device=graphics->GetDevice();
	for(const auto& source:sources){
#ifdef PH3_PERF_SCAN
		const double begin=emscripten_get_now();
#endif
		auto path=StringUtility::ConvertMultiToWide(source,CP_UTF8);
		auto shader=shaderManager->CreateFromFile(path);
		if(shader==NULL||FAILED(device->WarmEffectProgram(shader->GetEffect())))
			throw std::runtime_error("Cannot prewarm Web effect: "+source);
		// CreateFromFile only guarantees ShaderData retention. Keep the Shader
		// itself in the manager too, otherwise its destructor releases the data
		// immediately and the first gameplay use links the same program again.
		shaderManager->AddShader(path,shader);
#ifdef PH3_PERF_SCAN
		const double elapsed=emscripten_get_now()-begin;
		EM_ASM({
			const d=globalThis.ph3Diagnostics;if(!d)return;
			const list=d.effectPrewarm||(d.effectPrewarm=[]);
			list.push(({path:UTF8ToString($0),ms:$1}));
		},source.c_str(),elapsed);
#endif
	}
}
#ifndef PH3_PERF_SCAN
}
#endif
#endif

/**********************************************************
//EApplication
**********************************************************/
EApplication::EApplication()
{
}
EApplication::~EApplication()
{
}
bool EApplication::_Initialize()
{
	ELogger* logger = ELogger::GetInstance();
	Logger::WriteTop(L"アプリケーション初期化");

	EFileManager* fileManager = EFileManager::CreateInstance();
	fileManager->Initialize();

	EFpsController* fpsController = EFpsController::CreateInstance();

	std::wstring appName = L"東方弾幕風 ph3 ";
	appName += DNH_VERSION;

	DnhConfiguration* config = DnhConfiguration::CreateInstance();
	std::wstring configWindowTitle = config->GetWindowTitle();
	if (configWindowTitle.size() > 0)
		appName = configWindowTitle;

	//マウス表示
	if (!config->IsMouseVisible())
		WindowUtility::SetMouseVisible(false);

	//DirectX初期化
	EDirectGraphics* graphics = EDirectGraphics::CreateInstance();
	graphics->Initialize();
	HWND hWndMain = graphics->GetWindowHandle();

#ifndef __EMSCRIPTEN__
	WindowLogger::InsertOpenCommandInSystemMenu(hWndMain);
	::SetWindowText(hWndMain, appName.c_str());
	::SetClassLong(hWndMain, GCL_HICON, (LONG)LoadIcon(GetApplicationHandle(), MAKEINTRESOURCE(IDI_ICON)));
#endif

	ErrorDialog::SetParentWindowHandle(hWndMain);

	ETextureManager* textureManager = ETextureManager::CreateInstance();
	textureManager->Initialize();

	EShaderManager* shaderManager = EShaderManager::CreateInstance();
	shaderManager->Initialize();

	EMeshManager* meshManager = EMeshManager::CreateInstance();
	meshManager->Initialize();

	EDxTextRenderer* textRenderer = EDxTextRenderer::CreateInstance();
	textRenderer->Initialize();

	EDirectSoundManager* soundManager = EDirectSoundManager::CreateInstance();
	soundManager->Initialize(hWndMain);

#ifdef __EMSCRIPTEN__
	// WebGL compiles/links Effect programs lazily. Pay that cost while the game
	// is still starting instead of on the first dialogue/transition draw.
	PrewarmWebEffects(shaderManager, graphics);
#endif

	EDirectInput* input = EDirectInput::CreateInstance();
	input->Initialize(hWndMain);

	ETaskManager* taskManager = ETaskManager::CreateInstance();
	taskManager->Initialize();

	gstd::ref_count_ptr<gstd::TaskInfoPanel> panelTask = new gstd::TaskInfoPanel();
	bool bAddTaskPanel = logger->AddPanel(panelTask, L"Task");
	if (bAddTaskPanel)
		taskManager->SetInfoPanel(panelTask);

	gstd::ref_count_ptr<directx::TextureInfoPanel> panelTexture = new directx::TextureInfoPanel();
	bool bTexturePanel = logger->AddPanel(panelTexture, L"Texture");
	if (bTexturePanel)
		textureManager->SetInfoPanel(panelTexture);

	gstd::ref_count_ptr<directx::DxMeshInfoPanel> panelMesh = new directx::DxMeshInfoPanel();
	bool bMeshPanel = logger->AddPanel(panelMesh, L"Mesh");
	if (bMeshPanel)
		meshManager->SetInfoPanel(panelMesh);

	gstd::ref_count_ptr<directx::SoundInfoPanel> panelSound = new directx::SoundInfoPanel();
	bool bSoundPanel = logger->AddPanel(panelSound, L"Sound");
	if (bSoundPanel)
		soundManager->SetInfoPanel(panelSound);

	gstd::ref_count_ptr<gstd::ScriptCommonDataInfoPanel> panelCommonData = logger->GetScriptCommonDataInfoPanel();
	logger->AddPanel(panelCommonData, L"Common Data");

	gstd::ref_count_ptr<ScriptInfoPanel> panelScript = new ScriptInfoPanel();
	logger->AddPanel(panelScript, L"Script");

	if (config->IsLogWindow()) {
		logger->LoadState();
		logger->SetWindowVisible(true);
	}

	SystemController* systemController = SystemController::CreateInstance();
	systemController->Reset();

	Logger::WriteTop(L"アプリケーション初期化完了");

	return true;
}
bool EApplication::_Loop()
{
#ifdef __EMSCRIPTEN__
	browserFrameWorked_ = false;
#endif
	ELogger* logger = ELogger::GetInstance();
	ETaskManager* taskManager = ETaskManager::GetInstance();
	EFpsController* fpsController = EFpsController::GetInstance();
	EDirectGraphics* graphics = EDirectGraphics::GetInstance();

	EDirectInput* input = EDirectInput::GetInstance();
#ifdef PH3_PERF_SCAN
	double perfBegin=emscripten_get_now();
#endif
	input->Update();
#ifdef PH3_PERF_SCAN
	ReportPerfPhase("input",emscripten_get_now()-perfBegin);
#endif
#ifdef __EMSCRIPTEN__
	SDL_Window* sdlFocused = SDL_GetKeyboardFocus();
	HWND hWndFocused = reinterpret_cast<HWND>(sdlFocused);
#else
	HWND hWndFocused = ::GetForegroundWindow();
#endif
	HWND hWndGraphics = graphics->GetWindowHandle();
	HWND hWndLogger = ELogger::GetInstance()->GetWindowHandle();
	if (hWndFocused != hWndGraphics && hWndFocused != hWndLogger) {
		return true;
	}
	if (input->GetKeyState(DIK_LCONTROL) == KEY_HOLD && input->GetKeyState(DIK_LSHIFT) == KEY_HOLD && input->GetKeyState(DIK_R) == KEY_PUSH) {
		//リセット
		SystemController* systemController = SystemController::CreateInstance();
		systemController->Reset();
	}

#ifdef __EMSCRIPTEN__
	browserFrameWorked_ = true;
#endif
#ifdef PH3_PERF_SCAN
	perfBegin=emscripten_get_now();
#endif
	taskManager->CallWorkFunction();
#ifdef PH3_PERF_SCAN
	ReportPerfPhase("work",emscripten_get_now()-perfBegin);
#endif

#ifdef __EMSCRIPTEN__
	if (browserRenderEnabled_) {
#else
	if (!fpsController->IsSkip()) {
#endif
#ifdef PH3_PERF_SCAN
		perfBegin=emscripten_get_now();
#endif
		graphics->BeginScene();
		taskManager->CallRenderFunction();
		graphics->EndScene();
#ifdef PH3_PERF_SCAN
		ReportPerfPhase("render",emscripten_get_now()-perfBegin);
#endif
	}

#ifdef __EMSCRIPTEN__
#ifdef PH3_PERF_SCAN
	perfBegin=emscripten_get_now();
#endif
	EDirectSoundManager::GetInstance()->Pump();
#ifdef PH3_PERF_SCAN
	ReportPerfPhase("audio",emscripten_get_now()-perfBegin);
#endif
#else
	fpsController->Wait();
#endif

	//ログ関連
#ifndef __EMSCRIPTEN__
	SYSTEMTIME time;
	GetLocalTime(&time);
	std::wstring fps = StringUtility::Format(L"Work：%.2ffps、Draw：%.2ffps",
		fpsController->GetCurrentWorkFps(),
		fpsController->GetCurrentRenderFps());
	logger->SetInfo(0, L"fps", fps);

	int widthConfig = graphics->GetConfigData().GetScreenWidth();
	int heightConfig = graphics->GetConfigData().GetScreenHeight();
	int widthScreen = widthConfig * graphics->GetScreenWidthRatio();
	int heightScreen = heightConfig * graphics->GetScreenHeightRatio();

	std::wstring screen = StringUtility::Format(L"width：%d/%d、height：%d/%d",
		widthScreen, widthConfig,
		heightScreen, heightConfig);
	logger->SetInfo(1, L"screen", screen);

	logger->SetInfo(2, L"font cache",
		StringUtility::Format(L"%d", EDxTextRenderer::GetInstance()->GetCacheCount()));
#endif

	//高速動作
	int fastModeKey = fpsController->GetFastModeKey();
	if (input->GetKeyState(fastModeKey) == KEY_HOLD) {
		if (!fpsController->IsFastMode())
			fpsController->SetFastMode(true);
	} else if (input->GetKeyState(fastModeKey) == KEY_PULL || input->GetKeyState(fastModeKey) == KEY_FREE) {
		if (fpsController->IsFastMode())
			fpsController->SetFastMode(false);
	}
	return true;
}
bool EApplication::RunFrame()
{
	return _Loop();
}
#ifdef __EMSCRIPTEN__
bool EApplication::RunFrame(bool render)
{
	browserRenderEnabled_ = render;
	return _Loop();
}
#endif
bool EApplication::_Finalize()
{
	Logger::WriteTop(L"アプリケーション終了処理開始");
	SystemController::DeleteInstance();
	ETaskManager::DeleteInstance();
	EFileManager::GetInstance()->EndLoadThread();
	EDirectInput::DeleteInstance();
	EDirectSoundManager::DeleteInstance();
	EDxTextRenderer::DeleteInstance();
	EMeshManager::DeleteInstance();
	EShaderManager::DeleteInstance();
	ETextureManager::DeleteInstance();
	EDirectGraphics::DeleteInstance();
	EFpsController::DeleteInstance();
	EFileManager::DeleteInstance();

	ELogger* logger = ELogger::GetInstance();
	logger->SaveState();

	Logger::WriteTop(L"アプリケーション終了処理完了");
	return true;
}

/**********************************************************
//EDirectGraphics
**********************************************************/
EDirectGraphics::EDirectGraphics()
{
}
EDirectGraphics::~EDirectGraphics()
{
}
bool EDirectGraphics::Initialize()
{
	DnhConfiguration* dnhConfig = DnhConfiguration::GetInstance();
	int screenWidth = dnhConfig->GetScreenWidth();
	int screenHeight = dnhConfig->GetScreenHeight();
	int screenMode = dnhConfig->GetScreenMode();
	int windowSize = dnhConfig->GetWindowSize();

	bool bUserSize = screenWidth != 640 || screenHeight != 480;
	if (!bUserSize) {
		if (windowSize == DnhConfiguration::WINDOW_SIZE_640x480 && screenWidth > 640)
			windowSize = DnhConfiguration::WINDOW_SIZE_800x600;
		if (windowSize == DnhConfiguration::WINDOW_SIZE_800x600 && screenWidth > 800)
			windowSize = DnhConfiguration::WINDOW_SIZE_960x720;
		if (windowSize == DnhConfiguration::WINDOW_SIZE_960x720 && screenWidth > 960)
			windowSize = DnhConfiguration::WINDOW_SIZE_1280x960;
		if (windowSize == DnhConfiguration::WINDOW_SIZE_1280x960 && screenWidth > 1280)
			windowSize = DnhConfiguration::WINDOW_SIZE_1600x1200;
		if (windowSize == DnhConfiguration::WINDOW_SIZE_1600x1200 && screenWidth > 1600)
			windowSize = DnhConfiguration::WINDOW_SIZE_1920x1200;
	}

	bool bShowWindow = screenMode == DirectGraphics::SCREENMODE_FULLSCREEN || windowSize == DnhConfiguration::WINDOW_SIZE_640x480;

	DirectGraphicsConfig dxConfig;
	dxConfig.SetScreenWidth(screenWidth);
	dxConfig.SetScreenHeight(screenHeight);
	dxConfig.SetShowWindow(bShowWindow);
	bool res = DirectGraphicsPrimaryWindow::Initialize(dxConfig);

#ifndef __EMSCRIPTEN__
	int wWidth = ::GetSystemMetrics(SM_CXFULLSCREEN);
	int wHeight = ::GetSystemMetrics(SM_CYFULLSCREEN);
	bool bFullScreenEnable = screenWidth <= wWidth && screenHeight <= wHeight;

	//コンフィグ反映
	if (screenMode == DirectGraphics::SCREENMODE_FULLSCREEN && bFullScreenEnable) {
		ChangeScreenMode();
	} else {
		if (windowSize != DnhConfiguration::WINDOW_SIZE_640x480 || bUserSize) {
			int width = screenWidth;
			int height = screenHeight;
			if (!bUserSize) {
				switch (windowSize) {
				case DnhConfiguration::WINDOW_SIZE_800x600:
					width = 800;
					height = 600; // + ::GetSystemMetrics(SM_CXEDGE) + 10 ; + ::GetSystemMetrics(SM_CXEDGE) + 10 ;
					break;
				case DnhConfiguration::WINDOW_SIZE_960x720:
					width = 960;
					height = 720; // + ::GetSystemMetrics(SM_CXEDGE) + 10 ; + ::GetSystemMetrics(SM_CXEDGE) + 10 ;
					break;
				case DnhConfiguration::WINDOW_SIZE_1280x960:
					width = 1280;
					height = 960; // + ::GetSystemMetrics(SM_CXEDGE) + 10 ; + ::GetSystemMetrics(SM_CXEDGE) + 10 ;
					break;
				case DnhConfiguration::WINDOW_SIZE_1600x1200:
					width = 1600;
					height = 1200; // + ::GetSystemMetrics(SM_CXEDGE) + 10 ; + ::GetSystemMetrics(SM_CXEDGE) + 10 ;
					break;
				case DnhConfiguration::WINDOW_SIZE_1920x1200:
					width = 1920;
					height = 1200; // + ::GetSystemMetrics(SM_CXEDGE) + 10 ; + ::GetSystemMetrics(SM_CXEDGE) + 10 ;
					break;
				}
			}

			double ratioWH = (double)screenWidth / (double)screenHeight;
			if (width > wWidth)
				width = wWidth;
			height = (double)width / ratioWH;

			double ratioHW = (double)screenHeight / (double)screenWidth;
			if (height > wHeight)
				height = wHeight;
			width = (double)height / ratioHW;

			int tw = ::GetSystemMetrics(SM_CXEDGE) + 10 + GetSystemMetrics(SM_CXBORDER) + GetSystemMetrics(SM_CXDLGFRAME);
			int th = ::GetSystemMetrics(SM_CYEDGE) + 10 + GetSystemMetrics(SM_CYBORDER) + GetSystemMetrics(SM_CYDLGFRAME) + GetSystemMetrics(SM_CYCAPTION);
			width += tw;
			height += th;

			SetBounds(0, 0, width, height);
			MoveWindowCenter();
		}
	}
#endif
	SetWindowVisible(true);

	return res;
}
void EDirectGraphics::SetRenderStateFor2D(int blend)
{
	DirectGraphics* graphics = DirectGraphics::GetBase();
	graphics->SetBlendMode(blend);
	graphics->SetZBufferEnable(false);
	graphics->SetZWriteEnalbe(false);
}
LRESULT EDirectGraphics::_WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifndef __EMSCRIPTEN__
	switch (uMsg) {
	case WM_SYSCOMMAND:
		int nId = wParam & 0xffff;
		if (nId == WindowLogger::MENU_ID_OPEN)
			ELogger::GetInstance()->ShowLogWindow();
		break;
	}
#endif
	return DirectGraphicsPrimaryWindow::_WindowProcedure(hWnd, uMsg, wParam, lParam);
}
