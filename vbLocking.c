/*
 * Title:	vbLocking.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	21Nov2003
 * Description:
 *	This module handles the locking on both the index and data files for the
 *	VBISAM library.
 * Version:
 *	$Id: vbLocking.c,v 1.4 2004/01/05 07:36:17 trev_vb Exp $
 * Modification History:
 *	$Log: vbLocking.c,v $
 *	Revision 1.4  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.3  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
 *	Revision 1.2  2003/12/22 04:42:14  trev_vb
 *	TvB 21Dec2003 Changed name of environment var (Prep for future)
 *	TvB 21Dec2003 Also, changed 'ID' cvs header to 'Id' (Duh on me!)
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:19  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

static	char	cNode0 [MAX_NODE_LENGTH];
static	int	iVBBufferLevel = -1;

/*
 * Prototypes
 */
int	iVBEnter (int, int);
int	iVBExit (int);
int	iVBForceExit (int);
int	iVBFileOpenLock (int, int);
int	iVBDataLock (int, int, off_t, int);
static	int	iVBLockInsert (int, off_t, int);
static	int	iVBLockDelete (int, off_t, int);

/*
 *	Overview
 *	========
 *	Ideally, I'd prefer using making use of the high bit for the file open
 *	locks and the row locks.  However, this is NOT allowed by Linux.
 *
 *	After a good deal of deliberation (followed by some snooping with good
 *	old strace), I've decided to make the locking strategy 100% compatible
 *	with Informix(R) CISAM 7.24UC2 as it exists for Linux.
 *	When used in 64-bit mode, it simply extends the values accordingly.
 *
 *	Index file locks (ALL locks are on the index file)
 *	==================================================
 *	Non exclusive file open lock
 *		Off:0x7fffffff	Len:0x00000001	Typ:RDLOCK
 *	Exclusive file open lock (i.e. ISEXCLLOCK)
 *		Off:0x7fffffff	Len:0x00000001	Typ:WRLOCK
 *	Enter a primary function - Non modifying
 *		Off:0x00000000	Len:0x3fffffff	Typ:RDLCKW
 *	Enter a primary function - Modifying
 *		Off:0x00000000	Len:0x3fffffff	Typ:WRLCKW
 *	Lock a data row (Add the row number to the offset)
 *		Off:0x40000000	Len:0x00000001	Typ:WRLOCK
 *	Lock *ALL* the data rows (i.e. islock is called)
 *		Off:0x40000000	Len:0x3fffffff	Typ:WRLOCK
 */

/*
 * Name:
 *	int	iVBEnter (int iHandle, int iModifying);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	int	iModifying
 *		0 - Acquire a read lock to inhibit others acquiring a write lock
 *		OTHER - Acquire a write lock
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADARG	Failure
 * Problems:
 *	NONE known
 * Comments:
 *	This function is called upon entry to any of the functions that require
 *	the index file to be in a 'stable' state throughout their life.
 *	If the calling function is going to MODIFY the index file, then it
 *	needs an exclusive lock on the index file.
 *	As a bonus, it also loads the dictionary node of the file into memory
 *	if the file is not opened EXCLLOCK.
 */
int
iVBEnter (int iHandle, int iModifying)
{
	int	iLockMode,
		iResult;
	off_t	tLength;

	if (!psVBFile [iHandle])
	{
		iserrno = ENOTOPEN;
		return (-1);
	}
	psVBFile [iHandle]->sFlags.iIndexChanged = 0;
	if ((psVBFile [iHandle]->iOpenMode & ISEXCLLOCK))
		return (0);
	if (psVBFile [iHandle]->sFlags.iIsDictLocked)
	{
		iserrno = EBADARG;
		return (-1);
	}
	if (iModifying)
		iLockMode = VBWRLCKW;
	else
		iLockMode = VBRDLCKW;
	memset (cNode0, 0xff, QUADSIZE);
	cNode0 [0] = 0x3f;
	tLength = ldquad (cNode0);
	if (!(psVBFile [iHandle]->iOpenMode & ISEXCLLOCK))
	{
		iResult = iVBLock (psVBFile [iHandle]->iIndexHandle, 0, tLength, iLockMode);
		if (iResult)
			return (-1);
		psVBFile [iHandle]->sFlags.iIsDictLocked = 1;
		//iResult = iVBNodeRead (iHandle, (void *) cNode0, 1);
		iResult = iVBBlockRead (iHandle, TRUE, (off_t) 1, cNode0);
		if (iResult)
		{
			psVBFile [iHandle]->sFlags.iIsDictLocked = 0;
			iVBExit (iHandle);
			iserrno = EBADFILE;
			return (-1);
		}
		memcpy ((void *) &psVBFile [iHandle]->sDictNode, (void *) cNode0, sizeof (struct DICTNODE));
	}
	psVBFile [iHandle]->sFlags.iIsDictLocked = 1;
	if (psVBFile [iHandle]->tTransLast == ldquad (psVBFile [iHandle]->sDictNode.cTransNumber))
		psVBFile [iHandle]->sFlags.iIndexChanged = 0;
	else
		psVBFile [iHandle]->sFlags.iIndexChanged = 1;
	return (0);
}

/*
 * Name:
 *	int	iVBExit (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	int	iUpdateTrans
 *		0 - Leave the dictionary node transaction number alone
 *		OTHER - Increment the dictionary node transaction number
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADARG	Failure
 * Problems:
 *	NONE known
 */
int
iVBExit (int iHandle)
{
	char	*pcEnviron;
	int	iResult,
		iLoop,
		iLoop2,
		iSaveError;
	off_t	tLength,
		tTransNumber;

	iSaveError = iserrno;
	if (!psVBFile [iHandle] || (!psVBFile [iHandle]->sFlags.iIsDictLocked && !(psVBFile [iHandle]->iOpenMode & ISEXCLLOCK)))
	{
		iserrno = EBADARG;
		return (-1);
	}
	tTransNumber = ldquad (psVBFile [iHandle]->sDictNode.cTransNumber);
	psVBFile [iHandle]->tTransLast = tTransNumber;
	if (psVBFile [iHandle]->sFlags.iIsDictLocked == 2)
	{
		psVBFile [iHandle]->tTransLast = tTransNumber + 1;
		stquad (tTransNumber + 1, psVBFile [iHandle]->sDictNode.cTransNumber);
		psVBFile [iHandle]->tTransLast = tTransNumber + 1;
	}
	if (psVBFile [iHandle]->iOpenMode & ISEXCLLOCK)
		return (0);
	if (psVBFile [iHandle]->sFlags.iIsDictLocked >= 2)
	{
		memset (cNode0, 0, MAX_NODE_LENGTH);
		memcpy ((void *) cNode0, (void *) &psVBFile [iHandle]->sDictNode, sizeof (struct DICTNODE));
		//iResult = iVBNodeWrite (iHandle, (void *) cNode0, 1);
		iResult = iVBBlockWrite (iHandle, TRUE, (off_t) 1, cNode0);
		if (iResult)
			iserrno = EBADFILE;
		else
			iserrno = 0;
	}
	memset (cNode0, 0xff, QUADSIZE);
	cNode0 [0] = 0x3f;
	tLength = ldquad (cNode0);
	iResult = iVBLock (psVBFile [iHandle]->iIndexHandle, 0, tLength, VBUNLOCK);
	if (iResult)
	{
		iserrno = errno;
		return (-1);
	}
	psVBFile [iHandle]->sFlags.iIsDictLocked = 0;
	// Free up any key/tree no longer wanted
	if (iVBBufferLevel == -1)
	{
		pcEnviron = getenv ("VB_TREE_LEVEL");
		if (pcEnviron)
			iVBBufferLevel = atoi (pcEnviron);
		else
			iVBBufferLevel = 4;
	}
	for (iLoop2 = 0; iLoop2 < psVBFile [iHandle]->iNKeys; iLoop2++)
	{
		struct	VBKEY
			*psKey,
			*psKeyCurr;

		psKey = psVBFile [iHandle]->psKeyCurr [iLoop2];
		/*
		 * This is a REAL fudge factor...
		 * We simply free up all the dynamically allocated memory
		 * associated with non-current nodes above a certain level.
		 */
		for (iLoop = 0; psKey && iLoop < iVBBufferLevel; iLoop++)
		{
			if (psKey->psParent->psParent)
				psKey = psKey->psParent->psParent->psKeyCurr;
			else
				psKey = VBKEY_NULL;
		}
		if (!psKey)
		{
			iserrno = iSaveError;
			return (0);
		}
		while (psKey)
		{
			for (psKeyCurr = psKey->psParent->psKeyFirst; psKeyCurr; psKeyCurr = psKeyCurr->psNext)
			{
				if (psKeyCurr != psKey && psKeyCurr->psChild)
				{
					vVBTreeAllFree (iHandle, iLoop2, psKeyCurr->psChild);
					psKeyCurr->psChild = VBTREE_NULL;
				}
			}
			if (psKey->psParent->psParent)
				psKey = psKey->psParent->psParent->psKeyCurr;
			else
				psKey = VBKEY_NULL;
		}
	}
	iserrno = iSaveError;
	return (0);
}

/*
 * Name:
 *	int	iVBForceExit (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADARG	Failure
 * Problems:
 *	NONE known
 */
int
iVBForceExit (int iHandle)
{
	int	iResult;
	//off_t	tTransNumber;

	if (psVBFile [iHandle]->sFlags.iIsDictLocked == 2)
	{
		//tTransNumber = ldquad (psVBFile [iHandle]->sDictNode.cTransNumber) + 1;
		//stquad (tTransNumber, psVBFile [iHandle]->sDictNode.cTransNumber);
		memset (cNode0, 0, MAX_NODE_LENGTH);
		memcpy ((void *) cNode0, (void *) &psVBFile [iHandle]->sDictNode, sizeof (struct DICTNODE));
		//iResult = iVBNodeWrite (iHandle, (void *) cNode0, 1);
		iResult = iVBBlockWrite (iHandle, TRUE, (off_t) 1, cNode0);
		if (iResult)
			iserrno = EBADFILE;
		else
			iserrno = 0;
	}
	psVBFile [iHandle]->sFlags.iIsDictLocked = 0;
	return (0);
}

/*
 * Name:
 *	int	iVBFileOpenLock (int iHandle, int iMode);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	int	iMode
 *		0 - A 'per-process' file open lock is removed
 *		1 - A 'per-process' file open lock is desired
 *		2 - An 'exclusive' file open lock is desired
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADARG
 *	EFLOCKED
 * Problems:
 *	NONE known
 * Comments:
 *	This routine is used to establish a 'file-open' lock on the index file
 */
int
iVBFileOpenLock (int iHandle, int iMode)
{
	int	iLockType,
		iResult;
	off_t	tOffset;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);

	memset (cNode0, 0xff, QUADSIZE);
	cNode0 [0] = 0x7f;
	tOffset = ldquad (cNode0);

	switch (iMode)
	{
	case	0:
		iLockType = VBUNLOCK;
		break;

	case	1:
		iLockType = VBRDLOCK;
		break;

	case	2:
		iLockType = VBWRLOCK;
		iResult = iVBBlockRead (iHandle, TRUE, (off_t) 1, cNode0);
		memcpy ((void *) &psVBFile [iHandle]->sDictNode, (void *) cNode0, sizeof (struct DICTNODE));
		break;

	default:
		return (EBADARG);
	}

	// Whether we're locking *OR* unlocking a region, retry forever on EINTR
	// BUG? - This *MAY* be a potential race condition?
	do
	{
		iResult = iVBLock (psVBFile [iHandle]->iIndexHandle, tOffset, 1, iLockType);
	} while (iResult != 0 && errno == EINTR);
	if (iResult != 0)
		return (EFLOCKED);

	return (0);
}

/*
 * Name:
 *	int	iVBDataLock (int iHandle, int iMode, off_t tRowNumber, int iIsTransaction);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	int	iMode
 *		VBUNLOCK
 *		VBWRLOCK
 *		VBWRLCKW
 *	off_t	tRowNumber
 *		The row number to be (un)locked
 *		If zero, then this is a file-wide (un)lock
 *	int	iIsTransaction
 *		If set, then this lock is a transactional lock
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 *	EFLOCKED
 * Problems:
 *	NONE known
 */
int
iVBDataLock (int iHandle, int iMode, off_t tRowNumber, int iIsTransaction)
{
	int	iResult;
	struct	VBLOCK
		*psLock;
	off_t	tLength = 1,
		tOffset;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);
	if (psVBFile [iHandle]->iOpenMode & ISEXCLLOCK)
		return (0);
	/*
	 * If this is a FILE lock (row = 0), then we may as well free up any
	 * other existing locks.
	 * BUG: What if some of those locks were within a transaction?
	 */
	if (tRowNumber == 0)
	{
		psLock = psVBFile [iHandle]->psLockHead;
		while (psLock)
		{
			psVBFile [iHandle]->psLockHead = psLock->psNext;
			vVBLockFree (psLock);
			psLock = psVBFile [iHandle]->psLockHead;
		}
		psVBFile [iHandle]->psLockTail = psVBFile [iHandle]->psLockHead;
		memset (cNode0, 0xff, QUADSIZE);
		cNode0 [0] = 0x3f;
		tLength = ldquad (cNode0);
		if (iMode == VBUNLOCK)
			psVBFile [iHandle]->sFlags.iIsDataLocked = FALSE;
		else
			psVBFile [iHandle]->sFlags.iIsDataLocked = TRUE;
	}
	/*
	 * A one byte row lock can be merged with previous / next row locks
	 * in the kernel in order to save on consuming valuable system locks
	 */
	memset (cNode0, 0x00, QUADSIZE);
	cNode0 [0] = 0x40;
	tOffset = ldquad (cNode0);
	iResult = iVBLock (psVBFile [iHandle]->iIndexHandle, tOffset + tRowNumber, tLength, iMode);
	if (iResult != 0)
		return (EBADFILE);
	if (iMode != VBUNLOCK && tRowNumber)
		return (iVBLockInsert (iHandle, tRowNumber, iIsTransaction));
	if (iMode == VBUNLOCK && tRowNumber)
		return (iVBLockDelete (iHandle, tRowNumber, iIsTransaction));
	return (0);
}

/*
 * Name:
 *	static	int	iVBLockInsert (int iHandle, off_t tRowNumber, int iIsTransaction);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	off_t	tRowNumber
 *		The row number to be added to the files lock list
 *	int	iIsTransaction
 *		If TRUE, then this rowlock is flagged such that it doesn't get
 *		unlocked except by a iscommit () / isrollback ()
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	Failure
 * Problems:
 *	NONE known
 */
static	int
iVBLockInsert (int iHandle, off_t tRowNumber, int iIsTransaction)
{
	struct	VBLOCK
		*psNewLock = VBLOCK_NULL,
		*psLock = VBLOCK_NULL;

	psLock = psVBFile [iHandle]->psLockHead;
	// Insertion at head of list
	if (psLock == VBLOCK_NULL || iIsTransaction < psLock->iIsTransaction || tRowNumber < psLock->tRowNumber)
	{
		psNewLock = psVBLockAllocate (iHandle);
		if (psNewLock == VBLOCK_NULL)
			return (errno);
		psNewLock->iIsTransaction = iIsTransaction;
		psNewLock->tRowNumber = tRowNumber;
		psNewLock->psNext = psLock;
		psVBFile [iHandle]->psLockHead = psNewLock;
		if (psVBFile [iHandle]->psLockTail == VBLOCK_NULL)
			psVBFile [iHandle]->psLockTail = psNewLock;
		return (0);
	}
	// Insertion at tail of list
	if (iIsTransaction >= psVBFile [iHandle]->psLockTail->iIsTransaction && tRowNumber > psVBFile [iHandle]->psLockTail->tRowNumber)
	{
		psNewLock = psVBLockAllocate (iHandle);
		if (psNewLock == VBLOCK_NULL)
			return (errno);
		psNewLock->iIsTransaction = iIsTransaction;
		psNewLock->tRowNumber = tRowNumber;
		psVBFile [iHandle]->psLockTail->psNext = psNewLock;
		psVBFile [iHandle]->psLockTail = psNewLock;
		return (0);
	}
	// Position psLock to insertion point (Keep in mind, we insert AFTER)
	while (psLock->psNext && iIsTransaction >= psLock->psNext->iIsTransaction && tRowNumber > psLock->psNext->tRowNumber)
	{
		// Are we promoting a non-transactional lock to transactional?
		if (tRowNumber == psLock->psNext->tRowNumber)
		{
			// Is it already 'correct'?
			if (iIsTransaction == psLock->psNext->iIsTransaction)
				return (0);	// Already OK!
			else
			{
				// Are we promoting the tail?
				if (psLock->psNext == psVBFile [iHandle]->psLockTail)
				{
					psLock->psNext->iIsTransaction = iIsTransaction;
					return (0);
				}
				psNewLock = psLock->psNext;
				psLock->psNext = psNewLock->psNext;
			}
		}
		psLock = psLock->psNext;
	}
	if (!psNewLock)
		psNewLock = psVBLockAllocate (iHandle);
	if (psNewLock == VBLOCK_NULL)
		return (errno);
	psNewLock->psNext = VBLOCK_NULL;
	psNewLock->iIsTransaction = iIsTransaction;
	psNewLock->tRowNumber = tRowNumber;
	// Insert psNewLock AFTER psLock
	psNewLock->psNext = psLock->psNext;
	psLock->psNext = psNewLock;

	return (0);
}

/*
 * Name:
 *	static	int	iVBLockDelete (int iHandle, off_t tRowNumber, int iIsTransaction);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	off_t	tRowNumber
 *		The row number to be removed from the files lock list
 *	int	iIsTransaction
 *		If set, then this lock is a transactional unlock
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADARG	Failure
 * Problems:
 *	NONE known
 */
static	int
iVBLockDelete (int iHandle, off_t tRowNumber, int iIsTransaction)
{
	struct	VBLOCK
		*psLockToDelete,
		*psLock = psVBFile [iHandle]->psLockHead;

	// Sanity check #1
	if (!psLock || psLock->tRowNumber > tRowNumber)
		return (EBADARG);
	// Check if deleting first entry in list
	if (psLock->tRowNumber == tRowNumber)
	{
		if (psVBFile [iHandle]->psLockHead->iIsTransaction && !iIsTransaction)
			return (ENOTRANS);
		psVBFile [iHandle]->psLockHead = psLock->psNext;
		if (!psVBFile [iHandle]->psLockHead)
			psVBFile [iHandle]->psLockTail = VBLOCK_NULL;
		vVBLockFree (psLock);
		return (0);
	}
	// Position psLock pointer to previous
	while (psLock->psNext && psLock->psNext->tRowNumber < tRowNumber)
		psLock = psLock->psNext;
	// Sanity check #2
	if (!psLock->psNext || psLock->psNext->tRowNumber != tRowNumber)
		return (EBADARG);
	if (psLock->psNext->iIsTransaction && !iIsTransaction)
		return (ENOTRANS);
	psLockToDelete = psLock->psNext;
	psLock->psNext = psLockToDelete->psNext;
	if (psLockToDelete == psVBFile [iHandle]->psLockTail)
			psVBFile [iHandle]->psLockTail = psLock;
	vVBLockFree (psLockToDelete);

	return (0);
}
