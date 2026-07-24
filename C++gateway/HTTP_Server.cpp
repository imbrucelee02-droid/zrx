#include "pch.h"
#include "HTTP_Server.h"

#include "aced.h"
#include "dbents.h"
#include "adslib.h"
#include "zdbapserv.h"
#include "rxmfcapi.h"
#include "dbsymtb.h"
#include "dbxutil.h"
#include "rxregsvc.h"
#include "acutads.h"
#include "adsmigr.h"
#include "dbtable.h"
#include "gepnt3d.h"

#include "nlohmann/json.hpp"
#include "Common.h"
#include "HTTP_Data.h"
#include "AgentTableSum.h"
#include "minizip/zip.h" // optional

// Mongoose header
#include "mongoose.h"

HTTP_Server* HTTP_Server::m_pSingleInstance = NULL;
static struct mg_mgr mgr;
static std::string g_httpResponse;
static std::mutex m_pipMutex;

using json = nlohmann::json;

static void http_callback(struct mg_connection* c, int ev, void* ev_data, void* fn_data);

static void WaitCommandFinished()
{
    if (2 != HTTP_IO_Data::Instance().getState() && 3 != HTTP_IO_Data::Instance().getState())
    {
        HTTP_IO_Data::Instance().wait();
    }
}

HTTP_Server::HTTP_Server()
{
    m_bExitFlag = false;
    m_pNamedPipeThread = NULL;
}

nlohmann::json process_callback(const std::string& fun_name, std::string& body, const ZcString& success_msg)
{
    nlohmann::json ret;
    try
    {
        nlohmann::json body_json = nlohmann::json::parse(body);

        nlohmann::json query;
        query["action"] = fun_name;
        query["arguments"] = body_json;

        HTTP_IO_Data::Instance().saveInput(query);

        ZcString strCmd(_T("HTTP_TOOL"));
        if (zcDocManager && zcDocManagerPtr())
        {
            strCmd.append(_T(" "));
            zcDocManager->sendStringToExecute(zcDocManagerPtr()->curDocument(), strCmd, true, false, false);
        }

        WaitCommandFinished();
        
        nlohmann::json result_data = HTTP_IO_Data::Instance().getOutput();

        ret["code"] = 0;
        ret["msg"] = success_msg.utf8Ptr();
        ret["data"] = result_data;

        HTTP_IO_Data::Instance().clear();
    }
    catch (json::parse_error& e)
    {
        ret["code"] = -1;
        ret["msg"] = (ZcString("JSON解析失败: ") + e.what()).utf8Ptr();
    }
    catch (json::out_of_range& e)
    {
        ret["code"] = -2;
        ret["msg"] = (ZcString("参数缺失: ") + e.what()).utf8Ptr();
    }
    catch (...)
    {
        ret["code"] = -99;
        ret["msg"] = ZcString("其他异常").utf8Ptr();
    }

    return ret;
}

void http_callback(struct mg_connection* c, int ev, void* ev_data, void* fn_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        json ret;
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;

        if (mg_match(hm->method, mg_str("OPTIONS"), nullptr))
        {
            mg_http_reply(c, 204,
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n", "");
            return;
        }

        if (mg_match(hm->uri, mg_str("/hello"), NULL))
        {
            ret["code"] = 0;
            ret["msg"] = GBKToUTF8("ZRX+Mongoose + nlohmann/json HTTP服务运行成功");
        }
        else if (mg_match(hm->uri, mg_str("/table_sum_reconize"), NULL) && hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0)
        {
            std::string body(hm->body.buf, hm->body.len);
            ret = process_callback("table_sum_reconize", body, ZcString("表格汇总:图纸识别完成!"));
        }
        else if (mg_match(hm->uri, mg_str("/table_sum_interact"), NULL) && hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0)
        {
            std::string body(hm->body.buf, hm->body.len);
            ret = process_callback("table_sum_interact", body, ZcString("表格汇总:交互识别完成!"));
        }
        else if (mg_match(hm->uri, mg_str("/table_sum_aitablesum"), NULL) && hm->method.len == 4 && memcmp(hm->method.buf, "POST", 4) == 0)
        {
            std::string body(hm->body.buf, hm->body.len);
            ret = process_callback("table_sum_aitablesum", body, ZcString("表格汇总:ai汇总完成!"));
        }
        else
        {
            ret["code"] = -404;
            ret["msg"] = ZcString("接口不存在").utf8Ptr();
        }

        std::lock_guard<std::mutex> lock(m_pipMutex);
        g_httpResponse = ret.dump();

        mg_http_reply(c, 200,
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n",
            "%s", g_httpResponse.c_str());
    }
}

HTTP_Server* HTTP_Server::GetInstance()
{
    return m_pSingleInstance;
}

void HTTP_Server::CreateInstance()
{
    if (!m_pSingleInstance)
    {
        m_pSingleInstance = new HTTP_Server;
    }
}

void HTTP_Server::DeleteInstance()
{
    if (m_pSingleInstance)
    {
        delete m_pSingleInstance;
        m_pSingleInstance = NULL;
    }
}

void HTTP_Server::cmdStartHttpServer()
{
    if (m_pNamedPipeThread != nullptr && m_pNamedPipeThread->joinable())
    {
        cmdStopHttpServer();
    }

    mg_mgr_init(&mgr);
    struct mg_connection* listener = mg_http_listen(&mgr, "0.0.0.0:8080", (mg_event_handler_t)http_callback, nullptr);
    if (listener == nullptr)
    {
        acutPrintf(_T("HTTP监听失败，端口8080被占用！\n"));
        mg_mgr_free(&mgr);
        return;
    }

    acutPrintf(_T("HTTP服务已启动，监听 0.0.0.0:8080\n StopHttpServer停止服务\n"));

    m_pNamedPipeThread.reset(new std::thread([this]()
        {
            while (true)
            {
                {
                    std::lock_guard<std::mutex> lock(m_pipMutex);
                    if (m_bExitFlag)
                    {
                        break;
                    }
                }

                mg_mgr_poll(&mgr, 50);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    ));
}

void HTTP_Server::cmdStopHttpServer()
{
    {
        std::lock_guard<std::mutex> lock(m_pipMutex);
        m_bExitFlag = true;
    }

    if (m_pNamedPipeThread != NULL)
    {
        if (m_pNamedPipeThread->joinable())
        {
            m_pNamedPipeThread->join();
            m_pNamedPipeThread.reset();
        }
    }

    mg_mgr_free(&mgr);
    acutPrintf(_T("HTTP服务已关闭\n"));
}
