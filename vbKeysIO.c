/*
 * Title:	vbKeysIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	25Nov2003
 * Description:
 *	This module handles ALL the key manipulation for the VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: vbKeysIO.c,v $
 *	Revision 1.1  2003/12/20 20:11:24  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	iVBCheckKey (int, struct keydesc *, int, int, int);
int	iVBRowInsert (int, char *, off_t);
int	iVBRowDelete (int, off_t);
int	iVBRowUpdate (int, char *, off_t);
void	vVBMakeKey (int, int, char *, char *);
int	iVBKeySearch (int, int, int, int, char *, off_t);
int	iVBKeyLocateRow (int, int, off_t);
int	iVBKeyLoad (int, int, int, int, struct VBKEY **);
int	iVBMakeKeysFromData (int, int);
int	iVBDelNodes (int, int, off_t);
static	int	iKeyCompare (int, int, int, char *, char *);
static	int	iTreeLoad (int, int, int, char *, off_t);
static	int	iNodeLoad (int, int, struct VBTREE *, off_t, int);
static	int	iNodeSave (int, int, struct VBTREE *, off_t);
static	int	iNodeSplit (int, int, struct VBTREE *, struct VBKEY *, int);
static	int	iNewRoot (int, int, struct VBTREE *, struct VBTREE *, struct VBTREE *, struct VBKEY *[], off_t, off_t, int);
static	int	iKeyInsert (int, struct VBTREE *, int, char *, off_t, off_t, struct VBTREE *);
static	int	iKeyDelete (int, int);
static	void	vLowValueKeySet (struct keydesc *, char *);
static	void	vHighValueKeySet (struct keydesc *, char *);
static	int	iPartCompare (int, int, unsigned char *, unsigned char *);
#ifdef	DEBUG
void	vDumpKey (struct VBKEY *, struct VBTREE *, int);
void	vDumpTree (struct VBTREE *, int);
int	iDumpTree (int);
#endif	// DEBUG

#define		TCC	(' ')		// Trailing Compression Character
/*
 * Local scope variables
 */
static	char	cNode0 [MAX_NODE_LENGTH];

/*
 * Name:
 *	int	iVBCheckKey (int iHandle, struct keydesc *psKey, int iMode, int iRowLength, int iIsBuild);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	struct	keydesc	*psKey
 *		The key description to be tested
 *	int	iMode
 *		0 - Test whether the key is 'valid' (Uses iRowLength)
 *		1 - Test whether the key is 'valid' and not already present
 *		2 - Test whether the key already exists
 *	int	iRowLength
 *		Only used when iMode = 0.  The minimum length of a row in bytes
 *	int	iIsBuild
 *		If non-zero, this allows isbuild to create a NULL primary key
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	If iMode == 0
 *		0	Success (Passed the test in iMode)
 *	If iMode != 0
 *		n	Where n is the key number that matched
 * Problems:
 *	NONE known
 */
int
iVBCheckKey (int iHandle, struct keydesc *psKey, int iMode, int iRowLength, int iIsBuild)
{
	int	iLoop,
		iPart,
		iType;
	struct	keydesc
		*psLocalKey;

	if (iMode)
		iRowLength = psVBFile [iHandle]->iMinRowLength;
	if (iMode < 2)
	{
	// Basic key validity test
		psKey->iKeyLength = 0;
		if (psKey->iFlags < 0 || psKey->iFlags > COMPRESS + ISDUPS)
			goto VBCheckKeyExit;
		if (psKey->iNParts >= NPARTS || psKey->iNParts < 0)
			goto VBCheckKeyExit;
		if (psKey->iNParts == 0 && !iIsBuild)
			goto VBCheckKeyExit;
		for (iPart = 0; iPart < psKey->iNParts; iPart++)
		{
		// Wierdly enough, a single keypart CAN span multiple instances
		// EG: Part number 1 might contain 4 long values
			psKey->iKeyLength += psKey->sPart [iPart].iLength;
			if (psKey->iKeyLength > VB_MAX_KEYLEN)
				goto VBCheckKeyExit;
			iType = psKey->sPart [iPart].iType & ~ ISDESC;
			switch (iType)
			{
			case	CHARTYPE:
				break;

			case	INTTYPE:
				if (psKey->sPart [iPart].iLength % INTSIZE)
					goto VBCheckKeyExit;
				break;

			case	LONGTYPE:
				if (psKey->sPart [iPart].iLength % LONGSIZE)
					goto VBCheckKeyExit;
				break;

			case	QUADTYPE:
				if (psKey->sPart [iPart].iLength % QUADSIZE)
					goto VBCheckKeyExit;
				break;

			case	FLOATTYPE:
				if (psKey->sPart [iPart].iLength % FLOATSIZE)
					goto VBCheckKeyExit;
				break;

			case	DOUBLETYPE:
				if (psKey->sPart [iPart].iLength % DOUBLESIZE)
					goto VBCheckKeyExit;
				break;

			default:
				goto VBCheckKeyExit;
			}
			if (psKey->sPart [iPart].iStart + psKey->sPart [iPart].iLength > iRowLength)
				goto VBCheckKeyExit;
			if (psKey->sPart [iPart].iStart < 0)
				goto VBCheckKeyExit;
		}
		if (!iMode)
			return (0);
	}

	// Check whether the key already exists
	for (iLoop = 0; iLoop < psVBFile [iHandle]->iNKeys; iLoop++)
	{
		psLocalKey = psVBFile [iHandle]->psKeydesc [iLoop];
		if (psLocalKey->iNParts != psKey->iNParts)
			continue;
		for (iPart = 0; iPart < psLocalKey->iNParts; iPart++)
		{
			if (psLocalKey->sPart [iPart].iStart != psKey->sPart [iPart].iStart)
				break;
			if (psLocalKey->sPart [iPart].iLength != psKey->sPart [iPart].iLength)
				break;
			if (psLocalKey->sPart [iPart].iType != psKey->sPart [iPart].iType)
				break;
		}
		if (iPart == psLocalKey->iNParts)
			break;		/* found */
	}
	if (iLoop == psVBFile [iHandle]->iNKeys)
	{
		if (iMode == 2)
			goto VBCheckKeyExit;
		return (iLoop);
	}
	if (iMode == 1)
		goto VBCheckKeyExit;
	return (iLoop);

VBCheckKeyExit:
	iserrno = EBADKEY;
	return (-1);
}

/*
 * Name:
 *	int	iVBRowInsert (int iHandle, char *pcRowBuffer, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	char	*pcRowBuffer
 *		A pointer to the buffer containing the row to be added
 *	off_t	tRowNumber
 *		The absolute row number of the row to be added
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
iVBRowInsert (int iHandle, char *pcRowBuffer, off_t tRowNumber)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iKeyNumber,
		iResult;
	struct	VBKEY
		*psKey;
	off_t	tDupNumber [MAXSUBS];

	/*
	 * Step 1:
	 *	Check each index for a potential ISNODUPS error (EDUPL)
	 *	Also, calculate the duplicate number as needed
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		vVBMakeKey (iHandle, iKeyNumber, pcRowBuffer, cKeyValue);
		iResult = iVBKeySearch (iHandle, ISGREAT, iKeyNumber, 0, cKeyValue, 0);
		tDupNumber [iKeyNumber] = 0;
		if (iResult >= 0 && !iVBKeyLoad (iHandle, iKeyNumber, ISPREV, FALSE, &psKey) && !memcmp (psKey->cKey, cKeyValue, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength))
		{
			iserrno = EDUPL;
			if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iFlags & ISDUPS)
				tDupNumber [iKeyNumber] = psKey->tDupNumber + 1;
			else
				return (-1);
		}
	}

	// Step 2: Perform the actual insertion into each index
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		vVBMakeKey (iHandle, iKeyNumber, pcRowBuffer, cKeyValue);
		iResult = iKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tRowNumber, tDupNumber [iKeyNumber], VBTREE_NULL);
		if (iResult)
		{
// BUG - do something SANE here
		// Eeek, an error occured.  Let's remove what we added
			//while (iKeyNumber >= 0)
			//{
				//iKeyDelete (iHandle, iKeyNumber);
				//iKeyNumber--;
			//}
			return (iResult);
		}
	}

	return (0);
}

/*
 * Name:
 *	int	iVBRowDelete (int iHandle, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	off_t	tRowNumber
 *		The row number to be deleted
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
iVBRowDelete (int iHandle, off_t tRowNumber)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iKeyNumber,
		iResult;
	off_t	tDupNumber [MAXSUBS];

	/*
	 * Step 1:
	 *	Check each index for existance of tRowNumber
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		iResult = iVBKeyLocateRow (iHandle, iKeyNumber, tRowNumber);
		iserrno = EBADFILE;
		if (iResult)
			return (-1);
		tDupNumber [iKeyNumber] = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tDupNumber;
	}

	/*
	 * Step 2:
	 *	Perform the actual deletion from each index
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		iResult = iKeyDelete (iHandle, iKeyNumber);
		if (iResult)
		{
		// Eeek, an error occured.  Let's put back what we removed!
			while (iKeyNumber >= 0)
			{
				vVBMakeKey (iHandle, iKeyNumber, *(psVBFile [iHandle]->ppcRowBuffer), cKeyValue);
				iKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tRowNumber, tDupNumber [iKeyNumber], VBTREE_NULL);
				iKeyNumber--;
			}
			iserrno = iResult;
			return (-1);
		}
	}

	return (0);
}

/*
 * Name:
 *	int	iVBRowUpdate (int iHandle, char *pcRowBuffer, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	char	*pcRowBuffer
 *		A pointer to the buffer containing the updated row
 *	off_t	tRowNumber
 *		The row number to be updated
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int
iVBRowUpdate (int iHandle, char *pcRowBuffer, off_t tRowNumber)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iKeyNumber,
		iResult;
	struct	VBKEY
		*psKey;
	off_t	tDupNumber = 0;

	/*
	 * Step 1:
	 *	For each index that's changing, confirm that the NEW value
	 *	doesn't conflict with an existing ISNODUPS flag.
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iFlags & ISDUPS)
			continue;
		vVBMakeKey (iHandle, iKeyNumber, pcRowBuffer, cKeyValue);
		iResult = iVBKeySearch (iHandle, ISGTEQ, iKeyNumber, 0, cKeyValue, 0);
		if (iResult != 0 || tRowNumber == psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode)
			continue;
		iserrno = EDUPL;
		return (-1);
	}

	/*
	 * Step 2:
	 *	Check each index for existance of tRowNumber
	 *	This 'preload' additionally helps determine which indexes change
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		iResult = iVBKeyLocateRow (iHandle, iKeyNumber, tRowNumber);
		iserrno = EBADFILE;
		if (iResult)
			return (-1);
	}

	/*
	 * Step 3:
	 *	Perform the actual deletion / insertion with each index
	 *	But *ONLY* for those indexes that have actually CHANGED!
	 */
	for (iKeyNumber = 0; iKeyNumber < psVBFile [iHandle]->iNKeys; iKeyNumber++)
	{
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
			continue;
		vVBMakeKey (iHandle, iKeyNumber, pcRowBuffer, cKeyValue);
		iResult = memcmp (cKeyValue, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->cKey, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength);
		if (iResult)
			iResult = iKeyDelete (iHandle, iKeyNumber);
		else
			continue;
		if (iResult)
		{
		// Eeek, an error occured.  Let's put back what we removed!
			while (iKeyNumber >= 0)
			{
// BUG - We need to do SOMETHING sane here? Dunno WHAT
				iKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tRowNumber, tDupNumber, VBTREE_NULL);
				iKeyNumber--;
				vVBMakeKey (iHandle, iKeyNumber, *(psVBFile [iHandle]->ppcRowBuffer), cKeyValue);
			}
			iserrno = iResult;
			return (-1);
		}
		tDupNumber = 0;
		if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iFlags & ISDUPS)
		{
			if (!iVBKeyLoad (iHandle, iKeyNumber, ISPREV, FALSE, &psKey) && !memcmp (psKey->cKey, cKeyValue, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength))
				tDupNumber = psKey->tDupNumber + 1;
		}
		iResult = iVBKeySearch (iHandle, ISGREAT, iKeyNumber, 0, cKeyValue, tDupNumber + 1);
		if (iResult >= 0)
			iResult = iKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tRowNumber, tDupNumber, VBTREE_NULL);
		if (iResult)
		{
		// Eeek, an error occured.  Let's remove what we added
			while (iKeyNumber >= 0)
			{
// BUG - This is WRONG, we should re-establish what we had before!
				//iKeyDelete (iHandle, iKeyNumber);
				iKeyNumber--;
			}
			return (iResult);
		}
	}

	return (0);
}

/*
 * Name:
 *	void	vVBMakeKey (int iHandle, int iKeyNumber, char *pcRowBuffer, char *pcKeyValue);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The absolute key number within the index file (0-n)
 *	char	*pcRowBuffer
 *		A pointer to the raw row data containing the key parts
 *	char	*pcKeyValue
 *		A pointer to the receiving area for the contiguous key
 * Prerequisites:
 *	Assumes pcKeyValue is *LARGE* enough to store the result
 * Returns:
 *	NOTHING
 * Problems:
 *	NONE known
 * Comments:
 *	Extracts the various parts from a pcRowBuffer to create a contiguous key
 *	in pcKeyValue
 */
void
vVBMakeKey (int iHandle, int iKeyNumber, char *pcRowBuffer, char *pcKeyValue)
{
	char	*pcSource;
	int	iPart;
	struct	keydesc
		*psKeydesc;

	// Wierdly enough, a single keypart *CAN* span multiple instances
	// EG: Part number 1 might contain 4 long values
	psKeydesc = psVBFile [iHandle]->psKeydesc [iKeyNumber];
	for (iPart = 0; iPart < psKeydesc->iNParts; iPart++)
	{
		pcSource = pcRowBuffer + psKeydesc->sPart [iPart].iStart;
		memcpy (pcKeyValue, pcSource, psKeydesc->sPart [iPart].iLength);
		pcKeyValue += psKeydesc->sPart [iPart].iLength;
	}
}

/*
 * Name:
 *	int	iVBKeySearch (int iHandle, int iMode, int iKeyNumber, int iLength, char *pcKeyValue, off_t tDupNumber)
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	struct	keydesc	*psKey
 *		The key description to be tested
 *	int	iMode
 *		ISFIRST
 *		ISLAST
 *		ISNEXT
 *		ISPREV
 *		ISCURR
 *		ISEQUAL
 *		ISGREAT
 *		ISGTEQ
 *	int	iKeyNumber
 *		The absolute key number within the index file (0-n)
 *	int	iLength
 *		The length of the key to compare (0 = FULL KEY!)
 *	char	*pcKeyValue
 *		The key value being searched for
 *	off_t	tDupNumber
 *		The duplicate number being searched for
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success, pcKeyValue < psVBFile[iHandle]->psKeyCurr[iKeyNumber]
 *	1	Success, pcKeyValue = psVBFile[iHandle]->psKeyCurr[iKeyNumber]
 *	2	Index file is EMPTY
 * Problems:
 *	NONE known
 */
int
iVBKeySearch (int iHandle, int iMode, int iKeyNumber, int iLength, char *pcKeyValue, off_t tDupNumber)
{
	char	cBuffer [QUADSIZE],
		cKeyValue [VB_MAX_KEYLEN];
	int	iResult;
	struct	VBKEY
		*psKey;
	struct	keydesc
		*psKeydesc;

	psKeydesc = psVBFile [iHandle]->psKeydesc [iKeyNumber];
	if (iLength == 0)
		iLength = psKeydesc->iKeyLength;
	switch (iMode)
	{
	case	ISFIRST:
		vLowValueKeySet (psKeydesc, cKeyValue);
		tDupNumber = -1;
		return (iTreeLoad (iHandle, iKeyNumber, iLength, cKeyValue, tDupNumber));

	case	ISLAST:
		vHighValueKeySet (psKeydesc, cKeyValue);
		memset (cBuffer, 0xff, QUADSIZE);
		cBuffer [0] = 0x7f;
		tDupNumber = ldquad (cBuffer);
		return (iTreeLoad (iHandle, iKeyNumber, iLength, cKeyValue, tDupNumber) - 1);

	case	ISNEXT:
		iResult = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
		iserrno = iResult;
		if (iResult == EENDFILE)
			iResult = 0;
		if (iResult)
			return (-1);
		return (1);		// "NEXT" can NEVER be an exact match!

	case	ISPREV:
		iResult = iVBKeyLoad (iHandle, iKeyNumber, ISPREV, TRUE, &psKey);
		iserrno = iResult;
		if (iResult == EENDFILE)
			iResult = 0;
		if (iResult)
			return (-1);
		return (1);		// "PREV" can NEVER be an exact match

	case	ISCURR:
		return (iTreeLoad (iHandle, iKeyNumber, iLength, pcKeyValue, tDupNumber) - 1);

	case	ISEQUAL:	// Falls thru to ISGTEQ
		tDupNumber = 0;
	case	ISGTEQ:
		return (iTreeLoad (iHandle, iKeyNumber, iLength, pcKeyValue, tDupNumber));

	case	ISGREAT:
		memset (cBuffer, 0xff, QUADSIZE);
		cBuffer [0] = 0x7f;
		tDupNumber = ldquad (cBuffer);
		return (iTreeLoad (iHandle, iKeyNumber, iLength, pcKeyValue, tDupNumber));

	default:
		fprintf (stderr, "HUGE ERROR! File %s, Line %d iMode %d\n", __FILE__, __LINE__, iMode);
		exit (1);
	}
}

/*
 * Name:
 *	static	int	iVBKeyLocateRow (int iHandle, int iKeyNumber, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The absolute key number within the index file (0-n)
 *	off_t	tRowNumber
 *		The row number to locate within the key
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success (Fills the psVBFile [iHandle]->psTree linked list)
 * Problems:
 *	NONE known
 * Comments:
 *	The purpose of this function is to populate the VBTREE linked list
 *	associated with iKeyNumber such that the index entry being pointed to
 *	matches the entry for the row number passed into this function.
 */
int
iVBKeyLocateRow (int iHandle, int iKeyNumber, off_t tRowNumber)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iResult;
	struct	VBKEY
		sKey,
		*psKey;
	struct	VBTREE
		*psTree;

	/*
	 * Step 1:
	 *	The easy way out...
	 *	If it is already the current index pointer *AND*
	 *	the index file has remained unchanged since then,
	 *	we don't need to do anything
	 */
	iResult = TRUE;
	psKey = psVBFile [iHandle]->psKeyCurr [iKeyNumber];
	if (psKey && psKey->tRowNode == tRowNumber)
	{
		psKey->psParent->psKeyCurr = psKey;
		// Position psKeyCurr all the way up to the root to point at us
		psTree = psKey->psParent;
		while (psTree->psParent)
		{
			for (psTree->psParent->psKeyCurr = psTree->psParent->psKeyFirst; psTree->psParent->psKeyCurr && psTree->psParent->psKeyCurr->psChild != psTree; psTree->psParent->psKeyCurr = psTree->psParent->psKeyCurr->psNext)
				;
			if (!psTree->psParent->psKeyCurr)
				iResult = FALSE;
			psTree = psTree->psParent;
		}
		if (iResult)	// Technically, if iResult == FALSE it's a BUG!
			return (0);
	}

	/*
	 * Step 2:
	 *	It's a valid and non-deleted row.  Therefore, let's make a
	 *	contiguous key from it to search by.
	 *	Find the damn key!
	 */
	vVBMakeKey (iHandle, iKeyNumber, *(psVBFile [iHandle]->ppcRowBuffer), cKeyValue);
	iResult = iVBKeySearch (iHandle, ISGTEQ, iKeyNumber, 0, cKeyValue, 0);
	if (iResult == 0)	// If we found <, get next!
	{
		iResult = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
		if (iResult == EENDFILE)
			iResult = 0;
		if (iResult)
		{
			iserrno = EBADFILE;
			return (-1);
		}
	}
	if (iResult)
		return (-1);
	psKey = &sKey;
	while (psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode != tRowNumber && !memcmp (cKeyValue, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->cKey, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength))
	{
		iResult = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
		if (iResult == EENDFILE)
			iResult = ENOREC;
		if (iResult)
			goto KeyLocateRowExit;
	}
	return (0);

KeyLocateRowExit:
	iserrno = iResult;
	return (-1);
}

/*
 * Name:
 *	int	iVBKeyLoad (int iHandle, int iKeyNumber, int iMode, int iSetCurr, struct VBKEY **ppsKey)
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	int	iMode
 *		ISPREV - Search previous
 *		ISNEXT - Search next
 *	int	iSetCurr
 *		0 - Leave psVBFile [iHandle]->psTree [iKeyNumber]->psKeyCurr
 *		Other - SET it!
 *		The actual uncompressed key to delete
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
int
iVBKeyLoad (int iHandle, int iKeyNumber, int iMode, int iSetCurr, struct VBKEY **ppsKey)
{
	int	iResult;
	struct	VBKEY
		*psKey,
		*psKeyHold;
	struct	VBTREE
		*psTree;

 	psKey = psVBFile [iHandle]->psKeyCurr [iKeyNumber];
	if (psKey->psParent->sFlags.iLevel)
		return (EBADFILE);
	switch (iMode)
	{
	case	ISPREV:
		if (psKey->psPrev)
		{
			*ppsKey = psKey->psPrev;
			if (iSetCurr)
			{
 				psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey->psPrev;
				psKey->psParent->psKeyCurr = psKey->psPrev;
			}
			return (0);
		}
		psTree = psKey->psParent;
		if (psTree->sFlags.iIsTOF)
			return (EENDFILE);
		// Back up the tree until we find a node where there is a <
		while (psTree->psKeyCurr == psTree->psKeyFirst)
		{
			if (psTree->psParent)
				psTree = psTree->psParent;
			else
				break;
		}
		// Back down the tree selecting the LAST valid key of each
		psKey = psTree->psKeyCurr->psPrev;
		if (iSetCurr)
			psTree->psKeyCurr = psTree->psKeyCurr->psPrev;
		while (psTree->sFlags.iLevel)
		{
			if (iSetCurr)
				psTree->psKeyCurr = psKey;
			if (!psKey->psChild || psVBFile [iHandle]->sFlags.iIndexChanged)
			{
				if (!psKey->psChild)
				{
					psKey->psChild = psVBTreeAllocate (iHandle);
					if (!psKey->psChild)
						return (errno);
					psKey->psChild->psParent = psKey->psParent;
					if (psKey->psParent->sFlags.iIsTOF && psKey == psKey->psParent->psKeyFirst)
						psKey->psChild->sFlags.iIsTOF = 1;
					if (psKey->psParent->sFlags.iIsEOF && psKey == psKey->psParent->psKeyLast->psPrev)
						psKey->psChild->sFlags.iIsEOF = 1;
				}
				psKeyHold = psKey;
				iResult = iNodeLoad (iHandle, iKeyNumber, psKey->psChild, psKey->tRowNode, psTree->sFlags.iLevel);
				if (iResult)
				{
				// Ooops, make sure the tree is not corrupt
					vVBTreeAllFree (iHandle, iKeyNumber, psKeyHold->psChild);
					psKeyHold->psChild = VBTREE_NULL;
					return (iResult);
				}
			}
			psTree = psKey->psChild;
			// Last key is always the dummy, so backup by one
			psKey = psTree->psKeyLast->psPrev;
		}
		if (iSetCurr)
		{
			psTree->psKeyCurr = psKey;
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey;
		}
		*ppsKey = psKey;
		break;

	case	ISNEXT:
		if (psKey->psNext && !psKey->psNext->sFlags.iIsDummy)
		{
			*ppsKey = psKey->psNext;
			if (iSetCurr)
			{
 				psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey->psNext;
				psKey->psParent->psKeyCurr = psKey->psNext;
			}
			return (0);
		}
		psTree = psKey->psParent;
		if (psTree->sFlags.iIsEOF)
			return (EENDFILE);
		psTree = psTree->psParent;
		// Back up the tree until we find a node where there is a >
		while (1)
		{
			if (psTree->psKeyLast->psPrev != psTree->psKeyCurr)
				break;
			psTree = psTree->psParent;
		}
		psKey = psTree->psKeyCurr->psNext;
		if (iSetCurr)
			psTree->psKeyCurr = psTree->psKeyCurr->psNext;
		// Back down the tree selecting the FIRST valid key of each
		while (psTree->sFlags.iLevel)
		{
			if (iSetCurr)
				psTree->psKeyCurr = psKey;
			if (!psKey->psChild || psVBFile [iHandle]->sFlags.iIndexChanged)
			{
				if (!psKey->psChild)
				{
					psKey->psChild = psVBTreeAllocate (iHandle);
					if (!psKey->psChild)
						return (errno);
					psKey->psChild->psParent = psKey->psParent;
					if (psKey->psParent->sFlags.iIsTOF && psKey == psKey->psParent->psKeyFirst)
						psKey->psChild->sFlags.iIsTOF = 1;
					if (psKey->psParent->sFlags.iIsEOF && psKey == psKey->psParent->psKeyLast->psPrev)
						psKey->psChild->sFlags.iIsEOF = 1;
				}
				psKeyHold = psKey;
				iResult = iNodeLoad (iHandle, iKeyNumber, psKey->psChild, psKey->tRowNode, psTree->sFlags.iLevel);
				if (iResult)
				{
				// Ooops, make sure the tree is not corrupt
					vVBTreeAllFree (iHandle, iKeyNumber, psKeyHold->psChild);
					psKeyHold->psChild = VBTREE_NULL;
					return (iResult);
				}
			}
			psTree = psKey->psChild;
			psKey = psTree->psKeyFirst;
		}
		if (iSetCurr)
		{
			psTree->psKeyCurr = psKey;
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey;
		}
		*ppsKey = psKey;
		break;

	default:
		fprintf (stderr, "HUGE ERROR! File %s, Line %d Mode %d\n", __FILE__, __LINE__, iMode);
		exit (1);
	}
	return (0);
}

/*
 * Name:
 *	int	iVBMakeKeysFromData (int iHandle, int iKeyNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The absolute key number within the index file (0-n)
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 * Comments:
 *	Reads in EVERY data row and creates a key entry for it
 */
int
iVBMakeKeysFromData (int iHandle, int iKeyNumber)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iDeleted,
		iResult;
	struct	VBKEY
		*psKey;
	off_t	tDupNumber,
		tLoop;

	// Don't have to insert if the key is a NULL key!
	if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts == 0)
		return (0);

	for (tLoop = 1; tLoop < ldquad (psVBFile [iHandle]->sDictNode.cDataCount); tLoop++)
	{	
		/*
		 * Step 1:
		 *	Read in the existing data row (Just the min rowlength)
		 */
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tLoop, FALSE);
		if (iserrno)
			return (-1);
		if (iDeleted)
			continue;
		/*
		 * Step 2:
		 *	Check the index for a potential ISNODUPS error (EDUPL)
		 *	Also, calculate the duplicate number as needed
		 */
		vVBMakeKey (iHandle, iKeyNumber, *(psVBFile [iHandle]->ppcRowBuffer), cKeyValue);
		iResult = iVBKeySearch (iHandle, ISGREAT, iKeyNumber, 0, cKeyValue, 0);
		tDupNumber = 0;
		if (iResult >= 0 && !iVBKeyLoad (iHandle, iKeyNumber, ISPREV, FALSE, &psKey) && !memcmp (psKey->cKey, cKeyValue, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength))
		{
			iserrno = EDUPL;
			if (psVBFile [iHandle]->psKeydesc [iKeyNumber]->iFlags & ISDUPS)
				tDupNumber = psKey->tDupNumber + 1;
			else
				return (-1);
		}

		/*
		 * Step 3:
		 * Perform the actual insertion into the index
		 */
		iResult = iKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tLoop, tDupNumber, VBTREE_NULL);
		if (iResult)
			return (iResult);
	}
	return (0);
}

/*
 * Name:
 *	int	iVBDelNodes (int iHandle, int iKeyNumber, off_t tRootNode);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The key number being deleted
 *	off_t	tRootNode
 *		The root node of the index being deleted
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 * Comments:
 *	ONLY used by isdelindex!
 */
int
iVBDelNodes (int iHandle, int iKeyNumber, off_t tRootNode)
{
	char	cLclNode [MAX_NODE_LENGTH],
		*pcSrcPtr;
	int	iDuplicate,
		iKeyLength,
		iCompLength = 0,
		iNodeUsed,
		iResult = 0;
	struct	keydesc
		*psKeydesc;

	psKeydesc = psVBFile [iHandle]->psKeydesc [iKeyNumber];
	//iResult = iVBNodeRead (iHandle, (void *) cLclNode, tRootNode);
	iResult = iVBBlockRead (iHandle, TRUE, tRootNode, cLclNode);
	if (iResult)
		return (iResult);
	// Recurse for non-leaf nodes
	if (*(cLclNode + psVBFile [iHandle]->iNodeSize - 2))
	{
		iNodeUsed = ldint (cLclNode);
#if	_FILE_OFFSET_BITS == 64
		pcSrcPtr = cLclNode + INTSIZE + QUADSIZE;
#else	// _FILE_OFFSET_BITS == 64
		pcSrcPtr = cLclNode + INTSIZE;
#endif	// _FILE_OFFSET_BITS == 64
		iDuplicate = FALSE;
		while (pcSrcPtr - cLclNode < iNodeUsed)
		{
			if (iDuplicate)
			{
				if (!(*(pcSrcPtr + QUADSIZE) & 0x80))
					iDuplicate = FALSE;
				*(pcSrcPtr + QUADSIZE) &= ~0x80;
				iResult = iVBDelNodes (iHandle, iKeyNumber, ldquad (pcSrcPtr + QUADSIZE));
				if (iResult)
					return (iResult);
				pcSrcPtr += (QUADSIZE * 2);
			}
			iKeyLength = psKeydesc->iKeyLength;
			if (psKeydesc->iFlags & LCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				iCompLength = ldint (pcSrcPtr);
				pcSrcPtr += INTSIZE;
				iKeyLength -= (iCompLength - 2);
#else	// _FILE_OFFSET_BITS == 64
//PROBABLY A BUG HERE!
				iCompLength = *(pcSrcPtr);
				pcSrcPtr++;
				iKeyLength -= (iCompLength - 1);
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (psKeydesc->iFlags & TCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				iCompLength = ldint (pcSrcPtr);
				pcSrcPtr += INTSIZE;
				iKeyLength -= (iCompLength - 2);
#else	// _FILE_OFFSET_BITS == 64
				iCompLength = *pcSrcPtr;
				pcSrcPtr++;
				iKeyLength -= (iCompLength - 1);
#endif	// _FILE_OFFSET_BITS == 64
			}
			pcSrcPtr += iKeyLength;
			if (psKeydesc->iFlags & ISDUPS)
			{
				pcSrcPtr += QUADSIZE;
				if (*pcSrcPtr & 0x80)
					iDuplicate = TRUE;
			}
			iResult = iVBDelNodes (iHandle, iKeyNumber, ldquad (pcSrcPtr));
			if (iResult)
				return (iResult);
			pcSrcPtr += QUADSIZE;
		}
	}
	iResult = iVBNodeFree (iHandle, tRootNode);
	return (iResult);
}

/*
 * Name:
 *	static	int	iKeyCompare (int iHandle, int iKeyNumber, int iLength, char *pcKey1, char *pcKey2);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The absolute key number within the index file
 *	int	iLength
 *		The length (in bytes) of the key to be compared.  If 0, uses ALL
 *	char	*pcKey1
 *		The first (contiguous) key value
 *	char	*pcKey2
 *		The second (contiguous) key value
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	pcKey1 is LESS THAN pcKey2
 *	0	pcKey1 is EQUAL TO pcKey2
 *	1	pcKey1 is GREATER THAN pcKey2
 * Problems:
 *	NONE known
 */
static	int
iKeyCompare (int iHandle, int iKeyNumber, int iLength, char *pcKey1, char *pcKey2)
{
	int	iPart,
		iLengthToCompare,
		iResult = 0;
	struct	keydesc
		*psKeydesc;

	psKeydesc = psVBFile [iHandle]->psKeydesc [iKeyNumber];
	if (iLength == 0)
		iLength = psKeydesc->iKeyLength;
	for (iPart = 0; iLength > 0 && iPart < psKeydesc->iNParts; iPart++)
	{
		if (iLength >= psKeydesc->sPart [iPart].iLength)
			iLengthToCompare = psKeydesc->sPart [iPart].iLength;
		else
			iLengthToCompare = iLength;
		iResult = iPartCompare (psKeydesc->sPart [iPart].iType, iLengthToCompare, (unsigned char *) pcKey1, (unsigned char *) pcKey2);
		if (iResult)
			return (iResult);
		pcKey1 += iLengthToCompare;
		pcKey2 += iLengthToCompare;
		iLength -= iLengthToCompare;
	}
	return (iResult);
}

/*
 * Name:
 *	static	int	iTreeLoad (int iHandle, int iKeyNumber, int iLength, char *pcKeyValue, off_t tDupNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The absolute key number within the index file (0-n)
 *	int	iLength
 *		The length of the key being compared
 *	char	*pcKeyValue
 *		The (uncompressed) key being searched for
 *	off_t	tDupNumber
 *		The duplicate number to search for
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success, pcKeyValue < psVBFile[iHandle]->psKeyCurr[iKeyNumber]
 *	1	Success, pcKeyValue = psVBFile[iHandle]->psKeyCurr[iKeyNumber]
 *	2	Index file is EMPTY
 * Problems:
 *	NONE known
 * Comments:
 *	The purpose of this function is to populate the VBTREE/VBKEY structures
 *	associated with iKeyNumber such that the index entry being pointed to
 *	matches the entry (>=) in the pcKeyValue passed into this function.
 */
static	int
iTreeLoad (int iHandle, int iKeyNumber, int iLength, char *pcKeyValue, off_t tDupNumber)
{
	int	iResult = 0;
	struct	VBKEY
		*psKey;
	struct	VBTREE
		*psTree;

	psTree = psVBFile [iHandle]->psTree [iKeyNumber];
	if (!psTree)
	{
		psTree = psVBTreeAllocate (iHandle);
		psVBFile [iHandle]->psTree [iKeyNumber] = psTree;
		iserrno = errno;
		if (!psTree)
			goto TreeLoadExit;
		psTree->sFlags.iIsRoot = 1;
		psTree->sFlags.iIsTOF = 1;
		psTree->sFlags.iIsEOF = 1;
		iserrno = iNodeLoad (iHandle, iKeyNumber, psTree, psVBFile [iHandle]->psKeydesc [iKeyNumber]->tRootNode, -1);
		if (iserrno)
		{
			vVBTreeAllFree (iHandle, iKeyNumber, psTree);
			psVBFile [iHandle]->psTree [iKeyNumber] = VBTREE_NULL;
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = VBKEY_NULL;
			goto TreeLoadExit;
		}
	}
	if (psVBFile [iHandle]->sFlags.iIndexChanged)
	{
		iserrno = iNodeLoad (iHandle, iKeyNumber, psTree, psVBFile [iHandle]->psKeydesc [iKeyNumber]->tRootNode, -1);
		if (iserrno)
		{
			vVBTreeAllFree (iHandle, iKeyNumber, psTree);
			psVBFile [iHandle]->psTree [iKeyNumber] = VBTREE_NULL;
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = VBKEY_NULL;
			goto TreeLoadExit;
		}
	}
	iserrno = EBADFILE;
	if (psTree->tNodeNumber != psVBFile [iHandle]->psKeydesc [iKeyNumber]->tRootNode)
		goto TreeLoadExit;
	psTree->sFlags.iIsRoot = 1;
	psTree->sFlags.iIsTOF = 1;
	psTree->sFlags.iIsEOF = 1;
	while (1)
	{
		for (psTree->psKeyCurr = psTree->psKeyFirst; psTree->psKeyCurr && psTree->psKeyCurr->psNext; psTree->psKeyCurr = psTree->psKeyCurr->psNext)
		{
			if (psTree->psKeyCurr->sFlags.iIsHigh)
				break;
			iResult = iKeyCompare (iHandle, iKeyNumber, iLength, pcKeyValue, psTree->psKeyCurr->cKey);
			if (iResult < 0)
				break;		// Exit the for loop
			if (iResult > 0)
				continue;
			if (tDupNumber < psTree->psKeyCurr->tDupNumber)
			{
				iResult = -1;
				break;		// Exit the for loop
			}
			if (tDupNumber == psTree->psKeyCurr->tDupNumber)
				break;		// Exit the for loop
		}
		if (!psTree->sFlags.iLevel)
			break;			// Exit the while loop
		if (!psTree->psKeyCurr)
			goto TreeLoadExit;
		if (!psTree->psKeyCurr->psChild || psVBFile [iHandle]->sFlags.iIndexChanged)
		{
			psKey = psTree->psKeyCurr;
			if (!psTree->psKeyCurr->psChild)
			{
				psKey->psChild = psVBTreeAllocate (iHandle);
				iserrno = errno;
				if (!psKey->psChild)
					goto TreeLoadExit;
				psKey->psChild->psParent = psKey->psParent;
				if (psKey->psParent->sFlags.iIsTOF && psKey == psKey->psParent->psKeyFirst)
					psKey->psChild->sFlags.iIsTOF = 1;
				if (psKey->psParent->sFlags.iIsEOF && psKey == psKey->psParent->psKeyLast->psPrev)
					psKey->psChild->sFlags.iIsEOF = 1;
			}
			iserrno = iNodeLoad (iHandle, iKeyNumber, psTree->psKeyCurr->psChild, psTree->psKeyCurr->tRowNode, psTree->sFlags.iLevel);
			if (iserrno)
			{
				vVBTreeAllFree (iHandle, iKeyNumber, psKey->psChild);
				psKey->psChild = VBTREE_NULL;
				goto TreeLoadExit;
			}
			psTree->psKeyCurr->psParent = psTree;
			psTree->psKeyCurr->psChild->psParent = psTree;
			if (psTree->sFlags.iIsTOF && psTree->psKeyCurr == psTree->psKeyFirst)
				psTree->psKeyCurr->psChild->sFlags.iIsTOF = 1;
			if (psTree->sFlags.iIsEOF && psTree->psKeyCurr == psTree->psKeyLast)
				psTree->psKeyCurr->psChild->sFlags.iIsEOF = 1;
		}
		psTree = psTree->psKeyCurr->psChild;
	}
	/*
	 * When we get here, iResult is set depending upon whether the located
	 * key was:
	 * -1	LESS than the desired value
	 * 0	EQUAL to the desired value (including a tDupNumber match!)
	 * 1	GREATER than the desired value
	 * By simply adding one to the value, we're cool for a NON-STANDARD
	 * comparison return value.
	 */
	psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psTree->psKeyCurr;
	if (!psTree->psKeyCurr)
	{
		iserrno = EBADFILE;
		return (-1);
	}
	iserrno = 0;
	if (psTree->psKeyCurr->sFlags.iIsDummy)
	{
		iserrno = EENDFILE;
		if (psTree->psKeyCurr->psPrev)
			return (0);	// EOF
		else
			return (2);	// Empty file!
	}
	return (iResult + 1);

TreeLoadExit:
	return (-1);
}

/*
 * Name:
 *	static	int	iNodeLoad (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber, int iPrevLvl);
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
static	int
iNodeLoad (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber, int iPrevLvl)
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
	vLowValueKeySet (psKeydesc, cPrevKey);
	vHighValueKeySet (psKeydesc, cHighKey);
	//iResult = iVBNodeRead (iHandle, (char *) cNode0, tNodeNumber);
	iResult = iVBBlockRead (iHandle, TRUE, tNodeNumber, cNode0);
	if (iResult)
		return (iResult);
	if (iPrevLvl != -1)
		if (*(cNode0 + psVBFile [iHandle]->iNodeSize - 2) != iPrevLvl - 1)
			return (EBADFILE);
	psTree->tNodeNumber = tNodeNumber;
	psTree->sFlags.iLevel = *(cNode0 + psVBFile [iHandle]->iNodeSize - 2);
	iNodeLen = ldint (cNode0);
#if	_FILE_OFFSET_BITS == 64
	pcNodePtr = cNode0 + INTSIZE + QUADSIZE;
	tTransNumber = ldquad (cNode0 + INTSIZE);
	if (tTransNumber == psTree->tTransNumber)
		return (0);
#else	// _FILE_OFFSET_BITS == 64
	pcNodePtr = cNode0 + INTSIZE;
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
	while (pcNodePtr - cNode0 < iNodeLen)
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
			memset (cPrevKey + psKeydesc->iKeyLength - iCountTC, TCC, psKeydesc->iKeyLength - iCountTC);
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
	return (0);
}

/*
 * Name:
 *	static	int	iNodeSave (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber);
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
static	int
iNodeSave (int iHandle, int iKeyNumber, struct VBTREE *psTree, off_t tNodeNumber)
{
	char	cPrevKey [VB_MAX_KEYLEN],
		*pcNodePtr;
	int	iCountLC = 0,		// Leading compression
		iCountTC = 0,		// Trailing compression
		iIsLo = 0,
		iKeyLen,
		iLoop,
		iNodeLen,
		iResult;
	struct	VBKEY
		*psKey,
		*psKeyHalfway = VBKEY_NULL;
	struct	keydesc
		*psKeydesc;

	psKeydesc = psVBFile [iHandle]->psKeydesc[iKeyNumber];
	vLowValueKeySet (psKeydesc, cPrevKey);
	memset (cNode0, 0, MAX_NODE_LENGTH);
#if	_FILE_OFFSET_BITS == 64
	iNodeLen = INTSIZE + QUADSIZE;
	stquad (ldquad (psVBFile [iHandle]->sDictNode.cTransNumber) + 1, cNode0 + INTSIZE);
#else	// _FILE_OFFSET_BITS == 64
	iNodeLen = INTSIZE;
#endif	// _FILE_OFFSET_BITS == 64
	pcNodePtr = cNode0 + iNodeLen;
	*(cNode0 + psVBFile [iHandle]->iNodeSize - 2) = psTree->sFlags.iLevel;
	for (psKey = psTree->psKeyFirst; psKey && psKey->sFlags.iIsDummy == 0; psKey = psKey->psNext)
	{
		if (psKey->sFlags.iIsNew)
			if (!psKeyHalfway)
				iIsLo = 1;
		if (!psKeyHalfway)
		{
			if (pcNodePtr - cNode0 > psVBFile [iHandle]->iNodeSize / 2)
				psKeyHalfway = psKey;
		}
		iKeyLen = psKeydesc->iKeyLength;
		if (psKeydesc->iFlags & TCOMPRESS)
		{
			iCountTC = 0;
			for (iLoop = psKeydesc->iKeyLength - 1; iLoop && psKey->cKey [iLoop] == TCC; iLoop--)
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
			for (iLoop = 0; iLoop < (psKeydesc->iKeyLength - iCountTC) && psKey->cKey [iLoop] == cPrevKey [iLoop]; iLoop++)
				iCountLC++;
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
			// If the key is a duplicate and it's not first in node
#if	_FILE_OFFSET_BITS == 64
			if ((psKey->sFlags.iIsHigh) || (!memcmp (psKey->cKey, cPrevKey, psKeydesc->iKeyLength) && pcNodePtr > cNode0 + INTSIZE + QUADSIZE))
#else	// _FILE_OFFSET_BITS == 64
			if ((psKey->sFlags.iIsHigh) || (!memcmp (psKey->cKey, cPrevKey, psKeydesc->iKeyLength) && pcNodePtr > cNode0 + INTSIZE))
#endif	// _FILE_OFFSET_BITS == 64
				iKeyLen = QUADSIZE;
			else
				iKeyLen += QUADSIZE;
		}
		iKeyLen += QUADSIZE;
		// Split?
		if (pcNodePtr - cNode0 + iKeyLen > psVBFile [iHandle]->iNodeSize - 2)
		{
			if (psTree->psKeyLast->psPrev->sFlags.iIsNew == 1)
			{
				psKeyHalfway = psTree->psKeyLast->psPrev;
				iIsLo = 0;
			}
			if (psTree->psKeyLast->psPrev->sFlags.iIsHigh == 1 && psTree->psKeyLast->psPrev->psPrev->sFlags.iIsNew == 1)
			{
				psKeyHalfway = psTree->psKeyLast->psPrev->psPrev;
				iIsLo = 0;
			}
			iResult = iNodeSplit (iHandle, iKeyNumber, psTree, psKeyHalfway, iIsLo);
			return (iResult);
		}
		if ((psKeydesc->iFlags & DCOMPRESS && psKeydesc->iFlags & ISDUPS && iKeyLen == (QUADSIZE * 2)) || (psKeydesc->iFlags & DCOMPRESS && !(psKeydesc->iFlags & ISDUPS) && iKeyLen == QUADSIZE))
			*(pcNodePtr - QUADSIZE) |= 0x80;
		else
		{
			if (psKeydesc->iFlags & LCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				stint (iCountLC, pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				*pcNodePtr = iCountLC;
				pcNodePtr++;
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (psKeydesc->iFlags & TCOMPRESS)
			{
#if	_FILE_OFFSET_BITS == 64
				stint (iCountTC, pcNodePtr);
				pcNodePtr += INTSIZE;
#else	// _FILE_OFFSET_BITS == 64
				*pcNodePtr = iCountTC;
				pcNodePtr++;
#endif	// _FILE_OFFSET_BITS == 64
			}
			if (iCountLC != psKeydesc->iKeyLength)
			{
				memcpy (pcNodePtr, psKey->cKey + iCountLC, psKeydesc->iKeyLength - (iCountLC + iCountTC));
				pcNodePtr += psKeydesc->iKeyLength - (iCountLC + iCountTC);
			}
			memcpy (cPrevKey, psKey->cKey, psKeydesc->iKeyLength);
		}
		if (psKeydesc->iFlags & ISDUPS)
		{
			stquad (psKey->tDupNumber, pcNodePtr);
			pcNodePtr += QUADSIZE;
		}
		stquad (psKey->tRowNode, pcNodePtr);
		pcNodePtr += QUADSIZE;
	}
	stint (pcNodePtr - cNode0, cNode0);
	//iResult = iVBNodeWrite (iHandle, (char *) cNode0, tNodeNumber);
	iResult = iVBBlockWrite (iHandle, TRUE, tNodeNumber, cNode0);
	if (iResult)
		return (iResult);
	for (psKey = psTree->psKeyFirst; psKey && psKey->sFlags.iIsDummy == 0; psKey = psKey->psNext)
		psKey->sFlags.iIsNew = 0;
	return (0);
}

/*
 * Name:
 *	static	int	iNodeSplit (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBKEY *psKeyHalfway, int iIsLo);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 *	struct	VBTREE	*psTree
 *		The VBTREE structure containing data to split with this call
 *	struct	VBKEY	*psKeyHalfway
 *		A pointer within the psTree defining WHERE to split
 *	int	iIsLo
 *		0	- Write the UPPER portion to a lower node number
 *		OTHER	- Write the LOWER portion to a lower node number
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	int
iNodeSplit (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBKEY *psKeyHalfway, int iIsLo)
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

	psNewTree->psKeyLast = psTree->psKeyLast;
	psKey = psKeyHalfway;
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
	psKey->sFlags.iIsNew = 0;
	if (psTree->sFlags.iIsRoot)
		return (iNewRoot (iHandle, iKeyNumber, psTree, psNewTree, psRootTree, psRootKey, tNewNode1, tNewNode2, iIsLo));
	else
	{
		/*
		 * psNewTree is the new LEAF node
		 * psTree is the original node
		 */
		psNewTree->tNodeNumber = tNewNode1;
		iResult = iNodeSave (iHandle, iKeyNumber, psNewTree, psNewTree->tNodeNumber);
		if (iResult)
			return (iResult);
		iResult = iNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);
		if (iResult)
			return (iResult);
		psKey = psTree->psKeyLast->psPrev;
		if (psKey->sFlags.iIsHigh)
			psKey = psKey->psPrev;
		psHoldKeyCurr = psVBFile [iHandle]->psKeyCurr [iKeyNumber];
		psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey->psParent->psParent->psKeyCurr;
		tNewNode2 = psKey->psParent->psParent->psKeyCurr->tRowNode;
		psKey->psParent->psParent->psKeyCurr->tRowNode = tNewNode1;
		psKey->psParent->psParent->psKeyCurr->psChild = psNewTree;
		for (psKeyTemp = psTree->psKeyFirst; psKeyTemp; psKeyTemp = psKeyTemp->psNext)
			if (psKeyTemp->psChild)
				psKeyTemp->psChild->psParent = psTree;
		for (psKeyTemp = psNewTree->psKeyFirst; psKeyTemp; psKeyTemp = psKeyTemp->psNext)
			if (psKeyTemp->psChild)
				psKeyTemp->psChild->psParent = psNewTree;
		iResult = iKeyInsert (iHandle, psKey->psParent->psParent, iKeyNumber, psKey->cKey, tNewNode2, psKey->tDupNumber, psTree);
		psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psHoldKeyCurr;
		if (iResult)
			return (iResult);
	}
	return (0);
}

/*
 * Name:
 *	static	int	iNewRoot (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBTREE *psNewTree, struct VBTREE *psRootTree, struct VBKEY *psRootKey[], off_t tNewNode1, off_t tNewNode2, int iIsLo)
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
 *	int	iIsLo
 *		0	- Write the UPPER portion to a lower node number
 *		OTHER	- Write the LOWER portion to a lower node number
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	int
iNewRoot (int iHandle, int iKeyNumber, struct VBTREE *psTree, struct VBTREE *psNewTree, struct VBTREE *psRootTree, struct VBKEY *psRootKey[], off_t tNewNode1, off_t tNewNode2, int iIsLo)
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
	psRootKey [0]->tRowNode = iIsLo ? tNewNode2 : tNewNode1;
	psRootKey [1]->tRowNode = iIsLo ? tNewNode1 : tNewNode2;
	psRootKey [1]->sFlags.iIsHigh = 1;
	psRootKey [2]->sFlags.iIsDummy = 1;
	memcpy (psRootKey [0]->cKey, psTree->psKeyLast->psPrev->cKey, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength);
	psRootKey [0]->tDupNumber = psTree->psKeyLast->psPrev->tDupNumber;
	vHighValueKeySet (psVBFile [iHandle]->psKeydesc [iKeyNumber], psRootKey [1]->cKey);
	/*
	 * psRootTree is the new ROOT node
	 * psNewTree is the new LEAF node
	 * psTree is the original node (saved in a new place)
	 */
	psRootTree->psKeyFirst = psRootKey [0];
	psRootTree->psKeyCurr = iIsLo ? psRootKey [0] : psRootKey [1];
	psRootTree->psKeyLast = psRootKey [2];
	psRootTree->tNodeNumber = psTree->tNodeNumber;
	psRootTree->sFlags.iLevel = psTree->sFlags.iLevel + 1;
	psRootTree->sFlags.iIsRoot = 1;
	psRootTree->sFlags.iIsTOF = 1;
	psRootTree->sFlags.iIsEOF = 1;
	psTree->psParent = psRootTree;
	psTree->tNodeNumber = iIsLo ? tNewNode2 : tNewNode1;
	psTree->sFlags.iIsRoot = 0;
	psTree->sFlags.iIsEOF = 0;
	psNewTree->psParent = psRootTree;
	psNewTree->tNodeNumber = iIsLo ? tNewNode1 : tNewNode2;
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
	iResult = iNodeSave (iHandle, iKeyNumber, psNewTree, psNewTree->tNodeNumber);
	if (iResult)
		return (iResult);
	iResult = iNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);
	if (iResult)
		return (iResult);
	return (iNodeSave (iHandle, iKeyNumber, psRootTree, psRootTree->tNodeNumber));
}

/*
 * Name:
 *	static	int	iKeyInsert (int iHandle, struct VBTREE *psTree, int iKeyNumber, char *pcKeyValue, off_t tRowNode, off_t tDupNumber, struct VBTREE *psChild);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	struct	VBTREE	*psTree
 *		The VBTREE structure containing data to be used with this call
 *	int	iKeyNumber
 *		The index number in question
 *	char	*pcKeyValue
 *		The actual uncompressed key to insert
 *	off_t	tRowNode
 *		The row number (LEAF NODE) or node number to be inserted
 *	off_t	tDupNumber
 *		The duplicate number (0 = first)
 *	struct	VBTREE	*psChild
 *		The VBTREE structure that this new entry will point to
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 * Comments:
 *	Insert the key pcKeyValue into the index.
 *	Assumes that psVBFile [iHandle]->psTree [iKeyNumber] is setup
 *	Always inserts the key BEFORE psTree->psKeyCurr
 *	Sets psVBFile [iHandle]->psKeyCurr [iKeyNumber] to the newly added key
 */
static	int
iKeyInsert (int iHandle, struct VBTREE *psTree, int iKeyNumber, char *pcKeyValue, off_t tRowNode, off_t tDupNumber, struct VBTREE *psChild)
{
	int	iResult;
	struct	VBKEY
		*psKey;

	psKey = psVBKeyAllocate (iHandle, iKeyNumber);
	if (!psKey)
		return (errno);
	if (!psVBFile [iHandle]->psKeyCurr [iKeyNumber])
		return (EBADFILE);
	if (!psTree)
		psTree = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psParent; 
	psKey->psParent = psTree;
	psKey->psChild = psChild;
	psKey->tRowNode = tRowNode;
	psKey->tDupNumber = tDupNumber;
	psKey->sFlags.iIsNew = 1;
	memcpy (psKey->cKey, pcKeyValue, psVBFile [iHandle]->psKeydesc [iKeyNumber]->iKeyLength);
	psKey->psNext = psTree->psKeyCurr;
	psKey->psPrev = psTree->psKeyCurr->psPrev;
	if (psTree->psKeyCurr->psPrev)
		psTree->psKeyCurr->psPrev->psNext = psKey;
	else
		psTree->psKeyFirst = psKey;
	psTree->psKeyCurr->psPrev = psKey;
	psTree->psKeyCurr = psKey;
	psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psKey;
	iResult = iNodeSave (iHandle, iKeyNumber, psKey->psParent, psKey->psParent->tNodeNumber);
	return (iResult);
}

/*
 * Name:
 *	static	int	iKeyDelete (int iHandle, int iKeyNumber);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	int	iKeyNumber
 *		The index number in question
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	int
iKeyDelete (int iHandle, int iKeyNumber)
{
	int	iResult;
	struct	VBKEY
		*psKey,
		*psKeyHold;
	struct	VBTREE
		*psTree,
		*psTreeRoot;

	psKey = psVBFile [iHandle]->psKeyCurr [iKeyNumber];
	/*
	 * We're going to *TRY* to keep the index buffer populated!
	 * However, since it's technically feasible for the current node to be
	 * removed in it's entirety, we can only do this if there is at least 1
	 * other key in the node that's not the dummy entry.
	 * Since the current key is guaranteed to be at the LEAF node level (0),
	 * it's impossible to ever have an iIsHigh entry in the node.
	 */
	if (psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psPrev)
		psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psPrev;
	else
		if (psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psNext && psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psNext->sFlags.iIsDummy == 0)
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->psNext;
		else
			psVBFile [iHandle]->psKeyCurr [iKeyNumber] = VBKEY_NULL;
	while (1)
	{
		psTree = psKey->psParent;
		if (psKey->sFlags.iIsHigh)
		{
		// Handle removal of the high key in a node
			if (psKey->psPrev)
			{
				psKey->psChild = psKey->psPrev->psChild;
				psKey->tRowNode = psKey->psPrev->tRowNode;
				psKey->tDupNumber = psKey->psPrev->tDupNumber;
				psKey = psKey->psPrev;
			}
			else
			{
				iResult = iVBNodeFree (iHandle, psTree->tNodeNumber);	// BUG - didn't check iResult
				psTree = psTree->psParent;
				vVBTreeAllFree (iHandle, iKeyNumber, psTree->psKeyCurr->psChild);
				psKey = psTree->psKeyCurr;
				psKey->psChild = VBTREE_NULL;
				continue;
			}
		}
		if (psKey->psPrev)
			psKey->psPrev->psNext = psKey->psNext;
		else
			psTree->psKeyFirst = psKey->psNext;
		if (psKey->psNext)
			psKey->psNext->psPrev = psKey->psPrev;
		psKey->psParent = VBTREE_NULL;
		psKey->psChild = VBTREE_NULL;
		vVBKeyFree (iHandle, iKeyNumber, psKey);
		if (!psTree->sFlags.iIsRoot && psTree->psKeyFirst->sFlags.iIsDummy)
		{
		// Handle removal of the last key in a node
			iResult = iVBNodeFree (iHandle, psTree->tNodeNumber);	// BUG - didn't check iResult
			psTree = psTree->psParent;
			vVBTreeAllFree (iHandle, iKeyNumber, psTree->psKeyCurr->psChild);
			psTree->psKeyCurr->psChild = VBTREE_NULL;
			psKey = psTree->psKeyCurr;
			continue;
		}
		break;
	}
	while (psTree->psKeyFirst->sFlags.iIsHigh && psTree->sFlags.iIsRoot)
	{
		psKey = psTree->psKeyFirst;
		if (!psKey->psChild || psVBFile [iHandle]->sFlags.iIndexChanged)
		{
			if (!psKey->psChild)
			{
				psKey->psChild = psVBTreeAllocate (iHandle);
				if (!psKey->psChild)
					return (errno);
				psKey->psChild->psParent = psKey->psParent;
				if (psKey->psParent->sFlags.iIsTOF && psKey == psKey->psParent->psKeyFirst)
					psKey->psChild->sFlags.iIsTOF = 1;
				if (psKey->psParent->sFlags.iIsEOF && psKey == psKey->psParent->psKeyLast->psPrev)
					psKey->psChild->sFlags.iIsEOF = 1;
			}
			psKeyHold = psKey;
			iResult = iNodeLoad (iHandle, iKeyNumber, psKey->psChild, psKey->tRowNode, psTree->sFlags.iLevel);
			if (iResult)
			{
			// Ooops, make sure the tree is not corrupt
				vVBTreeAllFree (iHandle, iKeyNumber, psKeyHold->psChild);
				psKeyHold->psChild = VBTREE_NULL;
				return (iResult);
			}
		}
		// Time to collapse the root node (Yippee!)
		iResult = iVBNodeFree (iHandle, psTree->psKeyFirst->psChild->tNodeNumber);	// BUG - didn't check iResult
		psTreeRoot = psTree->psKeyFirst->psChild;
		psTreeRoot->tNodeNumber = psTree->tNodeNumber;
		psTreeRoot->psParent = VBTREE_NULL;
		psTreeRoot->sFlags.iIsTOF = 1;
		psTreeRoot->sFlags.iIsEOF = 1;
		psTreeRoot->sFlags.iIsRoot = 1;
		psVBFile [iHandle]->psTree [iKeyNumber] = psTreeRoot;
		psTree->psKeyFirst->psChild = VBTREE_NULL;
		vVBTreeAllFree (iHandle, iKeyNumber, psTree);
		psTree = psTreeRoot;
		if (psTree->sFlags.iLevel)
		{
			psTree->psKeyLast->psPrev->sFlags.iIsHigh = 1;
			vHighValueKeySet (psVBFile [iHandle]->psKeydesc [iKeyNumber], psTree->psKeyLast->psPrev->cKey);
		}
		iResult = iNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);	// Bug - didn't check result
	}
	iResult = iNodeSave (iHandle, iKeyNumber, psTree, psTree->tNodeNumber);	// BUG - didn't check iResult

	return (0);
}

/*
 * Name:
 *	static	void	vLowValueKeySet (struct keydesc *psKeydesc, char *pcKeyValue)
 * Arguments:
 *	struct	keydesc	*psKeydesc
 *		The keydesc structure to use
 *	char	*pcKeyValue
 *		The receiving uncompressed key
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	void
vLowValueKeySet (struct keydesc *psKeydesc, char *pcKeyValue)
{
	char	cBuffer [QUADSIZE];
	int	iPart,
		iRemainder;

	for (iPart = 0; iPart < psKeydesc->iNParts; iPart++)
	{
		switch (psKeydesc->sPart [iPart].iType & ~ISDESC)
		{
		case	CHARTYPE:
			memset (pcKeyValue, 0, psKeydesc->sPart [iPart].iLength);
			pcKeyValue += psKeydesc->sPart [iPart].iLength;
			break;

		case	INTTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stint (MINSHORT, pcKeyValue);
				pcKeyValue += INTSIZE;
				iRemainder -= INTSIZE;
			}
			break;

		case	LONGTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stlong (MINLONG, pcKeyValue);
				pcKeyValue += LONGSIZE;
				iRemainder -= LONGSIZE;
			}
			break;

		case	QUADTYPE:
			memset (cBuffer, 0, QUADSIZE);
			cBuffer [0] = 0x80;
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				memcpy (pcKeyValue, cBuffer, QUADSIZE);
				pcKeyValue += QUADSIZE;
				iRemainder -= QUADSIZE;
			}
			break;

		case	FLOATTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stfloat (MINFLOAT, pcKeyValue);
				pcKeyValue += FLOATSIZE;
				iRemainder -= FLOATSIZE;
			}
			break;

		case	DOUBLETYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stint (MINDOUBLE, pcKeyValue);
				pcKeyValue += DOUBLESIZE;
				iRemainder -= DOUBLESIZE;
			}
			break;

		default:
			fprintf (stderr, "HUGE ERROR! File %s, Line %d Type %d\n", __FILE__, __LINE__, psKeydesc->sPart [iPart].iType);
			exit (1);
		}
	}
}

/*
 * Name:
 *	static	void	vHighValueKeySet (struct keydesc *psKeydesc, char *pcKeyValue)
 * Arguments:
 *	struct	keydesc	*psKeydesc
 *		The keydesc structure to use
 *	char	*pcKeyValue
 *		The receiving uncompressed key
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	As per sub function calls
 * Problems:
 *	NONE known
 */
static	void
vHighValueKeySet (struct keydesc *psKeydesc, char *pcKeyValue)
{
	char	cBuffer [QUADSIZE];
	int	iPart,
		iRemainder;

	for (iPart = 0; iPart < psKeydesc->iNParts; iPart++)
	{
		switch (psKeydesc->sPart [iPart].iType & ~ISDESC)
		{
		case	CHARTYPE:
			memset (pcKeyValue, 0xff, psKeydesc->sPart [iPart].iLength);
			pcKeyValue += psKeydesc->sPart [iPart].iLength;
			break;

		case	INTTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stint (MAXSHORT, pcKeyValue);
				pcKeyValue += INTSIZE;
				iRemainder -= INTSIZE;
			}
			break;

		case	LONGTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stlong (MAXLONG, pcKeyValue);
				pcKeyValue += LONGSIZE;
				iRemainder -= LONGSIZE;
			}
			break;

		case	QUADTYPE:
			memset (cBuffer, 0xff, QUADSIZE);
			cBuffer [0] = 0x7f;
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				memcpy (pcKeyValue, cBuffer, QUADSIZE);
				pcKeyValue += QUADSIZE;
				iRemainder -= QUADSIZE;
			}
			break;

		case	FLOATTYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stfloat (MAXFLOAT, pcKeyValue);
				pcKeyValue += FLOATSIZE;
				iRemainder -= FLOATSIZE;
			}
			break;

		case	DOUBLETYPE:
			iRemainder = psKeydesc->sPart [iPart].iLength;
			while (iRemainder > 0)
			{
				stdbl (MAXDOUBLE, pcKeyValue);
				pcKeyValue += DOUBLESIZE;
				iRemainder -= DOUBLESIZE;
			}
			break;

		default:
			fprintf (stderr, "HUGE ERROR! File %s, Line %d Type %d\n", __FILE__, __LINE__, psKeydesc->sPart [iPart].iType);
			exit (1);
		}
	}
}

/*
 * Name:
 * static int	iPartCompare (int iType, int iLength, unsigned char *pcPart1, unsigned char *pcPart2);
 * Arguments:
 *	int	iType
 *		The type of the part from the keydesc (possibly with ISDESC)
 *	int	iLength
 *		The length of the part to compare
 *	char	*pcPart1
 *		The first part being compared
 *	char	*pcPart2
 *		The second part being compared
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	pcPart1 is LESS THAN pcPart2
 *	0	pcPart1 is EQUAL TO pcPart2
 *	1	pcPart1 is GREATER THAN pcPart2
 * Problems:
 *	NONE known
 * Comments:
 *	If iLength ends part way through a keypart, ignore that keypart
 */
static	int
iPartCompare (int iType, int iLength, unsigned char *pcPart1, unsigned char *pcPart2)
{
	int	iDescBias = 1,
		iLoop,
		iValue1,
		iValue2;
	long	lValue1,
		lValue2;
	float	fValue1,
		fValue2;
	double	dValue1,
		dValue2;
	off_t	tValue1,
		tValue2;

	if (iType & ISDESC)
		iDescBias = -1;
	switch (iType & ~ISDESC)
	{
	case	CHARTYPE:
		for (iLoop = 0; iLoop < iLength; iLoop++)
		{
			if (*pcPart1 < *pcPart2)
				return (-iDescBias);
			if (*pcPart1 > *pcPart2)
				return (iDescBias);
			pcPart1++;
			pcPart2++;
		}
		return (0);

	case	INTTYPE:
		while (iLength >= INTSIZE)
		{
			iValue1 = ldint (pcPart1);
			iValue2 = ldint (pcPart2);
			if (iValue1 < iValue2)
				return (-iDescBias);
			if (iValue1 > iValue2)
				return (iDescBias);
			pcPart1 += INTSIZE;
			pcPart2 += INTSIZE;
			iLength -= INTSIZE;
		}
		return (0);

	case	LONGTYPE:
		while (iLength >= LONGSIZE)
		{
			lValue1 = ldlong (pcPart1);
			lValue2 = ldlong (pcPart2);
			if (lValue1 < lValue2)
				return (-iDescBias);
			if (lValue1 > lValue2)
				return (iDescBias);
			pcPart1 += LONGSIZE;
			pcPart2 += LONGSIZE;
			iLength -= LONGSIZE;
		}
		return (0);

	case	QUADTYPE:
		while (iLength >= QUADSIZE)
		{
			tValue1 = ldquad (pcPart1);
			tValue2 = ldquad (pcPart2);
			if (tValue1 < tValue2)
				return (-iDescBias);
			if (tValue1 > tValue2)
				return (iDescBias);
			pcPart1 += QUADSIZE;
			pcPart2 += QUADSIZE;
			iLength -= QUADSIZE;
		}
		return (0);

	case	FLOATTYPE:
		while (iLength >= FLOATSIZE)
		{
			fValue1 = ldfloat (pcPart1);
			fValue2 = ldfloat (pcPart2);
			if (fValue1 < fValue2)
				return (-iDescBias);
			if (fValue1 > fValue2)
				return (iDescBias);
			pcPart1 += FLOATSIZE;
			pcPart2 += FLOATSIZE;
			iLength -= FLOATSIZE;
		}
		return (0);

	case	DOUBLETYPE:
		while (iLength >= DOUBLESIZE)
		{
			dValue1 = lddbl (pcPart1);
			dValue2 = lddbl (pcPart2);
			if (dValue1 < dValue2)
				return (-iDescBias);
			if (dValue1 > dValue2)
				return (iDescBias);
			pcPart1 += DOUBLESIZE;
			pcPart2 += DOUBLESIZE;
			iLength -= DOUBLESIZE;
		}
		return (0);

	default:
		fprintf (stderr, "HUGE ERROR! File %s, Line %d iType %d\n", __FILE__, __LINE__, iType);
		exit (1);
	}
}

#ifdef	DEBUG
void
vDumpKey (struct VBKEY *psKey, struct VBTREE *psTree, int iIndent)
{
	unsigned char
		cBuffer [INTSIZE];
	int	iKey;

	memcpy (cBuffer, psKey->cKey, 2);
	iKey = ldint (cBuffer);
#if _FILE_OFFSET_BITS == 64
	printf ("%-*.*s KEY :%02X%02llX\t[0x%04X] NXT:[0x%04X] PRV:[0x%04X] DAD:[0x%04X] KID:[0x%04X] ROW:%lld", iIndent, iIndent, "          ", cBuffer [0], psKey->tDupNumber, (int) psKey, (int) psKey->psNext, (int) psKey->psPrev, (int) psKey->psParent, (int) psKey->psChild, psKey->tRowNode);
#else	//_FILE_OFFSET_BITS == 64
	printf ("%-*.*s KEY :%02X%02lX\t[0x%04X] NXT:[0x%04X] PRV:[0x%04X] DAD:[0x%04X] KID:[0x%04X] ROW:%ld", iIndent, iIndent, "          ", cBuffer [0], psKey->tDupNumber, (int) psKey, (int) psKey->psNext, (int) psKey->psPrev, (int) psKey->psParent, (int) psKey->psChild, psKey->tRowNode);
#endif	// _FILE_OFFSET_BITS == 64
	fflush (stdout);
	if (psKey == psTree->psKeyFirst)
		printf (" FIRST");
	if (psKey == psTree->psKeyCurr)
		printf (" CURR");
	if (psKey == psTree->psKeyLast)
		printf (" LAST");
	if (psKey->sFlags.iIsNew)
		printf (" NEW");
	if (psKey->sFlags.iIsHigh)
		printf (" HIGH");
	if (psKey->sFlags.iIsDummy)
		printf (" DUMMY");
	printf ("\n");
	if (psKey->psChild)
		vDumpTree (psKey->psChild, iIndent + 1);
}

void
vDumpTree (struct VBTREE *psTree, int iIndent)
{
	struct	VBKEY
		*psKey;

#if	_FILE_OFFSET_BITS == 64
	printf ("%-*.*sNODE:%lld  \t[0x%04X] DAD:[0x%04X] LVL:[%02X] CUR:[0x%04X]", iIndent, iIndent, "          ", psTree->tNodeNumber, (int) psTree, (int) psTree->psParent, psTree->sFlags.iLevel, (int) psTree->psKeyCurr);
#else	//_FILE_OFFSET_BITS == 64
	printf ("%-*.*sNODE:%ld  \t[0x%04X] DAD:[0x%04X] LVL:[%02X] CUR:[0x%04X]", iIndent, iIndent, "          ", psTree->tNodeNumber, (int) psTree, (int) psTree->psParent, psTree->sFlags.iLevel, (int) psTree->psKeyCurr);
#endif	//_FILE_OFFSET_BITS == 64
	fflush (stdout);
	if (psTree->sFlags.iIsRoot)
		printf (" ROOT");
	if (psTree->sFlags.iIsTOF)
		printf (" TOF");
	if (psTree->sFlags.iIsEOF)
		printf (" EOF");
	printf ("\n");
	for (psKey = psTree->psKeyFirst; psKey; psKey = psKey->psNext)
		vDumpKey (psKey, psTree, iIndent);
}

int
iDumpTree (int iHandle)
{
	struct	VBTREE
		*psTree = psVBFile [iHandle]->psTree [0];

//	printf ("DICT->psKeyCurr=%d[0x%04X]\n", psVBFile [iHandle]->psKeyCurr [0]->cKey [1], (int) psVBFile [iHandle]->psKeyCurr [0]);
	fflush (stdout);
	vDumpTree (psTree, 0);
	return (0);
}
#endif	//DEBUG
