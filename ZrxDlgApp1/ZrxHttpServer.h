#pragma once
#include <string>
#include <atomic>
#include <thread>
#include "nlohmann/json.hpp"

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
        std::string HandleRequest(const std::string& method, const std::string& path, const std::string& body);

        std::atomic<bool> m_bRunning{ false };
        std::thread m_serverThread;
    };
}