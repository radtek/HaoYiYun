////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2003	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.01.07
//	File	:	OSThread.h
//	Contains:	Define the Win32 Operate System Thread Base Class...
//	History	:
//		1.0	:	2003.01.07 - First Version...
//		1.1 :	2003.01.15 - Add Thread Mark tag for PostThreadMessage()
//			:	2003.06.16 - Add GetDateInFormat()
//	Mailto	:	Omega@Kuihua.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _OSTHREAD_H
#define	_OSTHREAD_H

#include "StrPtrLen.h"

class OSThread
{
public:
	static	void		Initialize();								// OSThread Static Initialize 
	static	void		UnInitialize();
	static	OSThread *	GetCurrent();								// Get Current Thread Pointer.
	static	GM_Error	GetOSErr() { return ::GetLastError(); }
	static	DWORD		GetCurrentThreadID() { return ::GetCurrentThreadId(); }

	virtual void		Entry() = 0;								// Thread Virtual Entry. 
			void 		Start();									// Thread Start Point.
			void		StopAndWaitForThread();						// Stop And Wait.
			void		SendStopRequest()	{ fStopRequested = ((fThreadID != NULL) ? TRUE : FALSE); }
			BOOL		IsStopRequested()	{ return fStopRequested; }
			HANDLE		GetThreadHandle()	{ return fThreadID; }	// Get Thread Handle.
			DWORD		GetThreadMark()		{ return fThreadMark; }
			int			GetThreadPriority() { return ::GetThreadPriority(fThreadID); }
			BOOL		SetThreadPriority(int nPriority) { return ::SetThreadPriority(fThreadID, nPriority); }

			virtual 	~OSThread();
						OSThread();
protected: 
			BOOL		fStopRequested;								// Stop Flag
			HANDLE		fThreadID;									// Thread Handle.
private:
			
			UINT		fThreadMark;								// Thread Mark For Thread Message
			BOOL		fJoined;									// We have Exit Flag		
	static	DWORD		sThreadStorageIndex;						// Thread Storage Index
	static	UINT WINAPI _Entry(LPVOID inThread);					// Static Thread Entry.
};

#endif	// _OSTHREAD_H