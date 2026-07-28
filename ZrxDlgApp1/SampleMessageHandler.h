#pragma once
#include "IMessageHandler.h"
#include <string>

// Example message handler demonstrating Vue <-> C++ communication.
// Replace or extend this class with your business logic.
class SampleMessageHandler : public IMessageHandler
{
public:
    void OnMessage(const std::wstring& type,
                   const nlohmann::json& payload,
                   WebView2Host* host) override;

private:
    void HandleReady(const nlohmann::json& payload, WebView2Host* host);
    void HandleEcho(const nlohmann::json& payload, WebView2Host* host);
    void HandleGetInfo(WebView2Host* host);
    void HandleClose(const nlohmann::json& payload, WebView2Host* host);
};
