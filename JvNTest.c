/*
 * Title:	JvNTest.c
 * Copyright:	(C) 2003 Trevor van Bremen
 * License:	LGPL - See COPYING.LIB
 * Author:	Trevor van Bremen
 * Created:	17Nov2003
 * Description:
 *	This is a simple stub test program that emulates Johann vN's test suite.
 *	It tries to eliminate any other 'clutter' from confusing me. :)
 *	It is dependant on the JvN supplied test data file that I have renamed
 *	to 'src'.
 * Version:
 *	$Id: JvNTest.c,v 1.1 2004/03/23 15:13:19 trev_vb Exp $
 * Modification History:
 *	$Log: JvNTest.c,v $
 *	Revision 1.1  2004/03/23 15:13:19  trev_vb
 *	TvB 23Mar2004 Changes made to fix bugs highlighted by JvN's test suite.  Many thanks go out to JvN for highlighting my obvious mistakes.
 *	
 */
#include	<stdio.h>
#include	"vbisam.h"

#define	FILE_COUNT	10
#define	INDX_COUNT	10

struct	keydesc
	gsKey [10] =
	{
		{ISNODUPS,	1,  0,  8, 0},
		{ISDUPS,	1,  8, 16, 0},
		{ISDUPS,	1, 24,  8, 0},
		{ISDUPS,	1, 40,  2, 0},
		{ISDUPS,	1, 42, 14, 0},
		{ISDUPS,	1, 56, 18, 0},
		{ISDUPS,	3, 42, 14, 0, 56, 18, 0, 74,  2, 0},
		{ISDUPS,	2, 40,  2, 0, 42, 14, 0},
		{ISDUPS,	2, 56, 18, 0,  8, 16, 0},
		{ISDUPS,	2,118, 10, 0,  8, 16, 0}
	};

main ()
{
	char	cBuffer [1024],
		cName [32];
	int	iHandle [10],
		iLoop,
		iLoop1,
		iResult;
	FILE	*psHandle;

	for (iLoop = 0; iLoop < INDX_COUNT; iLoop++)
	{
		gsKey [iLoop].k_len = 0;
		for (iLoop1 = 0; iLoop1 < gsKey [iLoop].k_nparts; iLoop1++)
			gsKey [iLoop].k_len += gsKey [iLoop].k_part [iLoop1].kp_leng;
	}
	for (iLoop = 0; iLoop < FILE_COUNT; iLoop++)
	{
		sprintf (cName, "File%d", iLoop);
		iserase (cName);
		iHandle [iLoop] = isbuild (cName, 170, &gsKey [0], ISINOUT + ISEXCLLOCK);
		if (iHandle [iLoop] < 0)
		{
			printf ("isbuild error %d for %s file\n", iserrno, cName);
			exit (1);
		}
	}
	for (iLoop = 0; iLoop < FILE_COUNT; iLoop++)
	{
		for (iLoop1 = 1; iLoop1 < INDX_COUNT; iLoop1++)
		{
			iResult = isaddindex (iHandle [iLoop], &gsKey [iLoop1]);
			if (iResult)
			{
				printf ("isaddindex error %d on handle %d index %d\n", iserrno, iLoop, iLoop1);
				exit (1);
			}
		}
	}
	psHandle = fopen ("src", "r");
	if (psHandle == (FILE *) 0)
	{
		printf ("Error opening source file!\n");
		exit (1);
	}
	iLoop1 = 0;
	while (fgets (cBuffer, 1024, psHandle) != NULL)
	{
		iLoop1++;
		cBuffer [170] = 0;
		for (iLoop = 0; iLoop < FILE_COUNT; iLoop++)
			if (iswrite (iHandle [iLoop], cBuffer))
			{
				printf ("Error %d writing row %d to file %d\n", iserrno, iLoop1, iLoop);
				exit (1);
			}
	}
	fclose (psHandle);
	for (iLoop = 0; iLoop < FILE_COUNT; iLoop++)
		isclose (iHandle [iLoop]);
	return (0);
}
