
#pragma once

#include "HYDefine.h"
#include "json.h"

class CUtilTool
{
public:
	static string		GetGMTDate();				// Get GMT Current Date. 
	static string		GetCHSDate();				// Get CHS Current Date.

	static string		GetSystemVer();
	static HMODULE		GetSelfModuleHandle();
	static string		GetExePath(HINSTANCE hInstance);
	static void			GetExePath(LPSTR outPath);
	static void			GetFilePath(LPSTR outPath, LPCTSTR inFile);

	static string		GetGMTTime(__time64_t inTime = 0);
	static LPCTSTR		GetTimeString(__time64_t inTime, LPCTSTR lpszFormat);
	static void			DbgLogInfo(LPCTSTR lpFormat, ...);
	
	static GM_Error		MsgLog( LOG_TXT eType, LPCTSTR lpFormat, ... );		// 打印各种层次的格式信息到控制台或存盘；
	static GM_Error		MsgLog( GM_Error nErr,				// 错误号码: GM_Error
								LOG_LEVEL nLevel,			// 错误类型: kLogGM
							    LPCTSTR lpFile,				// 文件名称: __FILE__
							    int nCodeLine );			// 代码行号: __LINE__ 

	static string		GetFileVersionStr(LPCTSTR lpszFileName);
	static ULONGLONG	GetFileFullSize(LPCTSTR lpszFileName);
	static ULONGLONG	GetLocalFileTime();
	static int			VersionCmp( LPCTSTR lpVer1,LPCTSTR lpVer2 );
	
	static string		GetFileModTime(FILETIME * lpLastWrite);
	static string		GetFileSizeStr(DWORD dwHigh, DWORD dwLow);
	static LPCTSTR		GetErrLevel(LOG_LEVEL inLevel);

	static string		getJsonString(Json::Value & inValue);
	static string		GUIDToStr(REFCLSID rCLSID, BOOL bBracket = false);
	static string		UTF8_ANSI(LPCTSTR lpUValue);
	static string		ANSI_UTF8(LPCTSTR lpSValue);
	static string		IntToStr(int inValue);
	static BOOL			IsUTF8(LPCTSTR lpszValue);
	static BOOL			DelDir( LPCTSTR lpszPath );
	static int			CaculDirDepth(LPCTSTR lpszPath);
	static BOOL			CreateDir(LPCTSTR lpszPath);

	static LPCTSTR		GetServerProductName();
	static LPCTSTR		GetServerBasicRealm();
	static LPCTSTR		GetServerBuildDate();
	static LPCTSTR		GetServerUserName();
	static LPCTSTR		GetServerPlatform();
	static LPCTSTR		GetServerDNSName();
	static LPCTSTR		GetServerVersion();
	static LPCTSTR		GetServerHeader();
	static LPCTSTR		GetServerName();
	static LPCTSTR		GetServerOS();
};

class CGMLogWriter
{
public:
	static		CGMLogWriter & GetInstance();
	GM_Error	MsgLog(LOG_TXT eType, LPCTSTR lpszLog);
	void		SetLoggerFile(const string & strFile, BOOL bIsFullPath = false) { m_strLogFile = strFile; m_bIsFullPath = bIsFullPath; }
private:
	GM_Error	InitLogTxt(FILE ** lppFile);
	GM_Error	DoTextLog(LPCTSTR lpszLog);
private:
	CGMLogWriter();
	~CGMLogWriter();
private:
	LOG_TXT		m_eType;			 // 日志类型
	string		m_strLogFile;		 // 日志文件
	BOOL		m_bIsFullPath;		 // 是否是完整路径
};

#define ANSI_Value(x) CUtilTool::UTF8_ANSI(x).c_str()
#define UTF8_Value(x) CUtilTool::ANSI_UTF8(x).c_str()

#define MsgLogGM(nErr) CUtilTool::MsgLog(nErr, kLogGM, __FILE__, __LINE__)			// 记录逻辑错误(Logger.txt)
#define MsgLogSYS(nErr) CUtilTool::MsgLog(nErr, kLogSYS, __FILE__, __LINE__)		// 记录系统错误(Logger.txt)
#define MsgLogINFO(sErr) CUtilTool::MsgLog(kTxtLogger, _T("%s\r\n"), TEXT(sErr))	// 记录系统信息(Logger.txt)
#define MsgExpINFO(sErr) CUtilTool::MsgLog(kTxtExcept, _T("%s\r\n"), TEXT(sErr))	// 记录异常信息(Except.txt)
#define MsgExpFile(sFile) CUtilTool::MsgLog(kTxtExcept, _T("%s(%lu)\r\n异常文件 = %s"), __FILE__, __LINE__, TEXT(sFile))	// 记录异常信息(Except.txt)
#define MsgExpCode(nCode) CUtilTool::MsgLog(kTxtExcept, _T("%s(%lu)\r\n异常代号 = 0x%x(%lu)"), __FILE__, __LINE__, nCode, nCode)	// 记录异常信息(Except.txt)
