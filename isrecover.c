/*
 * Title:	isrecover.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	30May2004
 * Description:
 *	This is the module that deals solely with the isrecover function in the
 *	VBISAM library.
 * Version:
 *	$Id: isrecover.c,v 1.3 2004/06/22 09:44:41 trev_vb Exp $
 * Modification History:
 *	$Log: isrecover.c,v $
 *	Revision 1.3  2004/06/22 09:44:41  trev_vb
 *	22June2004 TvB Full rewrite.  Currently works as per C-ISAM.  I'll extend it
 *	22June2004 TvB to include far greater functionality!
 *	
 *	Revision 1.2  2004/06/11 22:16:16  trev_vb
 *	11Jun2004 TvB As always, see the CHANGELOG for details. This is an interim
 *	checkin that will not be immediately made into a release.
 *	
 *	Revision 1.1  2004/06/06 20:52:21  trev_vb
 *	06Jun2004 TvB Lots of changes! Performance, stability, bugfixes.  See CHANGELOG
 *	
 *	Revision 1.5  2004/01/06 14:31:59  trev_vb
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	isrecover (void);
static	int	iRollBackAll (void);
static	void	vCloseAll (void);
static	int	iGetRcvHandle (int, int);
static	struct	RCV_HDL	*psRcvAllocate (void);
static	void	vRcvFree (struct RCV_HDL *);
static	int	iRcvCheckTrans (int, int);
static	int	iIgnore (int);
static	int	iRcvBuild (char *);
static	int	iRcvBegin (char *, int);
static	int	iRcvCreateIndex (char *);
static	int	iRcvCluster (char *);
static	int	iRcvCommit (char *);
static	int	iRcvDelete (char *);
static	int	iRcvDeleteIndex (char *);
static	int	iRcvFileErase (char *);
static	int	iRcvFileClose (char *);
static	int	iRcvFileOpen (char *);
static	int	iRcvFileRename (char *);
static	int	iRcvInsert (char *);
static	int	iRcvRollBack (void);
static	int	iRcvSetUnique (char *);
static	int	iRcvUniqueID (char *);
static	int	iRcvUpdate (char *);

struct	RCV_HDL
{
	struct	RCV_HDL
		*psNext,
		*psPrev;
	int	iPID,
		iHandle,
		iTransLock;
};
#define	RH_NULL	((struct RCV_HDL *) 0)
static	struct	RCV_HDL
	*psRecoverHandle [VB_MAX_FILES + 1];

struct	STRANS
{
	struct	STRANS
		*psNext,
		*psPrev;
	int	iPID,
		iIgnoreMe;	// If TRUE, isrecover will NOT process this one
};
#define	STRANS_NULL	((struct STRANS *) 0)
static	struct	STRANS
	*psTransHead = STRANS_NULL;
static	char	cLclBuffer [65536];

/*
 * Name:
 *	int	isrecover (void)
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 * Comments:
 *	At present, isrecover () has a limitation of MAX_OPEN_TRANS 'open'
 *	transactions at any given point in time.  If this limit is exceeded,
 *	isrecover will FAIL with iserrno = EBADMEM
 */
int
isrecover (void)
{
	char	*pcBuffer;
	int	iLoop,
		iSaveError;
	off_t	tLength,
		tLength2,
		tOffset;

	// Initialize by stating that *ALL* tables must be closed!
	for (iLoop = 0; iLoop <= VB_MAX_FILES; iLoop++)
		if (psVBFile [iLoop])
			return (ETOOMANY);
	iVBInTrans = VBRECOVER;
	for (iLoop = 0; iLoop <= VB_MAX_FILES; iLoop++)
		psRecoverHandle [iLoop] = RH_NULL;
	psVBLogHeader = (struct SLOGHDR *) (cVBTransBuffer - INTSIZE);
	// Begin by reading the header of the first transaction
	iserrno = EBADFILE;
	if (tVBLseek (iVBLogfileHandle, 0, SEEK_SET) != 0)
		return (-1);
	if (tVBRead (iVBLogfileHandle, cVBTransBuffer, INTSIZE) != INTSIZE)
		return (0);	// Nothing to do if the file is empty
	tOffset = 0;
	tLength = ldint (cVBTransBuffer);
	// Now, recurse forwards
	while (1)
	{
		tLength2 = tVBRead (iVBLogfileHandle, cVBTransBuffer, tLength);
		iserrno = EBADFILE;
		if (tLength2 != tLength && tLength2 != tLength - INTSIZE)
			break;
		pcBuffer = cVBTransBuffer - INTSIZE + sizeof (struct SLOGHDR);
#ifdef	DEBUG
		printf ("Offset:%08llx PID:%d Type: %-2.2s Row:%08llx\n", tOffset, ldint (psVBLogHeader->cPID), psVBLogHeader->cOperation, ldquad (pcBuffer + INTSIZE));
		fflush (stdout);
#endif	// DEBUG
		if (!memcmp (psVBLogHeader->cOperation, VBL_BUILD, 2))
			iserrno = iRcvBuild (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_BEGIN, 2))
			iserrno = iRcvBegin (pcBuffer, ldint (cVBTransBuffer + tLength - INTSIZE));
		else if (!memcmp (psVBLogHeader->cOperation, VBL_CREINDEX, 2))
			iserrno = iRcvCreateIndex (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_CLUSTER, 2))
			iserrno = iRcvCluster (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_COMMIT, 2))
			iserrno = iRcvCommit (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_DELETE, 2))
			iserrno = iRcvDelete (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_DELINDEX, 2))
			iserrno = iRcvDeleteIndex (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_FILEERASE, 2))
			iserrno = iRcvFileErase (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_FILECLOSE, 2))
			iserrno = iRcvFileClose (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_FILEOPEN, 2))
			iserrno = iRcvFileOpen (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_INSERT, 2))
			iserrno = iRcvInsert (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_RENAME, 2))
			iserrno = iRcvFileRename (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_ROLLBACK, 2))
			iserrno = iRcvRollBack ();
		else if (!memcmp (psVBLogHeader->cOperation, VBL_SETUNIQUE, 2))
			iserrno = iRcvSetUnique (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_UNIQUEID, 2))
			iserrno = iRcvUniqueID (pcBuffer);
		else if (!memcmp (psVBLogHeader->cOperation, VBL_UPDATE, 2))
			iserrno = iRcvUpdate (pcBuffer);
		if (iserrno)
			break;
		tOffset += tLength2;
		if (tLength2 == tLength - INTSIZE)
			break;
		tLength = ldint (cVBTransBuffer + tLength - INTSIZE);
	}
	iSaveError = iserrno;
	// We now rollback any transactions that were never 'closed'
	iserrno = iRollBackAll ();
	if (!iSaveError)
		iSaveError = iserrno;
	vCloseAll ();
	iserrno = iSaveError;
	if (iserrno)
		return (-1);
	return (0);
}

static	int
iRollBackAll (void)
{
	while (psTransHead)
	{
		stint (psTransHead->iPID, psVBLogHeader->cPID);
		iRcvRollBack ();
	}
	return (0);
}

static	void
vCloseAll (void)
{
	int	iLoop;

	for (iLoop = 0; iLoop <= VB_MAX_FILES; iLoop++)
		if (psVBFile [iLoop] && psVBFile [iLoop]->iIsOpen == 0)
			isclose (iLoop);
}

/*
 * Name:
 *	static	int	iGetRcvHandle (int iHandle, int iPID);
 * Arguments:
 *	int	iHandle
 *		The handle as extracted from the transaction
 *	int	iPID
 *		The PID as extracted from the transaction
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure
 *	Other	The *live* handle to use in place of the transaction handle
 * Problems:
 *	NONE known
 * Comments:
 *	This function is to translate the handle from a transaction into one
 *	that can be used within the isrecover process to perform the action
 *	as defined within the transaction.
 */
static	int
iGetRcvHandle (int iHandle, int iPID)
{
	struct	RCV_HDL
		*psRcvHdl = psRecoverHandle [iHandle];

	while (psRcvHdl && psRcvHdl->iPID != iPID)
		psRcvHdl = psRcvHdl->psNext;
	if (psRcvHdl && psRcvHdl->iPID == iPID)
		return (psRcvHdl->iHandle);
	return (-1);
}

static	struct	RCV_HDL
*psRcvAllocate (void)
{
	return ((struct RCV_HDL *) pvVBMalloc (sizeof (struct RCV_HDL)));
}

static	void
vRcvFree (struct RCV_HDL *psRcv)
{
	vVBFree (psRcv, sizeof (struct RCV_HDL));
}

static	int
iRcvCheckTrans (int iLength, int iPID)
{
	int	iFound = FALSE,
		iResult = FALSE;
	off_t	tLength = iLength,
		tLength2,
		tRcvSaveOffset = tVBLseek (iVBLogfileHandle, 0, SEEK_CUR);

	psVBLogHeader = (struct SLOGHDR *) (cLclBuffer - INTSIZE);
	while (!iFound)
	{
		tLength2 = tVBRead (iVBLogfileHandle, cLclBuffer, (size_t) tLength);
		if (tLength2 != tLength && tLength2 != tLength - INTSIZE)
			break;
		if (ldint (psVBLogHeader->cPID) == iPID)
		{
			if (!memcmp (psVBLogHeader->cOperation, VBL_COMMIT, 2))
				iFound = TRUE;
			else if (!memcmp (psVBLogHeader->cOperation, VBL_BEGIN, 2))
			{
				iFound = TRUE;
				if (iVBRecvMode & RECOV_VB)
					iResult = TRUE;
			}
			else if (!memcmp (psVBLogHeader->cOperation, VBL_ROLLBACK, 2))
			{
				iFound = TRUE;
				iResult = TRUE;
			}
		}
		if (tLength2 == tLength - INTSIZE)
			break;
		tLength = ldint (cLclBuffer + tLength - INTSIZE);
	}

	psVBLogHeader = (struct SLOGHDR *) (cVBTransBuffer - INTSIZE);
	tVBLseek (iVBLogfileHandle, tRcvSaveOffset, SEEK_SET);
	return (iResult);
}

static	int
iIgnore (int iPID)
{
	struct	STRANS
		*psTrans = psTransHead;

	while (psTrans && psTrans->iPID != iPID)
		psTrans = psTrans->psNext;
	if (psTrans && psTrans->iPID == iPID)
		return (psTrans->iIgnoreMe);
	return (FALSE);
}

static	int
iRcvBuild (char *pcBuffer)
{
	char	*pcFilename;
	int	iHandle,
		iLoop,
		iMode,
		iMaxRowLen,
		iPID;
	struct	keydesc
		sKeydesc;

	iMode = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	isreclen = ldint (pcBuffer + INTSIZE);
	iMaxRowLen = ldint (pcBuffer + (INTSIZE * 2));
	sKeydesc.iFlags = ldint (pcBuffer + (INTSIZE * 3));
	sKeydesc.iNParts = ldint (pcBuffer + (INTSIZE * 4));
	pcBuffer += (INTSIZE * 6);
	for (iLoop = 0; iLoop < sKeydesc.iNParts; iLoop++)
	{
		sKeydesc.sPart [iLoop].iStart = ldint (pcBuffer + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iLength = ldint (pcBuffer + INTSIZE + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iType = ldint (pcBuffer + (INTSIZE * 2) + + (iLoop * 3 * INTSIZE));
	}
	pcFilename = pcBuffer + (sKeydesc.iNParts * 3 * INTSIZE);
	iHandle = isbuild (pcFilename, iMaxRowLen, &sKeydesc, iMode);
	iMode = iserrno;	// Save any error result
	if (iHandle != -1)
		isclose (iHandle);
	return (iMode);
}

static	int
iRcvBegin (char *pcBuffer, int iLength)
{
	int	iPID;
	struct	STRANS
		*psTrans = psTransHead;

	iPID = ldint (psVBLogHeader->cPID);
	while (psTrans && psTrans->iPID != iPID)
		psTrans = psTrans->psNext;
	if (psTrans)
	{
		printf ("Transaction for PID %d begun within current transaction!\n", iPID);
		iRcvRollBack ();
	}
	psTrans = pvVBMalloc (sizeof (struct STRANS));
	if (!psTrans)
		return (ENOMEM);
	psTrans->psPrev = STRANS_NULL;
	psTrans->psNext = psTransHead;
	if (psTransHead)
		psTransHead->psPrev = psTrans;
	psTransHead = psTrans;
	psTrans->iPID = iPID;
	psTrans->iIgnoreMe = iRcvCheckTrans (iLength, iPID);
	return (0);
}

static	int
iRcvCreateIndex (char *pcBuffer)
{
	int	iHandle,
		iLoop,
		iPID,
		iSaveError = 0;
	struct	keydesc
		sKeydesc;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	sKeydesc.iFlags = ldint (pcBuffer + INTSIZE);
	sKeydesc.iNParts = ldint (pcBuffer + (INTSIZE * 2));
	pcBuffer += (INTSIZE * 4);
	for (iLoop = 0; iLoop < sKeydesc.iNParts; iLoop++)
	{
		sKeydesc.sPart [iLoop].iStart = ldint (pcBuffer + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iLength = ldint (pcBuffer + INTSIZE + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iType = ldint (pcBuffer + (INTSIZE * 2) + + (iLoop * 3 * INTSIZE));
	}
	// Promote the file open lock to EXCLUSIVE
	iserrno = iVBFileOpenLock (iHandle, 2);
	if (iserrno)
		return (iserrno);
	psVBFile [iHandle]->iOpenMode |= ISEXCLLOCK;
	if (isaddindex (iHandle, &sKeydesc))
		iSaveError = iserrno;
	// Demote the file open lock back to SHARED
	psVBFile [iHandle]->iOpenMode &= ~ISEXCLLOCK;
	iVBFileOpenLock (iHandle, 0);
	iVBFileOpenLock (iHandle, 1);
	return (iSaveError);
}

static	int
iRcvCluster (char *pcBuffer)
{
	int	iPID;

	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	fprintf (stderr, "iRcvCluster not yet implemented!\n");
	return (0);
}

static	int
iRcvCommit (char *pcBuffer)
{
	int	iLoop,
		iPID;
	struct	RCV_HDL
		*psRcv;
	struct	STRANS
		*psTrans = psTransHead;

	iPID = ldint (psVBLogHeader->cPID);
	while (psTrans && psTrans->iPID != iPID)
		psTrans = psTrans->psNext;
	if (!psTrans)
	{
		printf ("Commit transaction for PID %d encountered without Begin!\n", iPID);
		return (EBADLOG);
	}
	if (psTrans->psNext)
		psTrans->psNext->psPrev = psTrans->psPrev;
	if (psTrans->psPrev)
		psTrans->psPrev->psNext = psTrans->psNext;
	else
		psTransHead = psTrans->psNext;
	vVBFree (psTrans, sizeof (struct STRANS));
	for (iLoop = 0; iLoop <= VB_MAX_FILES; iLoop++)
	{
		if (!psRecoverHandle [iLoop])
			continue;
		for (psRcv = psRecoverHandle [iLoop]; psRcv; psRcv = psRcv->psNext)
			if (psRcv->iTransLock)
				isrelease (psRcv->iHandle);
	}
	return (0);
}

static	int
iRcvDelete (char *pcBuffer)
{
	char	*pcRow;
	int	iHandle,
		iPID;
	off_t	tRowNumber;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	tRowNumber = ldquad (pcBuffer + INTSIZE);
	pcRow = pcBuffer + INTSIZE + QUADSIZE + INTSIZE;
	isdelrec (iHandle, tRowNumber);
	return (iserrno);
}

static	int
iRcvDeleteIndex (char *pcBuffer)
{
	int	iHandle,
		iLoop,
		iPID,
		iSaveError = 0;
	struct	keydesc
		sKeydesc;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	sKeydesc.iFlags = ldint (pcBuffer + INTSIZE);
	sKeydesc.iNParts = ldint (pcBuffer + (INTSIZE * 2));
	pcBuffer += (INTSIZE * 4);
	for (iLoop = 0; iLoop < sKeydesc.iNParts; iLoop++)
	{
		sKeydesc.sPart [iLoop].iStart = ldint (pcBuffer + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iLength = ldint (pcBuffer + INTSIZE + (iLoop * 3 * INTSIZE));
		sKeydesc.sPart [iLoop].iType = ldint (pcBuffer + (INTSIZE * 2) + + (iLoop * 3 * INTSIZE));
	}
	// Promote the file open lock to EXCLUSIVE
	iserrno = iVBFileOpenLock (iHandle, 2);
	if (iserrno)
		return (iserrno);
	psVBFile [iHandle]->iOpenMode |= ISEXCLLOCK;
	if (isdelindex (iHandle, &sKeydesc))
		iSaveError = iserrno;
	// Demote the file open lock back to SHARED
	psVBFile [iHandle]->iOpenMode &= ~ISEXCLLOCK;
	iVBFileOpenLock (iHandle, 0);
	iVBFileOpenLock (iHandle, 1);
	return (iSaveError);
}

static	int
iRcvFileErase (char *pcBuffer)
{
	int	iPID;

	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iserase (pcBuffer);
	return (iserrno);
}

static	int
iRcvFileClose (char *pcBuffer)
{
	int	iHandle,
		iVarlenFlag,
		iPID;
	struct	RCV_HDL
		*psRcv;

	iHandle = ldint (pcBuffer);
	iVarlenFlag = ldint (pcBuffer + INTSIZE);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	psRcv = psRecoverHandle [iHandle];
	while (psRcv && psRcv->iPID != iPID)
		psRcv = psRcv->psNext;
	if (!psRcv || psRcv->iPID != iPID)
		return (ENOTOPEN);		// It wasn't open!
	iserrno = 0;
	isclose (psRcv->iHandle);
	if (psRcv->psPrev)
		psRcv->psPrev->psNext = psRcv->psNext;
	else
		psRecoverHandle [iHandle] = psRcv->psNext;
	if (psRcv->psNext)
		psRcv->psNext->psPrev = psRcv->psPrev;

	return (iserrno);
}

static	int
iRcvFileOpen (char *pcBuffer)
{
	int	iHandle,
		iVarlenFlag,
		iPID;
	struct	RCV_HDL
		*psRcv;

	iHandle = ldint (pcBuffer);
	iVarlenFlag = ldint (pcBuffer + INTSIZE);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	if (iGetRcvHandle (iHandle, iPID) != -1)
		return (ENOTOPEN);		// It was already open!
	psRcv = psRcvAllocate ();
	if (psRcv == RH_NULL)
		return (ENOMEM);		// Oops
	psRcv->iHandle = isopen (pcBuffer + INTSIZE + INTSIZE, ISINOUT | ISMANULOCK | (iVarlenFlag ? ISVARLEN : ISFIXLEN));
	psRcv->iPID = iPID;
	if (psRcv->iHandle < 0)
	{
		vRcvFree (psRcv);
		return (iserrno);
	}
	psRcv->psPrev = RH_NULL;
	psRcv->psNext = psRecoverHandle [iHandle];
	if (psRecoverHandle [iHandle])
		psRecoverHandle [iHandle]->psPrev = psRcv;
	psRecoverHandle [iHandle] = psRcv;
	return (0);
}

static	int
iRcvInsert (char *pcBuffer)
{
	char	*pcRow;
	int	iHandle,
		iPID;
	off_t	tRowNumber;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	tRowNumber = ldquad (pcBuffer + INTSIZE);
	isreclen = ldint (pcBuffer + INTSIZE + QUADSIZE);
	pcRow = pcBuffer + INTSIZE + QUADSIZE + INTSIZE;
	if (iVBEnter (iHandle, TRUE))
		return (iserrno);
	psVBFile [iHandle]->sFlags.iIsDictLocked |= 0x02;
	if (iVBForceDataAllocate (iHandle, tRowNumber))
		return (EDUPL);
	if (iVBWriteRow (iHandle, pcRow, tRowNumber))
		return (iserrno);
	iVBExit (iHandle);
	return (iserrno);
}

// NEEDS TESTING!
static	int
iRcvFileRename (char *pcBuffer)
{
	char	*pcOldName,
		*pcNewName;
	int	iOldNameLength,
		iPID;

	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iOldNameLength = ldint (pcBuffer);
	pcOldName = pcBuffer + INTSIZE + INTSIZE;
	pcNewName = pcBuffer + INTSIZE + INTSIZE + iOldNameLength;
	isrename (pcOldName, pcNewName);
	return (iserrno);
}

static	int
iRcvRollBack ()
{
	int	iPID;
	struct	STRANS
		*psTrans = psTransHead;

	iPID = ldint (psVBLogHeader->cPID);
	while (psTrans && psTrans->iPID != iPID)
		psTrans = psTrans->psNext;
	if (!psTrans)
	{
		printf ("Rollback transaction for PID %d encountered without Begin!\n", iPID);
		return (EBADLOG);
	}
	if (psTrans->psNext)
		psTrans->psNext->psPrev = psTrans->psPrev;
	if (psTrans->psPrev)
		psTrans->psPrev->psNext = psTrans->psNext;
	else
		psTransHead = psTrans->psNext;
	vVBFree (psTrans, sizeof (struct STRANS));
	return (0);
}

static	int
iRcvSetUnique (char *pcBuffer)
{
	int	iHandle,
		iPID;
	off_t	tUniqueID;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	tUniqueID = ldquad (pcBuffer + INTSIZE);
	issetunique (iHandle, tUniqueID);
	return (iserrno);
}

static	int
iRcvUniqueID (char *pcBuffer)
{
	int	iHandle,
		iPID;
	off_t	tUniqueID;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	tUniqueID = ldquad (pcBuffer + INTSIZE);
	if (tUniqueID != ldquad (psVBFile [iHandle]->sDictNode.cUniqueID))
		return (EBADFILE);
	isuniqueid (iHandle, &tUniqueID);
	return (0);
}

static	int
iRcvUpdate (char *pcBuffer)
{
	char	*pcRow;
	int	iHandle,
		iPID;
	off_t	tRowNumber;

	iHandle = ldint (pcBuffer);
	iPID = ldint (psVBLogHeader->cPID);
	if (iIgnore (iPID))
		return (0);
	iHandle = iGetRcvHandle (iHandle, iPID);
	if (iHandle == -1)
		return (ENOTOPEN);
	tRowNumber = ldquad (pcBuffer + INTSIZE);
	isreclen = ldint (pcBuffer + INTSIZE + QUADSIZE);
	pcRow = pcBuffer + INTSIZE + QUADSIZE + INTSIZE + INTSIZE + isreclen;
	isreclen = ldint (pcBuffer + INTSIZE + QUADSIZE + INTSIZE);
	isrewrec (iHandle, tRowNumber, pcRow);
	return (iserrno);
}
