/*
 * Title:	vbNodeMemIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	25Nov2003
 * Description:
 *	This module handles the issues of reading a node into the memory-based
 *	linked-lists and writing a node to disk from the memory-based linked-
 *	lists.  The latter possibly causing a split to occur.
 * Version:
 *	$Id: vbNodeMemIO.c,v 1.1 2004/06/06 20:52:21 trev_vb Exp $
 * Modification History:
 *	$Log: vbNodeMemIO.c,v $
 *	Revision 1.1  2004/06/06 20:52:21  trev_vb
 *	06Jun2004 TvB Lots of changes! Performance, stability, bugfixes.  See CHANGELOG
 *	
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	iVBNodeLoad (int, int, struct VBTREE *, off_t, int);
int	iVBNodeSave (int, int, struct VBTREE *, off_t);
static	int	iNodeSplit (int, int, struct VBTREE *, struct VBKEY *);
static	int	iNewRoot (int, int, struct VBTREE *, struct VBTREE *, struct VBTREE *, struct VBKEY *[], off_t, off_t);
#define		TCC	(' ')		// Trailing Compression Character
/*
 * Local scope variables
 */


/*
 * Name:
 *	int	iVBNodeLoad (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber, int iPrevLvl);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	struct VBTREE	*psTree
 *		The VBTREE structure to be filled in with this call
 *	off_t	tNodeNumber
 *		The absolute node number within the index file (1..n)
 *	int	iPrevLvl
 *		-1	We're loading the root node
 *		Other	The node we're loading should be one less than this
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADFILE
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
int
iVBNodeLoad (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber, int iPrevLvl)
{
	char	cPrevKey [VB_MAX_KEYLEN],
		cHighKey [VB_MAX_KEYLEN],
		*pcNodePtr;
	int	iCountLC = 0,		// Leading compression
		iCountTC = 0,		// Trailing compression
		iDups = FALSE,
		iNodeLen,
		iResult;
	struct	VBKEY
		*psKey,
		*psKeyNext;
	struct	keydesc
		*psKeydesc;
#if	_FILE_OFFSET_BITS == 64
	off_t	tTransNumber;
#endif	// _FILE_OFFSET_BITS == 64

	psKeydesc = psVBFile [iHandle]->psKeydesc[iKeyNumber];
	vVBKeyValueSet (0, psKeydesc, cPrevKey);
	vVBKeyValueSet (1, psKeydesc, cHighKey);
	iResult = iVBBlockRead (iHandle, TRUE, tNodeNumber, cVBNode [0]);
	if (iResult)
		return (iResult);
	if (iPrevLvl != -1)
		if (*(cVBNode [0] + psVBFile [iHandle]->iNodeSize - 2) != iPrevLvl - 1)
			return (EBADFILE);
	psTree->tNodeNumber = tNodeNumber;
	psTree->sFlags.iLevel = *(cVBNode [0] + psVBFile [iHandle]->iNodeSize - 2);
	iNodeLen = ldint (cVBNode [0]);
#if	_FILE_OFFSET_BITS == 64
	pcNodePtr = cVBNode [0] + INTSIZE + QUADSIZE;
	tTransNumber = ldquad (cVBNode [0] + INTSIZE);
	if (tTransNumber == psTree->tTransNumber)
		return (0);
#else	// _FILE_OFFSET_BITS == 64
	pcNodePtr = cVBNode [0] + INTSIZE;
#endif	// _FILE_OFFSET_BITS == 64
	for (psKey = psTree->psKeyFirst; psKey; psKey = psKeyNext)
	{
		if (psKey->psChild)
			vVBTreeAllFree (iHandle, iKeyNumber, psKey->psChild);
		psKey->psChild = VBTREE_NULL;
		psKeyNext = psKey->psNext;
		vVBKeyFree (iHandle, iKeyNumber, psKey);
	}
	psTree->psKeyFirst = psTree->psKeyCurr = psTree->psKeyLast = VBKEY_NULL;
	psTree->sFlags.iKeysInNode = 0;
	while (pcNodePtr - cVBNode [0] < iNodeLen)
	{
		psKey = psVBKeyAllocate (iHandle, iKeyNumber);
		if (!psKey)
			return (errno);
		if (!iDups)
		{
			if (psKeydesc->iFlags & LCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				iCountLC = ldint (pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				iCountLC = *(pcNodePtr);
				pcNodePtr++;
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (psKeydesc->iFlags & TCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				iCountTC = ldint (pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				iCountTC = *(pcNodePtr);
				pcNodePtr++;
#endif	// _FILE_OFFSET_BITS == 64
			}
			memcpy (cPrevKey + iCountLC, pcNodePtr, psKeydesc->iKeyLength - (iCountLC + iCountTC));
			memset (cPrevKey + psKeydesc->iKeyLength - iCountTC, TCC, iCountTC);
			pcNodePtr += psKeydesc->iKeyLength - (iCountLC + iCountTC);
		}
		if (psKeydesc->iFlags & ISDUPS)
		{
			psKey->tDupNumber = ldquad (pcNodePtr);
			pcNodePtr += QUADSIZE;
		}
		else
			psKey->tDupNumber = 0;
		if (psKeydesc->iFlags & DCOMPRESS)
		{
			if (*pcNodePtr & 0x80)
				iDups = TRUE;
			else
				iDups = FALSE;
			*pcNodePtr &= ~0x80;
		}
		psKey->tRowNode = ldquad (pcNodePtr);
		pcNodePtr += QUADSIZE;
		psKey->psParent = psTree;
		if (psTree->psKeyFirst)
			psTree->psKeyLast->psNext = psKey;
		else
			psTree->psKeyFirst = psTree->psKeyCurr = psKey;
		psTree->psKeyList [psTree->sFlags.iKeysInNode] = psKey;
		psTree->sFlags.iKeysInNode++;
		psKey->psPrev = psTree->psKeyLast;
		psTree->psKeyLast = psKey;
		memcpy (psKey->cKey, cPrevKey, psKeydesc->iKeyLength);
	}
	if (psTree->sFlags.iLevel)
	{
		psTree->psKeyLast->sFlags.iIsHigh = 1;
		memcpy (psTree->psKeyLast->cKey, cHighKey, psKeydesc->iKeyLength);
	}
	psKey = psVBKeyAllocate (iHandle, iKeyNumber);
	if (!psKey)
		return (errno);
	psKey->psParent = psTree;
	psKey->sFlags.iIsDummy = 1;
	if (psTree->psKeyFirst)
		psTree->psKeyLast->psNext = psKey;
	else
		psTree->psKeyFirst = psTree->psKeyCurr = psKey;
	psKey->psPrev = psTree->psKeyLast;
	psTree->psKeyLast = psKey;
	psTree->psKeyList [psTree->sFlags.iKeysInNode] = psKey;
	psTree->sFlags.iKeysInNode++;
	return (0);
}

/*
 * Name:
 *	int	iVBNodeSave (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	struct	VBTREE	*psTree
 *		The VBTREE structure containing data to write with this call
 *	off_t	tNodeNumber
 *		The absolute node number within the index file (1..n)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	EBADFILE
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
int
iVBNodeSave (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber)
{
	char	*pcKeyEndPtr,
		*pcNodePtr,
		*pcNodeHalfway,
		*pcNodeEnd,
		*pcPrevKey = (char *) 0;
	int	iCountLC = 0,		// Leading compression
		iCountTC = 0,		// Trailing compression
		iLastTC = 0,
		iKeyLen,
		iMaxTC,
		iResult;
	struct	VBKEY
		*psKey,
		*psKeyHalfway = VBKEY_NULL;
	struct	keydesc
		*psKeydesc;

	pcNodeHalfway = cVBNode [0] + (psVBFile [iHandle]->iNodeSize / 2);
	pcNodeEnd = cVBNode [0] + psVBFile [iHandle]->iNodeSize - 2;
	psKeydesc = psVBFile [iHandle]->psKeydesc[iKeyNumber];
	memset (cVBNode [0], 0, MAX_NODE_LENGTH);
#if	_FILE_OFFSET_BITS == 64
	stquad (ldquad (psVBFile [iHandle]->sDictNode.cTransNumber) + 1, cVBNode [0] + INTSIZE);
	pcNodePtr = cVBNode [0] + INTSIZE + QUADSIZE;
#else	// _FILE_OFFSET_BITS == 64
	pcNodePtr = cVBNode [0] + INTSIZE;
#endif	// _FILE_OFFSET_BITS == 64
	*pcNodeEnd = psTree->sFlags.iLevel;
	psTree->sFlags.iKeysInNode = 0;
	for (psKey = psTree->psKeyFirst; psKey && !psKey->sFlags.iIsDummy; psKey = psKey->psNext)
	{
		psTree->psKeyList [psTree->sFlags.iKeysInNode] = psKey;
		psTree->sFlags.iKeysInNode++;
		if (!psKeyHalfway)
			if (pcNodePtr >= pcNodeHalfway)
				psKeyHalfway = psKey->psPrev;
		iKeyLen = psKeydesc->iKeyLength;
		if (psKeydesc->iFlags & TCOMPRESS)
		{
			iLastTC = iCountTC;
			iCountTC = 0;
			pcKeyEndPtr = psKey->cKey + iKeyLen - 1;
			while (*pcKeyEndPtr-- == TCC && pcKeyEndPtr != psKey->cKey)
				iCountTC++;
#if	_FILE_OFFSET_BITS == 64
			iKeyLen += INTSIZE - iCountTC;
#else	// _FILE_OFFSET_BITS == 64
			iKeyLen += 1 - iCountTC;
#endif	// _FILE_OFFSET_BITS == 64
		}
		if (psKeydesc->iFlags & LCOMPRESS)
		{
			iCountLC = 0;
			if (psKey != psTree->psKeyFirst)
			{
				iMaxTC = psKeydesc->iKeyLength - (iCountTC > iLastTC ? iCountTC : iLastTC);
				for (; psKey->cKey [iCountLC] == pcPrevKey [iCountLC] && iCountLC < iMaxTC; iCountLC++)
						;
			}
#if	_FILE_OFFSET_BITS == 64
			iKeyLen += INTSIZE - iCountLC;
#else	// _FILE_OFFSET_BITS == 64
			iKeyLen += 1 - iCountLC;
#endif	// _FILE_OFFSET_BITS == 64
			if (psKey->sFlags.iIsHigh && psKeydesc->iFlags && LCOMPRESS)
			{
				iCountLC = psKeydesc->iKeyLength;
				iCountTC = 0;
#if	_FILE_OFFSET_BITS == 64
				iKeyLen = INTSIZE * 2;
#else	// _FILE_OFFSET_BITS == 64
				iKeyLen = 2;
#endif	// _FILE_OFFSET_BITS == 64
				if (psKeydesc->iFlags & DCOMPRESS)
					iKeyLen = 0;
			}
		}
		if (psKeydesc->iFlags & ISDUPS)
		{
			iKeyLen += QUADSIZE;
			// If the key is a duplicate and it's not first in node
			if ((psKey->sFlags.iIsHigh) || (psKey != psTree->psKeyFirst && !memcmp (psKey->cKey, pcPrevKey, psKeydesc->iKeyLength)))
				if (psKeydesc->iFlags & DCOMPRESS)
					iKeyLen = QUADSIZE;
		}
		iKeyLen += QUADSIZE;
		// Split?
		if (pcNodePtr + iKeyLen >= pcNodeEnd - 1)
		{
			if (psTree->psKeyLast->psPrev->sFlags.iIsNew)
				psKeyHalfway = psTree->psKeyLast->psPrev->psPrev;
			if (psTree->psKeyLast->psPrev->sFlags.iIsHigh && psTree->psKeyLast->psPrev->psPrev->sFlags.iIsNew)
				psKeyHalfway = psTree->psKeyLast->psPrev->psPrev->psPrev;
			iResult = iNodeSplit (iHandle, iKeyNumber, psTree, psKeyHalfway);
			return (iResult);
		}
		if (((iKeyLen == (QUADSIZE * 2)) && ((psKeydesc->iFlags & (DCOMPRESS | ISDUPS)) == (DCOMPRESS | ISDUPS))) || (psKeydesc->iFlags & DCOMPRESS && !(psKeydesc->iFlags & ISDUPS) && iKeyLen == QUADSIZE))
			*(pcNodePtr - QUADSIZE) |= 0x80;
		else
		{
			if (psKeydesc->iFlags & LCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				stint (iCountLC, pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				*pcNodePtr++ = iCountLC;
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (psKeydesc->iFlags & TCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				stint (iCountTC, pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				*pcNodePtr++ = iCountTC;
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (iCountLC != psKeydesc->iKeyLength)
			{
				pcPrevKey = psKey->cKey + iCountLC;
				iMaxTC = psKeydesc->iKeyLength - (iCountLC + iCountTC);
				while (iMaxTC--)
					*pcNodePtr++ = *pcPrevKey++;
			}
			pcPrevKey = psKey->cKey;
		}
		if (psKeydesc->iFlags & ISDUPS)
		{
#if	_FILE_OFFSET_BITS == 64
			*pcNodePtr++ = (psKey->tDupNumber >> 56) & 0xff;
			*pcNodePtr++ = (psKey->tDupNumber >> 48) & 0xff;
			*pcNodePtr++ = (psKey->tDupNumber >> 40) & 0xff;
			*pcNodePtr++ = (psKey->tDupNumber >> 32) & 0xff;
#endif	// _FILE_OFFSET_BITS == 64
			*pcNodePtr++ = (psKey->tDupNumber >> 24) & 0xff;
			*pcNodePtr++ = (psKey->tDupNumber >> 16) & 0xff;
			*pcNodePtr++ = (psKey->tDupNumber >> 8) & 0xff;
			*pcNodePtr++ = psKey->tDupNumber & 0xff;
		}
#if	_FILE_OFFSET_BITS == 64
		*pcNodePtr++ = (psKey->tRowNode >> 56) & 0xff;
		*pcNodePtr++ = (psKey->tRowNode >> 48) & 0xff;
		*pcNodePtr++ = (psKey->tRowNode >> 40) & 0xff;
		*pcNodePtr++ = (psKey->tRowNode >> 32) & 0xff;
#endif	// _FILE_OFFSET_BITS == 64
		*pcNodePtr++ = (psKey->tRowNode >> 24) & 0xff;
		*pcNodePtr++ = (psKey->tRowNode >> 16) & 0xff;
		*pcNodePtr++ = (psKey->tRowNode >> 8) & 0xff;
		*pcNodePtr++ = psKey->tRowNode & 0xff;
	}
	if (psKey && psKey->sFlags.iIsDummy)
	{
		psTree->psKeyList [psTree->sFlags.iKeysInNode] = psKey;
		psTree->sFlags.iKeysInNode++;
	}
	stint (pcNodePtr - cVBNode [0], cVBNode [0]);
	iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cVBNode [0]);
	if (iResult)
		return (iResult);
	return (0);
}

/*
 * Name:
 *	static	int	iNodeSplit (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBKEY *psKeyHalfway);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	struct	VBTREE	*psTree
 *		The VBTREE structure containing data to split with this call
 *	struct	VBKEY	*psKeyHalfway
 *		A pointer within the psTree defining WHERE to split
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	int
iNodeSplit (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBKEY *psKeyHalfway)
{
	int	iResult;
	struct	VBKEY
		*psKey,
		*psKeyTemp,
		*psHoldKeyCurr,
		*psNewKey,
		*psRootKey [3];
	struct	VBTREE
		*psNewTree,
		*psRootTree = VBTREE_NULL;
	off_t	tNewNode1,
		tNewNode2 = 0;

	psNewTree = psVBTreeAllocate (iHandle);
	if (!psNewTree)
		return (errno);
	psNewTree->psParent = psTree;
	psNewKey = psVBKeyAllocate (iHandle, iKeyNumber);
	if (!psNewKey)
		return (errno);
	if (psTree->sFlags.iIsRoot)
	{
		psRootTree = psVBTreeAllocate (iHandle);
		if (!psRootTree)
			return (errno);
		psRootKey [0] = psVBKeyAllocate (iHandle, iKeyNumber);
		if (!psRootKey [0])
			return (errno);
		psRootKey [1] = psVBKeyAllocate (iHandle, iKeyNumber);
		if (!psRootKey [1])
			return (errno);
		psRootKey [2] = psVBKeyAllocate (iHandle, iKeyNumber);
		if (!psRootKey [2])
			return (errno);
		tNewNode2 = tVBNodeAllocate (iHandle);
		if (tNewNode2 == -1)
			return (iserrno);
	}
	tNewNode1 = tVBNodeAllocate (iHandle);
	if (tNewNode1 == -1)
		return (iserrno);

	if (psTree->sFlags.iIsRoot)
	{
		psNewTree->psKeyLast = psTree->psKeyLast;
		psKey = psKeyHalfway->psNext;
		psNewKey->psPrev = psKey->psPrev;
		psNewKey->psParent = psKey->psParent;
		psNewKey->sFlags.iIsDummy = 1;
		psKey->psPrev->psNext = psNewKey;
		psKey->psPrev = VBKEY_NULL;
		psTree->psKeyLast = psNewKey;
		psTree->psKeyCurr = psTree->psKeyFirst;
		psNewTree->psKeyFirst = psNewTree->psKeyCurr = psKey;
		psNewTree->sFlags.iLevel = psTree->sFlags.iLevel;
		psNewTree->psParent = psTree->psParent;
		psNewTree->sFlags.iIsEOF = psTree->sFlags.iIsEOF;
		psTree->sFlags.iIsEOF = 0;
		for (psKeyTemp = psKey; psKeyTemp; psKeyTemp = psKeyTemp->psNext)
			psKeyTemp->psParent = psNewTree;
		return (iNewRoot (iHandle, iKeyNumber, psTree, psNewTree, psRootTree, psRootKey, tNewNode1, tNewNode2));
	}
	else
	{
		psNewTree->psKeyFirst = psNewTree->psKeyCurr = psTree->psKeyFirst;
		psNewTree->psKeyLast = psNewKey;
		psTree->psKeyFirst = psTree->psKeyCurr = psKeyHalfway->psNext;
		psKeyHalfway->psNext->psPrev = VBKEY_NULL;
		psKeyHalfway->psNext = psNewKey;
		psNewKey->psPrev = psKeyHalfway;
		psNewKey->psNext = VBKEY_NULL;

		psNewKey->sFlags.iIsDummy = 1;
		for (psKey = psNewTree->psKeyFirst; psKey; psKey = psKey->psNext)
		{
			psKey->psParent = psNewTree;
			if (psKey->psChild)
				psKey->psChild->psParent = psNewTree;
		}
		psNewTree->sFlags.iLevel = psTree->sFlags.iLevel;
		psNewTree->psParent = psTree->psParent;
		psNewTree->sFlags.iIsTOF = psTree->sFlags.iIsTOF;
		psTree->sFlags.iIsTOF = 0;
		/*
		 * psNewTree is the new LEAF node but is stored in the OLD node
		 * psTree is the original node and contains the HIGH half
		 */
		psNewTree->tNodeNumber = tNewNode1;
		iResult = iVBNodeSave (iHandle, iKeyNumber, psNewTree, psNewTree->tNodeNumber);
		if (iResult)
			return (iResult);
		iResult = iVBNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);
		if (iResult)
			return (iResult);
		psHoldKeyCurr = psVBFile [iHandle]->psKeyCurr [iKeyNumber];
		psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psNewKey->psParent->psParent->psKeyCurr;
		iResult = iVBKeyInsert (iHandle, psKeyHalfway->psParent->psParent, iKeyNumber, psKeyHalfway->cKey, tNewNode1, psKeyHalfway->tDupNumber, psNewTree);
		psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psHoldKeyCurr;
		if (iResult)
			return (iResult);
		psHoldKeyCurr->psParent->psKeyCurr = psHoldKeyCurr;
	}
	return (0);
}

/*
 * Name:
 *	static	int	iNewRoot (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBTREE *psNewTree, struct VBTREE *psRootTree, struct VBKEY *psRootKey[], off_t tNewNode1, off_t tNewNode2)
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	struct	VBTREE	*psTree
 *		The VBTREE structure containing data to be used with this call
 *	struct	VBTREE	*psNewTree
 *		The VBTREE structure that will contain some of the data
 *	struct	VBKEY	*psRootKey []
 *		An array of three (3) keys to be filled in for the new root
 *	off_t	tNewNode1
 *		The receiving node number for some of the old root data
 *	off_t	tNewNode2
 *		The receiving node number for the rest of the old root data
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	int
iNewRoot (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBTREE *psNewTree, struct VBTREE *psRootTree, struct VBKEY *psRootKey[], off_t tNewNode1, off_t tNewNode2)
{
	int	iResult;
	struct	VBKEY
		*psKey;

	// Fill in the content for the new root node
	psRootKey [0]->psNext = psRootKey [1];
	psRootKey [1]->psNext = psRootKey [2];
	psRootKey [2]->psPrev = psRootKey [1];
	psRootKey [1]->psPrev = psRootKey [0];
	psRootKey [0]->psParent = psRootKey [1]->psParent = psRootKey [2]->psParent = psRootTree;
	psRootKey [0]->psChild = psTree;
	psRootKey [1]->psChild = psNewTree;
	psRootKey [0]->tRowNode = tNewNode2;
	psRootKey [1]->tRowNode = tNewNode1;
	psRootKey [1]->sFlags.iIsHigh = 1;
	psRootKey [2]->sFlags.iIsDummy = 1;
	memcpy (psRootKey [0]->cKey, psTree->psKeyLast->psPrev->cKey, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength);
	psRootKey [0]->tDupNumber = psTree->psKeyLast->psPrev->tDupNumber;
	vVBKeyValueSet (1, psVBFile [iHandle]->psKeydesc [iKeyNumber], psRootKey [1]->cKey);
	/*
	 * psRootTree is the new ROOT node
	 * psNewTree is the new LEAF node
	 * psTree is the original node (saved in a new place)
	 */
	psRootTree->psKeyFirst = psRootKey [0];
	psRootTree->psKeyCurr = psRootKey [0];
	psRootTree->psKeyLast = psRootKey [2];
	psRootTree->tNodeNumber = psTree->tNodeNumber;
	psRootTree->sFlags.iLevel = psTree->sFlags.iLevel + 1;
	psRootTree->sFlags.iIsRoot = 1;
	psRootTree->sFlags.iIsTOF = 1;
	psRootTree->sFlags.iIsEOF = 1;
	psTree->psParent = psRootTree;
	psTree->tNodeNumber = tNewNode2;
	psTree->sFlags.iIsRoot = 0;
	psTree->sFlags.iIsEOF = 0;
	psNewTree->psParent = psRootTree;
	psNewTree->tNodeNumber = tNewNode1;
	psNewTree->sFlags.iLevel = psTree->sFlags.iLevel;
	psNewTree->sFlags.iIsEOF = 1;
	psNewTree->psKeyCurr = psNewTree->psKeyFirst;
	psVBFile [iHandle]-> psTree [iKeyNumber] = psRootTree;
	for (psKey = psTree->psKeyFirst; psKey; psKey = psKey->psNext)
		if (psKey->psChild)
			psKey->psChild->psParent = psTree;
	for (psKey = psNewTree->psKeyFirst; psKey; psKey = psKey->psNext)
		if (psKey->psChild)
			psKey->psChild->psParent = psNewTree;
	iResult = iVBNodeSave (iHandle, iKeyNumber, psNewTree, psNewTree->tNodeNumber);
	if (iResult)
		return (iResult);
	iResult = iVBNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);
	if (iResult)
		return (iResult);
	return (iVBNodeSave (iHandle, iKeyNumber, psRootTree, psRootTree->tNodeNumber));
}
