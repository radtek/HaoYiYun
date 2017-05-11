////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Copyright (C) 2002	Kuihua Tech Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Author	:	Omega
//	Date	:	2002.09.06
//	File	:	StrPtrLen.h
/*	Contains:	Definition of class that tracks a string ptr and a length.
				Note: this is NOT a string container class! It is a string PTR container
				class. It therefore does not copy the string and store it internally. If
				you deallocate the string to which this object points to, and continue 
				to use it, you will be in deep doo-doo.
				
				It is also non-encapsulating, basically a struct with some simple methods.
*/
//	History	:
//		1.0	:	2002.09.06 - First Version...
//		1.1	:	2003.02.25 - Modify Equal() function...
//	Mailto	:	weiwei_263@263.net (Bug Report or Comments)
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef	_STRPTRLEN_H
#define	_STRPTRLEN_H

//#include "OSHeaders.h"

#define STRPTRLENTESTING 0

class StrPtrLen
{
	public:
		StrPtrLen();
		StrPtrLen(char * sp);
		StrPtrLen(char * sp, UINT len);
		~StrPtrLen();
		//
		//OPERATORS:
		//
		BOOL Equal(const StrPtrLen &compare) const;
		BOOL Equal(const char* compare, const UINT len) const;
		BOOL EqualIgnoreCase(const char* compare, const UINT len) const;
		BOOL EqualIgnoreCase(const StrPtrLen &compare) const { return EqualIgnoreCase(compare.Ptr, compare.Len); }
		BOOL NumEqualIgnoreCase(const char* compare, const UINT len) const;
		
		//void Delete();
		char *ToUpper();
		char *ToLower();
		
		char *FindStringCase(char *queryCharStr, StrPtrLen *resultStr, BOOL caseSensitive) const;

		char *FindString(StrPtrLen *queryStr, StrPtrLen *outResultStr);
		
		char *FindStringIgnoreCase(StrPtrLen *queryStr, StrPtrLen *outResultStr);

		char *FindString(StrPtrLen *queryStr);
		
		char *FindStringIgnoreCase(StrPtrLen *queryStr);
																					
		char *FindString(char *queryCharStr);
		char *FindStringIgnoreCase(char *queryCharStr);
		char *FindString(char *queryCharStr, StrPtrLen *outResultStr);
		char *FindStringIgnoreCase(char *queryCharStr, StrPtrLen *outResultStr);

		char *FindString(StrPtrLen &query, StrPtrLen *outResultStr);
		char *FindStringIgnoreCase(StrPtrLen &query, StrPtrLen *outResultStr);
		char *FindString(StrPtrLen &query);
		char *FindStringIgnoreCase(StrPtrLen &query);
		
		StrPtrLen& operator=(const StrPtrLen& newStr);
        char operator[](int i);
		void Set(char* inPtr, UINT inLen);
		void Set(char* inPtr);
		char* GetAsCString() const;					// convert to a "NEW'd" zero terminated char array
		//
		//This is a non-encapsulating interface. The class allows you to access its data.
		//
		char* 		Ptr;
		UINT		Len;
#if STRPTRLENTESTING
		static BOOL	Test();
#endif
	private:
		static BYTE 	sCaseInsensitiveMask[];
};
#endif	// _STRPTRLEN_H