////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2003	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Jackey
//	Date	:	2003.01.07
//	File	:	StringFormatter.h
//	Contains:	Define String Formatter Class...
//	History	:
//		1.0	:	2003.01.07 - First Version...
//		1.1 :	2003.03.03 - Add Put(StrPtrLen * inStr).
//		1.2 :	2003.05.06 - Add IsExcessedBuffer().
//		1.3 :	2003.05.23 - Add SetPrevBufLen(), GetPrevBufPtr() for special using.
//	Mailto	:	Omega@Kuihua.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef	_STRING_FORMATTER_H
#define	_STRING_FORMATTER_H

#include "StrPtrLen.h"

class StringFormatter
{
public:
	StringFormatter(const StringFormatter & theFormat) { *this = theFormat; }
	StringFormatter(char * buffer, UInt32 length);
	StringFormatter(StrPtrLen & buffer);
	~StringFormatter();
	
	void		Set(char *buffer, UInt32 length);
	void		Reset(UInt32 inNumBytesToLeave = 0)	{ fCurrentPut = fStartPut + inNumBytesToLeave; }
	//
	// Object does no bounds checking on the buffer. That is your responsibility!
	//
	void	 	Put(const SInt32 num);
	void		PutUInt(const UInt32 num);
	void		PutUI64(const UInt64 num);
	void		Put(char* buffer, UInt32 bufferSize);
	void		Put(char* str) 				{ Put(str, strlen(str)); }
	void		Put(const char * str)		{ Put((LPSTR)str, strlen(str)); }
	void		Put(StrPtrLen * inStr)		{ Put(inStr->Ptr, inStr->Len); }
	void		Put(const StrPtrLen &str)	{ Put(str.Ptr, str.Len); }
	void		Put(const string & str)		{ Put((LPSTR)str.c_str(), str.size()); }
	void		PutSpace() 			{ PutChar(' '); }
	void		PutEOL() 			{ Put(sEOL, sEOLLen); }
	void		PutChar(char c)		{ Put(&c, 1); }
	void		PutTerminator()		{ PutChar('\0'); }
	void		Put_u8(BYTE i)		{ Put((LPSTR)&i, 1); }
	void		Put_u16(WORD i)		{ Put_u8(i & 0xff); Put_u8((i >> 8) & 0xff); }
	void		Put_u32(DWORD i)	{ Put_u16(i & 0xffff); Put_u16((i >> 16) & 0xffff); }
	void		Put_u64(ULONGLONG i){ Put_u32(i & 0xffffffff); Put_u32((i >> 32) & 0xffffffff); }

	void		Put_u16X(WORD i)	 { Put_u8((i >> 8) & 0xff); Put_u8(i & 0xff); }
	void		Put_u24X(DWORD i)	 { Put_u8((i >> 16) & 0xff); Put_u16X(i & 0xffff); }
	void		Put_u32X(DWORD i)	 { Put_u16X((i >> 16) & 0xffff); Put_u16X(i & 0xffff); }
	void		Put_u64X(ULONGLONG i){ Put_u32X((i >> 32) & 0xffffffff); Put_u32X(i & 0xffffffff); }

	void		Put_str16(char *str);

	inline UInt32		GetCurrentOffset();
	inline UInt32		GetSpaceLeft();
	inline UInt32		GetTotalBufferSize();
	char *				GetCurrentPtr()		{ return fCurrentPut; }
	char *				GetBufPtr()			{ return fStartPut; }
	char *				GetPrevBufPtr()		{ return (fStartPut - fPrevBufLen); }
	//
	// Counts total bytes that have been written to this buffer (increments even when the buffer gets reset)
	//
	void				ResetBytesWritten()	{ fBytesWritten = 0; }
	UInt32				GetBytesWritten()	{ return fBytesWritten; }
	Bool16				IsExcessedBuffer()	{ return (fStartPut != fOriginalBuffer) ? TRUE : FALSE; }	
	void				SetPrevBufLen(UInt32 inPreLen) { fPrevBufLen = inPreLen; }
	StringFormatter	 &  operator=(const StringFormatter & newStr);
protected:
	//
	// If you fill up the StringFormatter buffer, this function will get called. By
	// default, no action is taken. But derived objects can clear out the data and reset the buffer
	void		BufferIsFull(char * inBuffer, UInt32 inBufferLen);
	char*		fOriginalBuffer;
	
	char *		fCurrentPut;
	char *		fStartPut;
	char *		fEndPut;
	UInt32		fBytesWritten;
	UInt32		fPrevBufLen;

	static char *	sEOL;
	static UInt32	sEOLLen;
};

inline UInt32 StringFormatter::GetCurrentOffset()
{
	ASSERT(fCurrentPut >= fStartPut);
	return (UInt32)(fCurrentPut - fStartPut);
}

inline UInt32 StringFormatter::GetSpaceLeft()
{
	ASSERT(fEndPut >= fCurrentPut);
	return (UInt32)(fEndPut - fCurrentPut);
}

inline UInt32 StringFormatter::GetTotalBufferSize()
{
	ASSERT(fEndPut >= fStartPut);
	return (UInt32)(fEndPut - fStartPut);
}

#endif	// _STRING_FORMATTER_H