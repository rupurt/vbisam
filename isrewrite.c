/*
 * Title:	isrewrite.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the rewriting to a file in the
 *	VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: isrewrite.c,v $
 *	Revision 1.1  2003/12/20 20:11:17  trev_vb
 *	Initial revision
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
			iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, FALSE);
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
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, psVBFile [iHandle]->tRowNumber, FALSE);
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

	if (tRowNumber > 0)
	{
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, FALSE);
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

	iVBExit (iHandle);
	return (iResult);
}
