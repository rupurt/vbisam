/*
 * Title:	vbLowLevel.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	18Nov2003
 * Description:
 *	This module handles ALL the low level file I/O operations for the
 *	VBISAM library.  By encapsulating all of these functions into wrappers
 *	within this module, it becomes easier to 'virtualize' the filesystem
 *	at a later date.
 *	In addition, it also performs the node-buffering in order to lower the
 *	quantity of lseek and read system calls required.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: vbLowLevel.c,v $
 *	Revision 1.1  2003/12/20 20:11:18  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

static	int	iInitialized = FALSE,
		iBlockCount = 0;

struct	VBBLOCK
{
	struct	VBBLOCK
		*psNext,
		*psPrev;
	int	iFileHandle;
	off_t	tBlockNumber;
	char	cBuffer [MAX_NODE_LENGTH];
};
#define	VBBLOCK_NULL	((struct VBBLOCK *) 0)
static	struct	VBBLOCK
	*psBlockHead = VBBLOCK_NULL,
	*psBlockTail = VBBLOCK_NULL;

/*
 * Prototypes
 */
int	iVBOpen (char *, int, mode_t);
int	iVBClose (int);
off_t	tVBLseek (int, off_t, int);
ssize_t	tVBRead (int, void *, size_t);
ssize_t	tVBWrite (int, void *, size_t);
int	iVBLock (int, off_t, off_t, int);
int	iVBLink (char *, char *);
int	iVBUnlink (char *);
int	iVBAccess (char *, int);
void	vVBBlockDeinit (void);
void	vVBBlockInvalidate (int iHandle);
int	iVBBlockRead (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer);
int	iVBBlockWrite (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer);
static	void	vBlockInit (void);

/*
 * Name:
 *	int	iVBOpen (char *pcFilename, int iFlags, mode_t tMode);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be opened
 *	int	iFlags
 *		As per the standard open () system call
 *	mode_t	tMode
 *		As per the standard open () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The handle to be used for accessing this file
 * Problems:
 *	NONE known
 */
int
iVBOpen (char *pcFilename, int iFlags, mode_t tMode)
{
	return (open (pcFilename, iFlags, tMode));
}

/*
 * Name:
 *	int	iVBClose (int iHandle);
 * Arguments:
 *	int	iHandle
 *		As per the standard close () system call
 * Prerequisites:
 *	Well, let's *HOPE* that iHandle is actually open!
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The close succeeded
 * Problems:
 *	NONE known
 */
int
iVBClose (int iHandle)
{
	return (close (iHandle));
}

/*
 * Name:
 *	off_t	tVBLseek (int iHandle, off_t tOffset, int iWhence);
 * Arguments:
 *	int	iHandle
 *		As per the standard lseek () system call
 *	off_t	tOffset
 *		As per the standard lseek () system call
 *	int	iWhence
 *		As per the standard lseek () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	(off_t) -1
 *		An error occurred.  errno contains the reason
 *	Other	The new offset within the file in question
 * Problems:
 *	NONE known
 */
off_t
tVBLseek (int iHandle, off_t tOffset, int iWhence)
{
	return (lseek (iHandle, tOffset, iWhence));
}

/*
 * Name:
 *	ssize_t	tVBRead (int iHandle, void *pvBuffer, size_t tCount);
 * Arguments:
 *	int	iHandle
 *		As per the standard read () system call
 *	void	*pvBuffer
 *		As per the standard read () system call
 *	size_t	tCount
 *		As per the standard read () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The actual number of bytes read
 * Problems:
 *	NONE known
 */
ssize_t
tVBRead (int iHandle, void *pvBuffer, size_t tCount)
{
	return (read (iHandle, pvBuffer, tCount));
}

/*
 * Name:
 *	ssize_t	tVBWrite (int iHandle, void *pvBuffer, size_t tCount);
 * Arguments:
 *	int	iHandle
 *		As per the standard write () system call
 *	void	*pvBuffer
 *		As per the standard write () system call
 *	size_t	tCount
 *		As per the standard write () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The actual number of bytes written
 * Problems:
 *	NONE known
 */
ssize_t
tVBWrite (int iHandle, void *pvBuffer, size_t tCount)
{
	return (write (iHandle, pvBuffer, tCount));
}

/*
 * Name:
 *	int	iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode);
 * Arguments:
 *	int	iHandle
 *		The handle of an open file
 *	off_t	tOffset
 *		The start address of the lock being placed
 *	off_t	tLength
 *		The length (in bytes) to lock
 *	int	iMode
 *		VBUNLOCK - Unlock the region
 *		VBRDLOCK - If already locked, error else read lock the region
 *		VBRDLCKW - As above but use a blocking lock
 *		VBRDLOCK - If already locked, error else write lock the region
 *		VBRDLCKW - As above but use a blocking lock
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The lock was successfully established
 * Problems:
 *	NONE known
 */
int
iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode)
{
	int	iCommand;
	struct	flock
		sFlock;

	switch (iMode)
	{
	case	VBUNLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_UNLCK;
		break;

	case	VBRDLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_RDLCK;
		break;

	case	VBRDLCKW:
		iCommand = F_SETLKW;
		sFlock.l_type = F_RDLCK;
		break;

	case	VBWRLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_WRLCK;
		break;

	case	VBWRLCKW:
		iCommand = F_SETLKW;
		sFlock.l_type = F_WRLCK;
		break;

	default:
		errno = EBADARG;
		return (-1);
	}
	sFlock.l_whence = SEEK_SET;
	sFlock.l_start = tOffset;
	sFlock.l_len = tLength;
	sFlock.l_pid = 0;
	if (fcntl (iHandle, iCommand, &sFlock))
		return (-1);
	return (0);
}

/*
 * Name:
 *	int	iVBLink (char *pcOldFilename, char *pcNewFilename);
 * Arguments:
 *	char	*pcOldFilename
 *		The null terminated old filename to be linked
 *	char	*pcNewFilename
 *		The null terminated new filename to be linked
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The link succeeded
 * Problems:
 *	NONE known
 */
int
iVBLink (char *pcOldFilename, char *pcNewFilename)
{
	return (link (pcOldFilename, pcNewFilename));
}

/*
 * Name:
 *	int	iVBUnlink (char *pcFilename);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be erased
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The unlink succeeded
 * Problems:
 *	NONE known
 */
int
iVBUnlink (char *pcFilename)
{
	return (unlink (pcFilename));
}

/*
 * Name:
 *	int	iVBAccess (char *pcFilename, int iMode);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be tested
 *	int	iMode
 *		See access(2) system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The requested access is granted
 * Problems:
 *	NONE known
 */
int
iVBAccess (char *pcFilename, int iMode)
{
	return (access (pcFilename, iMode));
}

/*
 * Name:
 *	void	vVBBlockDeinit (void)
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 * Comments:
 *	Releases all the allocated blocks back to the system
 */
void
vVBBlockDeinit (void)
{
	struct	VBBLOCK
		*psBlock = psBlockHead;

	for (; psBlock; psBlock = psBlockHead)
	{
		psBlockHead = psBlock->psNext;
		vVBFree (psBlock, sizeof (struct VBBLOCK));
	}
	psBlockHead = VBBLOCK_NULL;
	iInitialized = FALSE;
}

/*
 * Name:
 *	void	vVBBlockInvalidate (int iHandle)
 * Arguments:
 *	int	iHandle
 *		The VBISAM file handle
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 * Comments:
 *	Flags all blocks associated with iHandle as invalid and moves them all
 *	to the tail
 */
void
vVBBlockInvalidate (int iHandle)
{
	struct	VBBLOCK
		*psBlock = psBlockHead,
		*psBlockNext;

	for (; psBlock && psBlock->iFileHandle != -1; psBlock = psBlockNext)
	{
		psBlockNext = psBlock->psNext;
		if (psBlock->iFileHandle == psVBFile [iHandle]->iDataHandle || psBlock->iFileHandle == psVBFile [iHandle]->iIndexHandle)
		{
			// Set the filehandle to -1 and move it to the END
			psBlock->iFileHandle = -1;
			if (psBlock->psPrev)
				psBlock->psPrev->psNext = psBlock->psNext;
			else
				psBlockHead = psBlock->psNext;
			if (psBlock->psNext)
			{
				psBlock->psNext->psPrev = psBlock->psPrev;
				psBlock->psPrev = psBlockTail;
				psBlockTail->psNext = psBlock;
				psBlockTail = psBlock;
				psBlock->psNext = VBBLOCK_NULL;
			}
		}
	}
	psVBFile [iHandle]->sFlags.iIndexChanged = 2;
}

/*
 * Name:
 *	int	iVBBlockRead (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer)
 * Arguments:
 *	int	iHandle
 *		The VBISAM file handle
 *	int	iIsIndex
 *		TRUE
 *			We're reading the index file
 *		FALSE
 *			We're reading the data file
 *	off_t	tBlockNumber
 *		The absolute blocknumber to read (1 = TOF)
 *	char	*cBuffer
 *		The place to put the data read in
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
int
iVBBlockRead (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer)
{
	int	iFileHandle;
	struct	VBBLOCK
		*psBlock;
	ssize_t	tLength;
	off_t	tOffset,
		tResult;

	if (!iInitialized)
		vBlockInit ();
	if (iIsIndex)
		iFileHandle = psVBFile [iHandle]->iIndexHandle;
	else
		iFileHandle = psVBFile [iHandle]->iDataHandle;
	if (psVBFile [iHandle]->sFlags.iIndexChanged == 1)
		vVBBlockInvalidate (iHandle);
	for (psBlock = psBlockHead; psBlock; psBlock = psBlock->psNext)
		if (psBlock->iFileHandle == iFileHandle && psBlock->tBlockNumber == tBlockNumber)
			break;
	/*
	 * We *CANNOT* rely on buffering index node #1 since it *IS* the node
	 * that we *MUST* read in to determine whether the buffers are dirty.
	 * An obvious exception to this is ISEXCLLOCK mode.
	 */
	if (iIsIndex && tBlockNumber == 1 && !(psVBFile [iHandle]->iOpenMode & ISEXCLLOCK))
	{
		if (!psBlock)
			psBlock = psBlockTail;
	}
	else
	{
		if (psBlock)
		{
			memcpy (cBuffer, psBlock->cBuffer, psVBFile [iHandle]->iNodeSize);
			psBlock->iFileHandle = iFileHandle;
			psBlock->tBlockNumber = tBlockNumber;
			// Already at head of list?
			if (!psBlock->psPrev)
				return (0);
			// Remove psBlock from the list
			psBlock->psPrev->psNext = psBlock->psNext;
			if (psBlock->psNext)
				psBlock->psNext->psPrev = psBlock->psPrev;
			else
				psBlockTail = psBlock->psPrev;
			// Re-insert it at the head
			psBlock->psPrev = VBBLOCK_NULL;
			psBlock->psNext = psBlockHead;
			psBlockHead->psPrev = psBlock;
			psBlockHead = psBlock;
			return (0);
		}
		psBlock = psBlockTail;
	}
	tOffset = (tBlockNumber - 1) * psVBFile [iHandle]->iNodeSize;
	if (iIsIndex)
	{
		if (psVBFile [iHandle]->tIndexPosn == tOffset)
			tResult = tOffset;
		else
			tResult = tVBLseek (iFileHandle, tOffset, SEEK_SET);
	}
	else
	{
		if (psVBFile [iHandle]->tDataPosn == tOffset)
			tResult = tOffset;
		else
			tResult = tVBLseek (iFileHandle, tOffset, SEEK_SET);
	}
	if (tResult == tOffset)
	{
		tLength = tVBRead (iFileHandle, (void *) psBlock->cBuffer, (size_t) psVBFile [iHandle]->iNodeSize);
		if (tLength == (ssize_t) psVBFile [iHandle]->iNodeSize)
		{
			if (iIsIndex)
				psVBFile [iHandle]->tIndexPosn = tOffset + psVBFile [iHandle]->iNodeSize;
			else
				psVBFile [iHandle]->tDataPosn = tOffset + psVBFile [iHandle]->iNodeSize;
			memcpy (cBuffer, psBlock->cBuffer, psVBFile [iHandle]->iNodeSize);
			psBlock->iFileHandle = iFileHandle;
			psBlock->tBlockNumber = tBlockNumber;
			// Already at head of list?
			if (!psBlock->psPrev)
				return (0);
			// Remove psBlock from the list
			psBlock->psPrev->psNext = psBlock->psNext;
			if (psBlock->psNext)
				psBlock->psNext->psPrev = psBlock->psPrev;
			else
				psBlockTail = psBlock->psPrev;
			// Re-insert it at the head
			psBlock->psPrev = VBBLOCK_NULL;
			psBlock->psNext = psBlockHead;
			psBlockHead->psPrev = psBlock;
			psBlockHead = psBlock;
			return (0);
		}
	}
	// Oops, an error occured...
	iserrno = errno;
	if (iIsIndex)
		psVBFile [iHandle]->tIndexPosn = -1;
	else
		psVBFile [iHandle]->tDataPosn = -1;
	// Already at tail?
	if (!psBlock->psNext)
		return (-1);
	// Move the invalidated block to tail
	if (psBlock->psPrev)
		psBlock->psPrev->psNext = psBlock->psPrev;
	else
		psBlockHead = psBlock->psPrev;
	psBlock->psNext->psPrev = psBlock->psPrev;
	psBlockTail->psNext = psBlock;
	psBlock->psPrev = psBlockTail;
	psBlock->psNext = VBBLOCK_NULL;
	psBlockTail = psBlock;

	return (-1);
}

/*
 * Name:
 *	int	iVBBlockWrite (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer)
 * Arguments:
 *	int	iHandle
 *		The VBISAM file handle
 *	int	iIsIndex
 *		TRUE
 *			We're writing the index file
 *		FALSE
 *			We're writing the data file
 *	off_t	tBlockNumber
 *		The absolute blocknumber to read (1 = TOF)
 *	char	*cBuffer
 *		The data to write to the file
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */

int
iVBBlockWrite (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer)
{
	int	iFileHandle;
	struct	VBBLOCK
		*psBlock;
	ssize_t	tLength;
	off_t	tOffset,
		tResult;

	if (!iInitialized)
		vBlockInit ();
	if (iIsIndex)
		iFileHandle = psVBFile [iHandle]->iIndexHandle;
	else
		iFileHandle = psVBFile [iHandle]->iDataHandle;
	if (psVBFile [iHandle]->sFlags.iIndexChanged == 1)
		vVBBlockInvalidate (iHandle);
	for (psBlock = psBlockHead; psBlock; psBlock = psBlock->psNext)
		if (psBlock->iFileHandle == iFileHandle && psBlock->tBlockNumber == tBlockNumber)
			break;
	if (!psBlock)
		psBlock = psBlockTail;
	tOffset = (tBlockNumber - 1) * psVBFile [iHandle]->iNodeSize;
	if (iIsIndex)
	{
		if (psVBFile [iHandle]->tIndexPosn == tOffset)
			tResult = tOffset;
		else
			tResult = tVBLseek (iFileHandle, tOffset, SEEK_SET);
	}
	else
	{
		if (psVBFile [iHandle]->tDataPosn == tOffset)
			tResult = tOffset;
		else
			tResult = tVBLseek (iFileHandle, tOffset, SEEK_SET);
	}
	if (tResult == tOffset)
	{
		memcpy (psBlock->cBuffer, cBuffer, psVBFile [iHandle]->iNodeSize);
		tLength = tVBWrite (iFileHandle, (void *) psBlock->cBuffer, (size_t) psVBFile [iHandle]->iNodeSize);
		if (tLength == (ssize_t) psVBFile [iHandle]->iNodeSize)
		{
			if (iIsIndex)
				psVBFile [iHandle]->tIndexPosn = tOffset + psVBFile [iHandle]->iNodeSize;
			else
				psVBFile [iHandle]->tDataPosn = tOffset + psVBFile [iHandle]->iNodeSize;
			psBlock->iFileHandle = iFileHandle;
			psBlock->tBlockNumber = tBlockNumber;
			// Already at head of list?
			if (!psBlock->psPrev)
				return (0);
			// Remove psBlock from the list
			psBlock->psPrev->psNext = psBlock->psNext;
			if (psBlock->psNext)
				psBlock->psNext->psPrev = psBlock->psPrev;
			else
				psBlockTail = psBlock->psPrev;
			// Re-insert it at the head
			psBlock->psPrev = VBBLOCK_NULL;
			psBlock->psNext = psBlockHead;
			psBlockHead->psPrev = psBlock;
			psBlockHead = psBlock;
			return (0);
		}
	}
	// Oops, an error occured...
	iserrno = errno;
	if (iIsIndex)
		psVBFile [iHandle]->tIndexPosn = -1;
	else
		psVBFile [iHandle]->tDataPosn = -1;
	// Already at tail?
	if (!psBlock->psNext)
		return (-1);
	// Move the invalidated block to tail
	if (psBlock->psPrev)
		psBlock->psPrev->psNext = psBlock->psPrev;
	else
		psBlockHead = psBlock->psPrev;
	psBlock->psNext->psPrev = psBlock->psPrev;
	psBlockTail->psNext = psBlock;
	psBlock->psPrev = psBlockTail;
	psBlock->psNext = VBBLOCK_NULL;
	psBlockTail = psBlock;
	
	return (-1);
}

/*
 * Name:
 *	static	void	vBlockInit (void)
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 * Comments:
 *	Creates a linked list of blocks
 */
static	void
vBlockInit (void)
{
	char	*pcEnviron;
	struct	VBBLOCK
		*psBlock = VBBLOCK_NULL,
		*psBlockLast = VBBLOCK_NULL;

	iInitialized = TRUE;
	pcEnviron = getenv ("VB_BLOCK_BUFFERS");
	if (pcEnviron)
		iBlockCount = atoi (pcEnviron);
	if (iBlockCount < 4 || iBlockCount > 128)
		iBlockCount = 64;
	while (iBlockCount--)
	{
		psBlock = (struct VBBLOCK *) pvVBMalloc (sizeof (struct VBBLOCK));
		if (psBlock)
		{
			if (psBlockLast)
				psBlockLast->psNext = psBlock;
			else
				psBlockHead = psBlock;
			psBlock->psPrev = psBlockLast;
			psBlock->iFileHandle = -1;
			psBlockLast = psBlock;
		}
		else
		{
			fprintf (stderr, "Insufficient memory!\n");
			exit (-1);
		}
	}
	psBlock->psNext = VBBLOCK_NULL;
	psBlockTail = psBlock;
}
