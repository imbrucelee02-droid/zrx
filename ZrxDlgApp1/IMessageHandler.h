#pragma once
#include <string>
#include "json.hpp"

class WebView2Host;

// Interface for handling messages from the Vue frontend.
// Implement this to add custom message processing.
class IMessageHandler
{
public:
    virtual ~IMessageHandler() = default;

    // Called when the Vue frontend sends a JSON message.
    // |type|    -- the "type" field from the message
    // |payload| -- the full parsed JSON object
    // |host|    -- the WebView2Host that received the message (use to send replies)
    virtual void OnMessage(const std::wstring& type,
                           const nlohmann::json& payload,
                           WebView2Host* host) = 0;
};
