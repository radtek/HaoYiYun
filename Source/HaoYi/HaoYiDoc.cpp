
#include "stdafx.h"
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "HaoYi.h"
#endif

#include "HaoYiDoc.h"

#include <propkey.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CHaoYiDoc, CDocument)

BEGIN_MESSAGE_MAP(CHaoYiDoc, CDocument)
END_MESSAGE_MAP()

CHaoYiDoc::CHaoYiDoc()
{
}

CHaoYiDoc::~CHaoYiDoc()
{
}

BOOL CHaoYiDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: 在此添加重新初始化代码
	// (SDI 文档将重用该文档)

	return TRUE;
}

void CHaoYiDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: 在此添加存储代码
	}
	else
	{
		// TODO: 在此添加加载代码
	}
}

#ifdef _DEBUG
void CHaoYiDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CHaoYiDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG
