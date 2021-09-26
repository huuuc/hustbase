#include <string.h>
#include "RM_Manager.h"
#include "PF_Manager.h"
#include "str.h"


RC OpenScan(RM_FileScan *rmFileScan, RM_FileHandle *fileHandle, int conNum, Con *conditions)//³õÊ¼»¯É¨Ãè
{
	rmFileScan->bOpen = true;
	rmFileScan->pRMFileHandle = fileHandle;//记录文件的句柄
	rmFileScan->conNum = conNum;
	rmFileScan->conditions = conditions;//没有复制
	//初始化2，-1，表示第一次寻找的位置前一个
	rmFileScan->PageHandle.bOpen = false;
	rmFileScan->PageHandle.pFrame = nullptr;
	rmFileScan->pn = 2;
	rmFileScan->sn = -1;
	return SUCCESS;
}

RC CloseScan(RM_FileScan* rmFileScan)
{
	rmFileScan->pn = rmFileScan->sn = -1;
	rmFileScan->PageHandle.bOpen = false;
	rmFileScan->PageHandle.pFrame = nullptr;
	rmFileScan->bOpen = false;
	return SUCCESS;
}

//辅助函数，获取下一条记录的位置，条件：rmFileScan已经打开
RC nextRec(RM_FileScan* rmFileScan)
{
	/*if (rmFileScan == nullptr || rmFileScan->bOpen == false)
		return FAIL;*/
	if (rmFileScan->pn == -1 && rmFileScan->sn == -1)
		return ;//表示没有记录了
	int i, j;
	PF_FileHandle* pf;
	int FileID = rmFileScan->pRMFileHandle->fileID;
	RC result = GetFileHandle(FileID, &pf);
	if (result != SUCCESS)
		return result;
	PF_PageHandle pageHandle;
	pageHandle.bOpen = false;
	for (i = rmFileScan->pn; i <= pf->pFileSubHeader->pageCount; i++)
	{
		if ((pf->pBitmap[i / 8] & (1 << (i % 8))) == 0)//忽略空闲页
			continue;
		GetThisPage(FileID, i, &pageHandle);
		if (i == rmFileScan->pn)    //如果是上次的页，则查找的槽下标+1
			j = rmFileScan->sn + 1;
		else
			j = 0;
		for (; j < rmFileScan->pRMFileHandle->fileSubHeader->recordsPerPage; j++)
		{
			if ((pageHandle.pFrame->page.pData[j / 8] & (1 << (j % 8))) == 0)//空槽
				continue;
			else
				break;
		}
		UnpinPage(&pageHandle);
		if (j < rmFileScan->pRMFileHandle->fileSubHeader->recordsPerPage)
			break;
	}
	if (i > pf->pFileSubHeader->pageCount)
	{
		rmFileScan->PageHandle.bOpen = false;
		rmFileScan->PageHandle.pFrame = nullptr;
		rmFileScan->pn = -1;
		rmFileScan->sn = -1;

		return RM_EOF;//表示没有记录了
	}
	else {
		rmFileScan->PageHandle = pageHandle;
		rmFileScan->pn = i;
		rmFileScan->sn = j;
	}
	return SUCCESS;
}

bool selected(Con* pCon, char* pData)
{
	//假设左右类型是一样的，只有一个AttrType
	//字符串都已正确处理
	switch (pCon->compOp)
	{
	case EQual:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue == rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue == rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) == 0;
		}
		break;
	case NEqual:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue != rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue != rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) != 0;
		}
		break;
	case LessT:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue < rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue < rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) < 0;
		}
		break;
	case LEqual:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue <= rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue <= rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) <= 0;
		}
		break;
	case GreatT:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue > rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue > rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) > 0;
		}
		break;
	case GEqual:
		if (pCon->attrType == ints)
		{
			int lvalue = pCon->bLhsIsAttr == 1 ? *(int*)(pData + pCon->LattrOffset) : *(int*)(pCon->Lvalue);
			int rvalue = pCon->bRhsIsAttr == 1 ? *(int*)(pData + pCon->RattrOffset) : *(int*)(pCon->Rvalue);
			return lvalue >= rvalue;
		}
		else if (pCon->attrType == floats)
		{
			float lvalue = pCon->bLhsIsAttr == 1 ? *(float*)(pData + pCon->LattrOffset) : *(float*)(pCon->Lvalue);
			float rvalue = pCon->bRhsIsAttr == 1 ? *(float*)(pData + pCon->RattrOffset) : *(float*)(pCon->Rvalue);
			return lvalue >= rvalue;
		}
		else {
			char* lvalue = pCon->bLhsIsAttr == 1 ? pData + pCon->LattrOffset : (char*)(pCon->Lvalue);
			char* rvalue = pCon->bRhsIsAttr == 1 ? pData + pCon->RattrOffset : (char*)(pCon->Rvalue);
			return strcmp(lvalue, rvalue) > 0;
		}
		break;
	case NO_OP:
		break;
	default:
		break;
	}
	return false;
}

RC GetNextRec(RM_FileScan *rmFileScan, RM_Record *rec)
{
	int offset = rmFileScan->pRMFileHandle->fileSubHeader->firstRecordOffset;
	int size = rmFileScan->pRMFileHandle->fileSubHeader->recordSize;
	char *pData = nullptr;
	RC result;
	while ((result = nextRec(rmFileScan)) == SUCCESS) {	// 找到了一条记录
		//测试该页中的sn记录是否满足条件数组，符合返回
		//不符合，修改rmFileScan找到下一条记录,使用循环找到符合的一条，否则返回RM_EOF;
		GetThisPage(rmFileScan->pRMFileHandle->fileID, rmFileScan->pn, &rmFileScan->PageHandle);
		pData = rmFileScan->PageHandle.pFrame->page.pData + offset + rmFileScan->sn * size;//可能的数据地址
		if (rmFileScan->conNum == 0)
			break;
		int i;
		for (i = 0; i < rmFileScan->conNum; i++)	// 遍历条件总数
		{
			Con* pCon = rmFileScan->conditions + i;
			if (!selected(pCon, pData))
			{
				UnpinPage(&rmFileScan->PageHandle);
				break;//要全部满足
			}
		}
		if (i >= rmFileScan->conNum)//满足才输出，否则选择下一条记录
			break;
	}

	if (result != SUCCESS)
		return result;
	rec->bValid = true;
	//可以直接赋值pData
	rec->pData = pData;
	UnpinPage(&rmFileScan->PageHandle);
	rec->rid.bValid = true;
	rec->rid.pageNum = rmFileScan->pn;
	rec->rid.slotNum = rmFileScan->sn;
	return SUCCESS;
}

RC GetRec(RM_FileHandle *fileHandle, RID *rid, RM_Record *rec)
{
	int FileID = fileHandle->fileID;
	PF_FileHandle *pf;
	RC result = GetFileHandle(FileID, &pf);
	if (result != SUCCESS)
		return result;

	if (rid->pageNum <= 1 || rid->pageNum > pf->pFileSubHeader->pageCount)
		return RM_INVALIDRID;
	if ((pf->pBitmap[rid->pageNum / 8] & (1 << (rid->pageNum % 8))) == 0)//空闲页（空的）返回失败
		return RM_INVALIDRID;

	PF_PageHandle pageHandle;
	pageHandle.bOpen = false;
	result = GetThisPage(FileID, rid->pageNum, &pageHandle);
	if (result != SUCCESS)
		return result;
	if (((pageHandle.pFrame->page.pData[rid->slotNum / 8] & (1 << (rid->slotNum % 8))) == 0))
		return RM_INVALIDRID;

	int offset = ((RM_FileSubHeader*)fileHandle->pHdrPage->pData)->firstRecordOffset;
	int size = ((RM_FileSubHeader*)fileHandle->pHdrFrame->page.pData)->recordSize;
	rec->bValid = true;
	rec->rid = *rid;
	rec->rid.bValid = true;
	rec->pData = pageHandle.pFrame->page.pData + offset + (rid->slotNum) * size;//只返回记录地址
	UnpinPage(&pageHandle);
	return SUCCESS;
}

RC InsertRec(RM_FileHandle *fileHandle, char *pData, RID *rid)
{
	int FileID = fileHandle->fileID;
	int pageCount;
	bool isFull;	//判断插入记录后是否为满页
	RC result = GetPageCount(FileID, &pageCount);
	if (result != SUCCESS)
		return result;

	// 找到非满页
	int i;	//第i页为记录非满页
	int j;	//寻找未满页的第一个空记录槽，第j个位置为空
	int k;	//查找这个记录槽之后有无空的记录槽
	PF_FileHandle *pf_fileHandle;
	result = GetFileHandle(FileID, &pf_fileHandle);
	for (i = 0; i <= pageCount; i++) {
		if(((pf_fileHandle->pBitmap[i / 8] & (1 << (i % 8))) != 0) && ((fileHandle->pBitmap[i / 8] & (1 << (i % 8))) == 0))
			break;
	}

	int offset = fileHandle->fileSubHeader->firstRecordOffset;
	int size = fileHandle->fileSubHeader->recordSize;
	int recordsPerPage = fileHandle->fileSubHeader->recordsPerPage;
	PF_PageHandle pageHandle;
	pageHandle.bOpen = false;
	if (i <= pageCount) {
		result = GetThisPage(FileID, i, &pageHandle);
		if (result != SUCCESS)
			return result;

		for (j = 0; j < recordsPerPage; j++) {
			if (pageHandle.pFrame->page.pData[j / 8] & (1 << (j % 8)) == 0)
				break;
		}

		for (k = j + 1; k < recordsPerPage; k++) {
			if (pageHandle.pFrame->page.pData[k / 8] & (1 << (k % 8)) == 0)
				break;
		}
		if (k >= recordsPerPage)
			isFull = true;
		pageHandle.pFrame->page.pData[j / 8] |= (1 << (j % 8));
		memcpy(pageHandle.pFrame->page.pData + offset + j * size, pData, size);
		UnpinPage(&pageHandle);
		MarkDirty(&pageHandle);
		rid->bValid = true;
		rid->pageNum = i;
		rid->slotNum = j;
	}
	else {
		result = AllocatePage(FileID, &pageHandle);
		if (result != SUCCESS)
			return result;
		pageHandle.pFrame->page.pData[0] |= 1;
		if (recordsPerPage == 1)
			isFull = true;
		memcpy(pageHandle.pFrame->page.pData + offset, pData, size);
		MarkDirty(&pageHandle);
		UnpinPage(&pageHandle);
		rid->bValid = true;
		rid->pageNum = pageHandle.pFrame->page.pageNum;
		rid->slotNum = 0;
	}
	fileHandle->fileSubHeader->nRecords++;
	fileHandle->pHdrFrame->bDirty = true;
	//若isFull==true,置位示图相应位置为1
	if (isFull)
	{
		int pageNum = pageHandle.pFrame->page.pageNum;
		fileHandle->pBitmap[pageNum / 8] |= (1 << (pageNum % 8));
		//pf_fileHandle->pHdrFrame->bDirty = true;
	}
	return SUCCESS;
}

RC DeleteRec(RM_FileHandle *fileHandle, const RID *rid)
{
	PF_FileHandle* pf;//对应页面文件的句柄
	int FileID = fileHandle->fileID;
	RC result = GetFileHandle(FileID, &pf);
	if (result != SUCCESS)
		return result;
	if (rid->pageNum <= 1 || rid->pageNum > pf->pFileSubHeader->pageCount)//大于总页数
		return RM_INVALIDRID;
	if ((pf->pBitmap[rid->pageNum / 8] & (1 << (rid->pageNum % 8))) == 0)//空闲页（空的）返回失败
		return RM_INVALIDRID;
	PF_PageHandle pageHandle;
	pageHandle.bOpen = false;
	result = GetThisPage(FileID, rid->pageNum, &pageHandle);
	if (result != SUCCESS)
		return result;
	if (((pageHandle.pFrame->page.pData[rid->slotNum / 8] & (1 << (rid->slotNum % 8))) == 0))//插槽未使用
		return RM_INVALIDRID;
	//槽位置0，置为非满页0，总记录数-1
	pageHandle.pFrame->page.pData[rid->slotNum / 8] &= (~(1 << (rid->slotNum % 8)));
	fileHandle->fileSubHeader->nRecords--;
	fileHandle->pBitmap[rid->pageNum / 8] ^= (1 << (rid->pageNum % 8));
	MarkDirty(&pageHandle);
	fileHandle->pHdrFrame->bDirty = true;//没有构造pageHandle,直接置为dirty
	UnpinPage(&pageHandle);
	return SUCCESS;
}

RC UpdateRec(RM_FileHandle *fileHandle, const RM_Record *rec)
{
	PF_FileHandle* pf;
	int FileID = fileHandle->fileID;
	RC result = GetFileHandle(FileID, &pf);
	if (rec->rid.pageNum <= 1 || rec->rid.pageNum > pf->pFileSubHeader->pageCount)//大于总页数
		return RM_INVALIDRID;
	if ((pf->pBitmap[rec->rid.pageNum / 8] & (1 << (rec->rid.pageNum % 8))) == 0)//空闲页（空的）返回失败
		return RM_INVALIDRID;
	PF_PageHandle pageHandle;
	pageHandle.bOpen = false;
	result = GetThisPage(FileID, rec->rid.pageNum, &pageHandle);
	if (result != SUCCESS)
		return result;
	if (((pageHandle.pFrame->page.pData[rec->rid.slotNum / 8] & (1 << (rec->rid.slotNum % 8))) == 0))//插槽未使用
		return RM_INVALIDRID;
	//修改该条记录
	int offset = ((RM_FileSubHeader*)fileHandle->pHdrPage->pData)->firstRecordOffset;
	int size = ((RM_FileSubHeader*)fileHandle->pHdrFrame->page.pData)->recordSize;
	memcpy(pageHandle.pFrame->page.pData + offset + rec->rid.slotNum * size, rec->pData, size);
	MarkDirty(&pageHandle);
	UnpinPage(&pageHandle);
	return SUCCESS;
}

RC RM_CreateFile(char *fileName, int recordSize)
{
	RC result = CreateFile(fileName);
	if (result != SUCCESS)
		return result;

	int FileID;
	result = OpenFile(fileName, &FileID);
	if (result != SUCCESS)
		return result;

	PF_PageHandle *pageHandle = (PF_PageHandle*)malloc(sizeof(PF_PageHandle));
	result = AllocatePage(FileID, pageHandle);
	if (result != SUCCESS)
		return result;
	
	RM_FileSubHeader *rmFileSubHeader = (RM_FileSubHeader*)pageHandle->pFrame->page.pData;
	rmFileSubHeader->nRecords = 0;
	rmFileSubHeader->recordSize = recordSize;
	rmFileSubHeader->recordsPerPage = (int)((PF_PAGESIZE - 4) * 8 - 7) / (1 + 8 * recordSize);
	rmFileSubHeader->firstRecordOffset = (rmFileSubHeader->recordsPerPage - 1) / 8 + 1;
	char *bitmap = pageHandle->pFrame->page.pData + (int)sizeof(RM_FileSubHeader);
	bitmap[0] |= 0x3;

	result = CloseFile(FileID);
	if (result != SUCCESS)
		return result;
	result = MarkDirty(pageHandle);
	if (result != SUCCESS)
		return result;

	return SUCCESS;
}

RC RM_OpenFile(char *fileName, RM_FileHandle *fileHandle)
{
	int FileID;
	RC result = OpenFile(fileName, &FileID);
	if (result != SUCCESS)
		return result;

	PF_PageHandle* pageHandle = (PF_PageHandle*)malloc(sizeof(PF_PageHandle));
	pageHandle->bOpen = false;
	result = GetThisPage(FileID, 1, pageHandle);
	if (result != SUCCESS)
		return result;


	fileHandle->bOpen = true;
	fileHandle->fileName = fileName;
	fileHandle->fileID = FileID;
	fileHandle->pHdrFrame = pageHandle->pFrame;
	fileHandle->pHdrPage = &(pageHandle->pFrame->page);
	fileHandle->pBitmap = pageHandle->pFrame->page.pData + sizeof(RM_FileSubHeader);
	fileHandle->fileSubHeader = (RM_FileSubHeader *)pageHandle->pFrame->page.pData;

	free(pageHandle);
	return SUCCESS;
}

RC RM_CloseFile(RM_FileHandle *fileHandle)
{
	fileHandle->pHdrFrame->pinCount--;
	RC result = CloseFile(fileHandle->fileID);
	if (result != SUCCESS)
		return result;
	fileHandle->bOpen = false;

	return SUCCESS;
}