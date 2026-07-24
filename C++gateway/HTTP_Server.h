#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <memory>

class HTTP_Server
{
public:
	HTTP_Server();

	static HTTP_Server* GetInstance();
	static void DeleteInstance();
	static void CreateInstance();

	void cmdStartHttpServer();
	void cmdStopHttpServer();
	void WaitCommandFinished();
private:
	static HTTP_Server* m_pSingleInstance;

	bool							m_bExitFlag;
	std::unique_ptr<std::thread>	m_pNamedPipeThread;
};
