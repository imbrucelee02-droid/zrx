#include "pch.h"
#include "ZrxHttpServer.h"
#include "HTTP_Data.h"
#include "HTTP_Function_List.h"
#include "Common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <acdocman.h>

#pragma comment(lib, "ws2_32.lib")

namespace NS_ZrxHttp
{
    ZrxHttpServer& ZrxHttpServer::Instance()
    {
        static ZrxHttpServer instance;
        return instance;
    }

    ZrxHttpServer::ZrxHttpServer() {}

    ZrxHttpServer::~ZrxHttpServer()
    {
        Stop();
    }

    bool ZrxHttpServer::Start(int port)
    {
        if (m_bRunning) return true;
        m_bRunning = true;
        m_serverThread = std::thread(&ZrxHttpServer::ServerLoop, this, port);
        return true;
    }

    void ZrxHttpServer::Stop()
    {
        if (!m_bRunning) return;
        m_bRunning = false;
        if (m_serverThread.joinable())
        {
            m_serverThread.detach();
        }
    }

    static void WaitCommandFinished()
    {
        if (CmdStatus_Success != HTTP_IO_Data::Instance().getState() &&
            CmdStatus_Fail != HTTP_IO_Data::Instance().getState())
        {
            HTTP_IO_Data::Instance().wait();
        }
    }

    static nlohmann::json process_cad_action(const std::string& action_name, const nlohmann::json& body_args, const std::string& success_msg)
    {
        nlohmann::json ret;
        try
        {
            nlohmann::json query;
            query["action"] = action_name;
            query["arguments"] = body_args.is_null() ? nlohmann::json::object() : body_args;

            HTTP_IO_Data::Instance().saveInput(query);

            ZcString strCmd(_T("HTTP_TOOL"));
            if (acDocManager && acDocManager->curDocument())
            {
                strCmd.append(_T(" "));
                acDocManager->sendStringToExecute(acDocManager->curDocument(), strCmd, true, false, false);
            }

            WaitCommandFinished();

            nlohmann::json result_data = HTTP_IO_Data::Instance().getOutput();

            ret["success"] = (HTTP_IO_Data::Instance().getState() == CmdStatus_Success);
            ret["message"] = success_msg;
            ret["data"] = result_data;

            HTTP_IO_Data::Instance().clear();
        }
        catch (std::exception& e)
        {
            ret["success"] = false;
            ret["message"] = std::string("Gateway Exception: ") + e.what();
        }
        catch (...)
        {
            ret["success"] = false;
            ret["message"] = "Unknown Gateway Exception";
        }
        return ret;
    }

    void ZrxHttpServer::ServerLoop(int port)
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            m_bRunning = false;
            return;
        }

        SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSock == INVALID_SOCKET)
        {
            WSACleanup();
            m_bRunning = false;
            return;
        }

        int optVal = 1;
        setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal));

        sockaddr_in service;
        service.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);
        service.sin_port = htons((u_short)port);

        if (::bind(listenSock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
        {
            closesocket(listenSock);
            WSACleanup();
            m_bRunning = false;
            return;
        }

        if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
        {
            closesocket(listenSock);
            WSACleanup();
            m_bRunning = false;
            return;
        }

        while (m_bRunning)
        {
            SOCKET clientSock = accept(listenSock, NULL, NULL);
            if (clientSock == INVALID_SOCKET) continue;

            char recvBuf[16384] = { 0 };
            int bytesRecv = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0);
            if (bytesRecv > 0)
            {
                std::string reqStr(recvBuf, bytesRecv);
                std::istringstream iss(reqStr);
                std::string method, path;
                iss >> method >> path;

                std::string body = "";
                size_t bodyPos = reqStr.find("\r\n\r\n");
                if (bodyPos != std::string::npos)
                {
                    body = reqStr.substr(bodyPos + 4);
                }

                nlohmann::json res;

                // OPTIONS Preflight
                if (method == "OPTIONS")
                {
                    std::string optResponse =
                        "HTTP/1.1 204 No Content\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                        "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
                    send(clientSock, optResponse.c_str(), (int)optResponse.length(), 0);
                    closesocket(clientSock);
                    continue;
                }

                // Health check
                if ((path == "/api/status" || path == "/hello") && method == "GET")
                {
                    res["status"] = "ok";
                    res["plugin"] = "simple_table_prase.zrx";
                    res["cad_version"] = "ZWCAD 2025";
                }
                // Endpoint 1: Dedicated BOM selection route
                else if (path == "/api/select_bom" && method == "POST")
                {
                    nlohmann::json bodyArgs = nlohmann::json::object();
                    if (!body.empty()) {
                        try { bodyArgs = nlohmann::json::parse(body); } catch(...) {}
                    }
                    res = process_cad_action("select_bom", bodyArgs, "BOM selection & Dify processing completed");
                }
                // Endpoint 2: Dedicated TitleBlock selection route
                else if (path == "/api/select_titleblock" && method == "POST")
                {
                    nlohmann::json bodyArgs = nlohmann::json::object();
                    if (!body.empty()) {
                        try { bodyArgs = nlohmann::json::parse(body); } catch(...) {}
                    }
                    res = process_cad_action("select_titleblock", bodyArgs, "TitleBlock selection & recognition completed");
                }
                // Endpoint 3: General select_and_process
                else if (path == "/api/select_and_process" && method == "POST")
                {
                    int mode = 0;
                    nlohmann::json bodyArgs = nlohmann::json::object();
                    if (!body.empty()) {
                        size_t fb = body.find('{'), lb = body.rfind('}');
                        if (fb != std::string::npos && lb != std::string::npos && lb > fb) {
                            try {
                                bodyArgs = nlohmann::json::parse(body.substr(fb, lb - fb + 1));
                                if (bodyArgs.contains("convert_mode")) {
                                    if (bodyArgs["convert_mode"].is_number()) mode = bodyArgs["convert_mode"].get<int>();
                                    else if (bodyArgs["convert_mode"].is_string()) mode = std::stoi(bodyArgs["convert_mode"].get<std::string>());
                                }
                            } catch(...) {}
                        }
                    }
                    std::string actionName = (mode == 1) ? "select_bom" : "select_titleblock";
                    res = process_cad_action(actionName, bodyArgs, "CAD selection & Dify processing completed");
                }
                // Endpoint 4: Writeback table route
                else if (path == "/api/writeback_table" && method == "POST")
                {
                    nlohmann::json bodyArgs = nlohmann::json::object();
                    if (!body.empty()) {
                        size_t fb = body.find('{'), lb = body.rfind('}');
                        if (fb != std::string::npos && lb != std::string::npos && lb > fb) {
                            try { bodyArgs = nlohmann::json::parse(body.substr(fb, lb - fb + 1)); } catch(...) {}
                        }
                    }
                    res = process_cad_action("writeback_table", bodyArgs, "Table writeback completed");
                }
                else
                {
                    res["success"] = false;
                    res["error"] = "Route not found";
                }

                std::string resBody = res.dump(4);
                std::ostringstream responseStream;
                responseStream << "HTTP/1.1 200 OK\r\n"
                               << "Content-Type: application/json\r\n"
                               << "Access-Control-Allow-Origin: *\r\n"
                               << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                               << "Access-Control-Allow-Headers: Content-Type\r\n"
                               << "Content-Length: " << resBody.length() << "\r\n"
                               << "\r\n"
                               << resBody;

                std::string httpResponse = responseStream.str();
                send(clientSock, httpResponse.c_str(), (int)httpResponse.length(), 0);
            }

            closesocket(clientSock);
        }

        closesocket(listenSock);
        WSACleanup();
    }
}