/*
 * Title:	vbDataIO.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	21Nov2003
 * Description:
 *	This module handles ALL the low level data file I/O operations for the
 *	VBISAM library.
 * Version:
 *	$Id: vbDataIO.c,v 1.6 2004/01/06 14:31:59 trev_vb Exp $
 * Modification History:
 *	$Log: vbDataIO.c,v $
 *	Revision 1.6  2004/01/06 14:31:59  trev_vb
 *	TvB 06Jan2004 Added in VARLEN processing (In a fairly unstable sorta way)
 *	
 *	Revision 1.5  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.4  2004/01/03 07:14:21  trev_vb
 *	TvB 02Jan2004 Ooops, I should ALWAYS try to remember to be in the RIGHT
 *	TvB 02Jan2003 directory when I check code back into CVS!!!
 *	
 *	Revision 1.3  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
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
 *	However, it *DOES* set *(piDeletedRow) accordingly.
 *	The receiving buffer (pvBuffer) is only guaranteed to be long enough to
 *	hold the MINIMUM row length (exclusive of the 1 byte deleted flag) and
 *	thus we need to jump through hoops to avoid overwriting stuff beyond it.
 */
int
iVBDataRead (int iHandle, void *pvBuffer, int *piDeletedRow, off_t tRowNumber, int iVarlen)
{
	char	cFooter [1 + INTSIZE + QUADSIZE];
	int	iRowLength;
	off_t	tBlockNumber,
		tOffset,
		tSoFar = 0;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);

	iRowLength = psVBFile [iHandle]->iMinRowLength;
	iRowLength++;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
		iRowLength += INTSIZE + QUADSIZE;
	tOffset = iRowLength * (tRowNumber - 1);
	tBlockNumber = (tOffset / psVBFile [iHandle]->iNodeSize);
	tOffset -= (tBlockNumber * psVBFile [iHandle]->iNodeSize);
	if (iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0))
		return (EBADFILE);
	// Read in the *MINIMUM* rowlength and store it into pvBuffer
	while (tSoFar < psVBFile [iHandle]->iMinRowLength)
	{
		if ((psVBFile [iHandle]->iMinRowLength - tSoFar) < (psVBFile [iHandle]->iNodeSize - tOffset))
		{
			memcpy (pvBuffer + tSoFar, cNode0 + tOffset, psVBFile [iHandle]->iMinRowLength - tSoFar);
			tOffset += psVBFile [iHandle]->iMinRowLength - tSoFar;
			tSoFar = psVBFile [iHandle]->iMinRowLength;
			break;
		}
		memcpy (pvBuffer + tSoFar, cNode0 + tOffset, psVBFile [iHandle]->iNodeSize - tOffset);
		tBlockNumber++;
		tSoFar += psVBFile [iHandle]->iNodeSize - tOffset;
		tOffset = 0;
		if (iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0))
			return (EBADFILE);
	}
	pvBuffer += tSoFar;
	// OK, now for the footer.  Either 1 byte or 1 + INTSIZE + QUADSIZE.
	while (tSoFar < iRowLength)
	{
		if ((iRowLength - tSoFar) <= (psVBFile [iHandle]->iNodeSize - tOffset))
		{
			memcpy (cFooter + tSoFar - psVBFile [iHandle]->iMinRowLength, cNode0 + tOffset, iRowLength - tSoFar);
			break;
		}
		memcpy (cFooter + tSoFar - psVBFile [iHandle]->iMinRowLength, cNode0 + tOffset, psVBFile [iHandle]->iNodeSize - tOffset);
		tBlockNumber++;
		tSoFar += psVBFile [iHandle]->iNodeSize - tOffset;
		tOffset = 0;
		if (iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0))
			return (EBADFILE);
	}
	isreclen = psVBFile [iHandle]->iMinRowLength;
	*piDeletedRow = FALSE;
	if (cFooter [0] == 0x00)
		*piDeletedRow = TRUE;
	else
	{
		if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
		{
			psVBFile [iHandle]->iVarlenLength = ldint (cFooter + 1);
// VBISAM in 64 bit mode (4k Nodes) uses a ten bit slot number
// VBISAM in 32 bit mode (1k Nodes) uses an eight bit slot number
#if	_FILE_OFFSET_BITS == 64
			psVBFile [iHandle]->iVarlenSlot = ldint (cFooter + 1 + INTSIZE) >> 6;
			*(cFooter + 1 + INTSIZE + 1) &= 0x3f;
# if	MAX_NODE_LENGTH != 4096
BAD NODE LENGTH
# endif	// MAX_NODE_LENGTH
#else	// _FILE_OFFSET_BITS == 64
			psVBFile [iHandle]->iVarlenSlot = *(cFooter + 1 + INTSIZE);
# if	MAX_NODE_LENGTH != 1024
BAD NODE LENGTH
# endif	// MAX_NODE_LENGTH
#endif	// _FILE_OFFSET_BITS == 64
			*(cFooter + 1 + INTSIZE) = 0;
			psVBFile [iHandle]->tVarlenNode = ldquad (cFooter + 1 + INTSIZE);
			if (psVBFile [iHandle]->iVarlenLength)
				if (iVBVarlenRead (iHandle, (char *) pvBuffer, psVBFile [iHandle]->tVarlenNode, psVBFile [iHandle]->iVarlenSlot, psVBFile [iHandle]->iVarlenLength))
					return (iserrno);
			isreclen += psVBFile [iHandle]->iVarlenLength;
		}
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
	char	*pcTemp;
	int	iRowLength;
	off_t	tBlockNumber,
		tOffset,
		tSoFar = 0;

	// Sanity check - Is iHandle a currently open table?
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (ENOTOPEN);
	if (!psVBFile [iHandle])
		return (ENOTOPEN);

	// BUG? - Should we be allowed to write if the handle is open ISINPUT?

	iRowLength = psVBFile [iHandle]->iMinRowLength;
	tOffset = iRowLength + 1;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
		tOffset += INTSIZE + QUADSIZE;
	tOffset *= (tRowNumber - 1);
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
	{
		if (psVBFile [iHandle]->tVarlenNode)
			if (iVBVarlenDelete (iHandle, psVBFile [iHandle]->tVarlenNode, psVBFile [iHandle]->iVarlenSlot, psVBFile [iHandle]->iVarlenLength))
				return (-69);
		if (isreclen == psVBFile [iHandle]->iMinRowLength || iDeletedRow)
		{
			psVBFile [iHandle]->tVarlenNode = 0;
			psVBFile [iHandle]->iVarlenLength = 0;
			psVBFile [iHandle]->iVarlenSlot = 0;
		}
		else
			if (iVBVarlenWrite (iHandle, pvBuffer + psVBFile [iHandle]->iMinRowLength, isreclen - psVBFile [iHandle]->iMinRowLength))
				return (iserrno);
	}
	memcpy (pcWriteBuffer, pvBuffer, iRowLength);
	*(pcWriteBuffer + iRowLength) = iDeletedRow ? 0x00 : 0x0a;
	iRowLength++;
	if (psVBFile [iHandle]->iOpenMode & ISVARLEN)
	{
		stint (psVBFile [iHandle]->iVarlenLength, pcWriteBuffer + iRowLength);
		stquad (psVBFile [iHandle]->tVarlenNode, pcWriteBuffer + iRowLength + INTSIZE);
		pcTemp = pcWriteBuffer + iRowLength + INTSIZE;
#if	_FILE_OFFSET_BITS == 64
		stint ((psVBFile [iHandle]->iVarlenSlot << 6) + ldint (pcTemp), pcTemp);
#else	// _FILE_OFFSET_BITS == 64
		*pcTemp = psVBFile [iHandle]->iVarlenSlot;
#endif	// _FILE_OFFSET_BITS == 64
		iRowLength += INTSIZE + QUADSIZE;
	}

	tBlockNumber = (tOffset / psVBFile [iHandle]->iNodeSize);
	tOffset -= (tBlockNumber * psVBFile [iHandle]->iNodeSize);
	while (tSoFar < iRowLength)
	{
		memset (cNode0, 0, MAX_NODE_LENGTH);
		iVBBlockRead (iHandle, FALSE, tBlockNumber + 1, cNode0);	// Can fail!!
		if ((iRowLength - tSoFar) <= (psVBFile [iHandle]->iNodeSize - tOffset))
		{
			memcpy (cNode0 + tOffset, pcWriteBuffer + tSoFar, iRowLength - tSoFar);
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
