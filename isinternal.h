/*
 * Title:	isinternal.h
 * Copyright:	(C) 2003 Trevor van Bremen
 * Author:	Trevor van Bremen
 * Created:	17Nov2003
 * Description:
 *	This is the header that defines the internally used structures for the
 *	VBISAM library.
 * Version:
 *	$ID$
 * Modification History:
 *	$Log: isinternal.h,v $
 *	Revision 1.1  2003/12/20 20:11:19  trev_vb
 *	Initial revision
 *	
 */
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<values.h>
#define	VBISAM_LIB
#include	"vbisam.h"

#ifdef	FALSE
#undef	FALSE
#endif
#define	FALSE	(0)

#ifdef	TRUE
#undef	TRUE
#endif
#define	TRUE	((!FALSE))

// Implementation limits
// DEV versions have a fixed maximum node length of 256 bytes
// 64-bit versions have a maximum node length of 4096 bytes
// 32-bit versions have a maximum node length of 1024 bytes
#ifdef	DEV
#define	MAX_NODE_LENGTH	256
#else	// DEV
# if	_FILE_OFFSET_BITS == 64
#  define	MAX_NODE_LENGTH	4096
# else	_// FILE_OFFSET_BITS == 64
#  define	MAX_NODE_LENGTH	1024
# endif	// _FILE_OFFSET_BITS == 64
#endif	// DEV
#define	MAX_KEYS_PER_NODE	((MAX_NODE_LENGTH - (4 + (INTSIZE * 2) + QUADSIZE)) / ((INTSIZE * 2) + 2 + (QUADSIZE * 2)))
#define	MAX_PATH_LENGTH	128

// Arguments to iVBLock
#define	VBUNLOCK	0
#define	VBRDLOCK	1
#define	VBRDLCKW	2
#define	VBWRLOCK	3
#define	VBWRLCKW	4

#ifdef	VBISAMMAIN
#define	EXTERN
	int	iVBMaxUsedHandle = -1;	// The highest opened file handle
	int	iRowBufferLength = 0;
	char	*pcRowBuffer = (char *) 0;// A common area for a data row
#else	// VBISAMMAIN
#define	EXTERN	extern
extern	int	iVBMaxUsedHandle;
extern	int	iRowBufferLength;
extern	char	*pcRowBuffer;	// A common area for a data row
#endif	// VBISAMMAIN

EXTERN	int	iserrno,	// Value of error is returned here
		iserrio;	// Contains value of last function called
EXTERN	off_t	isreclen,	// Used for varlen tables
		isrecnum;	// Current row number

struct	VBLOCK
{
	struct	VBLOCK
		*psNext;
	int	iIsTransaction;	// If TRUE, it's a transaction modification lock
	off_t	tRowNumber;
};
#define	VBLOCK_NULL	((struct VBLOCK *) 0)

struct	VBKEY
{
	struct	VBKEY
		*psNext,		// Next key in this node
		*psPrev;		// Previous key in this node
	struct	VBTREE
		*psParent,		// Pointer towards ROOT
		*psChild;		// Pointer towards LEAF
	off_t	tRowNode,		// The row / node number
		tDupNumber;		// The duplicate number (1st = 0)
	struct
	{
		 unsigned int
			iIsNew:1,	// If this is a new entry (split use)
			iIsHigh:1,	// Is this a GREATER THAN key?
			iIsDummy:1,	// A simple end of node marker
			iRFU:29;	// Reserved
	} sFlags;
	char	cKey [1];		// Placeholder for the key itself
};
#define	VBKEY_NULL	((struct VBKEY *) 0)

struct	VBTREE
{
	struct	VBTREE
		*psNext,		// Used for the free list only!
		*psParent;		// The next level up from this node
	struct	VBKEY
		*psKeyFirst,		// Pointer to the FIRST key in this node
		*psKeyLast,		// Pointer to the LAST key in this node
		*psKeyCurr;		// Pointer to the CURRENT key
	off_t	tNodeNumber,		// The actual node
		tTransNumber;		// Transaction number stamp
	struct
	{
		unsigned int
			iLevel:16,	// The level number (0 = LEAF)
			iIsRoot:2,	// 1 = This is the ROOT node
			iIsTOF:2,	// 1 = First entry in index
			iIsEOF:2,	// 1 = Last entry in index
			iRFU:10;	// Reserved for Future Use
	} sFlags;
};
#define	VBTREE_NULL	((struct VBTREE *) 0)

struct	DICTNODE			// Offset	32Val	64Val
{					// 32IO	64IO
	char	cValidation [2],	// 0x00	0x00	0xfe53	0x5642
		cHeaderRsvd,		// 0x02 0x02	0x02	Same
		cFooterRsvd,		// 0x03	0x03	0x02	Same
		cRsvdPerKey,		// 0x04	0x04	0x04	0x08
		cRFU1,			// 0x05 0x05	0x04	Same
		cNodeSize [INTSIZE],	// 0x06	0x06	0x03ff	0x0fff
		cIndexCount [INTSIZE],	// 0x08	0x08	Varies	Same
		cRFU2 [2],		// 0x0a	0x0a	0x0704	Same
		cFileVersion,		// 0x0c	0x0c	0x00	Same
		cMinRowLength [INTSIZE],// 0x0d	0x0d	Varies	Same
		cNodeKeydesc [QUADSIZE],// 0x0f	0x0f	Normally 2
		cLocalIndex,		// 0x13	0x17	0x00	Same
		cRFU3 [5],		// 0x14	0x18	0x00...	Same
		cDataFree [QUADSIZE],	// 0x19	0x1d	Varies	Same
		cNodeFree [QUADSIZE],	// 0x1d	0x25	Varies	Same
		cDataCount [QUADSIZE],	// 0x21	0x2d	Varies	Same
		cNodeCount [QUADSIZE],	// 0x25	0x35	Varies	Same
		cTransNumber [QUADSIZE],// 0x29	0x3d	Varies	Same
		cUniqueID [QUADSIZE],	// 0x2d	0x45	Varies	Same
		cNodeAudit [QUADSIZE],	// 0x31	0x4d	Varies	Same
		cLockMethod [INTSIZE],	// 0x35	0x55	0x0008	Same
		cRFU4 [4],		// 0x37	0x57	0x00...	Same
		cMaxRowLength [INTSIZE],// 0x3b	0x5b	Varies	Same
		cRFUVarlen [20],	// 0x3d	0x5d	0x00...	Same
		cRFULocalIndex [36];	// 0x51	0x71	0x00...	Same
			//		   ---- ----
			// Length Total	   0x76	0x96
};

struct	DICTINFO
{
	short	iNKeys,		// Number of keys
		iActiveKey,	// Which key is the active key
		iNodeSize,	// Number of bytes in an index block
		iMinRowLength,	// Minimum data row length
		iMaxRowLength;	// Maximum data row length
	int	iDataHandle,	// open () file descriptor of the .dat file
		iIndexHandle,	// open () file descriptor of the .key file
		iOpenMode;	// The type of open which was used
	off_t	tDataPosn,	// Used to TRY to prevent an lseek system call
		tIndexPosn,	//  when sequential blocks are read / written
		tRowNumber,	// Which data row is "CURRENT" 0 if none
		tDupNumber,	// Which duplicate number is "CURRENT" (0=First)
		tRowStart,	// ONLY set to nonzero by isstart()
		tTransLast,	// Used to see whether to set iIndexChanged
		tNRows;		// Number of rows (0 IF EMPTY, 1 IF NOT)
	char	**ppcRowBuffer;	// tMinRowLength buffer for key (re)construction
	struct	DICTNODE
		sDictNode;	// Holds the dictionary node data
	struct	keydesc		// Array of key description information
		*psKeydesc [MAXSUBS];
	struct	VBTREE
		*psTree [MAXSUBS]; // Linked list of index nodes
	struct	VBKEY
		*psKeyFree [MAXSUBS], // An array of linked lists of free VBKEYs
		*psKeyCurr [MAXSUBS]; // An array of 'current' VBKEY pointers
	struct	VBLOCK		// Ordered linked list of locked row numbers
		*psLocks;
	struct
	{	
	unsigned int
		iIsDisjoint:1,	// If set, CURR & NEXT give same result
		iIsDataLocked:1,// If set, islock() is active
		iIsDictLocked:2,// Relates to sDictNode below
				// 	0: Content on file MIGHT be different
				// 	1: Content on file is EQUAL
				// 	2: sDictNode needs to be rewritten
		iIndexChanged:2,// Various
				//	0: Index has NOT changed since last time
				//	1: Index has changed, blocks invalid
				//	2: Index has changed, blocks are valid
		iRFU:26;
	} sFlags;
};
EXTERN	struct	DICTINFO
	*psVBFile [VB_MAX_FILES + 1];
// If the corresponding handle is not open, psVBFile [iHandle] == NULL

// Globally defined function prototypes
// isHelper.c
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

// isbuild.c
int	isbuild (char *, int, struct keydesc *, int);
int	isaddindex (int, struct keydesc *);
int	isdelindex (int, struct keydesc *);

// isdelete.c
int	isdelete (int, char *);
int	isdelcurr (int);
int	isdelrec (int, off_t);

// isopen.c
int	iscleanup (void);
int	isclose (int);
int	isindexinfo (int, struct keydesc *, int);
int	isopen (char *, int);

// isread.c
int	isread (int, char *, int);
int	isstart (int, struct keydesc *, int, char *, int);

// isrewrite.c
int	isrewrite (int, char *);
int	isrewcurr (int, char *);
int	isrewrec (int, off_t, char *);

// istrans.c

// iswrite.c
int	iswrcurr (int, char *);
int	iswrite (int, char *);

// vbDataIO.c
int	iVBDataRead (int, void *, int *, off_t, int);
int	iVBDataWrite (int, void *, int, off_t, int);

// vbIndexIO.c
int	iVBNodeRead (int, void *, off_t);
int	iVBNodeWrite (int, void *, off_t);
int	iVBUniqueIDSet (int, off_t);
off_t	tVBUniqueIDGetNext (int);
off_t	tVBNodeCountGetNext (int);
off_t	tVBDataCountGetNext (int);
int	iVBNodeFree (int, off_t);
int	iVBDataFree (int, off_t);
//iVBVarlenFree - Future
off_t	tVBNodeAllocate (int);
off_t	tVBDataAllocate (int iHandle);
//tVBVarlenAllocate - Future

// vbKeysIO.c
int	iVBCheckKey (int, struct keydesc *, int, int, int);
int	iVBRowInsert (int, char *, off_t);
int	iVBRowDelete (int, off_t);
int	iVBRowUpdate (int, char *, off_t);
void	vVBMakeKey (int, int, char *, char *);
int	iVBKeySearch (int, int, int, int, char *, off_t);
int	iVBKeyLocateRow (int, int, off_t);
int	iVBKeyLoad (int, int, int, int, struct VBKEY **);
int	iVBMakeKeysFromData (int, int);
int	iVBDelNodes (int, int, off_t);
#ifdef	DEBUG
void	vDumpKey (struct VBKEY *, struct VBTREE *, int);
void	vDumpTree (struct VBTREE *, int);
int	iDumpTree (int);
#endif	// DEBUG

// vbLocking.c
int	iVBEnter (int, int);
int	iVBExit (int);
int	iVBForceExit (int);
int	iVBFileOpenLock (int, int);
int	iVBDataLock (int, int, off_t, int);

// vbLowLovel.c
int	iVBOpen (char *, int, mode_t);
int	iVBClose (int);
off_t	tVBLseek (int, off_t, int);
ssize_t	tVBRead (int, void *, size_t);
ssize_t	tVBWrite (int, void *, size_t);
int	iVBLock (int, off_t, off_t, int);
int	iVBLink (char *, char *);
int	iVBUnlink (char *);
int	iVBAccess (char *, int);
void	vVBBlockDeinit (void);
void	vVBBlockInvalidate (int iHandle);
int	iVBBlockRead (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer);
int	iVBBlockWrite (int iHandle, int iIsIndex, off_t tBlockNumber, char *cBuffer);

// vbMemIO.c
struct	VBLOCK *psVBLockAllocate (int);
void	vVBLockFree (struct VBLOCK *);
struct	VBTREE *psVBTreeAllocate (int);
void	vVBTreeAllFree (int, int, struct VBTREE *);
struct	VBKEY *psVBKeyAllocate (int, int);
void	vVBKeyAllFree (int, int, struct VBTREE *);
void	vVBKeyFree (int, int, struct VBKEY *);
void	vVBKeyUnMalloc (int, int);
void	*pvVBMalloc (size_t);
void	vVBFree (void *, size_t);
void	vVBUnMalloc (void);
#ifdef	DEBUG
void	vVBMallocReport (void);
#endif	// DEBUG
