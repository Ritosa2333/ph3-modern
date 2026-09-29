#include"GcLibImpl.hpp"

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <emscripten/main_loop.h>
#endif

/**********************************************************
//Application entry
**********************************************************/
static void appMainLoop()
{
	EApplication* app = EApplication::GetInstance();
	if (app) {
		app->RunFrame();
	}
}

#ifdef __EMSCRIPTEN__
int main(int argc, char** argv)
{
	gstd::DebugUtility::DumpMemoryLeaksOnExit();
	try {
		DnhConfiguration* config = DnhConfiguration::CreateInstance();
		ELogger* logger = ELogger::CreateInstance();
		logger->Initialize(config->IsLogFile(), config->IsLogWindow());
		EPathProperty::CreateInstance();

		EApplication* app = EApplication::CreateInstance();
		app->Initialize();

		emscripten_set_main_loop(appMainLoop, 60, 1);
	} catch (std::exception& e) {
		std::wstring log = StringUtility::ConvertMultiToWide(e.what());
		Logger::WriteTop(log);
	} catch (gstd::wexception& e) {
		Logger::WriteTop(e.what());
	}
	return 0;
}
#else
int APIENTRY wWinMain(HINSTANCE hInstance,
                        HINSTANCE hPrevInstance,
                        LPWSTR lpCmdLine,
                        int nCmdShow )
{
	gstd::DebugUtility::DumpMemoryLeaksOnExit();
	try
	{
		DnhConfiguration* config = DnhConfiguration::CreateInstance();
		ELogger* logger = ELogger::CreateInstance();
		logger->Initialize(config->IsLogFile(), config->IsLogWindow());
		EPathProperty::CreateInstance();

		EApplication* app = EApplication::CreateInstance();
		app->Initialize();
		app->Run();
	}
	catch(std::exception& e)
	{
		std::wstring log = StringUtility::ConvertMultiToWide(e.what());
		Logger::WriteTop(log);
	}
	catch(gstd::wexception& e)
	{
		Logger::WriteTop(e.what());
	}
//	catch(...)
//	{
//		Logger::WriteTop("不明なエラー");
//	}

	EApplication::DeleteInstance();
	EPathProperty::DeleteInstance();
	ELogger::DeleteInstance();
	DnhConfiguration::DeleteInstance();

	return 0;
}
#endif
