/*
 * Title:	vbIndexIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	21Nov2003
 * Description:
 *	This module handles ALL the low level index file I/O operations for the
 *	VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: vbIndexIO.c,v $
 *	Revision 1.1  2003/12/20 20:11:17  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

static	char	cNode0 [MAX_NODE_LENGTH],
		cNode1 [MAX_NODE_LENGTH];

/*
 * Prototypes
 */
int	iVBNodeRead (int, void *, off_t);
int	iVBNodeWrite (int, void *, off_t);
int	iVBDictionarySet (int, off_t, off_t);
off_t	tVBDictionaryGet (int, off_t);
off_t	tVBDictionaryIncrement (int, off_t, int);
int	iVBUniqueIDSet (int, off_t);
off_t	tVBUniqueIDGetNext (int);
off_t	tVBTransNumberSet (int);
int	iVBTransNumberGet (int, off_t);
off_t	tVBNodeCountGetNext (int);
off_t	tVBDataCountGetNext (int);
int	iVBNodeFree (int, off_t);
int	iVBDataFree (int, off_t);
//iVBVarlenFree - Future
off_t	tVBNodeAllocate (int);
off_t	tVBDataAllocate (int iHandle);
//tVBVarlenAllocate - Future

/*
 * Name:
 *	int	iVBNodeRead (int iHandle, void *pvBuffer, off_t tNodeNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	void	*pvBuffer
 *		The address of the buffer
 *	off_t	tNodeNumber
 *		The node number to be read in
 * Prerequisites:
 *	Any needed locking is handled external to this function
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 * Problems:
 *	NONE known
 */
int
iVBNodeRead (int iHandle, void *pvBuffer, off_t tNodeNumber)
{
	off_t	tResult,
		tOffset;

	// Sanity check - Is iHandle a currently open table?
	if (!psVBFile [iHandle])
		return (ENOTOPEN);
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (EBADARG);

	tOffset = (off_t) ((tNodeNumber - 1) * psVBFile [iHandle]->iNodeSize);
	tResult = tVBLseek (psVBFile [iHandle]->iIndexHandle, tOffset, SEEK_SET);
	if (tResult == (off_t) -1)
		return (EBADFILE);

	tResult = (off_t) tVBRead (psVBFile [iHandle]->iIndexHandle, pvBuffer, (size_t) psVBFile [iHandle]->iNodeSize);
	if ((int) tResult != psVBFile [iHandle]->iNodeSize)
		return (EBADFILE);
	return (0);
}

/*
 * Name:
 *	int	iVBNodeWrite (int iHandle, void *pvBuffer, off_t tNodeNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	void	*pvBuffer
 *		The address of the buffer
 *	off_t	tNodeNumber
 *		The node number to be written out to
 * Prerequisites:
 *	Any needed locking is handled external to this function
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 * Problems:
 *	NONE known
 */
int
iVBNodeWrite (int iHandle, void *pvBuffer, off_t tNodeNumber)
{
	off_t	tResult,
		tOffset;

	// Sanity check - Is iHandle a currently open table?
	if (!psVBFile [iHandle])
		return (ENOTOPEN);
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (EBADARG);

	// BUG? - Should we be allowed to write if the handle is open ISINPUT?

	tOffset = (off_t) ((tNodeNumber - 1) * psVBFile [iHandle]->iNodeSize);
	tResult = tVBLseek (psVBFile [iHandle]->iIndexHandle, tOffset, SEEK_SET);
	if (tResult == (off_t) -1)
		return (EBADFILE);

	tResult = (off_t) tVBWrite (psVBFile [iHandle]->iIndexHandle, pvBuffer, (size_t) psVBFile [iHandle]->iNodeSize);
	if ((int) tResult != psVBFile [iHandle]->iNodeSize)
		return (EBADFILE);
	return (0);
}

/*
 * Name:
 *	int	iVBUniqueIDSet (int iHandle, off_t tUniqueID);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	off_t	tUniqueID
 *		The new starting unique ID
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADARG
 * Problems:
 *	NONE known
 * Comments:
 *	This routine is used to set the current unique id to a new starting
 *	number.  It REFUSES to set it to a lower number than is current!
 */
int
iVBUniqueIDSet (int iHandle, off_t tUniqueID)
{
	off_t	tValue;

	// Sanity check - Is iHandle a currently open table?
	if (!psVBFile [iHandle])
		return (ENOTOPEN);
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (EBADARG);

	tValue = ldquad (psVBFile [iHandle]->sDictNode.cUniqueID);
	if (tUniqueID > tValue)
	{
		stquad (tValue, psVBFile [iHandle]->sDictNode.cUniqueID);
		psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	}
	return (0);
}

/*
 * Name:
 *	off_t	tVBUniqueIDGetNext (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Failure (iserrno has more info)
 *	OTHER	The new unique id
 * Problems:
 *	NONE known
 */
off_t
tVBUniqueIDGetNext (int iHandle)
{
	off_t	tValue;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	tValue = ldquad (psVBFile [iHandle]->sDictNode.cUniqueID) + 1;
	stquad (tValue, psVBFile [iHandle]->sDictNode.cUniqueID);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	return (tValue);
}

/*
 * Name:
 *	off_t	tVBNodeCountGetNext (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Failure (iserrno has more info)
 *	OTHER	The new index node number
 * Problems:
 *	NONE known
 */
off_t
tVBNodeCountGetNext (int iHandle)
{
	off_t	tValue;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	tValue = ldquad (psVBFile [iHandle]->sDictNode.cNodeCount) + 1;
	stquad (tValue, psVBFile [iHandle]->sDictNode.cNodeCount);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	return (tValue);
}

/*
 * Name:
 *	off_t	tVBDataCountGetNext (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Failure (iserrno has more info)
 *	OTHER	The new (data) row number
 * Problems:
 *	NONE known
 */
off_t
tVBDataCountGetNext (int iHandle)
{
	off_t	tValue;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	tValue = ldquad (psVBFile [iHandle]->sDictNode.cDataCount) + 1;
	stquad (tValue, psVBFile [iHandle]->sDictNode.cDataCount);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	return (tValue);
}

/*
 * Name:
 *	int	iVBNodeFree (int iHandle, off_t tNodeNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	off_t	tNodeNumber
 *		The node number to append to the index node free list
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 *	EBADARG
 *	BUG - OTHER?
 * Problems:
 *	NONE known
 * Comments:
 *	By the time this function is called, tNodeNumber should be COMPLETELY
 *	unreferenced by any other node.
 */
int
iVBNodeFree (int iHandle, off_t tNodeNumber)
{
	int	iLengthUsed,
		iResult;
	off_t	tHeadNode;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	memset (cNode1, 0, (size_t) psVBFile [iHandle]->iNodeSize);
	memcpy (cNode1, "VB00", 4);
	stint ((4 + INTSIZE), &cNode1 [4]);

	tHeadNode = ldquad (psVBFile [iHandle]->sDictNode.cNodeFree);
	// If the list is empty, node tNodeNumber becomes the whole list
	if (tHeadNode == (off_t) 0)
	{
		memcpy (cNode1, "VB04", 4);
		stint ((4 + (INTSIZE * 2) + QUADSIZE), &cNode1 [4]);
		//iResult = iVBNodeWrite (iHandle, (void *) cNode1, tNodeNumber);
		iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cNode1);
		if (iResult)
			return (iResult);
		stquad (tNodeNumber, psVBFile [iHandle]->sDictNode.cNodeFree);
		psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
		return (0);
	}

	// Read in the head of the current free list
	//iResult = iVBNodeRead (iHandle, (void *) cNode0, tHeadNode);
	iResult = iVBBlockRead (iHandle, TRUE, tHeadNode, cNode0);
	if (iResult)
		return (iResult);
	if (memcmp (cNode0, "VB04", 4))
		return (EBADFILE);
	iLengthUsed = ldint (&cNode0 [4]);
	if (iLengthUsed >= psVBFile [iHandle]->iNodeSize)
	{
		// If there was no space left, tNodeNumber becomes the head
		memcpy (cNode1, "VB04", 4);
		stint ((4 + (INTSIZE * 2) + QUADSIZE), &cNode1 [4]);
		stquad (tHeadNode, &cNode1 [4 + (INTSIZE * 2)]);
		//iResult = iVBNodeWrite (iHandle, (void *) cNode1, tNodeNumber);
		iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cNode1);
		if (!iResult)
		{
			stquad (tNodeNumber, psVBFile [iHandle]->sDictNode.cNodeFree);
			psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
		}
		return (iResult);
	}

	// If we got here, there's space left in the tHeadNode to store it
	//iResult = iVBNodeWrite (iHandle, (void *) cNode1, tNodeNumber);
	iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cNode1);
	if (iResult)
		return (iResult);
	stquad (tNodeNumber, &cNode0 [iLengthUsed]);
	iLengthUsed += QUADSIZE;
	stint (iLengthUsed, &cNode0 [4]);
	//iResult = iVBNodeWrite (iHandle, (void *) cNode0, tHeadNode);
	iResult = iVBBlockWrite (iHandle, TRUE, tHeadNode, cNode0);

	return (0);
}

/*
 * Name:
 *	int	iVBDataFree (int iHandle, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	off_t	tRowNumber
 *		The absolute row number to be added onto the data row free list
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN?
 *	EBADFILE?
 *	EBADARG?
 *	BUG - OTHER?
 * Problems:
 *	NONE known
 */
int
iVBDataFree (int iHandle, off_t tRowNumber)
{
	int	iLengthUsed,
		iResult;
	off_t	tHeadNode,
		tNodeNumber;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	tHeadNode = ldquad (psVBFile [iHandle]->sDictNode.cDataFree);
	if (tHeadNode != (off_t) 0)
	{
		//iResult = iVBNodeRead (iHandle, (void *) cNode0, tHeadNode);
		iResult = iVBBlockRead (iHandle, TRUE, tHeadNode, cNode0);
		if (iResult)
			return (iResult);
		if (memcmp (cNode0, "VB04", 4))
			return (EBADFILE);
		iLengthUsed = ldint (&cNode0 [4]);
		if (iLengthUsed < psVBFile [iHandle]->iNodeSize)
		{
			// We need to add tRowNumber to the current node
			stquad ((off_t) tRowNumber, &cNode0 [iLengthUsed]);
			iLengthUsed += QUADSIZE;
			stint (iLengthUsed, &cNode0 [4]);
			//iResult = iVBNodeWrite (iHandle, (void *) cNode0, tHeadNode);
			iResult = iVBBlockWrite (iHandle, TRUE, tHeadNode, cNode0);
			return (iResult);
		}
	}
	// We need to allocate a new row-free node!
	// We append any existing nodes using the next pointer from the new node
	tNodeNumber = tVBNodeAllocate (iHandle);
	if (tNodeNumber == (off_t) -1)
		return (iserrno);
	memset (cNode0, 0, MAX_NODE_LENGTH);
	memcpy (cNode0, "VB04", 4);
	stint (4 + (INTSIZE * 2) + QUADSIZE + QUADSIZE, &cNode0 [4]);
	stquad (tHeadNode, &cNode0 [4 + (INTSIZE * 2)]);
	stquad (tRowNumber, &cNode0 [4 + (INTSIZE * 2) + QUADSIZE]);
	//iResult = iVBNodeWrite (iHandle, (void *) cNode0, tNodeNumber);
	iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cNode0);
	if (iResult)
		return (iResult);
	stquad (tNodeNumber, psVBFile [iHandle]->sDictNode.cDataFree);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	return (0);
}
//iVBVarlenFree - Future

/*
 * Name:
 *	off_t	tVBNodeAllocate (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno has more info)
 *	OTHER	The newly allocated node number
 * Problems:
 *	NONE known
 */
off_t
tVBNodeAllocate (int iHandle)
{
	int	iLengthUsed;
	off_t	tHeadNode,
		tValue;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	// If there's *ANY* nodes in the free list, use them first!
	tHeadNode = ldquad (psVBFile [iHandle]->sDictNode.cNodeFree);
	if (tHeadNode != (off_t) 0)
	{
		//iserrno = iVBNodeRead (iHandle, (void *) cNode0, tHeadNode);
		iserrno = iVBBlockRead (iHandle, TRUE, tHeadNode, cNode0);
		if (iserrno)
			return (-1);
		iserrno = EBADFILE;
		if (memcmp (cNode0, "VB04", 4))
			return (-1);
		iLengthUsed = ldint (&cNode0 [4]);
		if (iLengthUsed > 4 + (INTSIZE * 2) + QUADSIZE)
		{
			iLengthUsed -= QUADSIZE;
			stint (iLengthUsed, &cNode0 [4]);
			tValue = ldquad (&cNode0 [iLengthUsed]);
			stquad ((off_t) 0, &cNode0 [iLengthUsed]);
			//iserrno = iVBNodeRead (iHandle, (void *) cNode1, tValue);
			iserrno = iVBBlockRead (iHandle, TRUE, tValue, cNode1);
			if (iserrno || memcmp (cNode1, "VB00", 4))
				return (-1);
			//iserrno = iVBNodeWrite (iHandle, (void *) cNode0, tHeadNode);
			iserrno = iVBBlockWrite (iHandle, TRUE, tHeadNode, cNode0);
			if (iserrno)
				return (-1);
			return (tValue);
		}
		// If it's last entry in the node, use the node itself!
		tValue = ldquad (&cNode0 [4 + (INTSIZE * 2)]);
		stquad (tValue, psVBFile [iHandle]->sDictNode.cNodeFree);
		psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
		return (tHeadNode);
	}
	// If we get here, we need to allocate a NEW node.
	// Since we already hold a dictionary lock, we don't need another
	tValue = tVBNodeCountGetNext (iHandle);
	return (tValue);
}

/*
 * Name:
 *	off_t	tVBDataAllocate (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	ENOTOPEN?
 *	EBADFILE?
 *	EBADARG?
 *	BUG - OTHER?
 * Problems:
 *	NONE known
 */
off_t
tVBDataAllocate (int iHandle)
{
	int	iLengthUsed,
		iResult;
	off_t	tHeadNode,
		tNextNode,
		tValue;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADARG;
	if (!psVBFile [iHandle]->sFlags.iIsDictLocked)
		return (-1);
	iserrno = 0;

	// If there's *ANY* rows in the free list, use them first!
	tHeadNode = ldquad (psVBFile [iHandle]->sDictNode.cDataFree);
	while (tHeadNode != (off_t) 0)
	{
		//iserrno = iVBNodeRead (iHandle, (void *) cNode0, tHeadNode);
		iserrno = iVBBlockRead (iHandle, TRUE, tHeadNode, cNode0);
		if (iserrno)
			return (-1);
		iserrno = EBADFILE;
		if (memcmp (cNode0, "VB04", 4))
			return (-1);
		iLengthUsed = ldint (&cNode0 [4]);
		if (iLengthUsed > 4 + (INTSIZE * 2) + QUADSIZE)
		{
			iLengthUsed -= QUADSIZE;
			stint (iLengthUsed, &cNode0 [4]);
			tValue = ldquad (&cNode0 [iLengthUsed]);
			stquad ((off_t) 0, &cNode0 [iLengthUsed]);
			if (iLengthUsed > 4 + (INTSIZE * 2) + QUADSIZE)
			{
				//iserrno = iVBNodeWrite (iHandle, (void *) cNode0, tHeadNode);
				iserrno = iVBBlockWrite (iHandle, TRUE, tHeadNode, cNode0);
				if (iserrno)
					return (-1);
				return (tValue);
			}
			// If we're using the last entry in the node, advance
			tNextNode = ldquad (&cNode0 [4 + (INTSIZE * 2)]);
			iResult = iVBNodeFree (iHandle, tHeadNode);
			if (iResult)
				return (-1);
			stquad (tNextNode, psVBFile [iHandle]->sDictNode.cDataFree);
			psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
			return (tValue);
		}
		// Ummmm, this is an INTEGRITY ERROR of sorts!
		// However, let's fix it anyway!
		tNextNode = ldquad (&cNode0 [4 + (INTSIZE * 2)]);
		iResult = iVBNodeFree (iHandle, tHeadNode);
		if (iResult)
			return (-1);
		stquad (tNextNode, psVBFile [iHandle]->sDictNode.cDataFree);
		psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
		tHeadNode = tNextNode;
	}
	// If we get here, we need to allocate a NEW row number.
	// Since we already hold a dictionary lock, we don't need another
	tValue = tVBDataCountGetNext (iHandle);
	return (tValue);
}
//iVBVarlenAllocate - Future
