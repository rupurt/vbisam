/*
 * Title:	isread.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the reading from a file in the
 *	VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: isread.c,v $
 *	Revision 1.1  2003/12/20 20:11:22  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	isread (int, char *, int);
int	isstart (int, struct keydesc *, int, char *, int);
static	int	iStartRowNumber (int, int, int);

/*
 * Name:
 *	int	isread (int iHandle, char *pcRow, int iMode);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data to be read (see iMode below)
 *	int	iMode
 *		One of:
 *			ISCURR (Ignores pcRow)
 *			ISFIRST (Ignores pcRow)
 *			ISLAST (Ignores pcRow)
 *			ISNEXT (Ignores pcRow)
 *			ISPREV (Ignores pcRow)
 *			ISEQUAL
 *			ISGREAT
 *			ISGTEQ
 *		Plus 'OR' with any of:
 *			ISLOCK
 *			ISSKIPLOCK
 *			ISWAIT
 *			ISLCKW (ISLOCK + ISWAIT)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isread (int iHandle, char *pcRow, int iMode)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iDeleted = 0,
		iKeyNumber,
		iLockResult = 0,
		iReadMode,
		iReadResult = 0,
		iResult = -1;
	struct	VBKEY
		*psKey;

	iResult = iVBEnter (iHandle, FALSE);
	if (iResult)
		return (-1);

	iserrno = EBADKEY;
	iKeyNumber = psVBFile [iHandle]->iActiveKey;

	if (psVBFile [iHandle]->iOpenMode & ISAUTOLOCK)
		isrelease (iHandle);

	iReadMode = iMode & BYTEMASK;

	if (iKeyNumber == -1 || !psVBFile [iHandle]->psKeydesc [iKeyNumber]->iNParts)
	{
		/*
		 * This code relies on the fact that iStartRowNumber will
		 * populate the global VBISAM pcRowBuffer with the fixed-length
		 * portion of the row on success.
		 */
		iResult = iStartRowNumber (iHandle, iMode, TRUE);
		if (!iResult)
		{
			memcpy (pcRow, *(psVBFile [iHandle]->ppcRowBuffer), psVBFile [iHandle]->iMinRowLength);
			psVBFile [iHandle]->sFlags.iIsDisjoint = 0;
		}
		// BUG - VARLEN additions?
		goto ReadExit;
	}
	iserrno = 0;
	isrecnum = 0;
	switch (iReadMode)
	{
	case	ISFIRST:	// cKeyValue is just a placeholder for 1st/last
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)
			break;
		iResult = 0;
		break;

	case	ISLAST:		// cKeyValue is just a placeholder for 1st/last
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0 || iResult > 2)
		{
			iResult = -1;
			break;
		}
		iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISPREV, TRUE, &psKey);
		if (iserrno)
			iResult = -1;
		break;

	case	ISEQUAL:
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult == -1)		// Error
			break;
		// Map EQUAL onto OK and LESS THAN onto OK if the basekey is ==
		if (iResult == 1)
			iResult = 0;
		else
			if (iResult == 0 && memcmp (cKeyValue, psVBFile [iHandle]->psKeyCurr [psVBFile [iHandle]->iActiveKey]->cKey, psVBFile [iHandle]->psKeydesc [psVBFile [iHandle]->iActiveKey]->iKeyLength))
				iResult = 1;
		if (iResult == -1)
			iserrno = EBADFILE;
		break;

	case	ISGREAT:
	case	ISGTEQ:
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)		// Error is always error
			break;
		if (iResult < 2)
		{
			iResult = 0;
			break;
		}
		iserrno = EENDFILE;
		iResult = 1;
		break;

	case	ISPREV:
		if (psVBFile [iHandle]->tRowStart)
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowStart);
		else
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
		if (iResult)
			iserrno = EENDFILE;
		else
		{
			iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISPREV, TRUE, &psKey);
			if (iserrno)
				iResult = -1;
		}
		break;

	case	ISNEXT:			// Might fall thru to ISCURR
		if (!psVBFile [iHandle]->sFlags.iIsDisjoint)
		{
			if (psVBFile [iHandle]->tRowStart)
				iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowStart);
			else
				iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
			if (iResult)
				iserrno = EENDFILE;
			else
			{
				iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
				if (iserrno)
					iResult = -1;
			}
			break;		// Exit the switch case
		}
	case	ISCURR:
// BUG??
		if (psVBFile [iHandle]->tRowStart)
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowStart);
		else
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
		if (iResult)
			iserrno = EENDFILE;
		break;

	default:
		iserrno = EBADARG;
		iResult = -1;
	}
	if (!iResult)
		if (psVBFile [iHandle]->psKeyCurr [iKeyNumber]->sFlags.iIsDummy)
		{
			iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
			if (iserrno)
				iResult = -1;
		}
	if (!iResult)
	{
		psVBFile [iHandle]->tRowStart = 0;
		if (psVBFile [iHandle]->iOpenMode & ISAUTOLOCK || iMode & ISLOCK)
		{
			if (iVBDataLock (iHandle, iMode & ISWAIT ? VBWRLCKW : VBWRLOCK, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode, FALSE))
			{
				iserrno = iLockResult = ELOCKED;
				iResult = -1;
			}
		}
		if (!iLockResult)
			iReadResult = iVBDataRead (iHandle, pcRow, &iDeleted, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode, TRUE);
		if (!iReadResult && (!iLockResult || (iMode & ISSKIPLOCK && iserrno == ELOCKED)))
		{
			isrecnum = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode;
			psVBFile [iHandle]->tRowNumber = isrecnum;
			psVBFile [iHandle]->sFlags.iIsDisjoint = 0;
			iResult = 0;
		}
	}

// BUG - Application of new locks?

ReadExit:
	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	isstart (int iHandle, struct keydesc *psKeydesc, int iLength, char *pcRow, int iMode);
 * Arguments:
 *	int	iHandle
 *		The handle of the open VBISAM file
 *	struct	keydesc *psKeydesc
 *		The key description to change to
 *	int	iLength
 *		The length (possibly partial) of the key to use, 0 = ALL
 *	char	*pcRow
 *		A pre-filled buffer (in some cases) of where to start from
 *	int	iMode
 *		One of:
 *			ISFIRST (Ignores pcRow)
 *			ISLAST (Ignores pcRow)
 *			ISEQUAL
 *			ISGREAT
 *			ISGTEQ
 *		Optionally, 'OR' in:
 *			ISKEEPLOCK
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  iserrno contains the reason
 *	0	Success
 * Problems:
 *	NONE known
 */
int
isstart (int iHandle, struct keydesc *psKeydesc, int iLength, char *pcRow, int iMode)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iKeyNumber,
		iResult = -1;
	struct	VBKEY
		*psKey;

	iResult = iVBEnter (iHandle, FALSE);
	if (iResult)
		return (-1);

	iKeyNumber = iVBCheckKey (iHandle, psKeydesc, 2, 0, FALSE);
	iResult = -1;
	if (iKeyNumber == -1 && psKeydesc->iNParts)
		goto StartExit;
	psVBFile [iHandle]->iActiveKey = iKeyNumber;
	if (iMode & ISKEEPLOCK)
		iMode -= ISKEEPLOCK;
	else
		isrelease (iHandle);
	iMode &= ~ISKEEPLOCK;
	if (!psKeydesc->iNParts)
	{
		iResult = iStartRowNumber (iHandle, iMode, FALSE);
		if (iResult && iserrno == ENOREC && iMode <= ISLAST)
		{
			iResult = 0;
			iserrno = 0;
		}
		goto StartExit;
	}
	iserrno = 0;
	isrecnum = 0;
	switch (iMode)
	{
	case	ISFIRST:	// cKeyValue is just a placeholder for 1st/last
		psVBFile [iHandle]->sFlags.iIsDisjoint = 1;
		iResult = iVBKeySearch (iHandle, iMode & BYTEMASK, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)
			break;
		iResult = 0;
		break;

	case	ISLAST:		// cKeyValue is just a placeholder for 1st/last
		psVBFile [iHandle]->sFlags.iIsDisjoint = 0;
		iResult = iVBKeySearch (iHandle, iMode & BYTEMASK, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0 || iResult > 2)
		{
			iResult = -1;
			break;
		}
		iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISPREV, TRUE, &psKey);
		if (iserrno)
			iResult = -1;
		break;

	case	ISEQUAL:
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iMode & BYTEMASK, iKeyNumber, 0, cKeyValue, 0);
		iserrno = EBADFILE;
		if (iResult == -1)		// Error
			break;
		// Map EQUAL onto OK and LESS THAN onto OK if the basekey is ==
		if (iResult == 1)
			iResult = 0;
		else
			if (iResult == 0 && memcmp (cKeyValue, psVBFile [iHandle]->psKeyCurr [psVBFile [iHandle]->iActiveKey]->cKey, psVBFile [iHandle]->psKeydesc [psVBFile [iHandle]->iActiveKey]->iKeyLength))
			{
				iserrno = ENOREC;
				iResult = -1;
			}
		break;

	case	ISGREAT:
	case	ISGTEQ:
		psVBFile [iHandle]->sFlags.iIsDisjoint = 1;
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iMode & BYTEMASK, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)		// Error is always error
			break;
		if (iResult < 2)
		{
			iResult = 0;
			break;
		}
		iserrno = EENDFILE;
		iResult = 1;
		break;

	default:
		iserrno = EBADARG;
		iResult = -1;
	}
	if (!iResult)
	{
		iserrno = 0;
		isrecnum = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode;
		psVBFile [iHandle]->tRowStart = isrecnum;
	}
	else
	{
		psVBFile [iHandle]->tRowStart = isrecnum = 0;
		iResult = -1;
	}
StartExit:
	iVBExit (iHandle);
	return (iResult);
}

static	int
iStartRowNumber (int iHandle, int iMode, int iIsRead)
{
	int	iBias = 1,
		iDeleted = TRUE,
		iLockResult = 0,
		iResult = 0;

	switch (iMode & BYTEMASK)
	{
	case	ISFIRST:
		psVBFile [iHandle]->sFlags.iIsDisjoint = 1;
		isrecnum = 1;
		break;

	case	ISLAST:
		isrecnum = ldquad (psVBFile [iHandle]->sDictNode.cDataCount);
		iBias = -1;
		break;

	case	ISNEXT:		// Falls thru to next case!
		if (!iIsRead)
		{
			iserrno = EBADARG;
			return (-1);
		}
		isrecnum = psVBFile [iHandle]->tRowNumber;
		if (psVBFile [iHandle]->sFlags.iIsDisjoint)
		{
			iBias = 0;
			break;
		}
	case	ISGREAT:	// Falls thru to next case!
		isrecnum++;
	case	ISGTEQ:
		break;

	case	ISCURR:		// Falls thru to next case!
		if (!iIsRead)
		{
			iserrno = EBADARG;
			return (-1);
		}
	case	ISEQUAL:
		iBias = 0;
		break;

	case	ISPREV:
		if (!iIsRead)
		{
			iserrno = EBADARG;
			return (-1);
		}
		isrecnum = psVBFile [iHandle]->tRowNumber;
		isrecnum--;
		iBias = -1;
		break;

	default:
		iserrno = EBADARG;
		return (-1);
	}

	iserrno = ENOREC;
	while (iDeleted)
	{
		if (isrecnum > 0 && isrecnum <= ldquad (psVBFile [iHandle]->sDictNode.cDataCount))
		{
			if (psVBFile [iHandle]->iOpenMode & ISAUTOLOCK || iMode & ISLOCK)
			{
				if (iVBDataLock (iHandle, iMode & ISWAIT ? VBWRLCKW : VBWRLOCK, isrecnum, FALSE))
					iLockResult = ELOCKED;
			}
			if (!iLockResult)
				iResult = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, isrecnum, FALSE);
			if (iResult)
			{
				isrecnum = 0;
				iserrno = EBADFILE;
				return (-1);
			}
		}
		if (!iDeleted)
		{
			psVBFile [iHandle]->tRowNumber = isrecnum;
			iserrno = 0;
			return (0);
		}
		if (!iBias)
		{
			isrecnum = 0;
			return (-1);
		}
		isrecnum += iBias;
		if (isrecnum < 1 || isrecnum > ldquad (psVBFile [iHandle]->sDictNode.cDataCount))
		{
			isrecnum = 0;
			return (-1);
		}
	}
	return (0);
}
