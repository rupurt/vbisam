/*
 * Title:	isopen.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	24Nov2003
 * Description:
 *	This module deals with the opening and closing of VBISAM files
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: isopen.c,v $
 *	Revision 1.1  2003/12/20 20:11:21  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

static	int	iInitialized = FALSE;
static	char	cNode0 [MAX_NODE_LENGTH];

/*
 * Prototypes
 */
int	iscleanup (void);
int	isclose (int);
int	isindexinfo (int, struct keydesc *, int);
int	isopen (char *, int);
static	off_t	tCountRows (int);

/*
 * Name:
 *	int	iscleanup (void);
 * Arguments:
 *	NONE
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
iscleanup (void)
{
	int	iLoop,
		iResult,
		iResult2 = 0;

	for (iLoop = 0; iLoop <= iVBMaxUsedHandle; iLoop++)
		if (psVBFile [iLoop] != (struct DICTINFO *) 0)
		{
			iResult = isclose (iLoop);
			if (iResult)
				iResult2 = iserrno;
		}
	return (iResult2);
}

/*
 * Name:
 *	int	isclose (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The currently open VBISAM handle
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isclose (int iHandle)
{
	int	iLoop,
		iResult;
	struct	VBLOCK
		*psRowLock;

	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		goto CLOSE_ERR;
	if (psVBFile [iHandle] == (struct DICTINFO *) 0)
		goto CLOSE_ERR;
	if (psVBFile [iHandle]->iOpenMode & ISEXCLLOCK)
		iVBForceExit (iHandle);	// Bug - Check retval
	vVBBlockInvalidate (iHandle);
	iResult = iVBClose (psVBFile [iHandle]->iDataHandle);
	iResult = iVBClose (psVBFile [iHandle]->iIndexHandle);
	for (iLoop = 0; iLoop < MAXSUBS; iLoop++)
	{
		vVBTreeAllFree (iHandle, iLoop, psVBFile [iHandle]->psTree [iLoop]);
		if (psVBFile [iHandle]->psKeydesc [iLoop])
		{
			vVBKeyUnMalloc (iHandle, iLoop);
			vVBFree (psVBFile [iHandle]->psKeydesc [iLoop], sizeof (struct keydesc));
		}
	}
	while (psVBFile [iHandle]->psLocks)
	{
		psRowLock = psVBFile [iHandle]->psLocks->psNext;
		vVBLockFree (psVBFile [iHandle]->psLocks);
		psVBFile [iHandle]->psLocks = psRowLock;
	}
	vVBFree (psVBFile [iHandle], sizeof (struct DICTINFO));
	psVBFile [iHandle] = (struct DICTINFO *) 0;

	return (0);

CLOSE_ERR:
	iserrno = EBADARG;
	return (-1);
}

/*
 * Name:
 *	int	isindexinfo (int iHandle, struct keydesc *psKeydesc, int iKeyNumber);
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 *	struct	keydesc	*psKeydesc
 *		The receiving keydesc struct
 *		Note that if iKeyNumber == 0, then this is a dictinfo!
 *	int	iKeyNumber
 *		The keynumber (or 0 for a dictionary!)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	An error occurred.  iserrno contains the reason
 * Problems:
 *	NONE known
 */
int
isindexinfo (int iHandle, struct keydesc *psKeydesc, int iKeyNumber)
{
	int	iResult;
	struct	dictinfo
		sDict;

	// Sanity check - Is iHandle a currently open table?
	iserrno = ENOTOPEN;
	if (iHandle < 0 || iHandle > iVBMaxUsedHandle)
		return (-1);
	if (!psVBFile [iHandle])
		return (-1);
	iserrno = EBADKEY;
	if (iKeyNumber < 0 || iKeyNumber > psVBFile [iHandle]->iNKeys);
		return (-1);
	iserrno = 0;
	if (iKeyNumber)
	{
		memcpy (psKeydesc, psVBFile [iHandle]->psKeydesc [iKeyNumber - 1], sizeof (struct keydesc));
		return (0);
	}

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (-1);

	sDict.iNKeys = psVBFile [iHandle]->iNKeys;
	sDict.iMaxRowLength = psVBFile [iHandle]->iMaxRowLength;
	sDict.iIndexLength = psVBFile [iHandle]->iNodeSize;
	sDict.tNRows = tCountRows (iHandle);
	memcpy (psKeydesc, &sDict, sizeof (struct dictinfo));

	iVBExit (iHandle);
	return (0);
}

/*
 * Name:
 *	int	isopen (char *pcFilename, int iMode);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be built / opened
 *	int	iMode
 *		See isam.h
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  iserrno contains the reason
 *	Other	The handle to be used for accessing this file
 * Problems:
 *	NONE known
 */
int
isopen (char *pcFilename, int iMode)
{
	char	*pcTemp;
	int	iFlags,
		iFound = FALSE,
		iHandle,
		iIndexNumber = 0,
		iIndexPart,
		iKeydescLength,
		iLengthUsed,
		iLoop,
		iResult;
	struct	DICTINFO
		*psFile = (struct DICTINFO *) 0;
	off_t	tNodeNumber;

	if (!iInitialized)
	{
		iInitialized = TRUE;
		atexit (vVBUnMalloc);
	}
	iFlags = iMode & 0x03;
	if (iFlags == 3)
	{
	// Cannot be BOTH ISOUTPUT and ISINOUT
		iserrno = EBADARG;
		return (-1);
	}
	iserrno = EFNAME;
	if (strlen (pcFilename) > MAX_PATH_LENGTH - 4)
		return (-1);
	for (iHandle = 0; iHandle <= iVBMaxUsedHandle; iHandle++)
	{
		if (psVBFile [iHandle] == (struct DICTINFO *) 0)
		{
			iFound = TRUE;
			break;
		}
	}
	if (!iFound)
	{
		iserrno = ETOOMANY;
		if (iVBMaxUsedHandle >= VB_MAX_FILES)
			return (-1);
		iVBMaxUsedHandle++;
		iHandle = iVBMaxUsedHandle;
		psVBFile [iHandle] = (struct DICTINFO *) 0;
	}
	psVBFile [iHandle] = (struct DICTINFO *) pvVBMalloc (sizeof (struct DICTINFO));
	if (psVBFile [iHandle] == (struct DICTINFO *) 0)
		goto OPEN_ERR;
	psFile = psVBFile [iHandle];
	memset (psFile, 0, sizeof (struct DICTINFO));
	psFile->iDataHandle = -1;
	psFile->iIndexHandle = -1;
	sprintf (cNode0, "%s.dat", pcFilename);
	if (iVBAccess (cNode0, F_OK))
	{
		errno = ENOENT;
		goto OPEN_ERR;
	}
	sprintf (cNode0, "%s.idx", pcFilename);
	if (iVBAccess (cNode0, F_OK))
	{
		errno = ENOENT;
		goto OPEN_ERR;
	}
	psFile->iIndexHandle = iVBOpen (cNode0, O_RDWR, 0);
	if (psFile->iIndexHandle < 0)
		goto OPEN_ERR;
	sprintf (cNode0, "%s.dat", pcFilename);
	psFile->iDataHandle = iVBOpen (cNode0, O_RDWR, 0);
	if (psFile->iDataHandle < 0)
		goto OPEN_ERR;
	psVBFile [iHandle]->tDataPosn = -1;
	psVBFile [iHandle]->tIndexPosn = -1;

	psFile->iNodeSize = MAX_NODE_LENGTH;
	iResult = iVBEnter (iHandle, TRUE);	// Reads in dictionary node
	if (iResult)
		goto OPEN_ERR;
	errno = EBADFILE;
#if	_FILE_OFFSET_BITS == 64
	if (psVBFile [iHandle]->sDictNode.cValidation [0] != 0x56 || psVBFile [iHandle]->sDictNode.cValidation [1] != 0x42)
		goto OPEN_ERR;
#else	// _FILE_OFFSET_BITS == 64
	if (psVBFile [iHandle]->sDictNode.cValidation [0] != -2 || psVBFile [iHandle]->sDictNode.cValidation [1] != 0x53)
		goto OPEN_ERR;
#endif	// _FILE_OFFSET_BITS == 64
	psFile->iNodeSize = ldint (psVBFile [iHandle]->sDictNode.cNodeSize) + 1;
	psFile->iNKeys = ldint (psVBFile [iHandle]->sDictNode.cIndexCount);
	psFile->iMinRowLength = ldint (psVBFile [iHandle]->sDictNode.cMinRowLength);
	psFile->iMaxRowLength = ldint (psVBFile [iHandle]->sDictNode.cMaxRowLength);

	if (psFile->iMaxRowLength && psFile->iMaxRowLength != psFile->iMinRowLength)
	{
		errno = EROWSIZE;
		if (!(iMode & ISVARLEN))
			goto OPEN_ERR;
	}
	else
	{
		errno = EROWSIZE;
		if (iMode & ISVARLEN)
			goto OPEN_ERR;
	}
	psFile->iOpenMode = iMode;
	if (psFile->iMinRowLength + QUADSIZE > iRowBufferLength)
	{
		if (pcRowBuffer)
			vVBFree (pcRowBuffer, iRowBufferLength);
		iRowBufferLength = psFile->iMinRowLength + QUADSIZE;
		pcRowBuffer = (char *) pvVBMalloc (iRowBufferLength);
		if (!pcRowBuffer)
			goto OPEN_ERR;
	}
	psFile->ppcRowBuffer = &pcRowBuffer;
	tNodeNumber = ldquad (psVBFile [iHandle]->sDictNode.cNodeKeydesc);

	// Fill in the keydesc stuff
	while (tNodeNumber)
	{
		//iResult = iVBNodeRead (iHandle, (void *) cNode0, tNodeNumber);
		iResult = iVBBlockRead (iHandle, TRUE, tNodeNumber, cNode0);
		errno = iserrno;
		if (iResult)
			goto OPEN_ERR;
		pcTemp = cNode0;
		errno = EBADFILE;
		if (*(cNode0 + psFile->iNodeSize - 3) != -1 || *(cNode0 + psFile->iNodeSize - 2) != 0x7e)
			goto OPEN_ERR;
		iLengthUsed = ldint (pcTemp);
		pcTemp += INTSIZE;
		tNodeNumber = ldquad (pcTemp);
		pcTemp += QUADSIZE;
		iLengthUsed -= (INTSIZE + QUADSIZE);
		while (iLengthUsed > 0)
		{
			errno = EBADFILE;
			if (iIndexNumber >= MAXSUBS)
				goto OPEN_ERR;
			iKeydescLength = ldint (pcTemp);
			iLengthUsed -= iKeydescLength;
			pcTemp += INTSIZE;
			psFile->psKeydesc [iIndexNumber] = (struct keydesc *) pvVBMalloc (sizeof (struct keydesc));
			if (psFile->psKeydesc [iIndexNumber] == (struct keydesc *) 0)
				goto OPEN_ERR;
			psFile->psKeydesc [iIndexNumber]->iNParts = 0;
			psFile->psKeydesc [iIndexNumber]->iKeyLength = 0;
			psFile->psKeydesc [iIndexNumber]->tRootNode = ldquad (pcTemp);
			pcTemp += QUADSIZE;
			psFile->psKeydesc [iIndexNumber]->iFlags = (*pcTemp) * 2;
			pcTemp++;
			iKeydescLength -= (QUADSIZE + INTSIZE + 1);
			iIndexPart = 0;
			if (*pcTemp & 0x80)
				psFile->psKeydesc [iIndexNumber]->iFlags |= ISDUPS;
			*pcTemp &= ~0x80;
			while (iKeydescLength > 0)
			{
				psFile->psKeydesc [iIndexNumber]->iNParts++;
				psFile->psKeydesc [iIndexNumber]->sPart [iIndexPart].iLength = ldint (pcTemp);
				psFile->psKeydesc [iIndexNumber]->iKeyLength += psFile->psKeydesc [iIndexNumber]->sPart [iIndexPart].iLength;
				pcTemp += INTSIZE;
				psFile->psKeydesc [iIndexNumber]->sPart [iIndexPart].iStart = ldint (pcTemp);
				pcTemp += INTSIZE;
				psFile->psKeydesc [iIndexNumber]->sPart [iIndexPart].iType = *pcTemp;
				pcTemp++;
				iKeydescLength -= ((INTSIZE * 2) + 1);
				errno = EBADFILE;
				if (iKeydescLength < 0)
					goto OPEN_ERR;
				iIndexPart++;
			}
			iIndexNumber++;
		}
		if (iLengthUsed < 0)
			goto OPEN_ERR;
	}
	if (iMode & ISEXCLLOCK)
		iResult = iVBFileOpenLock (iHandle, 2);
	else
		iResult = iVBFileOpenLock (iHandle, 1);
	if (iResult)
	{
		errno = EFLOCKED;
		goto OPEN_ERR;
	}
	iVBExit (iHandle);
	iResult = isstart (iHandle, psVBFile [iHandle]->psKeydesc [0], 0, (char *) 0, ISFIRST);
	if (iResult)
	{
		errno = iserrno;
		goto OPEN_ERR;
	}

	psVBFile [iHandle]->tTransLast = -1;
	return (iHandle);
OPEN_ERR:
	iVBExit (iHandle);
	if (psVBFile [iHandle] != (struct DICTINFO *) 0)
	{
		for (iLoop = 0; iLoop < MAXSUBS; iLoop++)
			if (psFile->psKeydesc [iLoop])
				vVBFree (psFile->psKeydesc [iLoop], sizeof (struct keydesc));
		if (psFile->iDataHandle != -1)
			iVBClose (psFile->iDataHandle);
		if (psFile->iDataHandle != -1)
			iVBClose (psFile->iIndexHandle);
		vVBFree (psVBFile [iHandle], sizeof (struct DICTINFO));
	}
	psVBFile [iHandle] = (struct DICTINFO *) 0;
	iserrno = errno;
	return (-1);
}

/*
 * Name:
 *	static	off_t	tCountRows (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 * Prerequisites:
 *	NONE
 * Returns:
 *	N	Where n is the count of rows in existance in the VBISAM file
 * Problems:
 *	NONE known
 */
static	off_t
tCountRows (int iHandle)
{
	int	iNodeUsed;
	struct	FREENODE
	{
		char	cID [4];
		char	cNodeUsed [INTSIZE];
		char	cRFU [INTSIZE];
		char	cNextNode [QUADSIZE];
	} *psFree;
	off_t	tNodeNumber,
		tDataCount;

	psFree = (struct FREENODE *) &cNode0;
	tNodeNumber = ldquad ((char *) psVBFile [iHandle]->sDictNode.cDataFree);
	tDataCount = ldquad ((char *) psVBFile [iHandle]->sDictNode.cDataCount);
	while (tNodeNumber)
	{
		//if (iVBNodeRead (iHandle, (void *) cNode0, tNodeNumber))
		if (iVBBlockRead (iHandle, TRUE, tNodeNumber, cNode0))
			return (-1);
		iNodeUsed = ldint (psFree->cNodeUsed);
		iNodeUsed -= 4 + (INTSIZE * 2) + QUADSIZE;
		tDataCount -= (iNodeUsed / QUADSIZE);
		tNodeNumber = ldquad (psFree->cNextNode);
	}
	return (tDataCount);
}
