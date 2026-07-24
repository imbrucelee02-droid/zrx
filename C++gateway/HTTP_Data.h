#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <map>
#include <functional>
#include <condition_variable>
#include "nlohmann/json.hpp"

enum commandstatus {
	OnIdle = 0x00,    // 空闲状态
	OnCommand = 0x01, // 命令中
	Success = 0x02,   // 命令成功结束
	Fail = 0x04       // 命令执行失败
};

class HTTP_IO_Data
{
public:
	HTTP_IO_Data();
	~HTTP_IO_Data();

	// 获取单例实例 (线程安全)
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
