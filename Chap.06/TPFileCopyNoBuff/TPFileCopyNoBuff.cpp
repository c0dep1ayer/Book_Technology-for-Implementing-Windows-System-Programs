#include "stdafx.h"
#include "windows.h"
#include "iostream"
using namespace std;

#define DEF_BUFF_SIZE 65536

static SIZE_T PAGE_SIZE;
static SIZE_T BUFF_SIZE;

struct COPY_CHUNK
{
	OVERLAPPED	_ov;
	PVOID		_buff;
};
typedef COPY_CHUNK* PCOPY_CHUNK;

struct COPY_ENV
{
	PCOPY_CHUNK	  _pcpBufs;
	SIZE_T		  _cntBufs;
	HANDLE		  _hfSrc, _hfDst;
	HANDLE		  _hevComp;
	PTP_IO		  _prio, _pwio;
	LARGE_INTEGER _fsize, _offset;
	LONG		  _rpend, _wpend;
	DWORD		  _status;
};
typedef COPY_ENV* PCOPY_ENV;

///////////////////////////////////////////////////////////////////////////////
// 
// This is the callback function that is bound to the PTP_IO object 
// that's used to process read I/O completion events.  This means that
// the thread pool will call this function when a read I/O completes.
// 
///////////////////////////////////////////////////////////////////////////////
VOID CALLBACK ReadIoCompleteCB(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PCOPY_ENV pce = PCOPY_ENV(pCtx);
	PCOPY_CHUNK pcc = PCOPY_CHUNK(pov);

	if (ior != NO_ERROR)
	{
		cerr << "Read ptpIo error occurred, code " << ior << endl;
		goto $LABEL_ERROR;
	}

	--pce->_rpend;
	dwTrBytes = (dwTrBytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	++pce->_wpend;

	StartThreadpoolIo(pce->_pwio);
	BOOL bIsOK = WriteFile(pce->_hfDst, pcc->_buff,
		DWORD(dwTrBytes), NULL, &pcc->_ov);
	if (!bIsOK && (GetLastError() != ERROR_IO_PENDING))
	{
		ior = GetLastError();
		cerr << " WriteFile at " << pcc->_ov.Offset << " failed, error " << ior << endl;
		CancelThreadpoolIo(pce->_pwio);
		goto $LABEL_ERROR;
	}
	return;

$LABEL_ERROR:
	if (ior != ERROR_OPERATION_ABORTED)
	{
		CancelIoEx(pce->_hfSrc, NULL);
		CancelIoEx(pce->_hfDst, NULL);
		DisassociateCurrentThreadFromCallback(pInst);

		pce->_status = (DWORD)ior;
		WaitForThreadpoolIoCallbacks(pce->_prio, TRUE);
		WaitForThreadpoolIoCallbacks(pce->_pwio, TRUE);

		pce->_rpend = pce->_wpend = 0;
		SetEventWhenCallbackReturns(pInst, pce->_hevComp);
	}
}

///////////////////////////////////////////////////////////////////////////////
// 
// This is the callback function that is bound to the PTP_IO object 
// that's used to process write I/O completion events.  This means that
// the thread pool will call this function when a write I/O completes.
//
///////////////////////////////////////////////////////////////////////////////
VOID CALLBACK WriteIoCompleteCB(PTP_CALLBACK_INSTANCE pInst,
	PVOID pCtx, PVOID pov, ULONG ior, ULONG_PTR dwTrBytes, PTP_IO ptpIo)
{
	PCOPY_ENV pce = PCOPY_ENV(pCtx);
	PCOPY_CHUNK pcc = PCOPY_CHUNK(pov);

	if (ior != NO_ERROR)
	{
		cerr << "Write ptpIo error occurred, code " << ior << endl;
		goto $LABEL_ERROR;
	}
	if (pce->_status != ERROR_SUCCESS)
		return;

	printf(" <= Thr %d copy bytes : %I64d\n", GetCurrentThreadId(), pce->_offset.QuadPart);
	--pce->_wpend;

	if (pce->_offset.QuadPart < pce->_fsize.QuadPart)
	{
		pcc->_ov.Offset = pce->_offset.LowPart;
		pcc->_ov.OffsetHigh = pce->_offset.HighPart;
		pce->_offset.QuadPart += BUFF_SIZE;
		++pce->_rpend;

		StartThreadpoolIo(pce->_prio);
		BOOL bIsOK = ReadFile(pce->_hfSrc, pcc->_buff,
			DWORD(BUFF_SIZE), NULL, &pcc->_ov);
		if (!bIsOK && (GetLastError() != ERROR_IO_PENDING))
		{
			ior = GetLastError();
			cerr << " ReadFile at " << pcc->_ov.Offset << " failed, error " << ior << endl;
			CancelThreadpoolIo(pce->_prio);
			goto $LABEL_ERROR;
		}
	}
	else
	{
		if ((pce->_rpend <= 0) && (pce->_wpend <= 0))
			SetEventWhenCallbackReturns(pInst, pce->_hevComp);
	}
	return;

$LABEL_ERROR:
	if (ior != ERROR_OPERATION_ABORTED)
	{
		CancelIoEx(pce->_hfSrc, NULL);
		CancelIoEx(pce->_hfDst, NULL);
		DisassociateCurrentThreadFromCallback(pInst);

		pce->_status = (DWORD)ior;
		WaitForThreadpoolIoCallbacks(pce->_prio, TRUE);
		WaitForThreadpoolIoCallbacks(pce->_pwio, TRUE);

		pce->_rpend = pce->_wpend = 0;
		SetEventWhenCallbackReturns(pInst, pce->_hevComp);
	}
}

//////////////////////////////////////////////////////////////////
// 
// Work object callback function used to kick off the initial 
// set of asynchronous reads to do the copy.
// 
//////////////////////////////////////////////////////////////////

VOID CALLBACK KickoffCopyIo(PTP_CALLBACK_INSTANCE pInst, PVOID pCtx, PTP_WORK)
{
	PCOPY_ENV pce = PCOPY_ENV(pCtx);
	PCOPY_CHUNK pCurr = pce->_pcpBufs;
	for (SIZE_T i = 0; i < pce->_cntBufs; i++, pCurr++)
	{
		pce->_wpend++;
		WriteIoCompleteCB(pInst, pCtx, &pCurr->_ov, NO_ERROR, 0, pce->_pwio);
		if (pce->_status != ERROR_SUCCESS)
			return;
	}
}


HANDLE g_hevExit;
BOOL CtrlHandler(DWORD fdwCtrlType)
{
	if (g_hevExit != NULL)
		SetEvent(g_hevExit);
	return TRUE;
}

DWORD CopyFileUsingTP(HANDLE hfSrc, HANDLE hfDst, LARGE_INTEGER fSize, PCOPY_CHUNK parCpBufs, SIZE_T cntBufs)
{
	DWORD 				dwRlt = ERROR_SUCCESS;
	PTP_POOL 			pPool = NULL;
	PTP_CLEANUP_GROUP 	pcg = NULL;
	PTP_WORK 			pWork = NULL;
	COPY_ENV 			ce = { 0 };

	TP_CALLBACK_ENVIRON cbe;
	InitializeThreadpoolEnvironment(&cbe);

	ce._hfSrc = hfSrc;
	ce._hfDst = hfDst;
	ce._fsize.QuadPart = fSize.QuadPart;
	ce._pcpBufs = parCpBufs;
	ce._cntBufs = cntBufs;

	__try
	{
		ce._hevComp = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (ce._hevComp == NULL)
		{
			dwRlt = GetLastError();
			cerr << "CreateEvent failed with error " << dwRlt << endl;
			__leave;
		}

		pPool = CreateThreadpool(NULL);
		if (!pPool)
		{
			dwRlt = GetLastError();
			cerr << "CreateThreadpool failed with error " << dwRlt << endl;
			__leave;
		}

		if (!SetThreadpoolThreadMinimum(pPool, 1))
		{
			dwRlt = GetLastError();
			cerr << "SetThreadpoolThreadMinimum failed with error " << dwRlt << endl;
			__leave;
		}
		SetThreadpoolThreadMaximum(pPool, 1);
		SetThreadpoolCallbackPool(&cbe, pPool);

		pcg = CreateThreadpoolCleanupGroup();
		if (!pcg)
		{
			dwRlt = GetLastError();
			cerr << "CreateThreadpoolCleanupGroup failed with error " << dwRlt << endl;
			__leave;
		}
		SetThreadpoolCallbackCleanupGroup(&cbe, pcg, NULL);

		ce._prio = CreateThreadpoolIo(hfSrc, ReadIoCompleteCB, &ce, &cbe);
		if (!ce._prio)
		{
			dwRlt = GetLastError();
			cerr << "CreateThreadpoolIo failed with error " << dwRlt << endl;
			__leave;
		}
		ce._pwio = CreateThreadpoolIo(hfDst, WriteIoCompleteCB, &ce, &cbe);
		if (!ce._pwio)
		{
			dwRlt = GetLastError();
			cerr << "CreateThreadpoolIo failed with error " << dwRlt << endl;
			__leave;
		}

		pWork = CreateThreadpoolWork(KickoffCopyIo, &ce, &cbe);
		if (!pWork)
		{
			dwRlt = GetLastError();
			cerr << "CreateThreadpoolWork failed with error " << dwRlt << endl;
			__leave;
		}
		SubmitThreadpoolWork(pWork);

		HANDLE arhSyncs[2] = { g_hevExit, ce._hevComp };
		DWORD dwWaitRlt = WaitForMultipleObjects(2, arhSyncs, FALSE, INFINITE);
		if (dwWaitRlt == WAIT_OBJECT_0)
		{
			CancelIoEx(ce._hfSrc, NULL);
			CancelIoEx(ce._hfDst, NULL);

			WaitForThreadpoolIoCallbacks(ce._prio, TRUE);
			WaitForThreadpoolIoCallbacks(ce._pwio, TRUE);

			dwRlt = ERROR_OPERATION_ABORTED;
		}
		else
			dwRlt = ce._status;
	}
	__finally
	{
		if (pcg != NULL)
		{
			CloseThreadpoolCleanupGroupMembers(pcg, TRUE, NULL);
			if (dwRlt == ERROR_OPERATION_ABORTED)
				cout << "Copy operation canceled...." << endl;
			CloseThreadpoolCleanupGroup(pcg);
		}
		if (ce._hevComp != NULL)
			CloseHandle(ce._hevComp);
		if (pPool != NULL)
			CloseThreadpool(pPool);

		DestroyThreadpoolEnvironment(&cbe);
	}

	return dwRlt;
}


#define DEF_CHUNK_CNT	4
void _tmain(int argc, TCHAR* argv[])
{
	if (argc < 3)
	{
		cout << "Uasge : TPFileCopyNoBuff SourceFile DestinationFile ConcurrentIOCnt..." << endl;
		return;
	}
	SIZE_T nCopyCnt = DEF_CHUNK_CNT;
	if (argc > 3)
	{
		nCopyCnt = _ttoi(argv[3]);
		if (nCopyCnt <= 0)
		{
			cerr << "Max conurrency count must be > 0.\n";
			return;
		}
	}
	g_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
	{
		cout << "SetConsoleCtrlHandler failed, code : " << GetLastError() << endl;
		return;
	}

	SYSTEM_INFO si = { 0 };
	GetSystemInfo(&si);
	PAGE_SIZE = si.dwPageSize;
	BUFF_SIZE = (DEF_BUFF_SIZE + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	HANDLE hSrcFile = INVALID_HANDLE_VALUE;
	HANDLE hDstFile = INVALID_HANDLE_VALUE;
	PCOPY_CHUNK pCopyBufs = NULL;
	__try
	{
		// Allocate enough buffers to support
		// the maximum number of overlapped I/O's
		// that can be pending at any given time.
		SIZE_T allocSize = sizeof(COPY_CHUNK) * nCopyCnt;
		pCopyBufs = PCOPY_CHUNK(new BYTE[allocSize]);
		if (!pCopyBufs)
			__leave;
		memset(pCopyBufs, 0, allocSize);

		COPY_CHUNK* pCurr = pCopyBufs;
		for (SIZE_T i = 0; i < nCopyCnt; ++i, ++pCurr)
		{
			pCurr->_buff = VirtualAlloc
				(NULL, BUFF_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (!pCurr->_buff)
			{
				cout << "VirtualAlloc failed with " << GetLastError() << endl;
				__leave;
			}
		}

		hSrcFile = CreateFile(argv[1], GENERIC_READ, 
			FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
		if (hSrcFile == INVALID_HANDLE_VALUE)
		{
			cerr << "Failed to open source, error " << GetLastError() << endl;
			__leave;
		}
		LARGE_INTEGER llSrcSize;
		if (!GetFileSizeEx(hSrcFile, &llSrcSize))
		{
			cerr << "GetFileSizeEx failed with " << GetLastError() << endl;
			__leave;
		}

		hDstFile = CreateFile(argv[2], GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
			FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, hSrcFile);
		if (hDstFile == INVALID_HANDLE_VALUE)
		{
			cerr << L"Failed to create destination file, error " << GetLastError() << endl;
			__leave;
		}
		LARGE_INTEGER llInitSize;
		llInitSize.QuadPart = (llSrcSize.QuadPart + PAGE_SIZE - 1) & ~((DWORD_PTR)(PAGE_SIZE - 1));
		if (!SetFilePointerEx(hDstFile, llInitSize, NULL, FILE_BEGIN))
		{
			cerr << "SetFilePointerEx failed with " << GetLastError() << endl;
			__leave;
		}
		if (!SetEndOfFile(hDstFile))
		{
			cerr << "SetEndOfFile failed with " << GetLastError() << endl;
			__leave;
		}

		if (CopyFileUsingTP(hSrcFile, hDstFile, llSrcSize, pCopyBufs, nCopyCnt) != ERROR_SUCCESS)
			__leave;

		hDstFile = ReOpenFile(hDstFile, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0);
		if (hDstFile == INVALID_HANDLE_VALUE)
		{
			__leave;
		}
		if (!SetFilePointerEx(hDstFile, llSrcSize, NULL, FILE_BEGIN))
		{
			cerr << "Final SetFilePointerEx failed with " << GetLastError() << endl;
			__leave;
		}
		if (!SetEndOfFile(hDstFile))
		{
			cerr << "Setting end of file faile with " << GetLastError() << endl;
			__leave;
		}

		cout << "Copy operation completed...." << endl;
	}
	__finally
	{
		if (hSrcFile != INVALID_HANDLE_VALUE)
			CloseHandle(hSrcFile);
		if (hDstFile != INVALID_HANDLE_VALUE)
			CloseHandle(hDstFile);
		if (pCopyBufs != NULL)
		{
			COPY_CHUNK* pCurr = pCopyBufs;
			for (SIZE_T i = 0; i < nCopyCnt; ++i, ++pCurr)
			{
				if (pCurr->_buff != NULL)
					VirtualFree(pCurr->_buff, 0, MEM_RELEASE);
			}
			delete[] pCopyBufs;
		}
		if (g_hevExit != NULL)
			CloseHandle(g_hevExit);
	}
}

