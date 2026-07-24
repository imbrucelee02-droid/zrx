#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <memory>

namespace NS_ZrxHttp
{
    class ZrxHttpServer
    {
    public:
        static ZrxHttpServer& Instance();
        bool Start(int port = 18088);
        void Stop();
        bool IsRunning() const { return m_bRunning; }

    private:
        ZrxHttpServer();
        ~ZrxHttpServer();
        void ServerLoop(int port);

        bool m_bRunning = false;
        std::thread m_serverThread;
    };
}