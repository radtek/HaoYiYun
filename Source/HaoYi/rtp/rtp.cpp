
#include "stdafx.h"
#include "rtp.h"
#include "UtilTool.h"
#include "../XmlConfig.h"

// 全新的日志处理函数...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...)
{
	// 获取日志全路径...
	char szLogPath[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strUserPath = theConfig.GetUserAppPath();
	sprintf(szLogPath, "%s\\%s", strUserPath.c_str(), DEF_RTP_LOG);
	if( strlen(szLogPath) <= 0 ) {
		MsgLogGM(GM_Err_Config);
		return false;
	}
	// 准备日志头需要的时间信息...
	SYSTEMTIME wtm = {0};
	::GetLocalTime(&wtm);

	// 准备完整的日志头信息...
	int  log_size = -1;
	char log_data[LOG_MAX_SIZE] = {0};
	log_size = _snprintf(log_data, LOG_MAX_SIZE, 
		"[%d-%02d-%02d %02d:%02d:%02d.%03d][%d][%s:%d][%s] ", wtm.wYear, wtm.wMonth, wtm.wDay,
		wtm.wHour, wtm.wMinute, wtm.wSecond, (int)(wtm.wMilliseconds), getpid(), inFile, inLine,
		bIsDebug ? "debug" : "trace");
	// 确认日志头长度...
	if(log_size == -1) {
		return false;
	}
	// 打开日志文件...
	FILE * file_log_fd = NULL;
	if( !bIsDebug ) {
		// 用追加的方式打开...
		file_log_fd = fopen(szLogPath, "a+");
		if( file_log_fd == NULL )
			return false;
		// 当文件大于 3M 时回滚...
		long nSize = _filelength(_fileno(file_log_fd));
		if( nSize >= 3 * 1024 * 1024 ) {
			fclose(file_log_fd); file_log_fd = NULL;
			file_log_fd = fopen(szLogPath, "w");
		}
	}
	// 对数据进行格式化...
	va_list vl_list;
	va_start(vl_list, msg);
	log_size += vsnprintf(log_data + log_size, LOG_MAX_SIZE - log_size, msg, vl_list);
	va_end(vl_list);
	// 加入结尾符号...
	log_data[log_size++] = '\n';
	// 将格式化之后的数据写入文件...
	if( !bIsDebug ) {
		fwrite(log_data, 1, log_size, file_log_fd);
		fclose(file_log_fd);
	}
	// 将数据打印到控制台...
	TRACE(log_data);
	return true;
}