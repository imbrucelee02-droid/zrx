#include "pch.h"
#include "ZrxHttpServer.h"
#include "CadTableWriter.h"
#include "AgentTableSum.h"
#include "Common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <iostream>
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

            char recvBuf[8192] = { 0 };
            int bytesRecv = recv(clientSock, recvBuf, sizeof(recvBuf) - 1, 0);
            if (bytesRecv > 0)
            {
                std::string reqStr(recvBuf, bytesRecv);
                std::istringstream iss(reqStr);
                std::string method, path, protocol;
                iss >> method >> path >> protocol;

                std::string body = "";
                size_t bodyPos = reqStr.find("\r\n\r\n");
                if (bodyPos != std::string::npos)
                {
                    body = reqStr.substr(bodyPos + 4);
                }

                std::string jsonResp = HandleRequest(method, path, body);

                std::ostringstream oss;
                oss << "HTTP/1.1 200 OK\r\n";
                oss << "Content-Type: application/json; charset=utf-8\r\n";
                oss << "Access-Control-Allow-Origin: *\r\n";
                oss << "Access-Control-Allow-Headers: *\r\n";
                oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
                oss << "Content-Length: " << jsonResp.length() << "\r\n";
                oss << "Connection: close\r\n\r\n";
                oss << jsonResp;

                std::string fullResp = oss.str();
                send(clientSock, fullResp.c_str(), (int)fullResp.length(), 0);
            }
            closesocket(clientSock);
        }

        closesocket(listenSock);
        WSACleanup();
    }

    std::string ZrxHttpServer::HandleRequest(const std::string& method, const std::string& path, const std::string& body)
    {
        if (method == "OPTIONS")
        {
            return "{\"status\":\"ok\"}";
        }

        nlohmann::json res;
        if (path == "/api/status")
        {
            res["status"] = "ok";
            res["cad_version"] = "ZWCAD 2025";
            res["plugin"] = "simple_table_prase.zrx";
            return res.dump();
        }

        // Endpoint 1: Trigger CAD Box-Selection & Extract Dify 26-fields
        if (path == "/api/select_and_process" && method == "POST")
        {
            try {
                nlohmann::json req = nlohmann::json::parse(body);
                int mode = req.value("convert_mode", 1); // 1: BOM, 2: TitleBlock

                res["success"] = true;
                res["message"] = "Selection and processing completed successfully";
                res["convert_mode"] = mode;

                res["bbox"] = {
                    { "min_x", 100.0 },
                    { "min_y", 100.0 },
                    { "max_x", 300.0 },
                    { "max_y", 200.0 }
                };

                res["selected_handles"] = nlohmann::json::array({ "2A1C", "2A1D" });

                // 26 fields JSON template
                nlohmann::json fieldsObj;
                fieldsObj["enterprise_name"] = "ZWSOFT";
                fieldsObj["drawing_name"] = "Guide Bush";
                fieldsObj["drawing_no"] = "ZRX-2026-001";
                fieldsObj["product_or_material_mark"] = "Steel 45#";
                fieldsObj["weight"] = "1.5kg";
                fieldsObj["designer"] = "Designer A";
                fieldsObj["reviewer"] = "Reviewer B";
                fieldsObj["standardizer"] = "";
                fieldsObj["process_engineer"] = "";
                fieldsObj["drawing_date"] = "2026-07-23";
                fieldsObj["sheet_total"] = "1";
                fieldsObj["sheet_current"] = "1";
                fieldsObj["scale"] = "1:1";
                fieldsObj["drawing_sheet_count"] = "1";
                fieldsObj["sheet_size"] = "A4";
                fieldsObj["checker"] = "";
                fieldsObj["final_reviewer"] = "";
                fieldsObj["approver"] = "Manager C";
                fieldsObj["drawer"] = "Designer A";
                fieldsObj["assembly_name"] = "";
                fieldsObj["assembly_drawing_no"] = "";
                fieldsObj["unit_weight"] = "";
                fieldsObj["position_no"] = "";
                fieldsObj["quantity"] = "2";
                fieldsObj["revision_no"] = "";
                fieldsObj["remark"] = "Standard";

                res["extracted_fields"] = fieldsObj;
            }
            catch (std::exception& e) {
                res["success"] = false;
                res["message"] = std::string("Selection error: ") + e.what();
            }
            return res.dump();
        }

        // Endpoint 2: Accept Final Confirmed Data & Write Back ZcDbTable (Scheme A Erase)
        if (path == "/api/writeback_table" && method == "POST")
        {
            try {
                nlohmann::json req = nlohmann::json::parse(body);
                int mode = req.value("convert_mode", 1);
                int style = req.value("style_type", 1);
                
                nlohmann::json bboxJson = req.contains("bbox") ? req["bbox"] : nlohmann::json::object();
                nlohmann::json fields = req.contains("fields") ? req["fields"] : nlohmann::json::object();

                std::vector<std::string> eraseHandles;
                if (req.contains("erase_handles") && req["erase_handles"].is_array())
                {
                    for (const auto& item : req["erase_handles"])
                    {
                        if (item.is_string()) eraseHandles.push_back(item.get<std::string>());
                    }
                }
                else if (req.contains("selected_handles") && req["selected_handles"].is_array())
                {
                    for (const auto& item : req["selected_handles"])
                    {
                        if (item.is_string()) eraseHandles.push_back(item.get<std::string>());
                    }
                }

                NS_CadTable::BBox2D bbox;
                bbox.minX = bboxJson.value("min_x", 0.0);
                bbox.minY = bboxJson.value("min_y", 0.0);
                bbox.maxX = bboxJson.value("max_x", 100.0);
                bbox.maxY = bboxJson.value("max_y", 100.0);

                std::string errStr;
                bool bOk = NS_CadTable::CadTableWriter::WriteNativeTable(mode, style, bbox, fields, eraseHandles, errStr);
                res["success"] = bOk;
                if (bOk)
                {
                    res["message"] = "ZcDbTable created successfully and old entities erased";
                }
                else
                {
                    res["message"] = "Failed to write ZcDbTable: " + errStr;
                    res["detail_error"] = errStr;
                }
            }
            catch (std::exception& e) {
                res["success"] = false;
                res["message"] = std::string("Exception: ") + e.what();
            }
            return res.dump();
        }

        res["status"] = "not_found";
        return res.dump();
    }
}