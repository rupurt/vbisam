/*
 * Title:	isHelper.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	26Nov2003
 * Description:
 *	This is the module where all the various 'helper' functions are defined.
 *	Only functions with external linkage (i.e. is*, ld* and st*) should be
 *	defined within this module.
 * Version:
 *	$Id: isHelper.c,v 1.4 2004/01/05 07:36:17 trev_vb Exp $
 * Modification History:
 *	$Log: isHelper.c,v $
 *	Revision 1.4  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.3  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
 *	Revision 1.2  2003/12/22 04:45:31  trev_vb
 *	TvB 21Dec2003 Modified header to correct case ('Id')
 *	
 *	Revision 1.1.1.1  2003/12/20 20:11:19  trev_vb
 *	Init import
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	iscluster (int, struct keydesc *);
int	iserase (char *);
int	isflush (int);
int	islock (int);
int	isrelease (int);
int	isrename (char *, char *);
int	issetunique (int, off_t);
int	isuniqueid (int, off_t *);
int	isunlock (int);
void	ldchar (char *, int, char *);
void	stchar (char *, char *, int);
int	ldint (char *);
void	stint (int, char *);
long	ldlong (char *);
void	stlong (long, char *);
off_t	ldquad (char *);
void	stquad (off_t, char *);
double	ldfloat (char *);
void	stfloat (double, char *);
double	ldfltnull (char *, short *);
void	stfltnull (double, char *, short);
double	lddbl (char *);
void	stdbl (double, char *);
double	lddblnull (char *, short *);
void	stdblnull (double, char *, short);

/*
 * Name:
 *	int	iscluster (int iHandle, struct keydesc *psKeydesc)
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 *	struct	keydesc	*psKeydesc
 *		The index to order the data by
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
iscluster (int iHandle, struct keydesc *psKeydesc)
{
	// BUG Write iscluster() and don't forget to call iVBTransCluster
	return (0);
}

/*
 * Name:
 *	int	iserase (char *pcFilename)
 * Arguments:
 *	char	*pcFilename
 *		The name (sans extension) of the file to be deleted
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
iserase (char *pcFilename)
{
	char	cBuffer [256];

	sprintf (cBuffer, "%s.idx", pcFilename);
	if (iVBUnlink (cBuffer))
		goto EraseExit;
	sprintf (cBuffer, "%s.dat", pcFilename);
	if (iVBUnlink (cBuffer))
		goto EraseExit;
	return (iVBTransErase (pcFilename));
EraseExit:
	iserrno = errno;
	return (-1);
}

/*
 * Name:
 *	int	isflush (int iHandle);
 * Arguments:
 *	int	iHandle
 *		The handle of the VBISAM file about to be flushed
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 * Problems:
 *	NONE known
 * Comments:
 *	Since we write out data in real-time, no flushing is needed
 */
int
isflush (int iHandle)
{
	return (0);
}

/*
 * Name:
 *	int	islock (int iHandle)
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
islock (int iHandle)
{
	return (iVBDataLock (iHandle, VBWRLOCK, 0, FALSE));
}

/*
 * Name:
 *	int	isrelease (int iHandle)
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	We can NOT do a vVBEnter / vVBExit here as this is called from
 *	OTHER is* functions (isstart and isread)
 */
int
isrelease (int iHandle)
{
	struct	VBLOCK
		*psLock,
		*psLockNext;

	psLock = psVBFile [iHandle]->psLockHead;
	while (psLock)
	{
		psLockNext = psLock->psNext;
		if (psLock->iIsTransaction)
			return (0);
// Note: this implicitly relies on the following to reset the psLockHead / Tail
		iserrno = iVBDataLock (iHandle, VBUNLOCK, psLock->tRowNumber, FALSE);
		if (iserrno)
			return (-1);
		psLock = psLockNext;
	}
	return (0);
}

/*
 * Name:
 *	int	isrename (char *pcOldName, char *pcNewName);
 * Arguments:
 *	char	*pcOldName
 *		The current filename (sans extension)
 *	char	*pcNewName
 *		The new filename (sans extension)
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
isrename (char *pcOldName, char *pcNewName)
{
	char	cBuffer [2][256];
	int	iResult;

	sprintf (cBuffer [0], "%s.idx", pcOldName);
	sprintf (cBuffer [1], "%s.idx", pcNewName);
	iResult = iVBLink (cBuffer [0], cBuffer [1]);
	if (iResult == -1)
		goto RenameExit;
	iResult = iVBUnlink (cBuffer [0]);
	if (iResult == -1)
		goto RenameExit;
	sprintf (cBuffer [0], "%s.dat", pcOldName);
	sprintf (cBuffer [1], "%s.dat", pcNewName);
	iResult = iVBLink (cBuffer [0], cBuffer [1]);
	if (iResult == -1)
		goto RenameExit;
	iResult = iVBUnlink (cBuffer [0]);
	if (iResult == -1)
		goto RenameExit;
	return (iVBTransRename (pcOldName, pcNewName));
RenameExit:
	iserrno = errno;
	return (-1);
}

/*
 * Name:
 *	int	issetunique (int iHandle, off_t tUniqueID);
 * Arguments:
 *	int	iHandle
 *		The handle of the VBISAM file about to be set
 *	off_t	tUniqueID
 *		The *NEW* value to be set into the unique ID
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	OTHER	Failure (iserrno)
 * Problems:
 *	NONE known
 */
int
issetunique (int iHandle, off_t tUniqueID)
{
	int	iResult,
		iResult2;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);
	iResult = iVBUniqueIDSet (iHandle, tUniqueID);

	if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iResult = iVBTransSetUnique (iHandle, tUniqueID);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	iResult2 = iVBExit (iHandle);
	if (iResult)
		return (iResult);
	return (iResult2);
}

/*
 * Name:
 *	int	isuniqueid (int iHandle, off_t *ptUniqueID);
 * Arguments:
 *	int	iHandle
 *		The handle of the VBISAM file about to be set
 *	off_t	*ptUniqueID
 *		A pointer to the receiving location to be used
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success (and ptUniqueID is filled in)
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
isuniqueid (int iHandle, off_t *ptUniqueID)
{
	int	iResult;
	off_t	tValue;

	iResult = iVBEnter (iHandle, TRUE);
	if (iResult)
		return (iResult);

	tValue = tVBUniqueIDGetNext (iHandle);

	if (!iResult && iVBLogfileHandle != -1 && !(psVBFile [iHandle]->iOpenMode & ISNOLOG))
		iResult = iVBTransUniqueID (iHandle, tValue);
	psVBFile [iHandle]->sFlags.iIsDictLocked = 2;
	iResult = iVBExit (iHandle);
	*ptUniqueID = tValue;
	return (iResult);
}

/*
 * Name:
 *	int	isunlock (int iHandle)
 * Arguments:
 *	int	iHandle
 *		The handle of a currently open VBISAM file
 * Prerequisites:
 *	NONE
 * Returns:
 *	0	Success
 *	-1	Failure (iserrno has more info)
 * Problems:
 *	NONE known
 */
int
isunlock (int iHandle)
{
	return (iVBDataLock (iHandle, VBUNLOCK, 0, FALSE));
}

/*
 * Name:
 *	void	ldchar (char *pcSource, int iLength, char *pcDestination);
 * Arguments:
 *	char	*pcSource
 *		The source location
 *	int	iLength
 *		The number of characters
 *	char	*pcDestination
 *		The source location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
ldchar (char *pcSource, int iLength, char *pcDestination)
{
	char	*pcDst;

	memcpy ((void *) pcDestination, (void *) pcSource, iLength);
	for (pcDst = pcDestination + iLength - 1; pcDst >= (char *) pcDestination; pcDst--)
	{
		if (*pcDst != ' ')
		{
			pcDst++;
			*pcDst = 0;
			return;
		}
	}
	*(++pcDst) = 0;
	return;
}

/*
 * Name:
 *	void	stchar (char *pcSource, char *pcDestination, int iLength);
 * Arguments:
 *	char	*pcSource
 *		The source location
 *	char	*pcDestination
 *		The source location
 *	int	iLength
 *		The number of characters
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stchar (char *pcSource, char *pcDestination, int iLength)
{
	char	*pcSrc,
		*pcDst;
	int	iCount;

	pcSrc = pcSource;
	pcDst = pcDestination;
	for (iCount = iLength; iCount && *pcSrc; iCount--, pcSrc++, pcDst++)
		*pcDst = *pcSrc;
	for (; iCount; iCount--, pcDst++)
		*pcDst = ' ';
	return;
}

/*
 * Name:
 *	int	ldint (char *pcLocation);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
int
ldint (char *pcLocation)
{
	short	iValue = 0;
	char	*pcTemp = (char *) &iValue;

	*(pcTemp + 1) = *(pcLocation + 0);
	*(pcTemp + 0) = *(pcLocation + 1);
	return (iValue);
}

/*
 * Name:
 *	void	stint (int iValue, char *pcLocation);
 * Arguments:
 *	int	iValue
 *		The value to be stored
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stint (int iValue, char *pcLocation)
{
	char	*pcTemp = (char *) &iValue;

	*(pcLocation + 0) = *(pcTemp + 1);
	*(pcLocation + 1) = *(pcTemp + 0);
	return;
}

/*
 * Name:
 *	long	ldlong (char *pcLocation);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
long
ldlong (char *pcLocation)
{
	long	lValue;
	char	*pcTemp = (char *) &lValue;

	*(pcTemp + 3) = *(pcLocation + 0);
	*(pcTemp + 2) = *(pcLocation + 1);
	*(pcTemp + 1) = *(pcLocation + 2);
	*(pcTemp + 0) = *(pcLocation + 3);
	return (lValue);
}

/*
 * Name:
 *	void	stlong (long lValue, char *pcLocation);
 * Arguments:
 *	long	lValue
 *		The value to be stored
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stlong (long lValue, char *pcLocation)
{
	char	*pcTemp = (char *) &lValue;

	*(pcLocation + 0) = *(pcTemp + 3);
	*(pcLocation + 1) = *(pcTemp + 2);
	*(pcLocation + 2) = *(pcTemp + 1);
	*(pcLocation + 3) = *(pcTemp + 0);
	return;
}

/*
 * Name:
 *	off_t	ldquad (char *);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
off_t
ldquad (char *pcLocation)
{
	off_t	tValue;
	char	*pcTemp = (char *) &tValue;

#if	_FILE_OFFSET_BITS == 64
	*(pcTemp + 7) = *(pcLocation + 0);
	*(pcTemp + 6) = *(pcLocation + 1);
	*(pcTemp + 5) = *(pcLocation + 2);
	*(pcTemp + 4) = *(pcLocation + 3);
	*(pcTemp + 3) = *(pcLocation + 4);
	*(pcTemp + 2) = *(pcLocation + 5);
	*(pcTemp + 1) = *(pcLocation + 6);
	*(pcTemp + 0) = *(pcLocation + 7);
#else	// _FILE_OFFSET_BITS == 64
	*(pcTemp + 3) = *(pcLocation + 0);
	*(pcTemp + 2) = *(pcLocation + 1);
	*(pcTemp + 1) = *(pcLocation + 2);
	*(pcTemp + 0) = *(pcLocation + 3);
#endif	// _FILE_OFFSET_BITS == 64
	return (tValue);
}

/*
 * Name:
 *	void	stquad (off_t, char *);
 * Arguments:
 *	off_t	tValue
 *		The value to be stored
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stquad (off_t tValue, char *pcLocation)
{
	char	*pcTemp = (char *) &tValue;

#if	_FILE_OFFSET_BITS == 64
	*(pcLocation + 0) = *(pcTemp + 7);
	*(pcLocation + 1) = *(pcTemp + 6);
	*(pcLocation + 2) = *(pcTemp + 5);
	*(pcLocation + 3) = *(pcTemp + 4);
	*(pcLocation + 4) = *(pcTemp + 3);
	*(pcLocation + 5) = *(pcTemp + 2);
	*(pcLocation + 6) = *(pcTemp + 1);
	*(pcLocation + 7) = *(pcTemp + 0);
#else	// _FILE_OFFSET_BITS == 64
	*(pcLocation + 0) = *(pcTemp + 3);
	*(pcLocation + 1) = *(pcTemp + 2);
	*(pcLocation + 2) = *(pcTemp + 1);
	*(pcLocation + 3) = *(pcTemp + 0);
#endif	// _FILE_OFFSET_BITS == 64
	return;
}

/*
 * Name:
 *	double	ldfloat (char *pcLocation);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
double
ldfloat (char *pcLocation)
{
	float	fFloat;
	double	dDouble;

	memcpy (&fFloat, pcLocation, FLOATSIZE);
	dDouble = fFloat;
	return ((double) dDouble);
}

/*
 * Name:
 *	void	stfloat (double dSource, char *pcDestination);
 * Arguments:
 *	double	dSource
 *		Technically, it's a float that was promoted to a double by
 *		the compiler.
 *	char	*pcDestination
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stfloat (double dSource, char *pcDestination)
{
	float	fFloat;

	fFloat = dSource;
	memcpy (pcDestination, &fFloat, FLOATSIZE);
	return;
}

/*
 * Name:
 *	double	ldfltnull (char *pcLocation, short *piNullflag);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 *	short	*piNullflag
 *		Pointer to a null determining receiver
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
double
ldfltnull (char *pcLocation, short *piNullflag)
{
	double	dValue;

	*piNullflag = 0;
	dValue = ldfloat (pcLocation);
	return ((double) dValue);
}

/*
 * Name:
 *	void	stfltnull (double dSource, char *pcDestination, short iNullflag);
 * Arguments:
 *	double	dSource
 *		The double (promotoed from float) value to store
 *	char	*pcDestination
 *		The holding location
 *	short	iNullflag
 *		Null determinator
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stfltnull (double dSource, char *pcDestination, short iNullflag)
{
	stfloat (dSource, pcDestination);
	return;
}

/*
 * Name:
 *	double	lddbl (char *pcLocation);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
double
lddbl (char *pcLocation)
{
	double	dDouble;

	memcpy (&dDouble, pcLocation, DOUBLESIZE);
	return (dDouble);
}

/*
 * Name:
 *	void	stdbl (double dSource, char *pcDestination);
 * Arguments:
 *	double	dSource
 *		The (double) value to be stored
 *	char	*pcDestination
 *		The holding location
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stdbl (double dSource, char *pcDestination)
{
	memcpy (pcDestination, &dSource, DOUBLESIZE);
	return;
}

/*
 * Name:
 *	double	lddblnull (char *pcLocation, short *piNullflag);
 * Arguments:
 *	char	*pcLocation
 *		The holding location
 *	short	*piNullflag
 *		Pointer to a null determining receiver
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
double
lddblnull (char *pcLocation, short *piNullflag)
{
	*piNullflag = 0;
	return (lddbl (pcLocation));
}

/*
 * Name:
 *	void	stdblnull (double dSource, char *pcDestination, short iNullflag);
 * Arguments:
 *	double	dSource
 *		The double value to store
 *	char	*pcDestination
 *		The holding location
 *	short	iNullflag
 *		Null determinator
 * Prerequisites:
 *	NONE
 * Returns:
 *	NONE
 * Problems:
 *	NONE known
 */
void
stdblnull (double dSource, char *pcDestination, short iNullflag)
{
	stdbl (dSource, pcDestination);
}
