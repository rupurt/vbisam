/*
 * Title:	isaudit.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the auditting component within
 *	the VBISAM library.
 * Version:
 *	$Id: isaudit.c,v 1.1 2004/01/03 02:28:48 trev_vb Exp $
 * Modification History:
 *	$Log: isaudit.c,v $
 *	Revision 1.1  2004/01/03 02:28:48  trev_vb
 *	TvB 02Jan2004 WAY too many changes to enumerate!
 *	TvB 02Jan2003 Transaction processing done (excluding iscluster)
 *	
 */
#include	"isinternal.h"
#include	<time.h>

/*
 * Prototypes
 */
int	isaudit (int, char *, int);

/*
 * Name:
 *	int	isaudit (int iHandle, char *pcFilename, int iMode);
 * Arguments:
 *	int	iHandle
 *		The open file descriptor of the VBISAM file (Not the dat file)
 *	char	*pcFilename
 *		The name of the audit file
 *	int	iMode
 *		AUDSETNAME
 *		AUDGETNAME
 *		AUDSTART
 *		AUDSTOP
 *		AUDINFO
 *		AUDRECVR	BUG? Huh?
 * Prerequisites:
 *	NONE
 * Returns:
 *	-1	Failure (iserrno contains more info)
 *	0	Success
 * Problems:
 *	NONE known
 */
int	isaudit (int iHandle, char *pcFilename, int iMode)
{
	// BUG - Write isaudit
	return (0);
}
