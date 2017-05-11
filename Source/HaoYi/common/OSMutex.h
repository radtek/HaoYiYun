////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2003	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.01.07
//	File	:	OSMutex.h
//	Contains:	Define the Win32 Platform Mutex Class...
//	History	:
//		1.0	:	2003.01.07 - First Version...
//		1.1 :	2003.03.13 - Remove OSThread.h, For Normal Use...
//	Mailto	:	Omega@Kuihua.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef	_OSMUTEX_H
#define	_OSMUTEX_H

class OSMutex
{
public:
	OSMutex(BOOL bTrace = false);
	~OSMutex();

	void		Lock(LPCTSTR lpFile, int nLine)	{ this->RecursiveLock(lpFile, nLine); }
	void		Unlock()						{ this->RecursiveUnlock(); }
	UInt32		GetHoldCount()					{ return fHolderCount; }
	//	
	// Returns true on successful grab of the lock, false on failure
	Bool16		TryLock()	{ return this->RecursiveTryLock(); }
private:
	CRITICAL_SECTION	fMutex;
	DWORD				fHolder;
	UInt32				fHolderCount;
	BOOL				fTrace;
		
	void		RecursiveLock(LPCTSTR lpFile, int nLine);
	void		RecursiveUnlock();
	Bool16		RecursiveTryLock();
};

class OSMutexLocker
{
public:
	
	OSMutexLocker(OSMutex *inMutexP, LPCTSTR lpFile = NULL, int nLine = 0) : fMutex(inMutexP) { if (fMutex != NULL) fMutex->Lock(lpFile, nLine); }
	~OSMutexLocker()	{ if (fMutex != NULL) fMutex->Unlock(); }
	
	//void Lock() 		{ if (fMutex != NULL) fMutex->Lock(); }
	//void Unlock() 	{ if (fMutex != NULL) fMutex->Unlock(); }
private:
	OSMutex *	fMutex;
};

#define OS_MUTEX_LOCKER(A) OSMutexLocker theLock(A, __FILE__, __LINE__)

#endif	// _OSMUTEX_H