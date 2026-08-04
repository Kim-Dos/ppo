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

        auto bridge = std::make_unique<NetworkBridge>();
        TCPC tcpClient(ioservice, *bridge);
        tcpClient.Connect("127.0.0.1", SERVERPORT);

        // 예외로 스택이 풀려도 반드시 join되도록 RAII로 감싼다
        std::thread ioThread([&ioservice]() {
            try { ioservice.run(); }
            catch (const std::exception& e) {
                std::cout << "[ioThread] exception: " << e.what() << std::endl;
            }
            });

        struct ThreadJoiner {                       // 스코프 탈출 시 무조건 정리
            boost::asio::io_context& ioc;
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type>& wg;
            std::thread& t;
            ~ThreadJoiner() {
                wg.reset();
                ioc.stop();
                if (t.joinable()) t.join();
            }
        } joiner{ ioservice, work, ioThread };

        DummyApp theApp(hInstance, bridge.get());

        if (!theApp.Initialize())
            return 0;                               // joiner가 알아서 정리

        return theApp.Run();                        // 예외가 나도 joiner가 join
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}