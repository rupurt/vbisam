/*
 * Title:	isrewrite.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the rewriting to a file in the
 *	VBISAM library.
 * Version:
 *	$Id: isrewrite.c,v 1.4 2004/01/05 07:36:17 trev_vb Exp $
 * Modification History:
 *	$Log: isrewrite.c,v $
 *	Revision 1.4  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.3  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
 *	Revision 1.2  2003/12/22 04:47:34  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:17  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	isrewrite (int, char *);
int	isrewcurr (int, char *);
int	isrewrec (int, off_t, char *);

/*
 * Name:
 *	int	isrewrite (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data row to be rewritten
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isrewrite (int iHandle, char *pcRow)
{
	char	cKeyValue [VB_MAX_KEYLEN];
	int	iDeleted,
		iResult = 0;
	off_t	tRowNumber;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (-1);

	if (psVBFile [iHandle]->psKeydesc [0]->iFlags & ISDUPS)
		iserrno = ENOREC;
	else
	{
		vVBMakeKey (iHandle, 0, pcRow, cKeyValue);
		iResult = iVBKeySearch (iHandle, ISEQUAL, 0, 0, cKeyValue, 0);
		switch (iResult)
		{
		case	1:	// Exact match
			tRowNumber = psVBFile [iHandle]->psKeyCurr [0]->tRowNode;
			if (iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
			{
				iserrno = iVBDataLock (iHandle, VBWRLOCK, tRowNumber, TRUE);
				if (!iserrno)
					iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
			}
			else
				iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
			if (!iserrno && iDeleted)
				iserrno = ENOREC;
			if (iserrno)
				iResult = -1;
			if (!iResult)
				iResult = iVBRowUpdate (iHandle, pcRow, tRowNumber);
			if (!iResult)
			{
				isrecnum = tRowNumber;
				iResult = iVBDataWrite (iHandle, (void *) pcRow, FALSE, isrecnum, TRUE);
			}
			if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
				iVBTransUpdate (iHandle, tRowNumber, psVBFile [iHandle]->iMinRowLength, psVBFile [iHandle]->iMinRowLength, pcRow);		// BUG - Not varlen compliant!
			break;

		case	0:		// LESS than
		case	2:		// GREATER than
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

	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	isrewcurr (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data row to be rewritten
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isrewcurr (int iHandle, char *pcRow)
{
	int	iDeleted,
		iResult = 0;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (-1);

	if (psVBFile [iHandle]->tRowNumber > 0)
	{
		if (iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		{
			iserrno = iVBDataLock (iHandle, VBWRLOCK, psVBFile [iHandle]->tRowNumber, TRUE);
			if (!iserrno)
				iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, psVBFile [iHandle]->tRowNumber, TRUE);
		}
		else
			iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, psVBFile [iHandle]->tRowNumber, TRUE);
		if (!iserrno && iDeleted)
			iserrno = ENOREC;
		if (iserrno)
			iResult = -1;
		if (!iResult)
			iResult = iVBRowUpdate (iHandle, pcRow, psVBFile [iHandle]->tRowNumber);
	}
	if (!iResult)
	{
		isrecnum = psVBFile [iHandle]->tRowNumber;
		iResult = iVBDataWrite (iHandle, (void *) pcRow, FALSE, isrecnum, TRUE);
	}
	if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iVBTransUpdate (iHandle, psVBFile [iHandle]->tRowNumber, psVBFile [iHandle]->iMinRowLength, psVBFile [iHandle]->iMinRowLength, pcRow);		// BUG - Not varlen compliant!

	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	isrewrec (int iHandle, off_t tRowNumber, char *pcRow);
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
isrewrec (int iHandle, off_t tRowNumber, char *pcRow)
{
	int	iDeleted,
		iResult = 0;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (-1);

	if (tRowNumber < 1)
	{
		iResult = -1;
		iserrno = EBADARG;
	}
	else
	{
		if (iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		{
			iserrno = iVBDataLock (iHandle, VBWRLOCK, tRowNumber, TRUE);
			if (!iserrno)
				iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
		}
		else
			iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, TRUE);
		if (!iserrno && iDeleted)
			iserrno = ENOREC;
		if (iserrno)
			iResult = -1;
		if (!iResult)
			iResult = iVBRowUpdate (iHandle, pcRow, tRowNumber);
	}
	if (!iResult)
	{
		isrecnum = tRowNumber;
		iResult = iVBDataWrite (iHandle, (void *) pcRow, FALSE, isrecnum, TRUE);
	}
	if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iVBTransUpdate (iHandle, tRowNumber, psVBFile [iHandle]->iMinRowLength, psVBFile [iHandle]->iMinRowLength, pcRow);		// BUG - Not varlen compliant!

	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	iVBExit (iHandle);
	return (iResult);
}
