/*
 * Title:	isdelete.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the deleting from a file in the
 *	VBISAM library.
 * Version:
 *	$Id: isdelete.c,v 1.2 2003/12/22 04:46:09 trev_vb Exp $
 * Modification History:
 *	$Log: isdelete.c,v $
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
			iResult = 0;
			tRowNumber = psVBFile [iHandle]->psKeyCurr [0]->tRowNode;
			iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, tRowNumber, FALSE);
			if (!iserrno && iDeleted)
				iserrno = ENOREC;
			if (iserrno)
				iResult = -1;
			if (!iResult)
				iResult = iVBRowDelete (iHandle, tRowNumber);
			if (!iResult)
			{
				memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
				iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, tRowNumber, FALSE);
				if (iserrno)
					iResult = -1;
			}
			if (!iResult)
			{
				iserrno = iVBDataFree (iHandle, tRowNumber);
				if (iserrno)
					iResult = -1;
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

	iVBExit (iHandle);
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
			iResult = iVBRowDelete (iHandle, psVBFile [iHandle]->tRowNumber);
		if (!iResult)
		{
			memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
			iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, psVBFile [iHandle]->tRowNumber, FALSE);
			if (iserrno)
				iResult = -1;
		}
	}
	if (!iResult)
	{
		iserrno = iVBDataFree (iHandle, psVBFile [iHandle]->tRowNumber);
		if (iserrno)
			iResult = -1;
	}
	if (!iResult)
	{
		isrecnum = psVBFile [iHandle]->tRowNumber;
		psVBFile [iHandle]->tRowNumber = 0;
	}

	iVBExit (iHandle);
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

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (-1);

	if (tRowNumber > 0)
	{
		iserrno = iVBDataRead (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), &iDeleted, psVBFile [iHandle]->tRowNumber, FALSE);
		if (!iserrno && iDeleted)
			iserrno = ENOREC;
		if (iserrno)
			iResult = -1;
		if (!iResult)
			iResult = iVBRowDelete (iHandle, tRowNumber);
		if (!iResult)
		{
			memset ((void *) *(psVBFile [iHandle]->ppcRowBuffer), 0, psVBFile [iHandle]->iMinRowLength + QUADSIZE);
			iserrno = iVBDataWrite (iHandle, (void *) *(psVBFile [iHandle]->ppcRowBuffer), TRUE, tRowNumber, FALSE);
			if (iserrno)
				iResult = -1;
		}
	}
	if (!iResult)
	{
		iserrno = iVBDataFree (iHandle, tRowNumber);
		if (iserrno)
			iResult = -1;
	}
	if (!iResult)
	{
		isrecnum = tRowNumber;
		if (tRowNumber == psVBFile [iHandle]->tRowNumber)
			psVBFile [iHandle]->tRowNumber = 0;
	}

	iVBExit (iHandle);
	return (iResult);
}
