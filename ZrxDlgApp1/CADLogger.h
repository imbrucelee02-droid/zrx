#pragma once
#include <mutex>
#include <fstream>
#include <memory>

// 日志级别
enum class LogLevel
{
	Debug = 0,
	Info = 1,
	Warning = 2,
	Error = 3
};

// 日志输出目标
enum class LogTarget
{
	None = 0,
	Console = 1,
	File = 2,
	Both = 3  // Console | File
};

// 日志配置结构
struct LogConfig
{
	LogLevel    Level;              // 日志级别
	LogTarget   Target;             // 输出目标
	CString     FilePath;           // 文件路径
	bool        AutoFlush;          // 是否自动刷新
	size_t      MaxFileSize;        // 最大文件大小（字节），0表示不限制
	int         MaxBackupFiles;     // 最大备份文件数
	bool        EnableTimestamp;    // 是否包含时间戳
	bool        EnableThreadId;     // 是否包含线程ID
	bool        EnableSourceInfo;   // 是否包含源文件信息（预留）

	LogConfig()
		: Level(LogLevel::Info)
		, Target(LogTarget::Both)
		, AutoFlush(true)
		, MaxFileSize(10 * 1024 * 1024)  // 默认10MB
		, MaxBackupFiles(5)
		, EnableTimestamp(true)
		, EnableThreadId(false)
		, EnableSourceInfo(false)
	{
	}
};

// CAD日志记录器
class CADLogger
{
public:
	// 获取单例
	static CADLogger& GetInstance();

	// ========== 初始化接口 ==========
	
	/**
	 * @brief 初始化日志系统（推荐使用）
	 * @param config 日志配置
	 * @return 是否初始化成功
	 */
	bool Initialize(const LogConfig& config);

	/**
	 * @brief 快速初始化（便捷方法）
	 * @param logFilePath 日志文件路径
	 * @param level 日志级别
	 * @param target 输出目标
	 * @return 是否初始化成功
	 */
	bool Initialize(
		const CString& logFilePath,
		LogLevel level = LogLevel::Info,
		LogTarget target = LogTarget::Both);

	/**
	 * @brief 检查是否已初始化
	 */
	bool IsInitialized() const { return m_initialized; }

	/**
	 * @brief 关闭日志系统并刷新缓冲
	 */
	void Shutdown();

	// ========== 日志级别设置 ==========

	void SetLogLevel(LogLevel level);
	LogLevel GetLogLevel() const { return m_logLevel; }

	// ========== 日志记录接口 ==========

	void Debug(const CString& message);
	void Info(const CString& message);
	void Warning(const CString& message);
	void Error(const CString& message);

	// 格式化日志
	void DebugFormat(LPCTSTR format, ...);
	void InfoFormat(LPCTSTR format, ...);
	void WarningFormat(LPCTSTR format, ...);
	void ErrorFormat(LPCTSTR format, ...);

	// ========== 输出控制（兼容旧接口）==========

	void EnableConsoleOutput(bool enable);
	void EnableFileOutput(bool enable, const CString& filePath = _T(""));
	
	// 新增：设置输出目标
	void SetLogTarget(LogTarget target);
	
	// 新增：手动刷新缓冲
	void Flush();

	// ========== 文件管理 ==========
	
	/**
	 * @brief 设置文件大小限制
	 * @param maxSize 最大文件大小（字节），0表示不限制
	 */
	void SetMaxFileSize(size_t maxSize);

	/**
	 * @brief 设置最大备份文件数
	 */
	void SetMaxBackupFiles(int maxFiles);

	/**
	 * @brief 立即执行日志轮转
	 */
	void RotateLogFile();

	// ========== 其他设置 ==========

	void EnableAutoFlush(bool enable) { m_autoFlush = enable; }
	void EnableTimestamp(bool enable) { m_enableTimestamp = enable; }
	void EnableThreadId(bool enable) { m_enableThreadId = enable; }

private:
	CADLogger();
	~CADLogger();

	// 禁止拷贝
	CADLogger(const CADLogger&) = delete;
	CADLogger& operator=(const CADLogger&) = delete;

	// ========== 内部方法 ==========

	void Log(LogLevel level, const CString& message);
	void LogInternal(LogLevel level, const CString& message);  // 内部版本，不加锁
	void WriteToConsole(const CString& message);
	void WriteToFile(const CString& message);
	bool OpenLogFile();
	void CloseLogFile();
	bool CheckAndRotateFile();
	void RotateBackupFiles();
	
	CString FormatMessage(LogLevel level, const CString& message);
	CString GetLevelString(LogLevel level);
	CString GetTimestamp();
	CString GetThreadId();

private:
	// 初始化状态
	bool m_initialized;

	// 日志配置
	LogLevel m_logLevel;
	LogTarget m_logTarget;
	CString m_logFilePath;
	bool m_autoFlush;
	size_t m_maxFileSize;
	int m_maxBackupFiles;

	// 格式选项
	bool m_enableTimestamp;
	bool m_enableThreadId;

	// 文件输出
	std::wofstream m_logFile;
	bool m_fileOpened;

	// 线程安全
	mutable std::mutex m_mutex;

	// 兼容旧接口
	bool m_consoleOutput;
	bool m_fileOutput;
};

// ========== 便捷宏定义 ==========

#define LOG_DEBUG(msg)    CADLogger::GetInstance().Debug(msg)
#define LOG_INFO(msg)     CADLogger::GetInstance().Info(msg)
#define LOG_WARN(msg)     CADLogger::GetInstance().Warning(msg)
#define LOG_ERROR(msg)    CADLogger::GetInstance().Error(msg)

#define LOG_DEBUG_F(fmt, ...)   CADLogger::GetInstance().DebugFormat(fmt, __VA_ARGS__)
#define LOG_INFO_F(fmt, ...)    CADLogger::GetInstance().InfoFormat(fmt, __VA_ARGS__)
#define LOG_WARN_F(fmt, ...)    CADLogger::GetInstance().WarningFormat(fmt, __VA_ARGS__)
#define LOG_ERROR_F(fmt, ...)   CADLogger::GetInstance().ErrorFormat(fmt, __VA_ARGS__)

// ========== 初始化宏（便于项目复用）==========

/**
 * 快速初始化日志系统的宏
 * 用法：LOG_INIT(_T("C:\\Logs\\app.log"), LogLevel::Info)
 */
#define LOG_INIT(filepath, level) \
	CADLogger::GetInstance().Initialize(filepath, level, LogTarget::Both)

/**
 * 仅文件输出的初始化
 */
#define LOG_INIT_FILE_ONLY(filepath, level) \
	CADLogger::GetInstance().Initialize(filepath, level, LogTarget::File)

/**
 * 仅控制台输出的初始化
 */
#define LOG_INIT_CONSOLE_ONLY(level) \
	CADLogger::GetInstance().Initialize(_T(""), level, LogTarget::Console)

/**
 * 根据Debug/Release模式自动配置
 * Debug: 输出所有日志（包括DEBUG和INFO）
 * Release: 只输出WARN和ERROR
 */
#ifdef ZRXDEBUG
	#define LOG_INIT_AUTO(filepath) \
		CADLogger::GetInstance().Initialize(filepath, LogLevel::Debug, LogTarget::Both)
#else
	#define LOG_INIT_AUTO(filepath) \
		CADLogger::GetInstance().Initialize(filepath, LogLevel::Warning, LogTarget::Both)
#endif

/**
 * 关闭日志系统
 */
#define LOG_SHUTDOWN() \
	CADLogger::GetInstance().Shutdown()
