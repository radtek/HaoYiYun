#pragma once
 
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <Winioctl.h>  

#define DIV 1024

#define SystemBasicInformation 0
#define SystemPerformanceInformation 2
#define SystemTimeInformation 3

#define Li2Double(x) ((double)((x).HighPart) * 4.294967296E9 + (double)((x).LowPart))

typedef struct
{
	DWORD dwUnknown1;
	ULONG uKeMaximumIncrement;
	ULONG uPageSize;
	ULONG uMmNumberOfPhysicalPages;
	ULONG uMmLowestPhysicalPage;
	ULONG uMmHighestPhysicalPage;
	ULONG uAllocationGranularity;
	PVOID pLowestUserAddress;
	PVOID pMmHighestUserAddress;
	ULONG uKeActiveProcessors;
	BYTE bKeNumberProcessors;
	BYTE bUnknown2;
	WORD wUnknown3;
} SYSTEM_BASIC_INFORMATION;

typedef struct
{
	LARGE_INTEGER liIdleTime;
	DWORD dwSpare[76];
} SYSTEM_PERFORMANCE_INFORMATION;

typedef struct
{
	LARGE_INTEGER liKeBootTime;
	LARGE_INTEGER liKeSystemTime;
	LARGE_INTEGER liExpTimeZoneBias;
	ULONG uCurrentTimeZoneId;
	DWORD dwReserved;
} SYSTEM_TIME_INFORMATION;

class CCpuMemRate
{
public:
	CCpuMemRate(void);
	virtual ~CCpuMemRate(void);
public:
	BOOL	Initial(void);
	int		GetCpuRate(void);

	int		GetPhysicalMem();
	int		GetMemPerUsed();
	int		GetPhysicalFreeMem();
	int		GetPhysUsedMem();
	int		GetVirtualMem();
	int		GetFreeVirtualMem();
private:
	typedef LONG (WINAPI *PROCNTQSI)(UINT,PVOID,ULONG,PULONG);
	PROCNTQSI NtQuerySystemInformation;
private:
	MEMORYSTATUS					m_stat;

	SYSTEM_PERFORMANCE_INFORMATION	m_SysPerfInfo;
	SYSTEM_TIME_INFORMATION			m_SysTimeInfo;
	SYSTEM_BASIC_INFORMATION		m_SysBaseInfo;

	LARGE_INTEGER					m_liOldIdleTime ;
	LARGE_INTEGER					m_liOldSystemTime ;
};
