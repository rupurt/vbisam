/*
 * Title:	vbDataIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	21Nov2003
 * Description:
 *	This module handles ALL the low level data file I/O operations for the
 *	VBISAM library.
 * Version:
 *	$Id: vbDataIO.c,v 1.2 2003/12/22 04:48:27 trev_vb Exp $
 * Modification History:
 *	$Log: vbDataIO.c,v $
 *	Revision 1.2  2003/12/22 04:48:27  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:24  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

static	char	cNode0 [MAX_NODE_LENGTH];

/*
 * Prototypes
 */
int	iVBDataRead (int, void *, int *, off_t, int);
int	iVBDataWrite (int, void *, int, off_t, int);

/*
 * Name:
 *	int	iVBDataRead (int iHandle, void *pvBuffer, int *piDeletedRow, off_t tRowNumber, int iVarlen);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	void	*pvBuffer
 *		The address of the buffer
 *	int	*piDeletedRow
 *		If row is deleted, set to TRUE
 *		else set to FALSE
 *	off_t	tRowNumber
 *		The row number to be read in
 *	int	iVarlen
 *		If 0, only read in the FIXED LENGTH component
 * Prerequisites:
 *	Any needed locking is handled external to this function
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 * Problems:
 *	NONE known
 * Comments:
 *	This function is *NOT* concerned with whether the row is deleted or not
 */
int
iVBDataRead (int iHandle, void *pvBuffer, int *piDeletedRow, off_t tRowNumber, int iVarlen)
{
	char	*pcDeletePtr;
	off_t	tBlockNumber,
		tOffset,
		tRowLength,
		tSoFar = 0;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);

	tRowLength = psVBFile [iHandle]->iMinRowLength;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
		tRowLength += QUADSIZE;
	else
		tRowLength++;
	tOffset = tRowLength * (tRowNumber - 1);
	tBlockNumber = (tOffset / psVBFile [iHandle]->iNodeSize);
	tOffset -= (tBlockNumber * psVBFile [iHandle]->iNodeSize);
	while (tSoFar < tRowLength)
	{
		if (iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0))
			return (EBADFILE);
		if ((tRowLength - tSoFar) < (psVBFile [iHandle]->iNodeSize - tOffset))
		{
			memcpy (pvBuffer + tSoFar, cNode0 + tOffset, tRowLength - tSoFar);
			break;
		}
		memcpy (pvBuffer + tSoFar, cNode0 + tOffset, psVBFile [iHandle]->iNodeSize - tOffset);
		tBlockNumber++;
		tSoFar += psVBFile [iHandle]->iNodeSize - tOffset;
		tOffset = 0;
	}
	*piDeletedRow = FALSE;
	pcDeletePtr = pvBuffer;
	pcDeletePtr += psVBFile [iHandle]->iMinRowLength;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
	{
		if (*pcDeletePtr & 0x80)
			*piDeletedRow = TRUE;
	// BUG - Here's where we need to add in reading of varlen components
	// BUG - The length to read is possibly defined by ldquad (pcDeletePtr)?
	}
	else
	{
		if (*pcDeletePtr == 0x00)
			*piDeletedRow = TRUE;
	}
	return (0);
}

/*
 * Name:
 *	int	iVBDataWrite (int iHandle, void *pvBuffer, int iDeletedRow, off_t tRowNumber, int iVarlen);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the idx file)
 *	void	*pvBuffer
 *		The address of the buffer
 *	int	iDeletedRow
 *		If FALSE mark row as active
 *		else mark row as deleted
 *	off_t	tRowNumber
 *		The row number to be written out to
 *	int	iVarlen
 *		If 0, only read in the FIXED LENGTH component
 * Prerequisites:
 *	Any needed locking is handled external to this function
 * Returns:
 *	0	Success
 *	ENOTOPEN
 *	EBADFILE
 * Problems:
 *	NONE known
 */
int
iVBDataWrite (int iHandle, void *pvBuffer, int iDeletedRow, off_t tRowNumber, int iVarlen)
{
	off_t	tBlockNumber,
		tOffset,
		tRowLength,
		tSoFar = 0;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);

	// BUG? - Should we be allowed to write if the handle is open ISINPUT?

	tRowLength = psVBFile [iHandle]->iMinRowLength;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
		tOffset = tRowLength + QUADSIZE;
	else
		tOffset = tRowLength + 1;
	tOffset *= (tRowNumber - 1);
	memcpy (pcRowBuffer, pvBuffer, tRowLength);
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
	{
// BUG - The REST of the VARLEN row is where???
		stquad (isreclen, pcRowBuffer + tRowLength);
		if (iDeletedRow)
			*(pcRowBuffer + tRowLength) |= 0x80;
		tRowLength += QUADSIZE;
	}
	else
	{
		if (iDeletedRow)
			*(pcRowBuffer + tRowLength) = 0x00;
		else
			*(pcRowBuffer + tRowLength) = 0x0a;
		tRowLength++;
	}

	tBlockNumber = (tOffset / psVBFile [iHandle]->iNodeSize);
	tOffset -= (tBlockNumber * psVBFile [iHandle]->iNodeSize);
	while (tSoFar < tRowLength)
	{
		memset (cNode0, 0, MAX_NODE_LENGTH);
		iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0);	// Can fail!!
		if ((tRowLength - tSoFar) <= (psVBFile [iHandle]->iNodeSize - tOffset))
		{
			memcpy (cNode0 + tOffset, pcRowBuffer + tSoFar, tRowLength - tSoFar);
			if (iVBBlockWrite (iHandle, FALSE, tBlockNumber + 1, cNode0))
				return (EBADFILE);
			break;
		}
		memcpy (cNode0 + tOffset, pvBuffer + tSoFar, psVBFile [iHandle]->iNodeSize - tOffset);
		if (iVBBlockWrite (iHandle, FALSE, tBlockNumber + 1, cNode0))
			return (EBADFILE);
		tBlockNumber++;
		tSoFar += psVBFile [iHandle]->iNodeSize - tOffset;
		tOffset = 0;
	}
	return (0);
}
