/*
 * Title:	istrans.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	11Dec2003
 * Description:
 *	This is the module that deals with all the transaction processing for
 *	a file in the VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: istrans.c,v $
 *	Revision 1.1  2003/12/20 20:11:18  trev_vb
 *	Initial revision
 *	
 */
#include	"isinternal.h"

/*
 * Prototypes
 */
int	isaudit (int, char *, int);
int	isbegin (void);
int	iscommit (void);
int	islogclose (void);
int	islogopen (char *);
int	isrecover (void);
int	isrollback (void);
