/*
 * Title:	isread.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the reading from a file in the
 *	VBISAM library.
 * Version:
 *	$Id: isread.c,v 1.5 2004/01/06 14:31:59 trev_vb Exp $
 * Modification History:
 *	$Log: isread.c,v $
 *	Revision 1.5  2004/01/06 14:31:59  trev_vb
 *	TvB 06Jan2004 Added in VARLEN processing (In a fairly unstable sorta way)
 *	
 *	Revision 1.4  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.3  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
 *	Revision 1.2  2003/12/22 04:47:23  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:22  trev_vb
 *	Init import
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
		goto ReadExit;
	}
	iserrno = 0;
	isrecnum = 0;
	switch (iReadMode)
	{
	// cKeyValue is just a placeholder for ISFIRST
	case	ISFIRST:
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)
			break;
		if (iResult == 2)
			iserrno = EENDFILE;
		else
			iResult = 0;
		break;

	/*
	 * cKeyValue is just a placeholder for ISLAST
	 * Note that the KeySearch (ISLAST) will position the pointer onto the
	 * LAST key of the LAST tree which, by definition, is a DUMMY key
	 */
	case	ISLAST:	
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)
			break;
		if (iResult == 2)
			iserrno = EENDFILE;
		else
		{
			iResult = 0;
			iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISPREV, TRUE, &psKey);
			if (iserrno)
				iResult = -1;
		}
		break;

	case	ISEQUAL:
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult == -1)		// Error
			break;
		if (iResult == 1)	// Found it!
			iResult = 0;
		else
			iserrno = ENOREC;
		break;

	case	ISGREAT:
	case	ISGTEQ:
		vVBMakeKey (iHandle, iKeyNumber, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, iReadMode, iKeyNumber, 0, cKeyValue, 0);
		if (iResult < 0)		// Error is always error
			break;
		if (iResult == 2)
		{
			iserrno = EENDFILE;
			break;
		}
		if (iResult == 1)
			iResult = 0;
		else
		{
			iResult = 0;
			if (psVBFile [iHandle]->psKeyCurr [iKeyNumber]->sFlags.iIsDummy)
			{
				iserrno = EENDFILE;
				iResult = -1;
			}
		}
		break;

	case	ISPREV:
		if (psVBFile [iHandle]->tRowStart)
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowStart);
		else
			if (psVBFile [iHandle]->tRowNumber)
				iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
			else
			{
				iserrno = ENOCURR;
				iResult = -1;
				break;
			}
		if (iResult)
			iserrno = ENOCURR;
		else
		{
			iResult = 0;
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
				if (psVBFile [iHandle]->tRowNumber)
					iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
				else
				{
					iserrno = ENOCURR;
					iResult = -1;
					break;
				}
			if (iResult)
				iserrno = EENDFILE;
			else
			{
				iResult = 0;
				iserrno = iVBKeyLoad (iHandle, iKeyNumber, ISNEXT, TRUE, &psKey);
				if (iserrno)
					iResult = -1;
			}
			break;		// Exit the switch case
		}
	case	ISCURR:
		if (psVBFile [iHandle]->tRowStart)
			iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowStart);
		else
			if (psVBFile [iHandle]->tRowNumber)
				iResult = iVBKeyLocateRow (iHandle, iKeyNumber, psVBFile [iHandle]->tRowNumber);
			else
			{
				iserrno = ENOCURR;
				iResult = -1;
				break;
			}
		if (iResult)
			iserrno = ENOCURR;
		break;

	default:
		iserrno = EBADARG;
		iResult = -1;
	}
	// By the time we get here, we're done with index positioning...
	// If iResult == 0 then we have a valid row to read in
	if (!iResult)
	{
		psVBFile [iHandle]->tRowStart = 0;
		if (iMode & ISLOCK || psVBFile [iHandle]->iOpenMode & ISAUTOLOCK)
			if (iVBDataLock (iHandle, iMode & ISWAIT ? VBWRLCKW : VBWRLOCK, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode, FALSE))
				iserrno = iLockResult = ELOCKED;
		if (!iLockResult)
			iResult = iVBDataRead (iHandle, pcRow, &iDeleted, psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode, TRUE);
		if (!iResult && (!iLockResult || (iMode & ISSKIPLOCK && iserrno == ELOCKED)))
		{
			isrecnum = psVBFile [iHandle]->psKeyCurr [iKeyNumber]->tRowNode;
			psVBFile [iHandle]->tRowNumber = isrecnum;
			psVBFile [iHandle]->sFlags.iIsDisjoint = 0;
		}
	}

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
				iResult = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, isrecnum, TRUE);
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
