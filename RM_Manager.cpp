#include <string.h>
#include "RM_Manager.h"
#include "PF_Manager.h"
#include "str.h"


RC OpenScan(RM_FileScan *rmFileScan, RM_FileHandle *fileHandle, int conNum, Con *conditions)//³õÊ¼»¯É¨Ãè
{
	return FAIL;
}

RC CloseScan(RM_FileScan* rmFileScan) {
	return FAIL;
}
RC GetNextRec(RM_FileScan *rmFileScan, RM_Record *rec)
{
	return FAIL;
}

RC GetRec(RM_FileHandle *fileHandle, RID *rid, RM_Record *rec)
{
	return FAIL;
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