#include "pch.h"
#include "HTTP_Data.h"

HTTP_IO_Data::HTTP_IO_Data()
    : m_iState(CmdStatus_OnIdle)
{
}

HTTP_IO_Data::~HTTP_IO_Data()
{
    clear();
}

bool HTTP_IO_Data::isInputEmpty()
{
    if (m_jsonInputParams.empty())
    {
        return true;
    }
    return false;
}

bool HTTP_IO_Data::isOutputEmpty()
{
    if (m_jsonOutputResult.empty())
    {
        return true;
    }
    return false;
}

bool HTTP_IO_Data::saveInput(nlohmann::json& params)
{
    m_jsonInputParams = params;
    return true;
}

void HTTP_IO_Data::saveResult(nlohmann::json& result)
{
    m_jsonOutputResult = result;
}

const nlohmann::json& HTTP_IO_Data::getInput() const
{
    return m_jsonInputParams;
}

const nlohmann::json& HTTP_IO_Data::getOutput() const
{
    return m_jsonOutputResult;
}

void HTTP_IO_Data::clear()
{
    m_strId.clear();
    m_jsonInputParams.clear();
    m_jsonOutputResult.clear();
    {
        std::unique_lock<std::mutex> lock(m_CommandMutex);
        m_iState = CmdStatus_OnIdle;
    }
}

void HTTP_IO_Data::setState(commandstatus iState)
{
    {
        std::unique_lock<std::mutex> lock(m_CommandMutex);
        m_iState = iState;
    }
    if (CmdStatus_Success == iState || CmdStatus_Fail == iState)
    {
        m_cv.notify_one();
    }
}

commandstatus HTTP_IO_Data::getState()
{
    std::unique_lock<std::mutex> lock(m_CommandMutex);
    return m_iState;
}

void HTTP_IO_Data::wait()
{
    std::unique_lock<std::mutex> lock(m_CommandMutex);
    m_cv.wait(lock, [this] {
        return m_iState == CmdStatus_Success || m_iState == CmdStatus_Fail;
    });
}

void HTTP_IO_Data::notify()
{
    m_cv.notify_one();
}
