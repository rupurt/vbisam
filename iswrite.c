/*
 * Title:	iswrite.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the writing to a file in the
 *	VBISAM library.
 * Version:
 *	$Id: iswrite.c,v 1.5 2004/01/06 14:31:59 trev_vb Exp $
 * Modification History:
 *	$Log: iswrite.c,v $
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
int	iswrcurr (int, char *);
int	iswrite (int, char *);
int	iVBWriteRow (int, char *, off_t);

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
	off_t	tRowNumber;


	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);

	if (psVBFile [iHandle]->iOpenMode & ISVARLEN && (isreclen > psVBFile [iHandle]->iMaxRowLength || isreclen < psVBFile [iHandle]->iMinRowLength))
	{
		iserrno = EBADARG;
		return (-1);
	}

	tRowNumber = tVBDataCountGetNext (iHandle);
	if (tRowNumber == -1)
		return (-1);

	iResult = iVBWriteRow (iHandle, pcRow, tRowNumber);
	if (!iResult)
		psVBFile [iHandle]->tRowNumber = tRowNumber;

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
	off_t	tRowNumber;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);

	if (psVBFile [iHandle]->iOpenMode & ISVARLEN && (isreclen > psVBFile [iHandle]->iMaxRowLength || isreclen < psVBFile [iHandle]->iMinRowLength))
	{
		iserrno = EBADARG;
		return (-1);
	}

	tRowNumber = tVBDataCountGetNext (iHandle);
	if (tRowNumber == -1)
		return (-1);

	iResult = iVBWriteRow (iHandle, pcRow, tRowNumber);

	iVBExit (iHandle);
	return (iResult);
}

/*
 * Name:
 *	int	iVBWriteRow (int iHandle, char *pcRow, off_t tRowNumber);
 * Arguments:
 *	int	iHandle
 *		The open VBISAM file handle
 *	char	*pcRow
 *		The data to be written
 *	off_t	tRowNumber
 *		The row number to write to
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
iVBWriteRow (int iHandle, char *pcRow, off_t tRowNumber)
{
	int	iResult = 0;

	isrecnum = tRowNumber;
	if (iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iResult = iVBDataLock (iHandle, VBWRLOCK, tRowNumber, TRUE);
	if (!iResult)
	{
		psVBFile [iHandle]->tVarlenNode = 0;	// Stop it from removing
		iResult = iVBDataWrite (iHandle, (void *) pcRow, FALSE, tRowNumber, TRUE);
	}
	if (iResult)
	{
		iserrno = iResult;
		return (-1);
	}
	iResult = iVBRowInsert (iHandle, pcRow, tRowNumber);
	if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iResult = iVBTransInsert (iHandle, tRowNumber, isreclen, pcRow);
	if (!iResult)
		iserrno = 0;
	return (iResult);
}
