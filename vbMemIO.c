/*
 * Title:	vbMemIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	08Dec2003
 * Description:
 *	This is the module that deals with *ALL* memory (de-)allocation for the
 *	VBISAM library.
 * Version:
 *	$Id: vbMemIO.c,v 1.2 2003/12/22 04:49:30 trev_vb Exp $
 * Modification History:
 *	$Log: vbMemIO.c,v $
 *	Revision 1.2  2003/12/22 04:49:30  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:20  trev_vb
 *	Init import
 *	
 * BUG - We need to add in a 'garbage collection' function in this module to
 * BUG -  traverse the various free lists periodically and free them up.
 */
#define	VBISAMMAIN
#include	"isinternal.h"

static	int	iCurrHandle = -1;

/*
 * Prototypes
 */
struct	VBLOCK *psVBLockAllocate (int);
void	vVBLockFree (struct VBLOCK *);
struct	VBTREE	*psVBTreeAllocate (int);
void	vVBTreeAllFree (int, int, struct VBTREE *);
struct	VBKEY *psVBKeyAllocate (int, int);
void	vVBKeyAllFree (int, int, struct VBTREE *);
void	vVBKeyFree (int, int, struct VBKEY *);
void	vVBKeyUnMalloc (int, int);
void	*pvVBMalloc (size_t);
void	vVBFree (void *, size_t);
void	vVBUnMalloc (void);
#ifdef	DEBUG
void	vVBMallocReport (void);
#endif	// DEBUG

static	size_t
	tMallocUsed = 0,
	tMallocMax = 0;
static	struct	VBLOCK
	*psLockFree = VBLOCK_NULL;
static	struct	VBTREE
	*psTreeFree = VBTREE_NULL;

/*
 * Name:
 *	int	psVBLockAllocate (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The corresponding VBISAM file handle
 * Prerequisites:
 *	NONE
 * Returns:
 *	VBLOCK_NULL Ran out out memory
 *	OTHER Pointer to the allocated structure
 * Problems:
 *	NONE known
 */
struct	VBLOCK	*
psVBLockAllocate (int iHandle)
{
	struct	VBLOCK
		*psLock = psLockFree;

	iCurrHandle = iHandle;
	if (psLockFree != VBLOCK_NULL)
		psLockFree = psLockFree->psNext;
	else
		psLock = (struct VBLOCK *) pvVBMalloc (sizeof (struct VBLOCK));
	iCurrHandle = -1;
	if (psLock)
		memset (psLock, 0, sizeof (struct VBLOCK));
	return (psLock);
}

/*
 * Name:
 *	void	vVBLockFree (struct VBLOCK *psLock);
 * Arguments:
 *	struct	VBLOCK	*psLock
 *		A previously allocated lock structure
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 */
void
vVBLockFree (struct VBLOCK *psLock)
{
	psLock->psNext = psLockFree;
	psLockFree = psLock;
	return;
}

/*
 * Name:
 *	struct VBTREE	*psVBTreeAllocate (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The corresponding VBISAM file handle
 * Prerequisites:
 *	NONE
 * Returns:
 *	VBTREE_NULL Ran out out memory
 *	OTHER Pointer to the allocated structure
 * Problems:
 *	NONE known
 */
struct	VBTREE	*
psVBTreeAllocate (int iHandle)
{
	struct	VBTREE
		*psTree = psTreeFree;

	iCurrHandle = iHandle;
	if (psTreeFree != VBTREE_NULL)
		psTreeFree = psTreeFree->psNext;
	else
		psTree = (struct VBTREE *) pvVBMalloc (sizeof (struct VBTREE));
	iCurrHandle = -1;
	if (psTree)
		memset (psTree, 0, sizeof (struct VBTREE));
	return (psTree);
}

/*
 * Name:
 *	void	vVBTreeAllFree (int iHandle, int iKeyNumber, struct VBTREE *psTree);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM file handle
 *	int	iKeyNumber
 *		The key number in question
 *	struct	VBTREE	*psTree
 *		The head entry of the list of VBTREE's to be de-allocated
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 * Comments:
 *	Simply transfers an *ENTIRE* Tree to the free list
 *	Any associated VBKEY structures are moved to the relevant entry
 *	(psVBFile [iHandle]->psKeyFree [iKeyNumber])
 */
void
vVBTreeAllFree (int iHandle, int iKeyNumber, struct VBTREE *psTree)
{
	if (!psTree)
		return;
	vVBKeyAllFree (iHandle, iKeyNumber, psTree);
	psTree->psNext = psTreeFree;
	psTreeFree = psTree;
}

/*
 * Name:
 *	struct	VBKEY	*psVBKeyAllocate (int iHandle, int iKeyNumber);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM file handle
 *	int	iKeyNumber
 *		The key number in question
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 * Returns:
 *	VBKEY_NULL Ran out out memory
 *	OTHER Pointer to the allocated structure
 */
struct	VBKEY	*
psVBKeyAllocate (int iHandle, int iKeyNumber)
{
	int	iLength = 0;
	struct	VBKEY
		*psKey;

	psKey = psVBFile [iHandle]->psKeyFree [iKeyNumber];
	if (psKey == VBKEY_NULL)
	{
		iCurrHandle = iHandle;
		iLength = psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength;
		psKey = (struct VBKEY *) pvVBMalloc (sizeof (struct VBKEY) + iLength);
		iCurrHandle = -1;
	}
	else
		psVBFile [iHandle]->psKeyFree [iKeyNumber] = psVBFile [iHandle]->psKeyFree [iKeyNumber]->psNext;
	if (psKey)
		memset (psKey, 0, (sizeof (struct VBKEY) + iLength));
	return (psKey);
}

/*
 * Name:
 *	void	vVBKeyAllFree (int iHandle, int iKeyNumber, struct VBKEY *psKey);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM file handle
 *	int	iKeyNumber
 *		The key number in question
 *	struct	VBKEY	*psKey
 *		The head pointer of a list of keys to be moved to the free list
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 */
void	vVBKeyAllFree (int iHandle, int iKeyNumber, struct VBTREE *psTree)
{
	struct	VBKEY
		*psKeyCurr = psTree->psKeyFirst,
		*psKeyNext;

	while (psKeyCurr)
	{
		psKeyNext = psKeyCurr->psNext;
		if (psKeyCurr->psChild)
			vVBTreeAllFree (iHandle, iKeyNumber, psKeyCurr->psChild);
		psKeyCurr->psNext = psVBFile [iHandle]->psKeyFree [iKeyNumber];
		psVBFile [iHandle]->psKeyFree [iKeyNumber] = psKeyCurr;
		psKeyCurr = psKeyNext;
	}
	return;
}

/*
 * Name:
 *	void	vVBKeyFree (int iHandle, int iKeyNumber, struct VBKEY *psKey);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM file handle
 *	int	iKeyNumber
 *		The key number in question
 *	struct	VBKEY	*psKey
 *		The VBKEY structure to be moved to the free list
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 */
void	vVBKeyFree (int iHandle, int iKeyNumber, struct VBKEY *psKey)
{
	if (psKey->psChild)
		vVBTreeAllFree (iHandle, iKeyNumber, psKey->psChild);
	if (psKey->psNext)
		psKey->psNext->psPrev = psKey->psPrev;
	if (psKey->psPrev)
		psKey->psPrev->psNext = psKey->psNext;
	psKey->psNext = psVBFile [iHandle]->psKeyFree [iKeyNumber];
	psVBFile [iHandle]->psKeyFree [iKeyNumber] = psKey;
	return;
}

/*
 * Name:
 *	void	vVBKeyUnMalloc (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM file handle
 * Prerequisites:
 *	NONE
 * Problems:
 *	NONE known
 */
void
vVBKeyUnMalloc (int iHandle, int iKeyNumber)
{
	int	iLength;
	struct	VBKEY
		*psKeyCurr = psVBFile [iHandle]->psKeyFree [iKeyNumber];

	iLength = sizeof (struct VBKEY) + psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength;
	while (psKeyCurr)
	{
		psVBFile [iHandle]->psKeyFree [iKeyNumber] = psVBFile [iHandle]->psKeyFree [iKeyNumber]->psNext;
		vVBFree (psKeyCurr, iLength);
		psKeyCurr = psVBFile [iHandle]->psKeyFree [iKeyNumber];
	}
}

/*
 * Name:
 *	void	*pvVBMalloc (size_t tLength);
 * Arguments:
 *	size_t	tLength
 *		The desired length (in bytes) to allocate
 * Prerequisites:
 *	NONE
 * Returns:
 *	(void *) 0
 *		Ran out of RAM, errno = ENOMEN
 *	OTHER
 *		Pointer to the allocated memory
 * Problems:
 *	DONE BUG - We need to extend this function a LOT:
 *	DONE BUG - If the malloc call FAILS (ENOMEM), than we firstly free
 *	DONE BUG - all the entries in the various free lists.  Also, we can free
 *	DONE BUG - up any of the VBTREE / VBKEY lists on ANY open VBISAM file
 *	DONE BUG - where the current transaction number is now stale!
 *	DONE BUG - THEN we should retry the malloc
 */
void	*
pvVBMalloc (size_t tLength)
{
	int	iLoop,
		iLoop2;
	void	*pvPointer;
	struct	VBKEY
		*psKey;
	struct	VBLOCK
		*psLock;
	struct	VBTREE
		*psTree;

	pvPointer = malloc (tLength);
	if (!pvPointer)
	{
	// Firstly, try by freeing up the TRUELY free data
		for (iLoop = 0; iLoop <= iVBMaxUsedHandle; iLoop++)
		{
			if (psVBFile [iLoop])
			{
				for (iLoop2 = 0; iLoop2 < psVBFile [iLoop]->iNKeys; iLoop2++)
				{
					psKey = psVBFile [iLoop]->psKeyFree [iLoop2];
					while (psKey)
					{
						psVBFile [iLoop]->psKeyFree [iLoop2] = psKey->psNext;
						vVBFree (psKey, sizeof (struct VBKEY) + psVBFile [iLoop]->psKeydesc [iLoop2]->iKeyLength);
						psKey = psVBFile [iLoop]->psKeyFree [iLoop2];
					}
				}
			}
		}
		psLock = psLockFree;
		while (psLock)
		{
			psLockFree = psLockFree->psNext;
			vVBFree (psLock, sizeof (struct VBLOCK));
			psLock = psLockFree;
		}
		psTree = psTreeFree;
		while (psTree)
		{
			psTreeFree = psTreeFree->psNext;
			vVBFree (psTree, sizeof (struct VBTREE));
			psTree = psTreeFree;
		}
	}
	if (!pvPointer)
		pvPointer = malloc (tLength);
	if (!pvPointer)
	{
	// Nope, that wasn't enough, try harder!
		for (iLoop = 0; iCurrHandle != -1 && iLoop <= iVBMaxUsedHandle; iLoop++)
		{
			if (psVBFile [iLoop] && iLoop != iCurrHandle)
			{
				for (iLoop2 = 0; iLoop2 < psVBFile [iLoop]->iNKeys; iLoop2++)
				{
					vVBTreeAllFree (iLoop, iLoop2, psVBFile [iLoop]->psTree [iLoop2]);
					psVBFile [iLoop]->psTree [iLoop2] = VBTREE_NULL;
					psVBFile [iLoop]->psKeyCurr [iLoop2] = VBKEY_NULL;
					psKey = psVBFile [iLoop]->psKeyFree [iLoop2];
					while (psKey)
					{
						psVBFile [iLoop]->psKeyFree [iLoop2] = psKey->psNext;
						vVBFree (psKey, sizeof (struct VBKEY) + psVBFile [iLoop]->psKeydesc [iLoop2]->iKeyLength);
						psKey = psVBFile [iLoop]->psKeyFree [iLoop2];
					}
				}
			}
		}
		psTree = psTreeFree;
		while (psTree)
		{
			psTreeFree = psTreeFree->psNext;
			vVBFree (psTree, sizeof (struct VBTREE));
			psTree = psTreeFree;
		}
	}
	if (!pvPointer)
		pvPointer = malloc (tLength);
	// Note from Robin: "Holy pointers batman, we're REALLY out of memory!"
	if (!pvPointer)
		fprintf (stderr, "MALLOC FAULT!\n");
	else
		tMallocUsed += tLength;
	if (tMallocUsed > tMallocMax)
		tMallocMax = tMallocUsed;
	fflush (stderr);
	return (pvPointer);
}

/*
 * Name:
 *	void	vVBFree (void *pvPointer, size_t tLength);
 * Arguments:
 *	void	*pvPointer
 *		A pointer to the previously pvVBMalloc()'d memory
 *	size_t	tLength
 *		The length of the data being released
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
vVBFree (void *pvPointer, size_t tLength)
{
	tMallocUsed -= tLength;
	free (pvPointer);
}

/*
 * Name:
 *	void	vVBUnMalloc (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
vVBUnMalloc (void)
{
	struct	VBLOCK
		*psLockCurr = psLockFree;
	struct	VBTREE
		*psTreeCurr = psTreeFree;

	if (pcRowBuffer)
		vVBFree (pcRowBuffer, iRowBufferLength);
	pcRowBuffer = (char *) 0;
	while (psLockCurr)
	{
		psLockFree = psLockFree->psNext;
		vVBFree (psLockCurr, sizeof (struct VBLOCK));
		psLockCurr = psLockFree;
	}
	while (psTreeCurr)
	{
		psTreeFree = psTreeFree->psNext;
		vVBFree (psTreeCurr, sizeof (struct VBTREE));
		psTreeCurr = psTreeFree;
	}
	iscleanup ();
	vVBBlockDeinit ();
#ifdef	DEBUG
	vVBMallocReport ();
#endif
}

#ifdef	DEBUG
/*
 * Name:
 *	void	vVBMallocReport (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 * Comments:
 *	Simply allows a progam to determine the MAXIMUM allocate RAM usage
 */
void
vVBMallocReport (void)
{
	fprintf (stderr, "Maximum RAM allocation during this run was: 0x%08lX\n", (long) tMallocMax);
	fprintf (stderr, "RAM still allocated at termination is: 0x%08lX\n", (long) tMallocUsed);
	fflush (stderr);
}
#endif	// DEBUG
