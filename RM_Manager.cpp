#include <string.h>
#include "RM_Manager.h"
#include "PF_Manager.h"
#include "str.h"

extern PF_FileHandle * open_list;

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
	RC result = GetPageCount(FileID, &pageCount);
	if (result != SUCCESS)
		return result;

	// 找到非满页
	int i;	//第i页为记录非满页
	PF_FileHandle *fileHandle = open_list[FileID];
	for (i = 0; i <= pageCount; i++) {
		if(((fileHandle->pBitmap[i / 8] & (1 << (i % 8))) != 0) && ((fileHandle->pBitmap[i / 8] & (1 << (i % 8))) == 0))
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

	}
	return SUCCESS;
}

RC DeleteRec(RM_FileHandle *fileHandle, const RID *rid)
{
	return FAIL;
}

RC UpdateRec(RM_FileHandle *fileHandle, const RM_Record *rec)
{
	return FAIL;
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