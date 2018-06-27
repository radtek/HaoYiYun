
#include "stdafx.h"
#include "rtp.h"
#include "UtilTool.h"
#include "../XmlConfig.h"

// ȫ�µ���־������...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...)
{
	// ��ȡ��־ȫ·��...
	char szLogPath[MAX_PATH] = {0};
	CXmlConfig & theConfig = CXmlConfig::GMInstance();
	string & strUserPath = theConfig.GetUserAppPath();
	sprintf(szLogPath, "%s\\%s", strUserPath.c_str(), DEF_RTP_LOG);
	if( strlen(szLogPath) <= 0 ) {
		MsgLogGM(GM_Err_Config);
		return false;
	}
	// ׼����־ͷ��Ҫ��ʱ����Ϣ...
	SYSTEMTIME wtm = {0};
	::GetLocalTime(&wtm);

	// ׼����������־ͷ��Ϣ...
	int  log_size = -1;
	char log_data[LOG_MAX_SIZE] = {0};
	log_size = _snprintf(log_data, LOG_MAX_SIZE, 
		"[%d-%02d-%02d %02d:%02d:%02d.%03d][%d][%s:%d][%s] ", wtm.wYear, wtm.wMonth, wtm.wDay,
		wtm.wHour, wtm.wMinute, wtm.wSecond, (int)(wtm.wMilliseconds), getpid(), inFile, inLine,
		bIsDebug ? "debug" : "trace");
	// ȷ����־ͷ����...
	if(log_size == -1) {
		return false;
	}
	// ����־�ļ�...
	FILE * file_log_fd = NULL;
	if( !bIsDebug ) {
		// ��׷�ӵķ�ʽ��...
		file_log_fd = fopen(szLogPath, "a+");
		if( file_log_fd == NULL )
			return false;
		// ���ļ����� 3M ʱ�ع�...
		long nSize = _filelength(_fileno(file_log_fd));
		if( nSize >= 3 * 1024 * 1024 ) {
			fclose(file_log_fd); file_log_fd = NULL;
			file_log_fd = fopen(szLogPath, "w");
		}
	}
	// �����ݽ��и�ʽ��...
	va_list vl_list;
	va_start(vl_list, msg);
	log_size += vsnprintf(log_data + log_size, LOG_MAX_SIZE - log_size, msg, vl_list);
	va_end(vl_list);
	// �����β����...
	log_data[log_size++] = '\n';
	// ����ʽ��֮�������д���ļ�...
	if( !bIsDebug ) {
		fwrite(log_data, 1, log_size, file_log_fd);
		fclose(file_log_fd);
	}
	// �����ݴ�ӡ������̨...
	TRACE(log_data);
	return true;
}