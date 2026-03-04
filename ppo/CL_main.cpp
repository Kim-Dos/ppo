// ppo.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "Resource.h"
#include "DummyApp.h"
#include "NetworkBridge.h"
#include "../../Grad/GRClient/GRClient/TCPClient.hpp"

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
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
        // ── 통신 인프라 (통신스레드 소유) ──
        boost::asio::io_context ioservice;
        auto work = boost::asio::make_work_guard(ioservice);

        auto bridge = std::make_unique<NetworkBridge>();
        TCPC tcpClient(ioservice, bridge.get());

        // ── 게임 앱 (메인스레드 소유) ──
        DummyApp theApp(hInstance, bridge.get());

        if (!theApp.Initialize()) {
            work.reset();
            ioservice.stop();
            return 0;
        }

        // ── SendQ poll 타이머: 통신스레드에서 주기적으로 SendQ → 실제 전송 ──
        std::function<void(boost::asio::steady_timer&)> pollSendQ;
        boost::asio::steady_timer sendTimer(ioservice);

        pollSendQ = [&](boost::asio::steady_timer& timer)
            {
                auto packets = bridge->DequeueSendAll();
                for (auto& pkt : packets)
                {
                    tcpClient.SendRaw(pkt.data, pkt.length);
                }

                timer.expires_after(std::chrono::milliseconds(16));
                timer.async_wait([&](boost::system::error_code ec) {
                    if (!ec) pollSendQ(timer);
                    });
            };

        sendTimer.expires_after(std::chrono::milliseconds(16));
        sendTimer.async_wait([&](boost::system::error_code ec) {
            if (!ec) pollSendQ(sendTimer);
            });

        // ── 통신스레드 시작 ──
        std::thread ioThread([&ioservice]() { ioservice.run(); });

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