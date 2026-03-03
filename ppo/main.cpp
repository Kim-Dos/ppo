// ppo.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//


#include "Resource.h"
#include "DummyApp.h"


#include <boost/asio.hpp>
#include <thread>

#ifdef _DEBUG
#ifdef UNICODE
#pragma comment(linker, "/entry:wWinMainCRTStartup /subsystem:console")
#else
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#endif
#endif


int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR    lpCmdLine,
                      int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        boost::asio::io_context ioservice;

		auto work = boost::asio::make_work_guard(ioservice);
		std::thread ioThread([&ioservice]() { ioservice.run(); });

        DummyApp theApp(hInstance, std::ref(ioservice));

        if (!theApp.Initialize()) {
            work.reset();
            ioservice.stop();
            if (ioThread.joinable()) { ioThread.join(); }
			return 0;
        }

		int ret = theApp.Run();

        work.reset();
        ioservice.stop();
        if (ioThread.joinable()) { ioThread.join(); }

        return ret;
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}
