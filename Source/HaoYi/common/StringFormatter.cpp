
#include "StdAfx.h"
#include "StringFormatter.h"

char*	StringFormatter::sEOL = "\r\n";
UInt32	StringFormatter::sEOLLen = 2;

StringFormatter::StringFormatter(char *buffer, UInt32 length)
  :	fCurrentPut(buffer), 
	fStartPut(buffer),
	fEndPut(buffer + length),
	fBytesWritten(0),
	fOriginalBuffer(buffer),
	fPrevBufLen(0)
{
}

StringFormatter::StringFormatter(StrPtrLen &buffer)
  :	fCurrentPut(buffer.Ptr),
	fStartPut(buffer.Ptr),
	fEndPut(buffer.Ptr + buffer.Len),
	fBytesWritten(0),
	fOriginalBuffer(buffer.Ptr),
	fPrevBufLen(0)
{
}

StringFormatter::~StringFormatter()
{
	if(fStartPut != fOriginalBuffer)
	{
		delete [] (fStartPut - fPrevBufLen);
		fStartPut = NULL;
	}
}

void StringFormatter::Set(char *buffer, UInt32 length) 	
{	
	fOriginalBuffer = buffer;
	fCurrentPut = buffer; 
	fStartPut = buffer;
	fEndPut = buffer + length;
	fBytesWritten= 0;
}

void StringFormatter::Put(const SInt32 num)
{
	char buff[32] = {0};
	sprintf(buff, "%ld", num);
	Put(buff);
}

void StringFormatter::PutUInt(const UInt32 num)
{
	char buff[32] = {0};
	sprintf(buff, "%u", num);
	Put(buff);
}

void StringFormatter::PutUI64(const UInt64 num)
{
	char buff[32] = {0};
	sprintf(buff, "%I64d", num);
	Put(buff);
}

void StringFormatter::Put(char* buffer, UInt32 bufferSize)
{
	if((bufferSize == 1) && (fCurrentPut != fEndPut)) 
	{
		*(fCurrentPut++) = *buffer;
		fBytesWritten++;
		return;
	}
	//
	// Loop until the input buffer size is smaller than the space in the output buffer.
	// Call BufferIsFull at each pass through the loop.
	UInt32 spaceInBuffer = 0;
	while (((spaceInBuffer = (this->GetSpaceLeft() - 0)) < bufferSize) || (this->GetSpaceLeft() == 0))
	{
		if (this->GetSpaceLeft() > 0)
		{
			::memcpy(fCurrentPut, buffer, spaceInBuffer);
			fCurrentPut += spaceInBuffer;
			fBytesWritten += spaceInBuffer;
			buffer += spaceInBuffer;
			bufferSize -= spaceInBuffer;
		}
		this->BufferIsFull(fStartPut, this->GetCurrentOffset());
	}
	//
	// Copy the remaining chunk into the buffer
	//
	::memcpy(fCurrentPut, buffer, bufferSize);
	fCurrentPut += bufferSize;
	fBytesWritten += bufferSize;
}

void StringFormatter::BufferIsFull(char * inBuffer, UInt32 inBufferLen)
{
	//
	// 1.0 Allocate a buffer twice as big as the old one, and copy over the contents
	// (Should Append Previous Length)[2003.05.23]
	ASSERT( fPrevBufLen >= 0 );
	UInt32 theNewBufferSize = this->GetTotalBufferSize() * 2;
	theNewBufferSize  = (theNewBufferSize == 0) ? 64 : theNewBufferSize;
	theNewBufferSize += fPrevBufLen;

	char * theNewBuffer = new char[theNewBufferSize];
	::memset(theNewBuffer, 0, theNewBufferSize);
	::memcpy(theNewBuffer, inBuffer - fPrevBufLen, fPrevBufLen);
	::memcpy(theNewBuffer + fPrevBufLen, inBuffer, inBufferLen);
	//
	// 2.0 If the old buffer was dynamically allocated also, we'd better delete it.
	if (inBuffer != fOriginalBuffer)
	{
		delete [] (inBuffer - fPrevBufLen);
		inBuffer = NULL;
	}
	fStartPut	= theNewBuffer + fPrevBufLen;
	fCurrentPut = fStartPut	+ inBufferLen;
	fEndPut		= fStartPut + (theNewBufferSize - fPrevBufLen);
}

void StringFormatter::Put_str16(char *str)
{
	for( ;; )
	{
		WORD c = (BYTE)*str++;
		this->Put_u16(c);
		if( c == '\0' )
			break;
	}
}

StringFormatter & StringFormatter::operator=(const StringFormatter & newStr)
{
	fOriginalBuffer = newStr.fOriginalBuffer;
	fCurrentPut = newStr.fCurrentPut; 
	fStartPut = newStr.fStartPut;
	fEndPut = newStr.fEndPut;
	fBytesWritten = newStr.fBytesWritten;
	fPrevBufLen = newStr.fPrevBufLen;
	return *this;
}
