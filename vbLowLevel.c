/*
 * Title:	vbLowLevel.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	18Nov2003
 * Description:
 *	This module handles ALL the low level file I/O operations for the
 *	VBISAM library.  By encapsulating all of these functions into wrappers
 *	within this module, it becomes easier to 'virtualize' the filesystem
 *	at a later date.
 * Version:
 *	$Id: vbLowLevel.c,v 1.6 2004/06/06 20:52:21 trev_vb Exp $
 * Modification History:
 *	$Log: vbLowLevel.c,v $
 *	Revision 1.6  2004/06/06 20:52:21  trev_vb
 *	06Jun2004 TvB Lots of changes! Performance, stability, bugfixes.  See CHANGELOG
 *	
 *	Revision 1.5  2004/03/23 15:13:19  trev_vb
 *	TvB 23Mar2004 Changes made to fix bugs highlighted by JvN's test suite.  Many thanks go out to JvN for highlighting my obvious mistakes.
 *	
 *	Revision 1.4  2004/01/06 14:31:59  trev_vb
 *	TvB 06Jan2004 Added in VARLEN processing (In a fairly unstable sorta way)
 *	
 *	Revision 1.3  2004/01/05 07:36:17  trev_vb
 *	TvB 05Feb2002 Added licensing et al as Johann v. N. noted I'd overlooked it
 *	
 *	Revision 1.2  2003/12/22 04:49:04  trev_vb
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
int	iVBOpen (char *, int, mode_t);
int	iVBClose (int);
off_t	tVBLseek (int, off_t, int);
ssize_t	tVBRead (int, void *, size_t);
ssize_t	tVBWrite (int, void *, size_t);
int	iVBLock (int, off_t, off_t, int);
int	iVBLink (char *, char *);
int	iVBUnlink (char *);
int	iVBAccess (char *, int);

/*
 * Name:
 *	int	iVBOpen (char *pcFilename, int iFlags, mode_t tMode);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be opened
 *	int	iFlags
 *		As per the standard open () system call
 *	mode_t	tMode
 *		As per the standard open () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The handle to be used for accessing this file
 * Problems:
 *	NONE known
 */
int
iVBOpen (char *pcFilename, int iFlags, mode_t tMode)
{
	return (open (pcFilename, iFlags, tMode));
}

/*
 * Name:
 *	int	iVBClose (int iHandle);
 * Arguments:
 *	int	iHandle
 *		As per the standard close () system call
 * Prerequisites:
 *	Well, let's *HOPE* that iHandle is actually open!
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The close succeeded
 * Problems:
 *	NONE known
 */
int
iVBClose (int iHandle)
{
	return (close (iHandle));
}

/*
 * Name:
 *	off_t	tVBLseek (int iHandle, off_t tOffset, int iWhence);
 * Arguments:
 *	int	iHandle
 *		As per the standard lseek () system call
 *	off_t	tOffset
 *		As per the standard lseek () system call
 *	int	iWhence
 *		As per the standard lseek () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	(off_t) -1
 *		An error occurred.  errno contains the reason
 *	Other	The new offset within the file in question
 * Problems:
 *	NONE known
 */
off_t
tVBLseek (int iHandle, off_t tOffset, int iWhence)
{
	return (lseek (iHandle, tOffset, iWhence));
}

/*
 * Name:
 *	ssize_t	tVBRead (int iHandle, void *pvBuffer, size_t tCount);
 * Arguments:
 *	int	iHandle
 *		As per the standard read () system call
 *	void	*pvBuffer
 *		As per the standard read () system call
 *	size_t	tCount
 *		As per the standard read () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The actual number of bytes read
 * Problems:
 *	NONE known
 */
ssize_t
tVBRead (int iHandle, void *pvBuffer, size_t tCount)
{
	return (read (iHandle, pvBuffer, tCount));
}

/*
 * Name:
 *	ssize_t	tVBWrite (int iHandle, void *pvBuffer, size_t tCount);
 * Arguments:
 *	int	iHandle
 *		As per the standard write () system call
 *	void	*pvBuffer
 *		As per the standard write () system call
 *	size_t	tCount
 *		As per the standard write () system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	Other	The actual number of bytes written
 * Problems:
 *	NONE known
 */
ssize_t
tVBWrite (int iHandle, void *pvBuffer, size_t tCount)
{
	return (write (iHandle, pvBuffer, tCount));
}

/*
 * Name:
 *	int	iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode);
 * Arguments:
 *	int	iHandle
 *		The handle of an open file
 *	off_t	tOffset
 *		The start address of the lock being placed
 *	off_t	tLength
 *		The length (in bytes) to lock
 *	int	iMode
 *		VBUNLOCK - Unlock the region
 *		VBRDLOCK - If already locked, error else read lock the region
 *		VBRDLCKW - As above but use a blocking lock
 *		VBRDLOCK - If already locked, error else write lock the region
 *		VBRDLCKW - As above but use a blocking lock
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The lock was successfully established
 * Problems:
 *	NONE known
 */
int
iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode)
{
	int	iCommand;
	struct	flock
		sFlock;

	switch (iMode)
	{
	case	VBUNLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_UNLCK;
		break;

	case	VBRDLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_RDLCK;
		break;

	case	VBRDLCKW:
		iCommand = F_SETLKW;
		sFlock.l_type = F_RDLCK;
		break;

	case	VBWRLOCK:
		iCommand = F_SETLK;
		sFlock.l_type = F_WRLCK;
		break;

	case	VBWRLCKW:
		iCommand = F_SETLKW;
		sFlock.l_type = F_WRLCK;
		break;

	default:
		errno = EBADARG;
		return (-1);
	}
	sFlock.l_whence = SEEK_SET;
	sFlock.l_start = tOffset;
	sFlock.l_len = tLength;
	sFlock.l_pid = 0;
	if (fcntl (iHandle, iCommand, &sFlock))
		return (-1);
	return (0);
}

/*
 * Name:
 *	int	iVBLink (char *pcOldFilename, char *pcNewFilename);
 * Arguments:
 *	char	*pcOldFilename
 *		The null terminated old filename to be linked
 *	char	*pcNewFilename
 *		The null terminated new filename to be linked
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The link succeeded
 * Problems:
 *	NONE known
 */
int
iVBLink (char *pcOldFilename, char *pcNewFilename)
{
	return (link (pcOldFilename, pcNewFilename));
}

/*
 * Name:
 *	int	iVBUnlink (char *pcFilename);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be erased
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The unlink succeeded
 * Problems:
 *	NONE known
 */
int
iVBUnlink (char *pcFilename)
{
	return (unlink (pcFilename));
}

/*
 * Name:
 *	int	iVBAccess (char *pcFilename, int iMode);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be tested
 *	int	iMode
 *		See access(2) system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The requested access is granted
 * Problems:
 *	NONE known
 */
int
iVBAccess (char *pcFilename, int iMode)
{
	return (access (pcFilename, iMode));
}
