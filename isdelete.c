/*
 * Title:	isdelete.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the deleting from a file in the
 *	VBISAM library.
 * Version:
 *	$Id: isdelete.c,v 1.6 2004/06/06 20:52:21 trev_vb Exp $
 * Modification History:
 *	$Log: isdelete.c,v $
 *	Revision 1.6  2004/06/06 20:52:21  trev_vb
 *	06Jun2004 TvB Lots of changes! Performance, stability, bugfixes.  See CHANGELOG
 *	
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
 *	Revision 1.2  2003/12/22 04:46:09  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:18  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	isdelete (int, char *);
int	isdelcurr (int);
int	isdelrec (int, off_t);
static	int	iRowDelete (int, off_t);

/*
 * Name:
 *	int	isdelete (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data row to be deleted
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isdelete (int iHandle, char *pcRow)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iDeleted,
		iResult = 0;
	off_t	tRowNumber;

	if (iVBEnter (iHandle, TRUE))
		return (-1);

	if (psVBFile [iHandle]->psKeydesc [0]->iFlags & ISDUPS)
	{
		iserrno = ENOPRIM;
		iResult = -1;
	}
	else
	{
		vVBMakeKey (iHandle, 0, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, ISEQUAL, 0, 0, cKeyValue, 0);
		switch (iResult)
		{
		case	1:	// Exact match
			iResult = 0;
			tRowNumber = psVBFile [iHandle]->psKeyCurr [0]->tRowNode;
			if (psVBFile [iHandle]->iOpenMode & ISTRANS)
			{
				iserrno = iVBDataLock (iHandle, VBWRLOCK, tRowNumber, TRUE);
				if (iserrno)
				{
					iResult = -1;
					goto ISDeleteExit;
				}
			}
			iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
			if (!iserrno && iDeleted)
				iserrno = ENOREC;
			if (iserrno)
				iResult = -1;
			if (!iResult)
				iResult = iRowDelete (iHandle, tRowNumber);
			if (!iResult)
			{
				iVBTransDelete (iHandle, tRowNumber, isreclen);	// BUG - retval
				memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
				iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, tRowNumber, TRUE);
				if (iserrno)
					iResult = -1;
			}
			if (!iResult)
			{
				if (!(psVBFile [iHandle]->iOpenMode & ISTRANS) || iVBInTrans == VBNOTRANS || iVBInTrans == VBCOMMIT || iVBInTrans == VBROLLBACK)
				{
					iserrno = iVBDataFree (iHandle, tRowNumber);
					if (iserrno)
						iResult = -1;
				}
			}
			if (!iResult)
			{
				isrecnum = tRowNumber;
				if (tRowNumber == psVBFile [iHandle]->tRowNumber)
					psVBFile [iHandle]->tRowNumber = 0;
			}
			break;

		case	0:		// LESS than
		case	2:		// GREATER than (EOF)
		case	3:		// EMPTY file
			iserrno = ENOREC;
			iResult = -1;
			break;

		default:
			iserrno = EBADFILE;
			iResult = -1;
			break;
		}
	}

ISDeleteExit:
	if (iResult == 0)
		psVBFile [iHandle]->sFlags.iIsDictLocked |= 0x02;
	iResult |= iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	isdelcurr (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isdelcurr (int iHandle)
{
	int	iDeleted,
		iResult = 0;

	if (iVBEnter (iHandle, TRUE))
		return (-1);

	if (psVBFile [iHandle]->tRowNumber > 0)
	{
		if (psVBFile [iHandle]->iOpenMode & ISTRANS)
		{
			iserrno = iVBDataLock (iHandle, VBWRLOCK, psVBFile [iHandle]->tRowNumber, TRUE);
			if (iserrno)
			{
				iResult = -1;
				goto ISDelcurrExit;
			}
		}
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, psVBFile [iHandle]->tRowNumber, TRUE);
		if (!iserrno && iDeleted)
			iserrno = ENOREC;
		if (iserrno)
			iResult = -1;
		if (!iResult)
			iResult = iRowDelete (iHandle, psVBFile [iHandle]->tRowNumber);
		if (!iResult)
		{
			iVBTransDelete (iHandle, psVBFile [iHandle]->tRowNumber, isreclen);	// BUG - retval
			memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
			iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, psVBFile [iHandle]->tRowNumber, TRUE);
			if (iserrno)
				iResult = -1;
		}
		if (!iResult)
		{
			if (!(psVBFile [iHandle]->iOpenMode & ISTRANS) || iVBInTrans == VBNOTRANS || iVBInTrans == VBCOMMIT || iVBInTrans == VBROLLBACK)
			{
				iserrno = iVBDataFree (iHandle, psVBFile [iHandle]->tRowNumber);
				if (iserrno)
					iResult = -1;
			}
		}
		if (!iResult)
		{
			isrecnum = psVBFile [iHandle]->tRowNumber;
			psVBFile [iHandle]->tRowNumber = 0;
		}
	}

	psVBFile [iHandle]->sFlags.iIsDictLocked |= 0x02;
ISDelcurrExit:
	iResult |= iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	isdelrec (int iHandle, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	off_t	tRowNumber
 *		The data row number to be deleted
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isdelrec (int iHandle, off_t tRowNumber)
{
	int	iDeleted,
		iResult = 0;

	if (iVBEnter (iHandle, TRUE))
		return (-1);

	if (tRowNumber > 0)
	{
		if (psVBFile [iHandle]->iOpenMode & ISTRANS)
		{
			iserrno = iVBDataLock (iHandle, VBWRLOCK, tRowNumber, TRUE);
			if (iserrno)
			{
				iResult = -1;
				goto ISDelrecExit;
			}
		}
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
		if (!iserrno && iDeleted)
			iserrno = ENOREC;
		if (iserrno)
			iResult = -1;
		if (!iResult)
			iResult = iRowDelete (iHandle, tRowNumber);
		if (!iResult)
		{
			iVBTransDelete (iHandle, tRowNumber, isreclen);	// BUG - retval
			memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
			iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, tRowNumber, TRUE);
			if (iserrno)
				iResult = -1;
		}
		if (!iResult)
		{
			if (!(psVBFile [iHandle]->iOpenMode & ISTRANS) || iVBInTrans == VBNOTRANS || iVBInTrans == VBCOMMIT || iVBInTrans == VBROLLBACK)
			{
				iserrno = iVBDataFree (iHandle, tRowNumber);
				if (iserrno)
					iResult = -1;
			}
		}
		if (!iResult)
		{
			isrecnum = tRowNumber;
			if (tRowNumber == psVBFile [iHandle]->tRowNumber)
				psVBFile [iHandle]->tRowNumber = 0;
		}
	}

ISDelrecExit:
	psVBFile [iHandle]->sFlags.iIsDictLocked |= 0x02;
	iResult |= iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	static	int	iRowDelete (int iHandle, off_t tRowNumber);
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
static	int
iRowDelete (int iHandle, off_t tRowNumber)
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
		iResult = iVBKeyDelete (iHandle, iKeyNumber);
		if (iResult)
		{
		// Eeek, an error occured.  Let's put back what we removed!
			while (iKeyNumber >= 0)
			{
				vVBMakeKey (iHandle, iKeyNumber, *(psVBFile [iHandle]->ppcRowBuffer), cKeyValue);
				iVBKeyInsert (iHandle, VBTREE_NULL, iKeyNumber, cKeyValue, tRowNumber, tDupNumber [iKeyNumber], VBTREE_NULL);	// BUG - retval
				iKeyNumber--;
			}
			iserrno = iResult;
			return (-1);
		}
	}

	return (0);
}
