#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <map>
#include <functional>
#include <condition_variable>
#include "nlohmann/json.hpp"

enum commandstatus {
	CmdStatus_OnIdle = 0x00,
	CmdStatus_OnCommand = 0x01,
	CmdStatus_Success = 0x02,
	CmdStatus_Fail = 0x04
};

class HTTP_IO_Data
{
public:
	HTTP_IO_Data();
	~HTTP_IO_Data();

	static HTTP_IO_Data& Instance() {
		static HTTP_IO_Data instance;
		return instance;
	}

	HTTP_IO_Data(const HTTP_IO_Data&) = delete;
	HTTP_IO_Data& operator=(const HTTP_IO_Data&) = delete;

	bool isInputEmpty();
	bool isOutputEmpty();
	bool saveInput(nlohmann::json& params);
	void saveResult(nlohmann::json& result);
	const nlohmann::json& getInput() const;
	const nlohmann::json& getOutput() const;
	void clear();

	void setState(commandstatus iState);
	commandstatus getState();

	void wait();
	void notify();

private:
	std::string				m_strId;
	nlohmann::json			m_jsonInputParams;
	nlohmann::json			m_jsonOutputResult;
	commandstatus			m_iState;

	std::condition_variable	m_cv;
	std::mutex				m_CommandMutex;
};
