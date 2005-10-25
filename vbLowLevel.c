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
 *	$Id: vbLowLevel.c,v 1.12 2005/10/25 14:01:43 zbenjamin Exp $
 * Modification History:
 *	$Log: vbLowLevel.c,v $
 *	Revision 1.12  2005/10/25 14:01:43  zbenjamin
 *	Fix for WIN32 Support
 *	
 *
 *      Revision 1.12  2005/10/25 15:59   zbenjamin
 *      25Oct2005  zBenjamin OOps forgot a final #endif 
 *  	
 *      Revision 1.11  2005/10/25 13:56:06  zbenjamin
 *	Added WIN32 Support
 *	   
 *
 *      Revision 1.11  2005/10/25 14:54   zbenjamin
 *      25Oct2005  zBenjamin Added Win32 Compatibility
 *
 *	Revision 1.10  2004/06/22 09:54:27  trev_vb
 *	22June2004 TvB Ooops, I put some code in iVBLock BEFORE the var declarations
 *	
 *	Revision 1.9  2004/06/13 07:52:17  trev_vb
 *	TvB 13June2004
 *	Implemented sharing of open files.
 *	Changed the locking strategy slightly to allow table-level locking granularity
 *	(i.e. A process opening the same table more than once can now lock itself!)
 *	
 *	Revision 1.8  2004/06/13 06:32:33  trev_vb
 *	TvB 12June2004 See CHANGELOG 1.03 (Too lazy to enumerate)
 *	
 *	Revision 1.7  2004/06/11 22:16:16  trev_vb
 *	11Jun2004 TvB As always, see the CHANGELOG for details. This is an interim
 *	checkin that will not be immediately made into a release.
 *	
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

static	int	iInitialized = FALSE;

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
int	iVBStat (char *, struct stat *);

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
	int	iLoop;
	struct	stat
		sStat;
	
        #ifdef WIN32
	iFlags = iFlags | O_BINARY;
        #endif

	if (!iInitialized)
	{
		iInitialized = TRUE;
		for (iLoop = 0; iLoop < VB_MAX_FILES * 3; iLoop++)
			sVBFile [iLoop].iRefCount = 0;
	}
	if (iVBStat (pcFilename, &sStat))
	{
		if (!iFlags & O_CREAT)
			return (-1);
	}
	#ifndef WIN32   //FIXME impelement this for Windows (there are no inodes on a windows system)
	else
	{
		for (iLoop = 0; iLoop < VB_MAX_FILES * 3; iLoop++)
		{
			if (sVBFile [iLoop].iRefCount && sVBFile [iLoop].tDevice == sStat.st_dev && sVBFile [iLoop].tInode == sStat.st_ino)
			{
				sVBFile [iLoop].iRefCount++;
				return (iLoop);
			}
		}
	}
	#endif
	for (iLoop = 0; iLoop < VB_MAX_FILES * 3; iLoop++)
	{
		if (sVBFile [iLoop].iRefCount == 0)
		{
                        #ifndef WIN32
                        sVBFile [iLoop].iHandle = open (pcFilename, iFlags, tMode);
                        #else
                        sVBFile [iLoop].iHandle = _open(pcFilename, iFlags, tMode);
                        #endif
			if (sVBFile [iLoop].iHandle == -1)
				break;
			if (iFlags & O_CREAT && iVBStat (pcFilename, &sStat))
				return (1);
			sVBFile [iLoop].tDevice = sStat.st_dev;
			sVBFile [iLoop].tInode = sStat.st_ino;
			sVBFile [iLoop].tPosn = 0;
			sVBFile [iLoop].iRefCount++;
			return (iLoop);
		}
	}
	errno = ENOENT;
	return (-1);
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
	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}
	sVBFile [iHandle].iRefCount--;
	if (!sVBFile [iHandle].iRefCount)
	        #ifndef WIN32
		return (close (sVBFile [iHandle].iHandle));
		#else
		return (_close (sVBFile [iHandle].iHandle));
		#endif
	return (0);
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
	off_t	tNewOffset;

	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}
	if (sVBFile [iHandle].tPosn == tOffset && iWhence == SEEK_SET)
		return (tOffset);	// Already there!
        #ifndef WIN32
	tNewOffset = lseek (sVBFile [iHandle].iHandle, tOffset, iWhence);
        #else
	tNewOffset = _lseek (sVBFile [iHandle].iHandle, tOffset, iWhence);
        #endif
	if (tNewOffset == tOffset && iWhence == SEEK_SET)
		sVBFile [iHandle].tPosn = tNewOffset;
	else
		sVBFile [iHandle].tPosn = -1;
	return (tNewOffset);
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
	ssize_t	tByteCount;

	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}
        #ifndef WIN32
	tByteCount = read (sVBFile [iHandle].iHandle, pvBuffer, tCount);
        #else
	tByteCount = _read (sVBFile [iHandle].iHandle, pvBuffer, tCount);
        #endif
	if (tByteCount == tCount)
		sVBFile [iHandle].tPosn += tByteCount;
	else
		sVBFile [iHandle].tPosn = -1;
	return (tByteCount);
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
	ssize_t	tByteCount;

	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}
        #ifndef WIN32
	tByteCount = write (sVBFile [iHandle].iHandle, pvBuffer, tCount);
        #else
	tByteCount = _write (sVBFile [iHandle].iHandle, pvBuffer, tCount);
        #endif
	if (tByteCount == tCount)
		sVBFile [iHandle].tPosn += tByteCount;
	else
		sVBFile [iHandle].tPosn = -1;
	return (tByteCount);
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
#ifndef WIN32
int
iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode)
{
	int	iCommand,
		iType,
		iResult = -1;
	struct	flock
		sFlock;

	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}
	switch (iMode)
	{
	case	VBUNLOCK:
		iCommand = F_SETLK;
		iType = F_UNLCK;
		break;

	case	VBRDLOCK:
		iCommand = F_SETLK;
		iType = F_RDLCK;
		break;

	case	VBRDLCKW:
		iCommand = F_SETLKW;
		iType = F_RDLCK;
		break;

	case	VBWRLOCK:
		iCommand = F_SETLK;
		iType = F_WRLCK;
		break;

	case	VBWRLCKW:
		iCommand = F_SETLKW;
		iType = F_WRLCK;
		break;

	default:
		errno = EBADARG;
		return (-1);
	}
	errno = EINTR;
	while (iResult && errno == EINTR)	// Just in case we're signalled
	{
		sFlock.l_type = iType;
		sFlock.l_whence = SEEK_SET;
		sFlock.l_start = tOffset;
		sFlock.l_len = tLength;
		sFlock.l_pid = 0;
		iResult = fcntl (sVBFile [iHandle].iHandle, iCommand, &sFlock);
	}
	return (iResult);
}
#else
/**
 * Implements the iVBLock functionality for WIN32
 *
 * Problems:
 *      Better Error Handling should be implemented 
 *      when FileLockEx or UnlockFileEx fails             
 */
int
iVBLock (int iHandle, off_t tOffset, off_t tLength, int iMode)
{
	HANDLE tW32FileHandle;
	OVERLAPPED tOverlapped;
	DWORD tFlags;
	BOOL bUnlock = FALSE;

	if (!sVBFile [iHandle].iRefCount)
	{
		errno = ENOENT;
		return (-1);
	}

	
	tW32FileHandle = (HANDLE)_get_osfhandle(sVBFile [iHandle].iHandle);
        if(tW32FileHandle == INVALID_HANDLE_VALUE)
	{
		errno = ENOENT;
		return (-1);
	}

	switch (iMode)
	{
	case	VBUNLOCK:
		bUnlock = TRUE;
		break;

	case	VBRDLOCK:
		tFlags = LOCKFILE_FAIL_IMMEDIATELY;
		break;

	case	VBRDLCKW:
		tFlags = 0;
		break;

	case	VBWRLOCK:
		tFlags = LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY;
		break;

	case	VBWRLCKW:
		tFlags = LOCKFILE_EXCLUSIVE_LOCK;
		break;

	default:
		errno = EBADARG;
		return (-1);
	}

	memset(&tOverlapped, '\0', sizeof(OVERLAPPED));
	tOverlapped.Offset = tOffset;
	if(!bUnlock)
	{
		/*FIXME insert error handling*/
		if(LockFileEx(tW32FileHandle,tFlags, 0, tLength, 0, &tOverlapped))
			return 0;
	}
	else
	{
		/*FIXME insert error handling*/
		if(UnlockFileEx(tW32FileHandle, 0, tLength, 0, &tOverlapped))
			return 0;
	}

	errno = EBADARG;
	return (-1);
}
#endif


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
        #ifndef WIN32
        return (link (pcOldFilename, pcNewFilename));
        #else
	return(rename(pcOldFilename,pcNewFilename));
        #endif
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
        #ifndef WIN32
	return (unlink (pcFilename));
        #else
	return (_unlink (pcFilename));
        #endif
}

/*
 * Name:
 *	int	iVBStat (char *pcFilename, struct stat *psStat);
 * Arguments:
 *	char	*pcFilename
 *		The null terminated filename to be tested
 *	struct	stat	*psStat
 *		See stat(2) system call
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	An error occurred.  errno contains the reason
 *	0	The requested access is granted
 * Problems:
 *	NONE known
 */
int
iVBStat (char *pcFilename, struct stat *psStat)
{
	return (stat (pcFilename, psStat));
}


#ifdef WIN32
/*
 * Name:
 *      int     getuid()
 * simulates getuid for windows
 */
int getuid()
{
	return (1);
}
#endif
