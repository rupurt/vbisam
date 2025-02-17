/*
 * Title:	MVTest.c
 * Copyright:	(C) 2004 Mikhail Verkhovski
 * License:	LGPL - See COPYING.LIB
 * Author:	Mikhail Verkhovski
 * Created:	??May2004
 * Description:
 *	This module tests a bunch of the features of VBISAM
 * Version:
 *	$Id: MVTest.c,v 1.5 2004/06/22 09:36:36 trev_vb Exp $
 * Modification History:
 *	$Log: MVTest.c,v $
 *	Revision 1.5  2004/06/22 09:36:36  trev_vb
 *	22June2004 TvB Added some 'nicer' output counters and stuff
 *	
 *	Revision 1.4  2004/06/16 10:53:55  trev_vb
 *	16June2004 TvB With about 150 lines of CHANGELOG entries, I am NOT gonna repeat
 *	16June2004 TvB them all HERE!  Go look yaself at the 1.03 CHANGELOG
 *	
 *	Revision 1.3  2004/06/13 06:32:33  trev_vb
 *	TvB 12June2004 See CHANGELOG 1.03 (Too lazy to enumerate)
 *	
 *	Revision 1.2  2004/06/11 22:16:16  trev_vb
 *	11Jun2004 TvB As always, see the CHANGELOG for details. This is an interim
 *	checkin that will not be immediately made into a release.
 *	
 *	Revision 1.1  2004/06/06 20:52:21  trev_vb
 *	06Jun2004 TvB Lots of changes! Performance, stability, bugfixes.  See CHANGELOG
 *	
 *	TvB 30May2004 Many thanks go out to MV for writing this test program
 */
#include	<time.h>
#include	<sys/types.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	"vbisam.h"
#include	"isinternal.h"

int	iVBRdCount = 0,
	iVBRdCommit = 0,
	iVBRdTotal = 0,
	iVBWrCount = 0,
	iVBWrCommit = 0,
	iVBWrTotal = 0,
	iVBDlCount = 0,
	iVBDlCommit = 0,
	iVBDlTotal = 0,
	iVBUpCount = 0,
	iVBUpCommit = 0,
	iVBUpTotal = 0;
int
main (int iArgc, char **ppcArgv)
{
	int	iResult,
		iLoop,
		iLoop2,
		iLoop3,
		iHandle;
	unsigned char
		cRecord [256];
	struct	keydesc
		sKeydesc;
	char	cLogfileName [100],
		cCommand [100];
	char	cFileName [] = "IsamTest";

	memset (&sKeydesc, 0, sizeof (sKeydesc));
	sKeydesc.k_flags = COMPRESS;
	sKeydesc.k_nparts = 1;
	sKeydesc.k_start = 0;
	sKeydesc.k_leng = 2;
	sKeydesc.k_type = CHARTYPE;

	if (iArgc == 1)
	{
		printf ("Usage:\n\t%s create\nOR\n\t%s <#iterations>\n", ppcArgv [0], ppcArgv [0]);
		exit (1);
	}

	if (iArgc > 1 && strcmp (ppcArgv [1], "create") == 0)
	{
		iserase (cFileName);
		iHandle = isbuild (cFileName, 255, &sKeydesc, ISINOUT+ISFIXLEN+ISEXCLLOCK);
		if (iHandle < 0)
		{
			printf ("Error creating database: %d\n", iserrno);
			exit (-1);
		}
		sKeydesc.k_flags |= ISDUPS;
		//for (sKeydesc.k_start = 1; sKeydesc.k_start < MAXSUBS; sKeydesc.k_start++)
		for (sKeydesc.k_start = 1; sKeydesc.k_start < 2; sKeydesc.k_start++)
			if (isaddindex (iHandle, &sKeydesc))
				printf ("Error %d adding index %d\n", iserrno, sKeydesc.k_start);
		isclose (iHandle);
		sprintf (cLogfileName, "RECOVER");
		sprintf (cCommand, "rm -f %s; touch %s", cLogfileName, cLogfileName);
		system (cCommand);
		return (0);
	}
	// The following is sort of cheating as it *assumes* we're running *nix
	// However, I have to admit to liking the fact that this will FAIL when
	// using WynDoze (TvB)
	sprintf (cLogfileName, "RECOVER");
	iResult = islogopen (cLogfileName);
	if (iResult < 0)
	{
		printf ("Error opening log: %d\n", iserrno);
		exit (-1);
	}

	srand (time (NULL));
	for (iLoop = 0; iLoop < atoi (ppcArgv [1]); iLoop++)
	{
		if (!(iLoop % 100))
			printf ("iLoop=%d\n", iLoop);

		iVBDlCount = 0;
		iVBRdCount = 0;
		iVBUpCount = 0;
		iVBWrCount = 0;
		iResult = isbegin ();
		if (iResult < 0)
		{
			printf ("Error begin transaction: %d\n", iserrno);
			exit (-1);
		}
		iHandle = isopen (cFileName, ISINOUT+ISFIXLEN+ISTRANS+ISAUTOLOCK);
		if (iHandle < 0)
		{
			printf ("Error opening database: %d\n", iserrno);
			exit (-1);
		}

		for (iLoop2 = 0; iLoop2 < 100; iLoop2++)
		{
			for (iLoop3 = 0; iLoop3 < 256; iLoop3++)
				cRecord [iLoop3] = rand () % 256;

			switch (rand () % 4)
			{
			case	0:
				if ((iResult = iswrite (iHandle, (char *) cRecord)) != 0)
				{
					if (iserrno != EDUPL && iserrno != ELOCKED)
					{
						printf ("Error writing: %d\n", iserrno);
						goto err;
					}
				}
				else
					iVBWrCount++;
				break;

			case	1:
				if ((iResult = isread (iHandle, (char *)cRecord, ISEQUAL)) != 0)
				{
					if (iserrno == ELOCKED)
						; //printf ("Locked during deletion\n");
					else if (iserrno != ENOREC)
					{
						printf ("Error reading: %d\n", iserrno);
						goto err;
					}
				}
				else
					iVBRdCount++;
				break;

			case	2:
				for (iLoop3 = 0; iLoop3 < 256; iLoop3++)
					cRecord [iLoop3] = rand () % 256;
				if ((iResult = isrewrite (iHandle, (char *)cRecord)) != 0)
				{
					if (iserrno == ELOCKED)
						; //printf ("Locked during rewrite\n");
					else if (iserrno != ENOREC)
					{
						printf ("Error rewriting: %d\n", iserrno);
						goto err;
					}
				}
				else
					iVBUpCount++;
				break;

			case	3:
				if ((iResult = isdelete (iHandle, (char *)cRecord)) != 0)
				{
					if (iserrno == ELOCKED)
						; //printf ("Locked during deletion\n");
					else if (iserrno != ENOREC)
					{
						printf ("Error deleting: %d\n", iserrno);
						goto err;
					}
				}
				else
					iVBDlCount++;
				break;
			}
		}

		iResult = isflush (iHandle);
		if (iResult < 0)
		{
			printf ("Error flush: %d\n", iserrno);
			exit (-1);
		}
		iResult = isclose (iHandle);
		if (iResult < 0)
		{
			printf ("Error closing database: %d\n", iserrno);
			exit (-1);
		}

		iVBDlTotal += iVBDlCount;
		iVBRdTotal += iVBRdCount;
		iVBUpTotal += iVBUpCount;
		iVBWrTotal += iVBWrCount;
		switch (rand () % 2)
		{
		case	0:
			iVBDlCommit += iVBDlCount;
			iVBRdCommit += iVBRdCount;
			iVBUpCommit += iVBUpCount;
			iVBWrCommit += iVBWrCount;
			iResult = iscommit ();
			if (iResult < 0)
			{
				printf ("Error commit: %d\n", iserrno);
				exit (-1);
			}
			break;

		case	1:
			iResult = isrollback ();
			if (iResult < 0)
			{
				if (iserrno == EDUPL || iserrno == ENOREC)
					printf ("Same BUG (%d) as in C-ISAM!\n", iserrno);
				else
				{
					printf ("Error rollback: %d\n", iserrno);
					exit (-1);
				}
			}
			break;
		}
	}
err:
	printf ("                 Total Commited\n");
	printf ("              -------- --------\n");
	printf ("Delete Count: %8d %8d\n", iVBDlTotal, iVBDlCommit);
	printf ("Read   Count: %8d %8d\n", iVBRdTotal, iVBRdCommit);
	printf ("Update Count: %8d %8d\n", iVBUpTotal, iVBUpCommit);
	printf ("Write  Count: %8d %8d\n", iVBWrTotal, iVBWrCommit);
	printf ("              -------- --------\n");
	printf ("OPS OVERALL : %8d %8d\n", (iVBDlTotal + iVBRdTotal + iVBUpTotal + iVBWrTotal), (iVBDlCommit + iVBRdCommit + iVBUpCommit + iVBWrCommit));
	printf ("                       ========\n");
	printf ("ROWS ADDED THIS RUN:   %8d\n", (iVBWrCommit - iVBDlCommit));
	printf ("                       ========\n");
	return (iResult);
}
