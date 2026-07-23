// ppo.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "Resource.h"
#include "DummyApp.h"
#include "NetworkBridge.h"
#include "../../Grad/GRClient/GRClient/TCPClient.hpp"

#include <boost/asio.hpp>
#include <thread>
#include <memory>

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

        auto bridge = std::make_unique< NetworkBridge>();

        TCPC tcpClient(ioservice, *bridge);
        tcpClient.Connect("127.0.0.1", SERVERPORT);
        // ↑ Connect 성공 시 TCPC 내부에서
        //   recv 루프 + SendQ 펌프(5ms)가 자동 시작됨.
        //   main에서 별도의 SendQ poll 타이머는 필요 없음.

        // io_context는 반드시 스레드 1개만! (spsc_queue 제약)
        std::thread ioThread([&ioservice]() { ioservice.run(); });

        DummyApp theApp(hInstance, bridge.get());

        if (!theApp.Initialize()) {
            work.reset();
            ioservice.stop();
            if (ioThread.joinable()) { ioThread.join(); }
            return 0;
        }

        // ── 메인스레드: Win32 메시지루프 + DX12 렌더링 ──
        int ret = theApp.Run();

        // ── 정리 ──
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