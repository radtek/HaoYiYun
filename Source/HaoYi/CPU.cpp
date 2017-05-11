
#include "StdAfx.h"
#include "CPU.h"

CCpuMemRate::CCpuMemRate(void)
{
	memset(&m_liOldIdleTime, 0, sizeof(m_liOldIdleTime));
	memset(&m_liOldSystemTime, 0, sizeof(m_liOldSystemTime));
	memset(&m_SysBaseInfo, 0, sizeof(m_SysBaseInfo));
	memset(&m_SysTimeInfo, 0, sizeof(m_SysTimeInfo));
	memset(&m_SysPerfInfo, 0, sizeof(m_SysPerfInfo));
	memset(&m_stat, 0, sizeof(m_stat));
}

CCpuMemRate::~CCpuMemRate(void)
{
}

int CCpuMemRate::GetCpuRate(void)
{
	double dbIdleTime = 0.0f;
	double dbSystemTime = 0.0f;
	LONG   status = 0;

	/*printf("\nCPU Usage (press any key to exit): ");
	while(!_kbhit())
	{*/
		// get new system time
		status = NtQuerySystemInformation(SystemTimeInformation,&m_SysTimeInfo,sizeof(m_SysTimeInfo),0);
		if (status!=NO_ERROR)
			return -1;

		// get new CPU's idle time
		status =NtQuerySystemInformation(SystemPerformanceInformation,&m_SysPerfInfo,sizeof(m_SysPerfInfo),NULL);
		if (status != NO_ERROR)
			return -1;

		// if it's a first call - skip it
		if (m_liOldIdleTime.QuadPart != 0)
		{
			// CurrentValue = NewValue - OldValue
			dbIdleTime = Li2Double(m_SysPerfInfo.liIdleTime) - Li2Double(m_liOldIdleTime);
			dbSystemTime = Li2Double(m_SysTimeInfo.liKeSystemTime) - 	Li2Double(m_liOldSystemTime);

			// CurrentCpuIdle = IdleTime / SystemTime
			dbIdleTime = dbIdleTime / dbSystemTime;

			// CurrentCpuUsage% = 100 - (CurrentCpuIdle * 100) / NumberOfProcessors
			dbIdleTime = 100.0 - dbIdleTime * 100.0 / (double)m_SysBaseInfo.bKeNumberProcessors + 0.5;

			//printf("\b\b\b\b%3d%%",(UINT)dbIdleTime);
		}
/*		else
		{
			return 0;
		}
*/
		// store new CPU's idle and system time
		m_liOldIdleTime = m_SysPerfInfo.liIdleTime;
		m_liOldSystemTime =m_SysTimeInfo.liKeSystemTime;

		// wait one second
		//Sleep(1000);
	//}
	return (int)dbIdleTime;
}

BOOL CCpuMemRate::Initial(void)
{
	LONG status;
	NtQuerySystemInformation = (PROCNTQSI)GetProcAddress(GetModuleHandle("ntdll"),"NtQuerySystemInformation");

	if (!NtQuerySystemInformation)
		return FALSE;

	// get number of processors in the system
	status = NtQuerySystemInformation(SystemBasicInformation,&m_SysBaseInfo,sizeof(m_SysBaseInfo),NULL);
	if (status != NO_ERROR)
		return FALSE;
	return TRUE;
}
/*MEMORYSTATUS stat;

GlobalMemoryStatus (&stat);

printf ("The MemoryStatus structure is %ld bytes long.\n",
stat.dwLength);
printf ("It should be %d.\n", sizeof (stat));
printf ("%ld percent of memory is in use.\n",
stat.dwMemoryLoad);
printf ("There are %*ld total %sbytes of physical memory.\n",
WIDTH, stat.dwTotalPhys/DIV, divisor);
printf ("There are %*ld free %sbytes of physical memory.\n",
WIDTH, stat.dwAvailPhys/DIV, divisor);
printf ("There are %*ld total %sbytes of paging file.\n",
WIDTH, stat.dwTotalPageFile/DIV, divisor);
printf ("There are %*ld free %sbytes of paging file.\n",
WIDTH, stat.dwAvailPageFile/DIV, divisor);
printf ("There are %*lx total %sbytes of virtual memory.\n",
WIDTH, stat.dwTotalVirtual/DIV, divisor);
printf ("There are %*lx free %sbytes of virtual memory.\n",
WIDTH, stat.dwAvailVirtual/DIV, divisor);*/

int CCpuMemRate::GetPhysicalMem()
{
	GlobalMemoryStatus(&m_stat);
	return (int)(m_stat.dwTotalPhys/DIV/DIV);
}

int CCpuMemRate::GetMemPerUsed()
{
	::GlobalMemoryStatus(&m_stat);
	return (int)(m_stat.dwMemoryLoad);
}

int CCpuMemRate::GetPhysicalFreeMem()
{
	GlobalMemoryStatus (&m_stat);
	return (int)(m_stat.dwAvailPhys/DIV/DIV);
}
int CCpuMemRate::GetPhysUsedMem()
{
	GlobalMemoryStatus (&m_stat);
	return (int)(m_stat.dwTotalPhys/DIV/DIV - m_stat.dwAvailPhys/DIV/DIV);
}
int CCpuMemRate::GetVirtualMem()
{
	GlobalMemoryStatus (&m_stat);
	return (int)(m_stat.dwTotalVirtual/DIV/DIV);
}

int CCpuMemRate::GetFreeVirtualMem()
{
	GlobalMemoryStatus (&m_stat);
	return (int)(m_stat.dwAvailVirtual/DIV/DIV);
}
