/*
 * Title:	iswrite.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the writing to a file in the
 *	VBISAM library.
 * Version:
 *	$Id: iswrite.c,v 1.2 2003/12/22 04:48:02 trev_vb Exp $
 * Modification History:
 *	$Log: iswrite.c,v $
 *	Revision 1.2  2003/12/22 04:48:02  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:20  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	iswrcurr (int iHandle, char *pcRow);
int	iswrite (int iHandle, char *pcRow);
static	int	iWrite (int iHandle, char *pcRow);

static	off_t	tRowWritten;

/*
 * Name:
 *	int	iswrcurr (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data to be written
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
iswrcurr (int iHandle, char *pcRow)
{
	int	iResult;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);

	iResult = iWrite (iHandle, pcRow);
	if (!iResult)
	{
		psVBFile [iHandle]->tRowNumber = tRowWritten;
	}

	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	iswrite (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data to be written
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
iswrite (int iHandle, char *pcRow)
{
	int	iResult;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);

	iResult = iWrite (iHandle, pcRow);

	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	static	int	iWrite (int iHandle, char *pcRow);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data to be written
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
static	int
iWrite (int iHandle, char *pcRow)
{
	int	iResult;

	tRowWritten = tVBDataCountGetNext (iHandle);
	if (tRowWritten == -1)
		return (-1);
	isrecnum = tRowWritten;
	iResult = iVBDataWrite (iHandle, (void *) pcRow, FALSE, tRowWritten, TRUE);
	if (iResult)
	{
		iserrno = iResult;
		return (-1);
	}
	return (iVBRowInsert (iHandle, pcRow, tRowWritten));
}
