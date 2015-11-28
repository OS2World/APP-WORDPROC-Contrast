/******************************************************************/
/*            contrast - conversion for os/2 of fulcon            */
/* adapted by B.N.H.Scott                                         */
/* from FULCON by Dave Pullin     This Version: 9/1/91            */
/******************************************************************/

/* This program contrasts two files by finding shared lines of data,
   and displaying the files with identical sections in the same
   colours.
*/

/*   The two files, either defined on the command line or in a dialog box, are
   read in, the number of lines counted, and arrays set up. A hash code for each
   line is calculated from its contents, and lines that occur exactly once in
   each file found (these are referred to as 'pins'). Lines that are the same in
   the two files, which are above or below pins, are also counted as pins. All
   other lines are treated as unique.
     A composite file is constructed, which is an attempt to show all
   the lines from both files in the most likely order. Matching blocks of lines
   are kept together; if two blocks of lines have swapped positions in the
   second file, the smaller one is shown twice, above and below the larger one.
   Unique lines are fitted in between the matched blocks in the order they come
   in the real files.
     The two real files may be displayed, or the composite. The text colour
   indicates which lines have been matched, and which are unique. The
   background colour shows either which file the lines belong to (if they are
   unique or have been moved to a different position in the second file), or
   that they are matched, and also not moved.
     A 'bar chart' of the two files is also displayed.
*/

/* 'Hungarian notation' prefixes used: ht for hash table index
                                       lt for line table index
*/

/* include headers for Dos, Win & Gpi calls & for string manipulation and
   character type checking
*/
#define INCL_DOSMISC
#define INCL_DOSMEMMGR
#define INCL_GPILCIDS
#define INCL_GPIPRIMITIVES
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WINERRORS
#define INCL_WINFRAMEMGR
#define INCL_WINHELP
#define INCL_WINHOOKS
#define INCL_WININPUT
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINPOINTERS
#define INCL_WINSCROLLBARS
#define INCL_WINSHELLDATA
#define INCL_WINSTATICS
#define INCL_WINSYS
#define INCL_WINWINDOWMGR
#define _MT
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <process.h>
#include <os2.h>

/* include header defining constants for commands, etc. */
#include "contrast.h"

/* macros for accessing huge file in memory, given a linear pointer */
#define MEM(a) MAKEP ((SELECTOROF(a) << usHugeShift) + selBase, OFFSETOF(a))
#define CMEM(a) * (PCH) MEM(a)

/* max & min macros */
#define MAX(a,b) ((a) > (b)) ? (a) : (b)
#define MIN(a,b) ((a) < (b)) ? (a) : (b)

typedef struct _FONT {
                       FATTRS fattrs;
                       struct _FONT *pfontNext;
                       USHORT usPointSize;
                     } FONT;
typedef FONT *PFONT;

/* special structure to stop trap Ds with some file systems;
   allow extra 20 bytes for file name overflow */
typedef struct _MYFILEFINDBUF {
                                FILEFINDBUF real;
                                char dummy[20];
                              } MYFILEFINDBUF;


#define UM_FAIL WM_USER
#define UM_TABLES_MADE WM_USER+1

#define COL_UNIQUE        4
#define COL_A_BGRND       5
#define COL_B_BGRND       6
#define COL_MATCHED_BGRND 7

/* ids for control windows */
#define ID_VSCROLL 1
#define ID_HSCROLL 2
#define ID_TITLE   3

/* local identifier for font */
#define LCID_SELECTED 1L

#define STACKSIZE 4096 /* for second thread */

/* function declarations */
void cdecl main (int argc, char *argv[]);
void BarChart (HPS hps, LONG cxClient, LONG cyClient, LONG cxBar);
LONG FillBlock (HPS hps, LONG cxClient, LONG cyClient, LONG cxBar,
                SHORT ltTop, SHORT ltBottom);
LONG PrintableLine (SHORT ltIndex, PLONG plColour, PLONG plBackCol);
SHORT RelLine (SHORT ltBaseLine, SHORT sInc);
MRESULT EXPENTRY ClientWndProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
void ShowError (PSZ string);
void ShowPMError (PSZ string);
PFONT InitialiseFonts (HWND hwnd);
MRESULT EXPENTRY AboutDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY ActualColoursProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
void InitialiseHelp (void);
MRESULT EXPENTRY OpenDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
void FillOpenListBoxes (PSZ szPath, HWND hwndFileList, HWND hwndDirList);
MRESULT EXPENTRY SetColoursProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
void CopyColours (PLONG lFromColour, PLONG lToColour);
USHORT MakeFullPath (PSZ szFileName);
BOOL GetDefaultDir (PSZ szDefDir);
MRESULT EXPENTRY FontsDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY MyEntryFieldProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
PVOID AllocMemPM (USHORT usSize);
void cdecl far MakeTables (void far *dummy);
SHORT CreateTables (void);
void ReleaseMemory (void);
ULONG OpenFile (PSZ szFileName, PHFILE pFileHandle);
SHORT ReadFile (HFILE FileHandle, ULONG ulStartMem, ULONG ulEndMem);
SHORT MakeHashValues (ULONG ulChar, ULONG ulEnd, SHORT ltIndex);
void ChainIdenticalLines (void);
void FindUniquePairs (void);
void ExpandUniquePairs (void);
void SetColours (void);
void Composite (SHORT ltTopA, SHORT ltBottomA, SHORT ltTopB,
                SHORT ltBottomB, SHORT ltPrecLine, SHORT ltFolLine);
void Add (PSHORT pltIndex, PSHORT pltPrecLine);
PVOID AllocMem (USHORT usSize);

/* All global variables are initialised. This is due to a quirk in the MS C6.0
   compiler (and maybe many others); if they aren't initialised, ADDRESSES of
   the variables are put in the data segment, so that a lot of indirection goes
   on during the program. If they are, the values are put in the data segment.
   Can't work out why, myself, but there you go. This gives over 10%
   improvement in speed.
*/

/* line table arrays */
ULONG *ulLine = NULL;                         /* pointer to text of line in memory */
SHORT *ltLinePair = NULL;                     /* line table indexes of identical line */
SHORT *ltLineNext = NULL, *ltLinePrev = NULL; /* next and previous lines of composite file */
USHORT *usLineCol = NULL, *usBackCol = NULL;  /* colour of line and background */
ULONG *ulLineHash = NULL;                     /* hash code of line */

SHORT *HashTable = NULL;                      /* index of line table entries */

/****************************DIAGRAM******************************************\

  The line tables consist of several arrays, each with one entry for every line
in each file.
  For each line in each file, a hash value is calculated. This is used as an
index for the hash table, which is an array of subscripts for the line tables.
If there is no subscript at that point in the hash table, the subscript of the
current line is entered.

HashTable      ltLinePair
ÚÄÄÄÄÄÄÄ¿      ÚÄÄÄÄÄÄÄÄ¿
³       ³  ÚÄÄ>ÆÍÍÍÍÍÍÍÍµ
³       ³  ³   ³        ³
³       ³  ³   ³        ³
³       ³  ³   ³        ³
ÆÍÍÍÍÍÍÍµÄÄÙ   ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
³       ³      ³        ³
ÀÄÄÄÄÄÄÄÙ      ³        ³
               ÀÄÄÄÄÄÄÄÄÙ

If there is a subscript present, the hash value of that line (stored in
ulLineHash) is compared with that of the current line. If they don't match, the
computer looks at the next entry in the hash table.
  However, if the hash values match, the computer has found an identical line,
so it changes the entry in the hash table to point to the current line, and
puts the index of the identical line in the ltLinePair entry of the current
line. Thus a chain of identical lines will be built up.


HashTable      ltLinePair
ÚÄÄÄÄÄÄÄ¿      ÚÄÄÄÄÄÄÄÄ¿
³       ³      ÆÍÍÍÍÍÍÍÍµ<ÄÄÄÄ¿
³       ³      ³        ³     ³
³       ³      ³        ³     ³
³       ³      ³        ³     ³
ÆÍÍÍÍÍÍÍµÄÄ¿   ÆÍÍÍÍÍÍÍÍµ<ÄÄ¿ÄÙ
³       ³  ³   ³        ³   ³
³       ³  ³   ³        ³   ³
³       ³  ³   ³        ³   ³
³       ³  ³   ÆÍÍÍÍÍÍÍÍµ<¿ÄÙ
³       ³  ³   ³        ³ ³
³       ³  ³   ³        ³ ³
³       ³  ³   ³        ³ ³
³       ³  ³   ³        ³ ³
ÀÄÄÄÄÄÄÄÙ  ÀÄÄ>ÆÍÍÍÍÍÍÍÍµÄÙ
               ÀÄÄÄÄÄÄÄÄÙ

After this has been done for all the lines in the two files, the computer looks
at each entry in the hash table. If an entry points to a pair of lines, one
from each file, the ltLinePair entry for each line is given the other's index.
In any other case (no matching line, two matching lines in the same file, or
more than two matching lines) the chain is broken up totally.


HashTable      ltLinePair
ÚÄÄÄÄÄÄÄ¿      ÚÄÄÄÄÄÄÄÄ¿
³       ³    ÚÄÆÍÍÍÍÍÍÍÍµ<¿
ÆÍÍÍÍÍÍÍµÄÄ¿ ³ ³        ³ ³
³       ³  ³ ³ ³        ³ ³
³       ³  ³ ³ ³        ³ ³
³       ³  ³ ³ ³        ³ ³
³       ³  À>À>ÆÍÍÍÍÍÍÍÍµÄÙ
³       ³      ³        ³
³       ³      ³        ³
³       ³    ÚÄÆÍÍÍÍÍÍÍÍµ<¿
³       ³    ³ ³        ³ ³
³       ³    ³ ³        ³ ³
³       ³    ³ ³        ³ ³
ÆÍÍÍÍÍÍÍµÄÄ¿ ³ ³        ³ ³
ÀÄÄÄÄÄÄÄÙ  À>À>ÆÍÍÍÍÍÍÍÍµÄÙ
               ÀÄÄÄÄÄÄÄÄÙ

  Lines either side of these 'pins' which match each other (but have not
already been made pins, because there were more than 2 of them) are then made
into pins.

\*************************END OF DIAGRAM**************************************/

HAB hab = NULL; /* handle to anchor block */

PSZ szFile[3] = {NULL, NULL, NULL}; /* names of files, and composite */

SHORT sNumLines[3] = {0, 0, 0}; /* number of lines in each file, and composite */
SHORT sTotalLines = 0; /* total number of lines in the two real files */

SHORT sMaxLength = 0; /* maximum line length in either file */

PFONT pfontHead = NULL; /* head of linked list of fonts */
PFONT pfontSelected = NULL; /* current font */

SEL selBase = 0;        /* variables for accessing huge memory for the files */
USHORT usHugeShift = 0;

char pWriteLine[512] = ""; /* output buffer */

/* user options - loadable and savable - initialise to defaults */
struct {
         LONG lColour[8];
         BOOL fIgnoreLeadingBlanks;
         BOOL fIgnoreTrailingBlanks;
         BOOL fIgnoreAllBlanks;
         BOOL fInterleaveUniqueLines;
         USHORT usPointSize;
         char szFacename[60];
         USHORT fsFontSelection;
       } uoUserOptions =
       {
/* keep up to date with CONTRAST.RC & lDefColour below */
         { CLR_BLUE,       /*   matched text colour        */
           CLR_DARKPINK,   /*      "     "     "           */
           CLR_DARKGREEN,  /*      "     "     "           */
           CLR_DARKCYAN,   /*      "     "     "           */
           CLR_BLACK,      /* unmatched  "     "           */
           CLR_YELLOW,     /* background to file A         */
           CLR_RED,        /*     "      "   "   B         */
           CLR_WHITE       /*     "      "  identical text */
         },
         FALSE,
         FALSE,
         TRUE,
         FALSE,
         0,
         "",
         0
       };

/* array of default colours - keep up to date with CONTRAST.RC & user options */
LONG lDefColour[8] = {  CLR_BLUE,
                        CLR_DARKPINK,
                        CLR_DARKGREEN,
                        CLR_DARKCYAN,
                        CLR_BLACK,
                        CLR_YELLOW,
                        CLR_RED,
                        CLR_WHITE
                  };

#define FILE_A    0
#define FILE_B    1
#define COMPOSITE 2

SHORT file = COMPOSITE; /* file currently displayed */

USHORT usMaxPathLength = 0; /* maximum path length allowed by filing system */

PFNWP pfnOldEntryFieldProc = NULL;          /* subcalssing of entryfields for */
BOOL fEntryFieldASubclassed = FALSE; /* Open dialog */
BOOL fEntryFieldBSubclassed = FALSE;

HWND hwndHelpInstance = NULL; /* handle for help instance */

HWND hwndFrame = NULL, hwndClient = NULL; /* handles for main window */

/* flags for thread communication */
BOOL fStartTables = TRUE; /* set by the PM thread if the table thread is to start a new
                             set of tables; if not, the table thread will terminate */
BOOL fLoadNewFiles = TRUE; /* set by the PM thread if the user has asked for
                              different files to be used */

ULONG ulClearTables = 0;
HSEM hsemClearTables = &ulClearTables; /* semaphore; if 0, the table thread will delete
                                          any old tables, and (depending on
                                          fStartTables) start new ones. At the
                                          beginning of this action, the table thread sets
                                          the semaphore. It periodically checks
                                          the semaphore to see if it has been
                                          cleared by the PM thread (if it has, it
                                          starts again), and when it has
                                          finished the tables, it waits on the
                                          semaphore until it is cleared. It then
                                          starts again. */


void cdecl main (int argc, char *argv[])
/**********************************************************************/
/* expected arguments:  if argc >= 3,                                 */
/*                         argv[1] = name of file A                   */
/*                         argv[2] = name of file B                   */
/*                      if argc < 3,                                  */
/*                         one or both names are missing              */
/*                                                                    */
/* function : Gets filenames from command line or dialog box. Creates */
/* application window, starts contrasting process, passes messages to */
/* window, and terminates application.                                */
/**********************************************************************/

{
    HMQ hmq;
    PSZ szClientClass = "Contrast";
    ULONG flFrameFlags = FCF_STANDARD, ulOptionsSize;
    QMSG qmsg;
    BOOL fCreateWindow;
    USHORT usResult;
    PBYTE Stack;

    if (hab = WinInitialize (0))
    {
        if (hmq = WinCreateMsgQueue (hab, 0))
        {

            InitialiseHelp ();

/* get max path length for file system */
            DosQSysInfo (0, (PBYTE) &usMaxPathLength, 2);

/* get user options */
        ulOptionsSize = sizeof(uoUserOptions);
        PrfQueryProfileData (HINI_USERPROFILE, "Contrast", "Options", &uoUserOptions, &ulOptionsSize);

/* allocate memory for full file paths */
            if ((szFile[0] = AllocMemPM (usMaxPathLength)) != NULL &&
                (szFile[1] = AllocMemPM (usMaxPathLength)) != NULL)
            {

/* usResult's bits mean as follows:
   bit 0: need Open dialog
   bit 1: couldn't get memory, so exit
*/
                if (argc >= 2)
                {
                    strcpy (szFile[0], argv[1]);
                    usResult = MakeFullPath (szFile[0]);
                }
                else
                {
                    if (GetDefaultDir (szFile[0]) == TRUE)
                        usResult = 1;
                    else
                        usResult = 2;
                }

                if (usResult != 2)
                {
                    if (argc >= 3)
                    {
/* if a directory was specified as the 1st parameter, we'll need the Open dialog */
                        if (szFile[0][strlen (szFile[0]) - 1] == '\\')
                        {
                            usResult = 1;
                        }

                        strcpy (szFile[1], argv[2]);
                        usResult |= MakeFullPath (szFile[1]);
                    }
                    else
                    {
                        strcpy (szFile[1], szFile[0]);
                        usResult = 1;
                    }
                }

                switch (usResult)
                {
                case 0:
                    fCreateWindow = TRUE;
                    break;
                case 1:
/* call the Open dialog - returns FALSE if Cancel button pressed */
                    fCreateWindow = WinDlgBox (HWND_DESKTOP, HWND_DESKTOP, OpenDlgProc,
                                         (HMODULE)NULL, DID_OPEN, NULL);
                    break;
                default:
                    fCreateWindow = FALSE;
                    break;
                }

                if (fCreateWindow == TRUE)
                {
                    if (WinRegisterClass (hab,
                                          szClientClass,
                                          ClientWndProc,
                                          CS_SIZEREDRAW,
                                          0))
                    {

                        if (hwndFrame = WinCreateStdWindow (HWND_DESKTOP,
                                                            WS_VISIBLE,
                                                            &flFrameFlags,
                                                            szClientClass,
                                                            NULL,
                                                            0L,
                                                            (HMODULE)NULL,
                                                            ID_CONTRAST,
                                                            &hwndClient))
                        {

                            if (WinAssociateHelpInstance (hwndHelpInstance, hwndFrame))
                            {

                                if (Stack = AllocMemPM (STACKSIZE))
                                {
                                    _beginthread (MakeTables,
                                                  Stack,
                                                  STACKSIZE,
                                                  NULL);

                                    while (WinGetMsg (hab, &qmsg, NULL, 0, 0))
                                        WinDispatchMsg (hab, &qmsg);

/* tell other thread it can release resources */
                                    fStartTables = FALSE;
                                    DosSemClear (hsemClearTables);
                                }
                            }
                            WinDestroyWindow (hwndFrame);
                        }
                    }
                }
            }

            WinDestroyHelpInstance (hwndHelpInstance);
            WinDestroyMsgQueue (hmq);
        }
    }
    WinTerminate (hab);
    return;

} /* end of main */



void BarChart (HPS hps, LONG cxClient, LONG cyClient, LONG cxBar)
/****************************************************************************/
/* expected parameters: hps = handle for display (input)                    */
/*                      cxClient & cyClient = size of client window (input) */
/*                      cxBar = width of bar (input)                        */
/*                                                                          */
/* function : Draws bar chart.                                              */
/****************************************************************************/

{
    SHORT ltTopABlock, ltTopBBlock, ltBottomBBlock, ltIndex, ltPair, ltTopBlock;
    POINTL point;

    ltTopABlock = 0;
    ltTopBBlock = ltBottomBBlock = 0;

    for (ltIndex = 1; ltIndex <= sNumLines[0] + 1; ltIndex++)
    {
        ltPair = ltLinePair[ltIndex];
        if (ltPair == ltBottomBBlock + 1)
            ltBottomBBlock = ltPair;    /* continuation of block */
        else
        {
            if (ltBottomBBlock != 0)    /* blocks to be coloured */
            {

/* draw blocks and connect with line */
                point.x = cxClient - 4 * cxBar;
                point.y = FillBlock (hps, cxClient, cyClient, cxBar,
                                        ltTopABlock, ltIndex - 1);
                GpiMove (hps, &point);

                point.x = cxClient - 2* cxBar;
                point.y = FillBlock (hps, cxClient, cyClient, cxBar,
                                         ltTopBBlock, ltBottomBBlock);
                GpiSetColor (hps, uoUserOptions.lColour[usLineCol[ltTopBBlock]]);
                GpiLine (hps, &point);

/* reset bottom of B block, i.e. no block current */
                ltBottomBBlock = 0;
            }

            if (ltPair != 0)            /* new block */
            {
                ltTopABlock = ltIndex;
                ltTopBBlock = ltBottomBBlock = ltPair;
            }
        }
    }

/* fill blocks for unique lines. This must be done after the matched blocks,
   so that single line changes are always shown. */
    ltTopBlock = 0;

    for (ltIndex = 1; ltIndex <= sTotalLines; ltIndex++)
    {
        if (ltIndex == sNumLines[0] + 1 && ltTopBlock != 0)
        {
/* we're at the end of the 1st file, and a block is pending, so fill it */
            FillBlock (hps, cxClient, cyClient, cxBar,
                       ltTopBlock, ltIndex - 1);

            ltTopBlock = 0;
        }

        if ((ltPair = ltLinePair[ltIndex]) != 0)
        {
            if (ltTopBlock != 0)
            {
/* we've just finished a block of unique lines, so fill it */
                FillBlock (hps, cxClient, cyClient, cxBar,
                           ltTopBlock, ltIndex - 1);

                ltTopBlock = 0;
            }
        }
        else if (ltTopBlock == 0)
            ltTopBlock = ltIndex;
    }

    if (ltTopBlock != 0)
/* we're at the end of the 2nd file, and a block is pending, so fill it */
        FillBlock (hps, cxClient, cyClient, cxBar, ltTopBlock, ltIndex - 1);

    return;
} /* end of BarChart () */

LONG FillBlock (HPS hps, LONG cxClient, LONG cyClient, LONG cxBar,
                SHORT ltTop, SHORT ltBottom)
/***************************************************************************/
/* expected parameters: hps = handle for display (input)                   */
/*                      cxClient, cyClient = size of client window (input) */
/*                      cxBar = width of bar (input)                       */
/*                      ltTop, ltBottom = limiting lines of block (input)  */
/* return value : y coordinate of centre of block                          */
/*                                                                         */
/* function : Draws a block in the appropriate column                      */
/***************************************************************************/

{
    SHORT sScaler;
    RECTL rectl;
    LONG lBlockColour;

    sScaler = MAX (sNumLines[0], sNumLines[1]);

    if (ltTop <= sNumLines[0])
    {
        rectl.xLeft = cxClient - 5 * cxBar;
        rectl.xRight = cxClient - 4 * cxBar;
        rectl.yTop = cyClient - (ltTop - 1) * cyClient / sScaler;
        rectl.yBottom = cyClient - ltBottom * cyClient / sScaler - 1;
    }
    else
    {
        rectl.xLeft = cxClient - 2 * cxBar;
        rectl.xRight = cxClient - cxBar;
        rectl.yTop = cyClient - (ltTop - 1 - sNumLines[0]) * cyClient / sScaler;
        rectl.yBottom = cyClient - (ltBottom - sNumLines[0]) * cyClient / sScaler - 1;
    }

    if (usLineCol[ltTop] != COL_UNIQUE)
        lBlockColour = uoUserOptions.lColour[usLineCol[ltTop]];
    else
        lBlockColour = uoUserOptions.lColour[usBackCol[ltTop]];

    WinFillRect (hps, &rectl, lBlockColour);

    return (rectl.yTop + rectl.yBottom ) / 2;

} /* end of FillBlock () */


LONG PrintableLine (SHORT ltIndex, PLONG plColour, PLONG plBackCol)
/***********************************************************************/
/* expected parameters: ltIndex = entry in line table (input)          */
/*                      plColour = colour of line (output)             */
/*                      plBackCol = background colour (output)         */
/* return value: number of characters in line                          */
/*                                                                     */
/* function : Converts a line of text in the file to a printable line, */
/*            taking care of control characters, and tab characters.   */
/***********************************************************************/

{
    ULONG ulChar;
    char Char;
    LONG lCount;
    int i;

    ulChar = ulLine[ltIndex];
    *plColour = uoUserOptions.lColour[usLineCol[ltIndex]];
    *plBackCol = uoUserOptions.lColour[usBackCol[ltIndex]];
    lCount = 0;

/*    while ((Char = CMEM (ulChar)) != 10 && Char != 13 && lCount < 512) */
    while ((Char = CMEM (ulChar)) != '\r' && lCount < 512)
    {
        if (isprint (Char))
            pWriteLine[lCount] = Char;
        else if (Char == '\t')
        {
            for (i = 0; i < 4; i++)
                pWriteLine[lCount + i] = ' ';
            lCount += 3;
        }
        else
            pWriteLine[lCount] = '?';

        ulChar++;
        lCount++;
    }
    return lCount;
} /* end of PrintableLine () */


SHORT RelLine (SHORT ltBaseLine, SHORT sInc)
/*****************************************************************************/
/* expected parameters: sBaseLine = reference line in composite file         */
/*                      sInc = number of lines forward                       */
/* return value: index of line in line table                                 */
/*                                                                           */
/* function : Finds a line in a real, or composite, file relative to a base. */
/*****************************************************************************/
{

    if (file != COMPOSITE)
        return (ltBaseLine + sInc);

    else if (sInc > 0)
        while (sInc-- > 0)
            ltBaseLine = ltLineNext[ltBaseLine];

    else
        while (sInc++ < 0)
            ltBaseLine = ltLinePrev[ltBaseLine];

    return ltBaseLine;
} /* end of RelLine () */


MRESULT EXPENTRY ClientWndProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/**********************************************************************/
/* All-purpose window procedure. For functions, see individual cases. */
/**********************************************************************/

{
    HPS hps;
    static LONG cxClient, cyClient, cxBar, cyBar, cyTitle;
    RECTL rclInvalid, rclLine, rclScroll, rclBar;
    static LONG cxCaps, cyChar, cyAsc;
    static SHORT sLinesPerPage, sColsPerPage, fTablesValid = 0;
    static SHORT ltTopLine[3], sColPos[3], sFileLine[3];
    static HWND hwndVScroll, hwndHScroll, hwndTitle, hwndMenu;
    LONG lCount, lPrintCol, lBackColour;
    POINTL point;
    SHORT ltIndex, sScaler, sIndex, ltFirstA, ltLastA, ltFirstB, ltLastB;
    SHORT sLineInc, sColInc;
    SHORT sStartPrint, sStopPrint, sPrintLine;
    FONTMETRICS fm;
    PSZ szTitle;
    static HPOINTER hptrWait, hptrArrow;

    switch (msg)
    {
    case WM_CREATE:
/******************************************************/
/* function : Initialises variables, fonts, title and */
/*            scroll bar windows, and user options.   */
/******************************************************/

        hwndMenu = WinWindowFromID (WinQueryWindow (hwnd, QW_PARENT, FALSE),
                                    FID_MENU);

        hptrWait = WinQuerySysPointer (HWND_DESKTOP, SPTR_WAIT, FALSE);
        hptrArrow = WinQuerySysPointer (HWND_DESKTOP, SPTR_ARROW, FALSE);

        szFile[COMPOSITE] = "Composite";

/* get width & height of system scroll bars */
        cxBar = WinQuerySysValue (HWND_DESKTOP, SV_CXVSCROLL);
        cyBar = WinQuerySysValue (HWND_DESKTOP, SV_CYHSCROLL);
        cyTitle = WinQuerySysValue (HWND_DESKTOP, SV_CYTITLEBAR);

/* create scroll bars */
        hwndVScroll = WinCreateWindow (hwnd,
                                       WC_SCROLLBAR,
                                       NULL,
                                       WS_VISIBLE | SBS_VERT,
                                       (SHORT) (cxClient - 7 * cxBar),
                                       (SHORT) cyBar,
                                       (SHORT) cxBar,
                                       (SHORT) (cyClient - cyBar - cyTitle),
                                       hwnd,
                                       HWND_BOTTOM,
                                       ID_VSCROLL,
                                       NULL,
                                       NULL);

        hwndHScroll = WinCreateWindow (hwnd,
                                       WC_SCROLLBAR,
                                       NULL,
                                       WS_VISIBLE | SBS_HORZ,
                                       0,
                                       0,
                                       (SHORT) (cxClient - 7 * cxBar),
                                       (SHORT) cyBar,
                                       hwnd,
                                       HWND_BOTTOM,
                                       ID_HSCROLL,
                                       NULL,
                                       NULL);

        hwndTitle = WinCreateWindow (hwnd,
                                     WC_STATIC,
                                     szFile[2],
                                     WS_VISIBLE | SS_TEXT | DT_CENTER,
                                     0,
                                     (SHORT) (cyClient - cyTitle),
                                     (SHORT) (cxClient - 6 * cxBar),
                                     (SHORT) cyTitle,
                                     hwnd,
                                     HWND_BOTTOM,
                                     ID_TITLE,
                                     NULL,
                                     NULL);

/* find fixed pitch font attributes */
        pfontHead = InitialiseFonts (hwnd);

/* set colour of title window */
        WinSetPresParam (hwndTitle, PP_BACKGROUNDCOLORINDEX, 4, &(uoUserOptions.lColour)[COL_A_BGRND+file]);

/* set checked status of menu items as appropriate */
        if (uoUserOptions.fIgnoreLeadingBlanks)
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_LEADING_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));

        if (uoUserOptions.fIgnoreTrailingBlanks)
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_TRAILING_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));

        if (uoUserOptions.fIgnoreAllBlanks)
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_ALL_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));

        if (uoUserOptions.fInterleaveUniqueLines)
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_INTERLEAVE_UNIQUE_LINES, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));

        return 0;

    case WM_SIZE:
/**********************************************************************/
/* function : Gets new size of window, and repositions child windows. */
/**********************************************************************/

        cxClient = (LONG) SHORT1FROMMP (mp2);
        cyClient = (LONG) SHORT2FROMMP (mp2);

        WinSetWindowPos (hwndVScroll,
                         HWND_BOTTOM,
                         (SHORT) (cxClient - 7 * cxBar),
                         (SHORT) cyBar,
                         (SHORT) cxBar,
                         (SHORT) (cyClient - cyBar - cyTitle),
                         SWP_SIZE | SWP_MOVE | SWP_SHOW);

        WinSetWindowPos (hwndHScroll,
                         HWND_BOTTOM,
                         0,
                         0,
                         (SHORT) (cxClient - 7 * cxBar),
                         (SHORT) cyBar,
                         SWP_SIZE | SWP_MOVE | SWP_SHOW);

        WinSetWindowPos (hwndTitle,
                         HWND_BOTTOM,
                         0,
                         (SHORT) (cyClient - cyTitle),
                         (SHORT) (cxClient - 6 * cxBar),
                         (SHORT) cyTitle,
                         SWP_SIZE | SWP_MOVE | SWP_SHOW);

        hps = WinGetPS (hwnd);

        if (pfontSelected != NULL)
        {
            if (GpiCreateLogFont (hps, NULL, LCID_SELECTED, &(pfontSelected->fattrs))
                != FONT_MATCH)
                ShowPMError ("GpiCreateLogFont");
            if (GpiSetCharSet (hps, LCID_SELECTED) == FALSE)
                ShowPMError ("GpiSetCharSet");
        }

        GpiQueryFontMetrics (hps, (LONG) sizeof (fm), &fm);
        cxCaps = fm.lAveCharWidth;
        cyChar = fm.lMaxBaselineExt + fm.lExternalLeading;
        cyAsc = fm.lLowerCaseAscent;
/*
        if (pfontSelected != NULL)
        {
            GpiSetCharSet (hps, LCID_DEFAULT);
            GpiDeleteSetId (hps, LCID_SELECTED);
        }
*/
        WinReleasePS (hps);

        sLinesPerPage = (SHORT) ((cyClient - cyTitle - cyBar) / cyChar);
        sColsPerPage = (SHORT) ((cxClient - 7 * cxBar) / cxCaps);

        return 0;

    case WM_PAINT:
/**********************************************************************/
/* function : Paints any area of the client window when it is needed. */
/**********************************************************************/

/* get coordinates of invalidated area, so the minimum of painting is done */
        hps = WinBeginPaint (hwnd, NULL, &rclInvalid);
        GpiErase (hps);

/* don't show lines until tables created */
        if (fTablesValid == FALSE)
        {
            WinSetPointer (HWND_DESKTOP, hptrWait);
            WinEndPaint (hps);
            return 0;
        }

        WinSetPointer (HWND_DESKTOP, hptrArrow);

        if (rclInvalid.xRight > cxClient - 7 * cxBar)
            BarChart (hps, cxClient, cyClient, cxBar);

/* set font. Don't bother with error messages - these will have been
   given during WM_CREATE, and would just slow the whole process down.
*/
        if (pfontSelected != NULL)
        {
            GpiCreateLogFont (hps, NULL, LCID_SELECTED, &(pfontSelected->fattrs));
            GpiSetCharSet (hps, LCID_SELECTED);
        }

/* set x coordinates of background line, and printing */
        rclLine.xLeft = rclInvalid.xLeft;
        rclLine.xRight = MIN (rclInvalid.xRight, cxClient - 7 * cxBar);
        rclInvalid.yTop = MIN (rclInvalid.yTop, cyClient - cyTitle);
        rclInvalid.yBottom = MAX (rclInvalid.yBottom, cyBar);
        point.x = (1 - sColPos[file]) * cxCaps;

/* find which lines are to be displayed */
        sStartPrint = (SHORT) ((cyClient - cyTitle - rclInvalid.yTop) / cyChar);
        sStopPrint = (SHORT) (MIN ((cyClient - cyTitle - rclInvalid.yBottom)
                                  / cyChar, sNumLines[file] - sFileLine[file]));

        ltIndex = ltTopLine[file];

        rclLine.yBottom = cyClient - cyTitle - sStartPrint * cyChar;

/* initialise variables for top of bar chart marks */
        ltFirstA = ltFirstB = 0;

/* get each line, and print it at appropriate y coordinates */
        for (sPrintLine = 0; sPrintLine <= sLinesPerPage; sPrintLine++)
        {

/* adjust variables for top and bottom of bar chart marks */
            if (ltIndex > 0 && ltIndex <= sNumLines[0])
            {
                if (file == COMPOSITE && usBackCol[ltIndex] == COL_MATCHED_BGRND)
                {
/* composite lines only come from A, so we must include the checking of B here */
                    if (ltFirstB == 0)
                        ltFirstB = ltLinePair[ltIndex];
                    ltLastB = ltLinePair[ltIndex];
                }
                if (ltFirstA == 0)
                    ltFirstA = ltIndex;
                ltLastA = ltIndex;
            }
            if (ltIndex >sNumLines[0])
            {
                if (ltFirstB == 0)
                    ltFirstB = ltIndex;
                ltLastB = ltIndex;
            }

            if (sPrintLine >= sStartPrint && sPrintLine <= sStopPrint)
            {
                lCount = PrintableLine (ltIndex, &lPrintCol, &lBackColour);
                GpiSetColor (hps, lPrintCol);
                GpiSetBackColor (hps, lBackColour);

                rclLine.yTop = rclLine.yBottom;
                rclLine.yBottom = MAX (rclInvalid.yBottom,
                                       rclLine.yBottom - cyChar);
                point.y = rclLine.yTop - cyAsc;

                GpiCharStringPosAt (hps, &point, &rclLine, CHS_OPAQUE | CHS_CLIP,
                                    lCount, pWriteLine, NULL);
            }

            ltIndex = RelLine (ltIndex, 1);
        }
/*
        if (pfontSelected != NULL)
        {
            GpiSetCharSet (hps, LCID_DEFAULT);
            GpiDeleteSetId (hps, LCID_SELECTED);
        }
*/
        WinEndPaint (hps);

/* adjust scroll bars */
        WinSendMsg (hwndVScroll,
                    SBM_SETSCROLLBAR,
                    MPFROMSHORT (sFileLine[file]),
                    MPFROM2SHORT (1, sNumLines[file] + 1 - sLinesPerPage));

        WinSendMsg (hwndVScroll,
                    SBM_SETTHUMBSIZE,
                    MPFROM2SHORT (sLinesPerPage, sNumLines[file]),
                    NULL);

        WinSendMsg (hwndHScroll,
                    SBM_SETSCROLLBAR,
                    MPFROMSHORT (sColPos[file]),
                    MPFROM2SHORT (1, sMaxLength + 1 - sColsPerPage));

         WinSendMsg (hwndHScroll,
                     SBM_SETTHUMBSIZE,
                     MPFROM2SHORT (sColsPerPage, sMaxLength),
                     NULL);

/* draw marks by bar charts to indicate position in file */
        hps = WinGetPS (hwnd);

/* erase old marks */
        rclBar.xLeft = cxClient - 11 * cxBar / 2;
        rclBar.xRight = cxClient - 21 * cxBar / 4;
        rclBar.yTop = cyClient;
        rclBar.yBottom = 0L;
        WinFillRect (hps, &rclBar, CLR_BACKGROUND);

        rclBar.xLeft = cxClient - 3 * cxBar / 4;
        rclBar.xRight = cxClient - cxBar / 2;
        WinFillRect (hps, &rclBar, CLR_BACKGROUND);

        sScaler = MAX (sNumLines[0], sNumLines[1]);
        if (ltFirstA != 0)
        {
            rclBar.xLeft = cxClient - 11 * cxBar / 2;
            rclBar.xRight = cxClient - 21 * cxBar / 4;
            rclBar.yTop = cyClient - (ltFirstA - 1) * cyClient / sScaler;
            rclBar.yBottom = cyClient - ltLastA * cyClient / sScaler - 1;
            WinFillRect (hps, &rclBar, CLR_NEUTRAL);
        }

        if (ltFirstB !=0)
        {
            rclBar.xLeft = cxClient - 3 * cxBar / 4;
            rclBar.xRight = cxClient - cxBar / 2;
            rclBar.yTop = cyClient - (ltFirstB - 1 - sNumLines[0]) * cyClient / sScaler;
            rclBar.yBottom = cyClient - (ltLastB - sNumLines[0]) * cyClient / sScaler - 1;
            WinFillRect (hps, &rclBar, CLR_NEUTRAL);
        }

        WinReleasePS (hps);

        return 0;


    case WM_COMMAND:
/************************************************/
/* function : Processes commands from the menu. */
/************************************************/

        switch (COMMANDMSG(&msg)->cmd)
        {
        case MID_EXIT:
            WinSendMsg (hwnd, WM_CLOSE, 0L,0L);
            return 0;

        case MID_FILEA:
        case MID_FILEB:
        case MID_COMPOSITE:
/* switch file displayed */
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (file + MID_FILEA, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, 0));

            file = COMMANDMSG(&msg)->cmd - MID_FILEA;

            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (file + MID_FILEA, TRUE),
                        MPFROM2SHORT (MIA_CHECKED, MIA_CHECKED));

            WinSetWindowText (hwndTitle, szFile[file]);

            WinSetPresParam (hwndTitle, PP_BACKGROUNDCOLORINDEX, 4, &(uoUserOptions.lColour)[COL_A_BGRND+file]);

            rclInvalid.xLeft = 0L;
            rclInvalid.xRight = (LONG) (cxClient - 7 * cxBar);
            rclInvalid.yTop = cyClient - cyTitle;
            rclInvalid.yBottom = (LONG) cyBar;
            WinInvalidateRect (hwnd, &rclInvalid, TRUE);

            return 0;

        case MID_ABOUT:
            WinDlgBox (HWND_DESKTOP, hwndFrame, AboutDlgProc, (HMODULE)NULL, DID_ABOUT, NULL);
            return 0;

        case MID_OPEN:
            if (WinDlgBox (HWND_DESKTOP, hwndFrame, OpenDlgProc,
                           (HMODULE)NULL, DID_OPEN, NULL) == TRUE)
            {
                fTablesValid = 0;
                fLoadNewFiles = TRUE;
                DosSemClear (hsemClearTables);
                WinSetPointer (HWND_DESKTOP, hptrWait);
            }

            return 0;

        case MID_IGNORE_LEADING_BLANKS:
            uoUserOptions.fIgnoreLeadingBlanks ^= TRUE;
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_LEADING_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED,
                                      uoUserOptions.fIgnoreLeadingBlanks ? MIA_CHECKED : 0));

            fTablesValid = 0;
            fLoadNewFiles = FALSE;
            DosSemClear (hsemClearTables);
            WinSetPointer (HWND_DESKTOP, hptrWait);

            return 0;

        case MID_IGNORE_TRAILING_BLANKS:
            uoUserOptions.fIgnoreTrailingBlanks ^= TRUE;
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_TRAILING_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED,
                                      uoUserOptions.fIgnoreTrailingBlanks ? MIA_CHECKED : 0));

            fTablesValid = 0;
            fLoadNewFiles = FALSE;
            DosSemClear (hsemClearTables);
            WinSetPointer (HWND_DESKTOP, hptrWait);

            return 0;

        case MID_IGNORE_ALL_BLANKS:
            uoUserOptions.fIgnoreAllBlanks ^= TRUE;
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_IGNORE_ALL_BLANKS, TRUE),
                        MPFROM2SHORT (MIA_CHECKED,
                                      uoUserOptions.fIgnoreAllBlanks ? MIA_CHECKED : 0));

            fTablesValid = 0;
            fLoadNewFiles = FALSE;
            DosSemClear (hsemClearTables);
            WinSetPointer (HWND_DESKTOP, hptrWait);

            return 0;

        case MID_INTERLEAVE_UNIQUE_LINES:
            uoUserOptions.fInterleaveUniqueLines ^= TRUE;
            WinSendMsg (hwndMenu,
                        MM_SETITEMATTR,
                        MPFROM2SHORT (MID_INTERLEAVE_UNIQUE_LINES, TRUE),
                        MPFROM2SHORT (MIA_CHECKED,
                                      uoUserOptions.fInterleaveUniqueLines ? MIA_CHECKED : 0));

            fTablesValid = 0;
            fLoadNewFiles = FALSE;
            DosSemClear (hsemClearTables);
            WinSetPointer (HWND_DESKTOP, hptrWait);

            return 0;

       case MID_SET_COLOURS:
            if (WinDlgBox (HWND_DESKTOP, hwndFrame, SetColoursProc,
                           (HMODULE)NULL, DID_COLOURS, NULL) == TRUE)
            {
                WinInvalidateRect (hwnd, NULL, TRUE);
            }
            return 0;

       case MID_SET_FONT:
            if (WinDlgBox (HWND_DESKTOP, hwndFrame, FontsDlgProc,
                           (HMODULE)NULL, DID_SET_FONT, NULL) == TRUE)
            {
                WinSendMsg (hwnd,
                            WM_SIZE,
                            MPFROM2SHORT ((SHORT) cxClient, (SHORT) cyClient),
                            MPFROM2SHORT ((SHORT) cxClient, (SHORT) cyClient));

/* only invalidate text area */
                rclInvalid.xLeft = 0L;
                rclInvalid.xRight = cxClient - 7 * cxBar;
                rclInvalid.yTop = cyClient - cyTitle;
                rclInvalid.yBottom = cyBar;
                WinInvalidateRect (hwnd, &rclInvalid, TRUE);
            }
            return 0;

       case MID_HELP_FOR_HELP:
            WinSendMsg (hwndHelpInstance, HM_DISPLAY_HELP, 0L, 0L);
            return 0;

        default:
            break;
        }
        break;

    case HM_QUERY_KEYS_HELP:
        return (MRESULT) HID_KEYS;

    case HM_INFORM:
        WinDlgBox (HWND_DESKTOP, HWND_DESKTOP, ActualColoursProc, (HMODULE)NULL, DID_SHOW_COLOURS, NULL);
        return 0;

    case WM_VSCROLL:
/****************************************************************/
/*  function : Adjusts place in file using scroll bar messages. */
/****************************************************************/

        if (fTablesValid == 0)
            return 0;

        switch (SHORT2FROMMP (mp2))
        {
        case SB_LINEUP:
            sLineInc = -1;
            break;
        case SB_LINEDOWN:
            sLineInc = 1;
            break;
        case SB_PAGEUP:
            sLineInc = 1 - sLinesPerPage;
            break;
        case SB_PAGEDOWN:
            sLineInc = sLinesPerPage - 1;
            break;
        case SB_SLIDERTRACK:
            sLineInc = SHORT1FROMMP (mp2) - sFileLine[file];
            break;
        default:
            sLineInc = 0;
            break;
        }

        sLineInc = MAX (1 - sFileLine[file],
                   MIN (sLineInc,
                       sNumLines[file] + 1 - sLinesPerPage - sFileLine[file]));

        if (sLineInc != 0)
        {
            sFileLine[file] += sLineInc;

            ltTopLine[file] = RelLine (ltTopLine[file], sLineInc);

            rclScroll.xLeft = 0L;
            rclScroll.xRight = cxClient - 7 * cxBar;
            rclScroll.yTop = cyClient - cyTitle;
            rclScroll.yBottom = cyBar;
            WinScrollWindow (hwnd, 0, (SHORT) (sLineInc * cyChar),
                             &rclScroll, &rclScroll, NULL, NULL, SW_INVALIDATERGN);

/* force update of window, since WM_PAINT messages have a low priority,
   and can get pushed to the back of the queue.
*/
            WinUpdateWindow (hwnd);
        }

        return 0;

    case WM_HSCROLL:
/************************************************/
/* function : Moves view of file left or right. */
/************************************************/

        if (fTablesValid == 0)
            return 0;

        switch (SHORT2FROMMP (mp2))
        {
        case SB_LINELEFT:
            sColInc = -1;
            break;
        case SB_LINERIGHT:
            sColInc = 1;
            break;
        case SB_PAGELEFT:
            sColInc = -sColsPerPage / 2;
            break;
        case SB_PAGERIGHT:
            sColInc = sColsPerPage / 2;
            break;
        case SB_SLIDERTRACK:
            sColInc = SHORT1FROMMP (mp2) - sColPos[file];
            break;
        default:
            sColInc = 0;
            break;
        }

        sColInc = MAX (1 - sColPos[file],
                  MIN (sColInc,
                      (sMaxLength) + 1 - sColsPerPage - sColPos[file]));

        if (sColInc != 0)
        {
            sColPos[file] += sColInc;

            rclScroll.xLeft = 0L;
            rclScroll.xRight = cxClient - 7 * cxBar;
            rclScroll.yTop = cyClient - cyTitle;
            rclScroll.yBottom = cyBar;
            WinScrollWindow (hwnd, (SHORT) (-sColInc * cxCaps), 0,
                             &rclScroll, &rclScroll, NULL, NULL, SW_INVALIDATERGN);

            WinUpdateWindow (hwnd);
        }

        return 0;

    case WM_CHAR:
/*********************************************************************/
/* function : Duplicates scroll bar action with cursor control keys. */
/*********************************************************************/

/* PM has same messages for keys as for scroll bars,
   so they just need to be passed on.
*/
        if ((SHORT1FROMMP (mp1) & KC_KEYUP) == 0)
            switch (CHARMSG (&msg) -> vkey)
            {
            case VK_UP:
            case VK_DOWN:
            case VK_PAGEUP:
            case VK_PAGEDOWN:
                return WinSendMsg (hwndVScroll, msg, mp1, mp2);
            case VK_LEFT:
            case VK_RIGHT:
                return WinSendMsg (hwndHScroll, msg, mp1, mp2);
            default:
                break;
            }

        break;

    case WM_SAVEAPPLICATION:
    case WM_CLOSE:
        if (pfontSelected != NULL)
        {
            strcpy (uoUserOptions.szFacename, pfontSelected->fattrs.szFacename);
            uoUserOptions.usPointSize = pfontSelected->usPointSize;
            uoUserOptions.fsFontSelection = pfontSelected->fattrs.fsSelection;
        }
        PrfWriteProfileData (HINI_USERPROFILE, "Contrast", "Options", &uoUserOptions, sizeof(uoUserOptions));
        break;

    case WM_MOUSEMOVE:
        if (fTablesValid == FALSE)
        {
            WinSetPointer (HWND_DESKTOP, hptrWait);
            return FALSE;
        }
        break;

    case UM_FAIL:
/*****************************************************************************/
/* function : Calls Open dialog box if the lookup tables cannot be made.     */
/*            Restarts program, or ends it, acccording to the user's wishes. */
/*****************************************************************************/

        ShowError (pWriteLine);
        if (WinDlgBox (HWND_DESKTOP, hwndFrame, OpenDlgProc,
                             (HMODULE)NULL, DID_OPEN, NULL) != TRUE)
        {
            fStartTables = FALSE;
            WinSendMsg (hwnd, WM_CLOSE, 0L, 0L);
        }

        fLoadNewFiles = TRUE;
        DosSemClear (hsemClearTables);
        return 0;

    case UM_TABLES_MADE:
/************************************************************/
/* function : Sets views of files when tables are complete. */
/************************************************************/

        for (sIndex = 0; sIndex < 3; sIndex++)
        {
            sFileLine[sIndex] = 1;
            sColPos[sIndex] = 1;
        }

        ltTopLine[0] = 1;
        ltTopLine[1] = sNumLines[0] + 1;
        ltTopLine[2] = ltLineNext[0];

        fTablesValid = 1;

        szTitle = AllocMemPM (strlen (szFile[0]) + strlen (szFile[1]) + 11);
        sprintf (szTitle, "Contrast %s %s", szFile[0], szFile[1]);

        WinSetWindowText (WinQueryWindow (hwnd, QW_PARENT, FALSE), szTitle);

        DosFreeSeg (SELECTOROF(szTitle));

        WinInvalidateRect (hwnd, NULL, TRUE);

        return 0;

    default:
        break;
    }

    return WinDefWindowProc (hwnd, msg, mp1, mp2);
} /* end of ClientWndProc() */




void ShowError(PSZ string)

{
    WinMessageBox (HWND_DESKTOP,
                   hwndFrame,
                   string,
                   "Problem",
                   ID_PROBLEM,
                   MB_OK | MB_ICONEXCLAMATION);

    return;
} /* end of ShowError () */


void ShowPMError (PSZ string)

{
    char buffer[256];

    sprintf (buffer, "%s error number 0x%x", string, WinGetLastError (hab));
    ShowError (buffer);

    return;
} /* end of ShowPMError () */


PFONT InitialiseFonts (HWND hwnd)
/***********************************************************/
/* expected parameter: hwnd = client window handle (input) */
/* return value: NULL = no fixed width font found          */
/*               other = pointer to head of font list      */
/*                                                         */
/* function : Gets attributes of fixed width fonts.        */
/***********************************************************/

{
    HPS hps;
    HDC hdc;
    FONTMETRICS *pfm;
    LONG lHorzRes, lVertRes, lRequestFonts, lNumberFonts, lIndex;
    USHORT usSmallestSize = 64000; /* anything very big */
    PFONT pfontNew, pfontCurrent = NULL;

    hps = WinGetPS (hwnd);

/* get device font resolution, so we can match the font to the display */
    hdc = GpiQueryDevice (hps);
    DevQueryCaps (hdc, CAPS_HORIZONTAL_FONT_RES, 1L, &lHorzRes);
    DevQueryCaps (hdc, CAPS_VERTICAL_FONT_RES, 1L, &lVertRes);

/* find out number of fonts available */
    lRequestFonts = 0;
    lNumberFonts = GpiQueryFonts (hps, QF_PUBLIC, NULL,
                                  &lRequestFonts, 0L, NULL);

/* get metrics of fonts */
    if ((pfm = AllocMemPM ((SHORT) lNumberFonts * sizeof (FONTMETRICS))) == NULL)
    {
        WinReleasePS (hps);
        return NULL;
    }

    GpiQueryFonts (hps, QF_PUBLIC, NULL, &lNumberFonts,
                   (LONG) sizeof (FONTMETRICS), pfm);

    WinReleasePS (hps);

/* search for fonts appropriate to display device */
    for (lIndex = 0; lIndex < lNumberFonts; lIndex++)
        if (pfm[lIndex].sXDeviceRes == (SHORT) lHorzRes &&
            pfm[lIndex].sYDeviceRes == (SHORT) lVertRes &&
            (pfm[lIndex].fsType & FM_TYPE_FIXED))
        {
            if ((pfontNew = AllocMemPM (sizeof (FONT))) == NULL)
                break;

/* link to list */
            pfontNew->pfontNext = pfontCurrent;
            pfontCurrent = pfontNew;

/* copy attributes */
            pfontNew->fattrs.lMatch = pfm[lIndex].lMatch;
            strcpy (pfontNew->fattrs.szFacename, pfm[lIndex].szFacename);
            pfontNew->fattrs.idRegistry = pfm[lIndex].idRegistry;
            pfontNew->fattrs.usCodePage = pfm[lIndex].usCodePage;
            pfontNew->fattrs.lMaxBaselineExt = pfm[lIndex].lMaxBaselineExt;
            pfontNew->fattrs.lAveCharWidth = pfm[lIndex].lAveCharWidth;
            pfontNew->fattrs.usRecordLength = sizeof (FATTRS);
            pfontNew->fattrs.fsSelection = 0;
            pfontNew->fattrs.fsType = 0;
            pfontNew->usPointSize = pfm[lIndex].sNominalPointSize / 10;
/* speed up printing - tell PM font will not interfere with graphics */
            pfontNew->fattrs.fsFontUse = FATTR_FONTUSE_NOMIX;

/* if it's the user's preferred font, use it and stop looking for others */
            if (!strcmp(pfontNew->fattrs.szFacename, uoUserOptions.szFacename) &&
                pfontNew->usPointSize == uoUserOptions.usPointSize)
            {
                pfontSelected = pfontNew;
                pfontNew->fattrs.fsSelection = uoUserOptions.fsFontSelection;
                usSmallestSize = 0;
            }

/* if it's the smallest found yet, use it */
            if (pfontNew->usPointSize < usSmallestSize)
            {
                pfontSelected = pfontNew;
                usSmallestSize = pfontNew->usPointSize;
            }
        }

    DosFreeSeg (SELECTOROF (pfm));

    return pfontCurrent;
} /* end of InitFont () */


MRESULT EXPENTRY AboutDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/***************************************************************************/
/* Simple dialog procedure. Prints some information about the program, and */
/* exits when the 'OK' button is pressed.                                  */
/***************************************************************************/

{

    switch (msg)
    {
    case WM_COMMAND:
        switch (COMMANDMSG (&msg)->cmd)
        {
        case DID_OK:
            WinDismissDlg (hwnd, TRUE);
            return 0;

        default:
            break;
        }

    default:
        break;
    }

    return WinDefDlgProc (hwnd, msg, mp1, mp2);
}


MRESULT EXPENTRY ActualColoursProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/**********************************************************************/
/* Displays the actual and default colours used, with an 'OK' button. */
/**********************************************************************/

{
    USHORT usColourIndex;
    LONG lUserColour;
    char szString[30];

    switch (msg)
    {
    case WM_INITDLG:
        for (usColourIndex = 0; usColourIndex < 8; usColourIndex++)
        {
            lUserColour = uoUserOptions.lColour[usColourIndex];

            if (lUserColour == CLR_WHITE)
            {
                lUserColour = 0L;
            }
            else if (lUserColour == CLR_BLACK)
            {
                lUserColour = 7L;
            }

            WinLoadString (hab, (HMODULE) NULL, SID_POSSIBLE_BASE + (USHORT)lUserColour, 30, szString);
            WinSetDlgItemText (hwnd, DID_ACTUAL_COLOURS + usColourIndex, szString);
        }
        return 0;

    case WM_COMMAND:
        switch (COMMANDMSG (&msg)->cmd)
        {
        case DID_OK:
            WinDismissDlg (hwnd, TRUE);
            return 0;

        default:
            break;
        }

    default:
        break;
    }

    return WinDefDlgProc (hwnd, msg, mp1, mp2);
}


void InitialiseHelp ()
/****************************************/
/* function : Sets up help information. */
/****************************************/

{
    HELPINIT helpinit;

    helpinit.cb = sizeof helpinit;
    helpinit.pszTutorialName = NULL;
    helpinit.phtHelpTable = (PHELPTABLE) (0xFFFF0000 | ID_CONTRAST);
    helpinit.hmodHelpTableModule = (HMODULE)NULL;
    helpinit.hmodAccelActionBarModule = (HMODULE)NULL;
    helpinit.idAccelTable = 0;
    helpinit.idActionBar = 0;
    helpinit.pszHelpWindowTitle = "Contrast Help Window";
    helpinit.usShowPanelId = CMIC_HIDE_PANEL_ID;
    helpinit.pszHelpLibraryName = "contrast.hlp";

    hwndHelpInstance = WinCreateHelpInstance (hab, &helpinit);
}


MRESULT EXPENTRY OpenDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/*************************************************************************/
/* Dialog procedure to get two file names from the user. As close to the */
/* spirit of CUA as a double file dialog will get.                       */
/*************************************************************************/

{

    PSZ szName, szPath, szEnd, szPath2;
    HWND hwndFile, hwndDir;
    USHORT id;

    switch (msg)
    {
    case WM_INITDLG:
        WinAssociateHelpInstance (hwndHelpInstance, hwnd);

        if ((szPath = AllocMemPM (usMaxPathLength)) == NULL)
        {
            WinSendMsg (hwnd, WM_CLOSE, NULL, NULL);
            return 0;
        }

/* split full path of A into subdirectory & name */
        szName = strrchr (szFile[0], '\\') + 1;
        WinSetDlgItemText (hwnd, DID_FILEA, szName);
        strcpy (szPath, "");
        strncat (szPath, szFile[0], szName - szFile[0]);
        WinSetDlgItemText (hwnd, DID_CUR_DIR_A, szPath);

/* fill listboxes for this subdirectory */
        hwndFile = WinWindowFromID (hwnd, DID_FILES_A);
        hwndDir = WinWindowFromID (hwnd, DID_DIRS_A);
        FillOpenListBoxes (szPath, hwndFile, hwndDir);

/* do the same for B */
        szName = strrchr (szFile[1], '\\') + 1;
        WinSetDlgItemText (hwnd, DID_FILEB, szName);
        strcpy (szPath, "");
        strncat (szPath, szFile[1], szName - szFile[1]);
        WinSetDlgItemText (hwnd, DID_CUR_DIR_B, szPath);

        hwndFile = WinWindowFromID (hwnd, DID_FILES_B);
        hwndDir = WinWindowFromID (hwnd, DID_DIRS_B);
        FillOpenListBoxes (szPath, hwndFile, hwndDir);

        DosFreeSeg (SELECTOROF(szPath));

        return 0;

    case WM_COMMAND:
        switch (COMMANDMSG (&msg)->cmd)
        {
         case DID_OK:
/* the user wants these files to be contrasted - get the names */
            if (((szPath = AllocMemPM (usMaxPathLength)) == NULL) ||
                ((szPath2 = AllocMemPM (usMaxPathLength)) == NULL) ||
                ((szName = AllocMemPM (usMaxPathLength)) == NULL))
            {
                WinDismissDlg (hwnd, FALSE);
                return 0;
            }

            WinQueryDlgItemText (hwnd, DID_FILEA, usMaxPathLength, szName);
            if (szName[1] != ':')
            {
                WinQueryDlgItemText (hwnd, DID_CUR_DIR_A, usMaxPathLength, szPath);
                strcat (szPath, szName);
            }
            else
            {
                strcpy (szPath, szName);
            }

            WinQueryDlgItemText (hwnd, DID_FILEB, usMaxPathLength, szName);
            if (szName[0] == '\0')
            {
                WinQueryDlgItemText (hwnd, DID_FILEA, usMaxPathLength, szName);
            }

            if (szName[1] != ':')
            {
                WinQueryDlgItemText (hwnd, DID_CUR_DIR_B, usMaxPathLength, szPath2);
                strcat (szPath2, szName);
            }
            else
            {
                strcpy (szPath2, szName);
            }


            if (stricmp (szPath, szPath2) != 0)
            {
                strcpy (szFile[0], szPath);
                strcpy (szFile[1], szPath2);
                WinDismissDlg (hwnd, TRUE);
            }

            DosFreeSeg (SELECTOROF(szName));
            DosFreeSeg (SELECTOROF(szPath));
            DosFreeSeg (SELECTOROF(szPath2));

            return 0;

       case DID_CANCEL:
/* leave dialog with no permanent changes */
            WinDismissDlg (hwnd, FALSE);
            return 0;

        case DID_HELP:
            WinSendMsg (hwndHelpInstance, HM_EXT_HELP, 0L, 0L);
            return 0;

        default:
            break;
        }

    case WM_CONTROL:

        if (((szPath = AllocMemPM (usMaxPathLength)) == NULL) ||
            ((szName = AllocMemPM (usMaxPathLength)) == NULL))
        {
            WinDismissDlg (hwnd, FALSE);
            return 0;
        }

        switch (SHORT2FROMMP(mp1))
        {
        case LN_SELECT: /* same value as EN_SETFOCUS */
            switch (SHORT1FROMMP(mp1))
            {
            case DID_FILES_A:
            case DID_FILES_B:
/* set the appropriate file entry field to the current selection */
                WinSendMsg ((HWND)mp2,
                            LM_QUERYITEMTEXT,
                            MPFROM2SHORT(SHORT1FROMMP(WinSendMsg ((HWND)mp2,
                                                                  LM_QUERYSELECTION,
                                                                  MPFROMSHORT(LIT_FIRST),
                                                                  NULL)),
                                         usMaxPathLength),
                            (MPARAM)szName);

                id = SHORT1FROMMP(mp1) - DID_FILES_A + DID_FILEA;
                WinSetDlgItemText (hwnd, id, szName);
                break;

            case DID_FILEA: /* really EN_SETFOCUS */
/* if this is the first time here, subclass the field to produce a DID_OK
   message when enter is pressed, and limit the test length. We do this here
   because it seems to be the 1st point at which the handle to the field is
   guaranteed OK. */
                if (!fEntryFieldASubclassed)
                {
                    WinSendMsg (HWNDFROMMP(mp2), EM_SETTEXTLIMIT,
                                MPFROMSHORT (usMaxPathLength), NULL);
                    pfnOldEntryFieldProc = WinSubclassWindow (HWNDFROMMP(mp2),
                                                              MyEntryFieldProc);
                    fEntryFieldASubclassed = TRUE;
                }
                break;

            case DID_FILEB:
/* subclass this field, and limit it. */
                if (!fEntryFieldBSubclassed)
                {
                    WinSendMsg (HWNDFROMMP(mp2), EM_SETTEXTLIMIT,
                                MPFROMSHORT (usMaxPathLength), NULL);
                    WinSubclassWindow (HWNDFROMMP(mp2), MyEntryFieldProc);
                    fEntryFieldBSubclassed = TRUE;
                }
                break;

            default:
                break;
            }

            break;

        case LN_ENTER:
            switch (SHORT1FROMMP(mp1))
            {
            case DID_DIRS_A:
            case DID_DIRS_B:
/* get the selection */
                WinSendMsg ((HWND)mp2,
                            LM_QUERYITEMTEXT,
                            MPFROM2SHORT(SHORT1FROMMP(WinSendMsg ((HWND)mp2,
                                                                  LM_QUERYSELECTION,
                                                                  MPFROMSHORT(LIT_FIRST),
                                                                  NULL)),
                                         usMaxPathLength),
                            (MPARAM)szPath);

/* if it's a drive, just get the drive letter from it */
                if (szPath[2] == ':')
                {
                    strcpy (szName, "C:\\");
                    szName[0] = szPath[1];
                }
                else
                {
                    id = SHORT1FROMMP(mp1) - DID_DIRS_A + DID_CUR_DIR_A;
                    WinQueryDlgItemText (hwnd, id, usMaxPathLength, szName);

                    if (strcmp (szPath, "..") == 0)
                    {
/* remove last subdirectory; start at the end of the path, skip the null, the
   last '\', and one character, and change the next '\' to a null */
                        szEnd = szName + strlen(szName) - 3;
                        while (*szEnd != '\\')
                        {
                            szEnd--;
                        }
                        *szEnd = '\0';
                    }
                    else
                    {
/* add subdirectory to current directory */
                        strcat (szName, szPath);
                    }

                    strcat (szName, "\\");
                }

/* put the directory in the text field */
                id = SHORT1FROMMP(mp1) - DID_DIRS_A + DID_CUR_DIR_A;
                WinSetDlgItemText (hwnd, id, szName);

/* refresh the listboxes */
                hwndFile = WinWindowFromID (hwnd, SHORT1FROMMP(mp1) - DID_DIRS_A + DID_FILES_A);
                hwndDir = WinWindowFromID (hwnd, SHORT1FROMMP(mp1));
                WinSendMsg (hwndFile, LM_DELETEALL, NULL, NULL);
                WinSendMsg (hwndDir, LM_DELETEALL, NULL, NULL);
                FillOpenListBoxes (szName, hwndFile, hwndDir);

/* use a null string as the starting filename for A; use filename A for B */
                if (SHORT1FROMMP(mp1) == DID_DIRS_A)
                {
                    strcpy(szName, "");
                }
                else
                {
                    WinQueryDlgItemText (hwnd, DID_FILEA, usMaxPathLength, szName);
                }

                id = SHORT1FROMMP(mp1) - DID_DIRS_A + DID_FILEA;
                WinSetDlgItemText (hwnd, id, szName);

                break;

            case DID_FILES_A:
            case DID_FILES_B:
/* get the full file paths, and get out of here */
                if (((szPath2 = AllocMemPM (usMaxPathLength)) == NULL))
                {
                    WinDismissDlg (hwnd, FALSE);
                    return 0;
                }

                WinQueryDlgItemText (hwnd, DID_FILEA, usMaxPathLength, szName);
                if (szName[1] != ':')
                {
                    WinQueryDlgItemText (hwnd, DID_CUR_DIR_A, usMaxPathLength, szPath);
                    strcat (szPath, szName);
                }
                else
                {
                    strcpy (szPath, szName);
                }

                WinQueryDlgItemText (hwnd, DID_FILEB, usMaxPathLength, szName);
                if (szName[0] == '\0')
                {
                    WinQueryDlgItemText (hwnd, DID_FILEA, usMaxPathLength, szName);
                }

                if (szName[1] != ':')
                {
                    WinQueryDlgItemText (hwnd, DID_CUR_DIR_B, usMaxPathLength, szPath2);
                    strcat (szPath2, szName);
                }
                else
                {
                    strcpy (szPath2, szName);
                }

                DosFreeSeg (SELECTOROF(szName));

                if (stricmp (szPath, szPath2) != 0)
                {
                    strcpy (szFile[0], szPath);
                    strcpy (szFile[1], szPath2);
                    WinDismissDlg (hwnd, TRUE);
                }

                DosFreeSeg (SELECTOROF(szPath2));

                break;

            default:
                break;
            }

            break;

        default:
            break;

        }

        DosFreeSeg (SELECTOROF(szPath));
        DosFreeSeg (SELECTOROF(szName));
        return 0;

    case HM_INFORM:
        WinDlgBox (HWND_DESKTOP, HWND_DESKTOP, ActualColoursProc, (HMODULE)NULL, DID_SHOW_COLOURS, NULL);
        return 0;

    default:
        break;
    }

    return WinDefDlgProc (hwnd, msg, mp1, mp2);
} /* end of OpenDlgProc () */


void FillOpenListBoxes (PSZ szPath, HWND hwndFileList, HWND hwndDirList)
/**************************************************************************/
/* expected parameters: szPath = full path of current directory (input)   */
/*                      hwndFileList = handle of File listbox (input)     */
/*                      hwndDirList = handle of Direectory listbox (input)*/
/**************************************************************************/

{
    HDIR hdir = 0xffff;
    MYFILEFINDBUF FileFindBuf;
    USHORT cFileNum = 1, usDriveNumber;
    ULONG ulLogicalDriveMap;
    char szDrive[5];

/* DosFindFirst needs a wildcard to search for files */
    strcat (szPath, "*");

    if (!DosFindFirst (szPath, &hdir, 0x10, &FileFindBuf, sizeof(FileFindBuf), &cFileNum, 0L))
    {
        if (FileFindBuf.real.attrFile & 0x10)
        {
/* subdirectory; put in directory listbox */
            if (strcmp (FileFindBuf.real.achName, "."))
            {
/* don't add "." to list */
                WinSendMsg (hwndDirList, LM_INSERTITEM,
                            MPFROMSHORT (LIT_SORTASCENDING), FileFindBuf.real.achName);
            }
        }
        else
        {
            WinSendMsg (hwndFileList, LM_INSERTITEM,
                        MPFROMSHORT (LIT_SORTASCENDING), FileFindBuf.real.achName);
        }

        while (!DosFindNext (hdir, &FileFindBuf, sizeof(FileFindBuf), &cFileNum))
        {
            if (FileFindBuf.real.attrFile & 0x10)
            {
                if (strcmp (FileFindBuf.real.achName, "."))
                {
                    WinSendMsg (hwndDirList, LM_INSERTITEM,
                                MPFROMSHORT (LIT_SORTASCENDING), FileFindBuf.real.achName);
                }
            }
            else
            {
                WinSendMsg (hwndFileList, LM_INSERTITEM,
                                   MPFROMSHORT (LIT_SORTASCENDING), FileFindBuf.real.achName);
            }
        }
    }

    DosFindClose (hdir);

/* add the drives to the directory listbox */
    DosQCurDisk (&usDriveNumber, &ulLogicalDriveMap);

    strcpy (szDrive, "[A:]");

/* check for each of the 26 possible drives */
    for (usDriveNumber = 'A'; usDriveNumber < 'Z'; usDriveNumber++)
    {
        if ((ulLogicalDriveMap & 1L) == 1)
        {
/* drive exists; is it the one currently named in the path? */
            if (usDriveNumber != szPath[0])
            {
                szDrive[1] = usDriveNumber;
                WinSendMsg (hwndDirList, LM_INSERTITEM,
                            MPFROMSHORT (LIT_SORTASCENDING), szDrive);

            }
        }

        ulLogicalDriveMap = ulLogicalDriveMap >> 1;
    }

    return;

} /* end of FillOpenListBoxes () */


MRESULT EXPENTRY SetColoursProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/*********************************************************/
/* Dialog procedure to allow the user to specify colours */
/*********************************************************/

{
    LONG lUserColour;
    static LONG lNewColour[8];
    USHORT usSampleIndex, usColourSel, usStringIndex;
    char szString[30], szFontSizeName[60];
    HWND hwndSample;

    switch (msg)
    {
    case WM_INITDLG:
        WinAssociateHelpInstance (hwndHelpInstance, hwnd);

/* put current colours in candidate colours */
        CopyColours (uoUserOptions.lColour, lNewColour);

/* put use-of-colour names in listbox */
        for (usStringIndex = 0; usStringIndex < 8; usStringIndex++)
        {
            WinLoadString (hab, (HMODULE) NULL, SID_USED_BASE + usStringIndex, 30, szString);
            WinSendDlgItemMsg (hwnd, DID_COLOURS_USED, LM_INSERTITEM,
                               MPFROMSHORT (LIT_END), szString);
        }

        WinSendDlgItemMsg (hwnd, DID_COLOURS_USED, LM_SELECTITEM,
                           (MPARAM)0, (MPARAM)TRUE);

/* The list of available colours is, due to the strange design of PM, itself strange.
   For most colours, their position in the listbox gives their index. However, CLR_WHITE
   and CLR_BLACK have negative indices, so they have been put at positions 0 and 7
   respectively. If I used CLR_BACKGROUND and CLR_NEUTRAL the coding would be simpler,
   but changing the default colours in the Control Panel would affect them, and I don't
   want that (nor do I want to try to explain it to a user). It thus depends on the
   colour constants staying the same.
*/

/* put possible-colour names in listbox */
        for (usStringIndex = 0; usStringIndex < 16; usStringIndex++)
        {
            WinLoadString (hab, (HMODULE) NULL, SID_POSSIBLE_BASE + usStringIndex, 30, szString);
            WinSendDlgItemMsg (hwnd, DID_POSSIBLE_COLOURS, LM_INSERTITEM,
                           MPFROMSHORT (LIT_END), szString);
        }

/* set the selection of the possible-colours listbox */
        lUserColour = lNewColour[0];

        if (lUserColour == CLR_WHITE)
        {
            lUserColour = 0L;
        }
        else if (lUserColour == CLR_BLACK)
        {
            lUserColour = 7L;
        }

        WinSendDlgItemMsg (hwnd, DID_POSSIBLE_COLOURS, LM_SELECTITEM,
                           MPFROMLONG(lUserColour), (MPARAM)TRUE);

/* set foreground & background of sample texts, & the font */
        sprintf (szFontSizeName, "%u.%s", pfontSelected->usPointSize,
                                          pfontSelected->fattrs.szFacename);

        for (usSampleIndex = 0; usSampleIndex < 14; usSampleIndex++)
        {

            hwndSample = WinWindowFromID (hwnd, DID_F1_BA + usSampleIndex);

            WinSetPresParam (hwndSample,
                             PP_FOREGROUNDCOLORINDEX,
                             4,
                             &lNewColour[usSampleIndex / 3]);

            WinSetPresParam (hwndSample,
                             PP_BACKGROUNDCOLORINDEX,
                             4,
                             &lNewColour[usSampleIndex % 3 + 5]);

            WinSetPresParam (hwndSample,
                             PP_FONTNAMESIZE,
                             sizeof (szFontSizeName),
                             szFontSizeName);

        }

        return 0;

    case WM_COMMAND:
        switch (COMMANDMSG (&msg)->cmd)
        {
        case DID_OK:
/* use the candidate colours */
            CopyColours (lNewColour, uoUserOptions.lColour);

            WinDismissDlg (hwnd, TRUE);
            return 0;

        case DID_CANCEL:
/* exit with no changes */
            WinDismissDlg (hwnd, FALSE);
            return 0;

        case DID_HELP:

            WinSendMsg (hwndHelpInstance, HM_EXT_HELP, 0L, 0L);
            return 0;

        case DID_DEFAULT:
/* put the defaults to the candidate colours */
            CopyColours (lDefColour, lNewColour);

/* reset the selection of the possible-colours listbox */
            lUserColour = lNewColour[SHORT1FROMMR(WinSendDlgItemMsg (hwnd,
                                                                    DID_COLOURS_USED,
                                                                    LM_QUERYSELECTION,
                                                                    NULL,
                                                                    NULL))];

            if (lUserColour == CLR_WHITE)
            {
                lUserColour = 0L;
            }
            else if (lUserColour == CLR_BLACK)
            {
                lUserColour = 7L;
            }

            WinSendDlgItemMsg (hwnd, DID_POSSIBLE_COLOURS, LM_SELECTITEM,
                               MPFROMLONG(lUserColour), (MPARAM)TRUE);

/* reset the colours of the sample texts */
        for (usSampleIndex = 0; usSampleIndex < 14; usSampleIndex++)
        {
            hwndSample = WinWindowFromID (hwnd, DID_F1_BA + usSampleIndex);

            WinSetPresParam (hwndSample,
                             PP_FOREGROUNDCOLORINDEX,
                             4,
                             &lNewColour[usSampleIndex / 3]);

            WinSetPresParam (hwndSample,
                             PP_BACKGROUNDCOLORINDEX,
                             4,
                             &lNewColour[usSampleIndex % 3 + 5]);
        }

            return 0;

        default:
            break;
        }

    case WM_CONTROL:
        switch (SHORT2FROMMP(mp1))
        {
        case LN_ENTER:
/* exit using the newly-set colours */
            CopyColours (lNewColour, uoUserOptions.lColour);

            WinDismissDlg (hwnd, TRUE);
            break;

        case LN_SELECT:

            if (SHORT1FROMMP(mp1) == DID_COLOURS_USED)
            {
/* reset the selection of the possible-colours listbox */
                lUserColour = lNewColour[SHORT1FROMMR(WinSendMsg (mp2,
                                                             LM_QUERYSELECTION,
                                                             NULL,
                                                             NULL))];

                if (lUserColour == CLR_WHITE)
                {
                    lUserColour = 0L;
                }
                else if (lUserColour == CLR_BLACK)
                {
                    lUserColour = 7L;
                }

                WinSendDlgItemMsg (hwnd, DID_POSSIBLE_COLOURS, LM_SELECTITEM,
                                   MPFROMLONG(lUserColour), (MPARAM)TRUE);
            }
            else if (SHORT1FROMMP(mp1) == DID_POSSIBLE_COLOURS)
            {
/* set the selected used-colour to the selected possible-colour */
                lUserColour = (LONG)WinSendMsg (mp2, LM_QUERYSELECTION, NULL, NULL);

                if (lUserColour == 0L)
                {
                    lUserColour = CLR_WHITE;
                }
                else if (lUserColour == 7L)
                {
                    lUserColour = CLR_BLACK;
                }

                usColourSel = SHORT1FROMMR(WinSendDlgItemMsg (hwnd,
                                                         DID_COLOURS_USED,
                                                         LM_QUERYSELECTION,
                                                         NULL,
                                                         NULL));

                lNewColour[usColourSel] = lUserColour;

/* reset the colours of the affected samples */
                if (usColourSel > 4)
                {
                    for (usSampleIndex = usColourSel - 5;
                         usSampleIndex < 14;
                         usSampleIndex += 3)
                    {
                        WinSetPresParam (WinWindowFromID (hwnd, DID_F1_BA + usSampleIndex),
                                         PP_BACKGROUNDCOLORINDEX,
                                         4,
                                         &lNewColour[usColourSel]);
                    }
                }
                else
                {
                    for (usSampleIndex = usColourSel * 3;
                         usSampleIndex < (usColourSel + 1) * 3 - usColourSel / 4;
                         usSampleIndex++)
                    {
                        WinSetPresParam (WinWindowFromID (hwnd, DID_F1_BA + usSampleIndex),
                                         PP_FOREGROUNDCOLORINDEX,
                                         4,
                                         &lNewColour[usColourSel]);
                    }

                }
            }

            break;

        default:
            break;
        }

        return NULL;

    case HM_INFORM:
        WinDlgBox (HWND_DESKTOP, HWND_DESKTOP, ActualColoursProc, (HMODULE)NULL, DID_SHOW_COLOURS, NULL);
        return 0;

    default:
        break;
    }

    return WinDefDlgProc (hwnd, msg, mp1, mp2);
}


void CopyColours (PLONG lFromColour, PLONG lToColour)
/**********************************************/
/* copies one set of colour values to another */
/**********************************************/

{
    USHORT usColIndex;

    for (usColIndex = 0; usColIndex < 8; usColIndex++)
    {
        lToColour[usColIndex] = lFromColour[usColIndex];
    }

}



USHORT MakeFullPath (PSZ szFileName)
/*****************************************************************************/
/* expected parameter : szFileName = file name to be expanded (input/output) */
/* return value: 0 = success                                                 */
/*               1 = no such file or directory                               */
/*               2 = failure (no memory)                                     */
/*                                                                           */
/* function : expands file name to a full directory path, including drive    */
/*****************************************************************************/

{
    PSZ szParse, szTemp, szCurPath;
    USHORT usDisk, usAttr;
    ULONG ulDriveMap;

    if ((szCurPath = AllocMemPM (usMaxPathLength)) == NULL ||
        (szTemp = AllocMemPM (usMaxPathLength)) == NULL)
        return 2;

/* convert to upper case - looks better, since OS/2 returns paths in upper
   case, and makes drive calculation easier */
    szParse = strupr(szFileName);

/* if a drive was supplied, use it; if not, use the current one */
    if (szParse[1] == ':')
    {
        usDisk = *szParse - 'A' + 1;
        szParse += 2;
    }
    else
    {
        DosQCurDisk (&usDisk, &ulDriveMap);
    }

/* if the path isn't specified from the root directory, use the current one */
    if (*szParse != '\\')
    {
        DosQCurDir (usDisk, szTemp, &usMaxPathLength);
        sprintf (szCurPath, "\\%s",szTemp);

/* don't add backslash if either current directory or rest of parameter are null */
        if (*szTemp != '\0' && *szParse != '\0')
            strcat (szCurPath, "\\");
    }
    else
    {
        strcpy (szCurPath, "");
    }

    strcpy (szTemp, szParse);
    sprintf (szFileName, "%c:%s%s", usDisk + 'A' - 1, szCurPath, szTemp);

    DosFreeSeg (SELECTOROF (szCurPath));

/* remove any "\dir\.." found in the string */
    while ((szParse = strstr (szFileName, "..")) != NULL)
    {
/* save rest of string after ".." */
        strcpy (szTemp, szParse + 2);

/* go back to before preceding '\' & 1 character */
        szParse -= 3;

/* look for preceding '\' and change to null character to truncate string */
        while (*szParse != '\\')
            szParse--;
        *szParse = '\0';

/* add rest of string */
        strcat (szFileName, szTemp);
    }

    DosFreeSeg (SELECTOROF (szTemp));

/* if the user has specified a sub-directory, return at once */
    if (szFileName[strlen (szFileName) - 1] == '\\')
    {
        return 0;
    }

/* If it is just a path, append a backslash. */

/* get file attributes */
    if (DosQFileMode (szFileName, &usAttr, 0L))
/* we can't find it, so default to the current directory, & say we need
   the dialog */
    {
        sprintf (pWriteLine,"Couldn't find %s",szFileName);
        ShowError (pWriteLine);
        if (GetDefaultDir (szFileName))
            return 1;
        else
            return 2;
    }
    else
    {
/* check to see if subdirectory */
        if (usAttr & 0x10)
        {
            strcat (szFileName, "\\");
        }
    }

    return 0;
} /* end of MakeFullPath () */


BOOL GetDefaultDir (PSZ szDefDir)
/*************************************************************/
/* expected parameter: szDefDir = current directory (output) */
/* return value: TRUE = success                              */
/*               FALSE = failure (no memory)                 */
/*************************************************************/

{

    USHORT usDisk;
    ULONG ulDriveMap;
    PSZ szDir;

    if ((szDir = AllocMemPM (usMaxPathLength)) == NULL)
    {
        return FALSE;
    }

    DosQCurDisk (&usDisk, &ulDriveMap);
    DosQCurDir (usDisk, szDir, &usMaxPathLength);
    sprintf (szDefDir, "%c:\\%s", usDisk - 1 + 'A', szDir);
    if (szDir[0] != '\0')
    {
        strcat (szDefDir, "\\");
    }

    DosFreeSeg (SELECTOROF(szDir));
    return TRUE;
} /* end of GetDefaultDir () */



MRESULT EXPENTRY FontsDlgProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/***************************************************************************/
/* allows different fonts to be selected                                   */
/***************************************************************************/

{
    static PFONT pfontNew;
    char szFontName[60];
    int iFontSel, iFontIndex;

    switch (msg)
    {
    case WM_INITDLG:
        WinAssociateHelpInstance (hwndHelpInstance, hwnd);

        iFontSel = 0;
        iFontIndex = 0;

/* walk list of fonts (created in InitialiseFonts ()) to get font names and
   sizes, and select the current font in the listbox */

        for (pfontNew = pfontHead; pfontNew != NULL; pfontNew = pfontNew->pfontNext)
        {
            sprintf (szFontName, "%s %u", pfontNew->fattrs.szFacename,
                                          pfontNew->usPointSize);

            WinSendDlgItemMsg (hwnd, DID_FONTS_AVAILABLE, LM_INSERTITEM,
                           MPFROMSHORT (LIT_END), szFontName);

            if (pfontNew == pfontSelected)
            {
                iFontSel = iFontIndex;
            }
            iFontIndex++;

        }

        WinSendDlgItemMsg (hwnd,
                           DID_FONTS_AVAILABLE,
                           LM_SELECTITEM,
                           MPFROMSHORT(iFontSel),
                           (MPARAM)TRUE);

/* from here on in the dialog, pfontNew is the candidate font */
        pfontNew = pfontSelected;

        return 0;

    case WM_COMMAND:
        switch (COMMANDMSG (&msg)->cmd)
        {
        case DID_OK:
/* set current font to new one */
            pfontSelected = pfontNew;

            WinDismissDlg (hwnd, TRUE);
            return 0;

        case DID_CANCEL:
/* return without changing the font */
            WinDismissDlg (hwnd, FALSE);
            return 0;

        case DID_HELP:

            WinSendMsg (hwndHelpInstance, HM_EXT_HELP, 0L, 0L);
            return 0;

        default:
            break;
        }

    case WM_CONTROL:
        switch (SHORT2FROMMP(mp1))
        {
/* PM assigns the same value (1) to LN_SELECT and BN_CLICKED, both of which
   we want to respond to, so we use one case statement for both, and check
   the ID of the control (which we really ought to do anyway) */

        case LN_SELECT:
            if (SHORT1FROMMP(mp1) == DID_FONTS_AVAILABLE)
            {
/* this really is a LN_SELECT message from the listbox, so find which item
   was selected */
                iFontSel = SHORT1FROMMR (WinSendMsg (mp2,
                                                     LM_QUERYSELECTION,
                                                     NULL,
                                                     NULL));

/* walk through the linked list of fonts to the correct one */
                pfontNew = pfontHead;

                for (iFontIndex = 0; iFontIndex < iFontSel; iFontIndex++)
                {
                    pfontNew = pfontNew->pfontNext;
                }

/* set the state of the bold & italic checkboxes to those last requested for
   this font */
                WinSendDlgItemMsg (hwnd,
                                   DID_BOLD,
                                   BM_SETCHECK,
                                   MPFROMSHORT ((pfontNew->fattrs.fsSelection & FATTR_SEL_BOLD) == FATTR_SEL_BOLD),
                                   NULL);

                WinSendDlgItemMsg (hwnd,
                                   DID_ITALIC,
                                   BM_SETCHECK,
                                   MPFROMSHORT ((pfontNew->fattrs.fsSelection & FATTR_SEL_ITALIC) == FATTR_SEL_ITALIC),
                                   NULL);
            }
            else
            {
/* this is really a BN_CLICKED message; toggle the state of the font
   characteristics, according to which checkbox it was */
                if (SHORT1FROMMP(mp1) == DID_BOLD)
                    pfontNew->fattrs.fsSelection ^= FATTR_SEL_BOLD;
                else
                    pfontNew->fattrs.fsSelection ^= FATTR_SEL_ITALIC;

/* toggle the state of the checkbox */
                WinSendDlgItemMsg (hwnd,
                                   SHORT1FROMMP(mp1),
                                   BM_SETCHECK,
                                   MPFROMSHORT(SHORT1FROMMR(WinSendDlgItemMsg (hwnd,
                                                                               SHORT1FROMMP(mp1),
                                                                               BM_QUERYCHECK,
                                                                               NULL,
                                                                               NULL)) ^ 1),
                                   NULL);
            }
            break;

        default:
            break;
        }

        return NULL;

    case HM_INFORM:
        WinDlgBox (HWND_DESKTOP, HWND_DESKTOP, ActualColoursProc, (HMODULE)NULL, DID_SHOW_COLOURS, NULL);
        return 0;

    default:
        break;
    }

    return WinDefDlgProc (hwnd, msg, mp1, mp2);
} /* end of FontsDlgProc () */


MRESULT EXPENTRY MyEntryFieldProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
/**************************************************************************/
/* subclasses the entry fields for the Open dialog, so a return causes a  */
/* DID_OK message. This is because we can't use a DEFPUSHBUTTON, since it */
/* ends the dialog when return is pressed in the directory list.          */
/**************************************************************************/
{
    if (msg == WM_CHAR &&
        (SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_KEYUP)) == KC_VIRTUALKEY &&
        (SHORT2FROMMP(mp2) == VK_ENTER) || (SHORT2FROMMP(mp2) == VK_NEWLINE))
    {
        WinPostMsg (WinQueryWindow (hwnd, QW_PARENT, 0),
                    WM_COMMAND,
                    MPFROMSHORT (DID_OK),
                    MPFROM2SHORT (CMDSRC_PUSHBUTTON, FALSE));
    }
    return pfnOldEntryFieldProc (hwnd, msg, mp1, mp2);
}


PVOID AllocMemPM (USHORT usSize)
/* allocates memory for PM thread */

{
    SEL Selector;
    char buffer[24];

    if (DosAllocSeg (usSize, &Selector, 0))
    {
        strcpy (buffer, "can't allocate memory");
        ShowError (buffer);
        return NULL;
    }
    return MAKEP (Selector, 0);

} /* end of AllocMemPM () */



/************************************************************/
/* The following functions are in the second (table) thread */
/************************************************************/

void cdecl far MakeTables (void far *dummy)
{
    while (fStartTables == TRUE)
    {
        DosSemSet (hsemClearTables);

        switch (CreateTables ())
        {
        case 0:
            WinPostMsg (hwndClient, UM_FAIL, NULL, NULL);
            break;

        case 1:
            WinPostMsg (hwndClient, UM_TABLES_MADE, NULL, NULL);
            break;

        default:
            break;
        }

        DosSemWait (hsemClearTables, -1L);

        ReleaseMemory ();
    }

    _endthread ();
}


SHORT CreateTables (void)
/***********************************************************************/
/* return value: 0 = failure                                           */
/*               1 = success                                           */
/*               2 = interrupted                                       */
/*                                                                     */
/* function: Reads in files to memory and creates hash and line tables */
/***********************************************************************/

{
    HFILE hFileA, hFileB; /* files opened by OpenFile, closed by ReadFile */
    static ULONG ulFileLengthA, ulFileLengthB, ulStartFileB, ulTotalLength;
    SHORT sArraySize;
    PSZ szDefFileName;

    if (fLoadNewFiles == TRUE)
    {
/* Check if second file is just a path, i.e. the user wants to use the
   same filename as the first file. If so, extract & append the filename. */
        if (szFile[1][strlen (szFile[1]) - 1] == '\\')
        {
            szDefFileName = strrchr (szFile[0], '\\') + 1;
            strcat (szFile[1], szDefFileName);
        }

        if ((ulFileLengthA = OpenFile (szFile[0], &hFileA)) == 0L)
            return 0;

/* work out space needed, including gap so second file is aligned on a
   32768 byte boundary
*/
    ulTotalLength = ulStartFileB = (ulFileLengthA / 32768 + 1) * 32768;

        if ((ulFileLengthB = OpenFile (szFile[1], &hFileB)) == 0L)
            return 0;

        ulTotalLength += ulFileLengthB;

        if (DosAllocHuge ((USHORT) (ulTotalLength / 65536L),
                          (USHORT) (ulTotalLength % 65536L),
                          &selBase,
                          0,
                          0))
        {
            strcpy (pWriteLine, "can't allocate memory for files");
            return 0;
        }

/* get selector shift for access to huge memory */
        DosGetHugeShift (&usHugeShift);

        if (ReadFile (hFileA, 0L, ulFileLengthA) == 0)
        {
            strcat (pWriteLine, szFile[0]);
            return 0;
        }

        if (ReadFile (hFileB, ulStartFileB, ulTotalLength) == 0)
        {
            strcat (pWriteLine, szFile[1]);
            return 0;
        }
    }

/* allocate full 64k for start-of-line and hash-value arrays. We will shrink
   them to the correct size later. This means the files are only read once. In
   future, if we haven't enough memory for this, we should count the line and
   try to reallocate, so small comparisons might fit in. If we do, we should
   let the user know we're trying to use less memory */

    if (   (ulLine = AllocMem (0)) == NULL
        || (ulLineHash = AllocMem (0)) == NULL)
        return 0;

    sMaxLength = 0;

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    sNumLines[0] = MakeHashValues (0L, ulFileLengthA, 1);

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    sTotalLines = MakeHashValues (ulStartFileB, ulTotalLength, sNumLines[0] + 1);

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    sNumLines[1] = sTotalLines - sNumLines[0];


/* limit maximum length to 512, since PM won't print longer strings */
    sMaxLength = MIN (sMaxLength, 512);

    if (sTotalLines >= 16384)
    {
        strcpy (pWriteLine, "too many lines - must be less than 16K in total");
        return 0;
    }

    sArraySize = sTotalLines + 1;

/************************************************************/
/* reallocate memory for line hash and start of line tables */
/************************************************************/
    DosReallocSeg (sArraySize * sizeof (ULONG), SELECTOROF (ulLine));
    DosReallocSeg (sArraySize * sizeof (ULONG), SELECTOROF (ulLineHash));

/* Allocate memory for other tables */

    if (   (ltLinePair = AllocMem (sArraySize * sizeof (SHORT))) == NULL
        || (ltLinePrev = AllocMem (sArraySize * sizeof (SHORT))) == NULL
        || (ltLineNext = AllocMem (sArraySize * sizeof (SHORT))) == NULL
        || (usLineCol = AllocMem (sArraySize * sizeof (USHORT))) == NULL
        || (usBackCol = AllocMem (sArraySize * sizeof (USHORT))) == NULL
        || (HashTable = AllocMem (sTotalLines * sizeof (SHORT))) == NULL)
        return 0;

    ChainIdenticalLines ();

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    FindUniquePairs ();

    DosFreeSeg (SELECTOROF (HashTable));

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    ExpandUniquePairs ();

    DosFreeSeg (SELECTOROF (ulLineHash));

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

    SetColours ();

    if (DosSemWait (hsemClearTables, 0L) == 0)
        return 2;

/* make composite file */
    sNumLines[2] = 0;
    Composite (1, sNumLines[0], sNumLines[0] + 1, sTotalLines, 0, 0);

    return 1;
} /* end of CreateTables () */


void ReleaseMemory (void)
{
    if (fLoadNewFiles == TRUE)
        DosFreeSeg (selBase);

    DosFreeSeg (SELECTOROF (ulLine));
    DosFreeSeg (SELECTOROF (ltLinePrev));
    DosFreeSeg (SELECTOROF (ltLineNext));
    DosFreeSeg (SELECTOROF (ltLinePair));
    DosFreeSeg (SELECTOROF (usLineCol));
    DosFreeSeg (SELECTOROF (usBackCol));

} /* end of ReleaseMemory () */


ULONG OpenFile (PSZ szFileName, PHFILE pFileHandle)
/***********************************************************************/
/* expected parameters : szFileName = name of file (input)             */
/*                       pFileHandle = pointer to file handle (output) */
/* return value:     0 = failure                                       */
/*               other = length of file in bytes                       */
/***********************************************************************/

{
    USHORT usAction;
    FILESTATUS FileInfo;

    if (DosOpen (szFileName,
                 pFileHandle,
                 &usAction,             /* file exists/created/replaced  */
                 0L,                    /* file size (ignored)           */
                 1,                     /* attributes (read only)        */
                 1,                     /* open if exists, fail if not   */
                 32,                    /* deny write, ask for read-only */
                 0L))                   /* reserved                      */
    {
        strcpy (pWriteLine, "failed to open ");
        strcat (pWriteLine, szFileName);
        return 0L;
    }

/* get file length from file information */
    if (DosQFileInfo (*pFileHandle,
                      1,                    /* level of info   */
                      (PCH) &FileInfo,      /* buffer for info */
                      sizeof FileInfo ))    /* size of buffer  */
    {
        strcpy (pWriteLine, "failed to get information for ");
        strcat (pWriteLine, szFileName);
        return 0L;
    }

    return FileInfo.cbFile;

} /* end of OpenFile () */


SHORT ReadFile (HFILE FileHandle, ULONG ulStartMem, ULONG ulEndMem)
/***********************************************************************/
/* expected parameters : FileHandle = handle of file (input)           */
/*                       ulStartMem = start of memory for file (input) */
/*                       ulEndMem   = end of memory for file (input)   */
/* return value : 0 = failure                                          */
/*                1 = success                                          */
/*                                                                     */
/* function : reads in a file and closes it                            */
/***********************************************************************/

{
    USHORT usBytesRequested = 32768, usBytesRead;

/********************************************************************/
/* read in file in blocks half a segment long, so no overlap occurs */
/********************************************************************/
    do
    {
        if (ulEndMem < ulStartMem + 32768)
        {
            usBytesRequested = ulEndMem - ulStartMem;
        }

        if (DosRead (FileHandle, (PVOID)MEM (ulStartMem), usBytesRequested, &usBytesRead))
        {
            strcpy (pWriteLine, "error in reading ");
            DosClose (FileHandle);
            return 0;
        }

        ulStartMem += usBytesRead;
    } while (usBytesRead != 0);

    DosClose (FileHandle);

    return 1;

} /* end of ReadFile () */

SHORT MakeHashValues (ULONG ulChar, ULONG ulEnd, SHORT ltIndex)
/******************************************************************/
/* expected parameters : ulChar = start of file in memory (input) */
/*                       ulEnd = end of file in memory (input)    */
/*                       ltIndex = index of first line (input)    */
/* return value : index of last line in file                      */
/*                                                                */
/* function : sets up tables for starts of lines and hash values, */
/*            and finds the length of the longest line            */
/******************************************************************/

{
    ULONG ulHashVal, ulHashValNoBlanks, ulRandom[512];
    BOOL fLastWasGraphic;
    USHORT usRandomIndex;
    char Char;

/* The hashing is done by setting up a table of pseudo-random numbers (the
   table is 512 long because PM won't print more characters than that, so
   we'll only pay attention to the first 512 chracters). The ASCII value of
   each character is multiplied by the next random number in the sequence,
   and added to the hash value for the line.

   Mathematical diversion:
   The random values and the hash values are 32 bits long; hopefully (but I'm
   not going to write a thesis to prove it) this will produce a distribution
   of hash values very close to perfect, in which case the chances of collision
   for N lines are given by: 1 - (2^32 - 1)! / ((2^32 - N)! * (2^32)^(N - 1) .
   (I think; my maths is getting weak, but it tallies with the case given in a
   text book of the chances of a shared birthday in a room of 23 people being
   0.5). This works out at 0.03 for the maximum N of 16384.

   The values for the pseudo-random sequence satisfy rules given in Kurth (Art
   of Computer Programming) for R(N) = (R(N-1) * A + C) mod M, i.e.
   1) C and M are relatively prime
   2) B = A - 1 is a multiple of every prime of M
   3) if M is a multiple of 4, B must be as well.
   Since M = 2^32, 2) is satisfied by satisfying 3), and 1) is satisfied by any
   odd number. This should give a sequence which only repeats every M values.
   To avoid 0 turning up in the ones used, I have started the sequence at C,
   i.e. the value following 0. Using B = 4 speeds up computation, and using a
   large C ensures that overflow will take place, which should help make the
   distribution of hash values even. */

    ulRandom[0] = 0xABCDEF01;
    for (usRandomIndex = 1; usRandomIndex < 512; usRandomIndex++)
    {
        ulRandom[usRandomIndex] = ulRandom[usRandomIndex - 1] * 5 + 0xABCDEF01;
    }

    while (ulChar < ulEnd && ltIndex <= 16384)
    {
        ulHashVal = 0;
        usRandomIndex = 0;

        ulLine[ltIndex] = ulChar;

/* White space is regarded as a space, null character or tab. Skip over any
   leading white space if required */

        if (uoUserOptions.fIgnoreLeadingBlanks == TRUE)
            while (ulChar < ulEnd &&
                   ((Char = CMEM (ulChar)) == ' ' ||
                     Char == '\0' ||
                     Char == '\t'))
                ulChar++;

        fLastWasGraphic = TRUE;

        do
        {
            Char = CMEM (ulChar);

/* Skip all white space if required. Ignore characters if we have run out of
   random multipliers. */

            if ((!uoUserOptions.fIgnoreAllBlanks ||
                 (Char != ' ' &&
                  Char != '\0' &&
                  Char != '\t')) &&
                usRandomIndex < 512)
            {

/* If ignoring trailing blanks, remember the last hash value from a non-blank
   character. */

                if (uoUserOptions.fIgnoreTrailingBlanks == TRUE)
                {
                    if (Char <= 32)
                    {
                        if (fLastWasGraphic == TRUE)
                        {
                            fLastWasGraphic = FALSE;
                            ulHashValNoBlanks = ulHashVal;
                        }
                    }
                    else
                        fLastWasGraphic = TRUE;

                }

                ulHashVal += ulRandom[usRandomIndex++] * Char;
            }
            ulChar++;
        } while (Char != '\n' && ulChar < ulEnd);

        if (uoUserOptions.fIgnoreTrailingBlanks && !uoUserOptions.fIgnoreAllBlanks)
            ulHashVal = ulHashValNoBlanks;

        if (Char == '\n')
        {
            sMaxLength = MAX (sMaxLength, (SHORT)(ulChar - ulLine[ltIndex]));
            ulLineHash[ltIndex] = ulHashVal;
            ltIndex++;
        }
    }


    return ltIndex - 1;

} /* end of MakeHashValues () */


void ChainIdenticalLines (void)
/**************************************************************************/
/* function : Finds lines with the same hash value, and forms them into a */
/* chain using the ltLinePair array as pointers. Also sets the foreground */
/* and background colours to the unmatched values.                        */
/**************************************************************************/

{
    SHORT htIndex, ltIndex, ltPair;
    USHORT usColBackgrnd;
    ULONG ulHashVal;

/* clear hash table */
    for (htIndex = 0; htIndex < sTotalLines; htIndex++)
        HashTable[htIndex] = 0;

    usColBackgrnd = COL_A_BGRND;

/* set up hash and line tables */
    for (ltIndex = 1; ltIndex <= sTotalLines; ltIndex++)
    {
        usBackCol[ltIndex] = usColBackgrnd;
        usLineCol[ltIndex] = COL_UNIQUE;

        ulHashVal = ulLineHash[ltIndex];
        htIndex = (SHORT) (ulHashVal % sTotalLines);

/* look for free place in hash table */
        while ((ltPair = HashTable[htIndex]) != 0
               && ulLineHash[ltPair] != ulHashVal)
            htIndex = (htIndex +1) % sTotalLines;

/* chain this line to any others with same hash value */
        ltLinePair[ltIndex] = ltPair;

        HashTable[htIndex] = ltIndex;

/* change background colour if starting on second file */
        if (ltIndex == sNumLines[0])
            usColBackgrnd = COL_B_BGRND;
    }
    return;
} /* end of ChainIdenticalLines () */


void FindUniquePairs (void)
/***************************************************************************/
/* function : pairs lines that occur exactly once in each file, and breaks */
/*            any other chains                                             */
/***************************************************************************/

{
    SHORT htIndex, ltIndex, ltPair;
    for (htIndex = 0; htIndex < sTotalLines; htIndex++)
    {
        if ((ltIndex = HashTable[htIndex]) == 0)
/* no such line */
            continue;
        if ((ltPair = ltLinePair[ltIndex]) != 0
            && ltLinePair[ltPair] == 0
            && ltIndex > sNumLines[0]
            && ltPair <= sNumLines[0])
/* one line in each file, so pair up */
            ltLinePair[ltPair] = ltIndex;
        else
/* identical lines in the same file, so break chain */
            while (ltIndex != 0)
            {
                ltPair = ltLinePair[ltIndex];
                ltLinePair[ltIndex] = 0;
                ltIndex = ltPair;
            }
    }
    return;
} /* end of FindUniquePairs () */


void ExpandUniquePairs (void)
/*********************************************************************/
/* function : Pairs up identical lines before or after unique pairs. */
/*            These will be lines that have 2 or more duplicates.    */
/*********************************************************************/

{
    SHORT ltIndex, ltPair, i;

    for (ltIndex = 0; ltIndex <= sNumLines[0]+1; ltIndex++)
    {

/***************************************************************************/
/* pretend there are unique pairs before and after the files, so the first */
/* and last lines will be paired if identical                              */
/***************************************************************************/
        if (ltIndex == 0)
            ltPair = sNumLines[0];
        else if (ltIndex > sNumLines[0])
            ltPair = sTotalLines+1;
        else
            ltPair = ltLinePair[ltIndex];

        if (ltPair != 0)
        {
/* move forwards looking for identical lines */
            i = 1;
            while (ltPair + i <= sTotalLines
                   && ltIndex + i <= sNumLines[0]
                   && ulLineHash[ltIndex + i] == ulLineHash[ltPair + i]
                   && ltLinePair[ltIndex + i] == 0
                   && ltLinePair[ltPair + i] == 0)
            {
                ltLinePair[ltIndex + i] = ltPair + i;
                ltLinePair[ltPair + i] = ltIndex + i;
                i++;
            }

/* and move backwards */
            i = -1;
            while (ltPair + i > sNumLines[0]
                   && ltIndex + i > 0
                   && ulLineHash[ltIndex + i] == ulLineHash[ltPair + i]
                   && ltLinePair[ltIndex + i] == 0
                   && ltLinePair[ltPair + i] == 0)
            {
                ltLinePair[ltIndex + i] = ltPair + i;
                ltLinePair[ltPair + i] = ltIndex + i;
                i--;
            }
        }
    }
    return;
} /* end of ExpandUniquePairs () */


void SetColours (void)
/***************************************************************************/
/* function : Finds colours to display matching lines in, so that adjacent */
/*            blocks have different colours.                               */
/***************************************************************************/

{
    SHORT ltTopABlock, ltTopBBlock, ltBottomBBlock, ltIndex, ltPair, ltLoop;
    USHORT usColIndex;

    ltTopABlock = 0;
    ltTopBBlock = ltBottomBBlock = 0;

    for (ltIndex = 1; ltIndex <= sNumLines[0] + 1; ltIndex++)
    {
        if ((ltPair = ltLinePair[ltIndex]) == ltBottomBBlock+1)
            ltBottomBBlock = ltPair;    /* continuation of block */
        else
        {
            if (ltBottomBBlock != 0)    /* blocks to be coloured */
            {
/* find colour different from adjacent lines */
                usColIndex = 0;
                while (ltTopABlock > 1
                        && usLineCol[ltTopABlock - 1] == usColIndex
                       || ltTopBBlock > sNumLines[0] + 1
                        && usLineCol[ltTopBBlock - 1] == usColIndex
                       || ltBottomBBlock < sTotalLines
                        && usLineCol[ltBottomBBlock + 1] == usColIndex)
                    usColIndex++;

/* set colours in line table entries */
                for (ltLoop = ltTopABlock; ltLoop < ltIndex; ltLoop++)
                    usLineCol[ltLoop] = usColIndex;

                for (ltLoop = ltTopBBlock; ltLoop <= ltBottomBBlock; ltLoop++)
                    usLineCol[ltLoop] = usColIndex;

/* reset bottom of B block, i.e. no block current */
                ltBottomBBlock = 0;
            }

            if (ltPair != 0)            /* new block */
            {
                ltTopABlock = ltIndex;
                ltTopBBlock = ltBottomBBlock = ltPair;
            }
        }
    }
    return;
} /* end of SetColours () */


void Composite (SHORT ltTopA, SHORT ltBottomA, SHORT ltTopB,
                SHORT ltBottomB, SHORT ltPrecLine, SHORT ltFolLine)
/*****************************************************************************/
/* expected parameters: ltTopA, ltBottomA, ltTopB, ltBottomB = lines         */
/*                                   bounding section to be composed (input) */
/*                      ltPrecLine, ltFolLine = lines composite section      */
/*                                              is to be fitted into (input) */
/*                                                                           */
/* function : Finds the largest matching block of lines in the section given */
/*            to it, puts them in the composite file, and then calls itself  */
/*            to do the same for the lines above the matching section, and   */
/*            then those below it. If no matching sections are found, it     */
/*            puts in unmatched lines, or those which match with lines in    */
/*            another section.                                               */
/*****************************************************************************/

{
    SHORT sMaxBlockLength, sCurBlockLength, ltA, ltB,
          ltMaxBlockStart, ltMaxBlockStop, ltCurBlockStart, ltIndex;
    USHORT usCurCol;

/* find largest matching block */
    sMaxBlockLength = 0;

    for (ltIndex = ltTopA; ltIndex <= ltBottomA; ltIndex++)
    {
        if (ltLinePair[ltIndex] < ltTopB
            || ltLinePair[ltIndex] >ltBottomB)
            continue; /* no match, or matches outside this section */

        if (ltIndex == ltTopA
            || usLineCol[ltIndex] != usLineCol[ltIndex - 1])
        { /* start of matching block */
            sCurBlockLength = 1;
            ltCurBlockStart = ltIndex;
        }

        else
            sCurBlockLength++;

        if (sCurBlockLength > sMaxBlockLength)
        {
            sMaxBlockLength = sCurBlockLength;
            ltMaxBlockStart = ltCurBlockStart;
        }
    }

    if (sMaxBlockLength > 0)
    { /* matching block found */
        ltMaxBlockStop = ltMaxBlockStart + sMaxBlockLength - 1;

/* link lines in block, and change background colour to indicate they are
   matched lines. Increase count of number of lines in composite file.
*/
        for (ltIndex = ltMaxBlockStart;
             ltIndex <= ltMaxBlockStop;
             ltIndex++)
        {
            if (ltIndex != ltMaxBlockStart)
                ltLinePrev[ltIndex] = ltIndex - 1;

            if (ltIndex != ltMaxBlockStop)
                ltLineNext[ltIndex] = ltIndex + 1;

            usBackCol[ltIndex] = usBackCol[ltLinePair[ltIndex]] = COL_MATCHED_BGRND;

            sNumLines[2]++;
        }

/* compose lines above matched section */
        Composite (ltTopA, ltMaxBlockStart - 1,
                   ltTopB, ltLinePair[ltMaxBlockStart] - 1,
                   ltPrecLine, ltMaxBlockStart);

/* now do those below it */
        Composite (ltMaxBlockStop + 1, ltBottomA,
                   ltLinePair[ltMaxBlockStop] + 1, ltBottomB,
                   ltMaxBlockStop, ltFolLine);

        return;
    }
    else
    { /* no matching lines found, so put in the odd ones */
        ltA = ltTopA;
        ltB = ltTopB;

/* do until all lines have been added */
        while (ltA <= ltBottomA || ltB <= ltBottomB)
        {
            if (uoUserOptions.fInterleaveUniqueLines)
            {
/* add any unique lines to the composite, alternately */
                while (ltA <= ltBottomA
                       && ltB <= ltBottomB
                       && ltLinePair[ltA] == 0
                       && ltLinePair[ltB] == 0)
                {
                    Add (&ltA, &ltPrecLine);
                    Add (&ltB, &ltPrecLine);
                }
            }

/* add remaining unique lines from file A */
            while (ltA <= ltBottomA && ltLinePair[ltA] == 0)
                Add (&ltA, &ltPrecLine);

/* and add those from file B */
            while (ltB <= ltBottomB && ltLinePair[ltB] == 0)
                Add (&ltB, &ltPrecLine);

            if (ltA <= ltBottomA)
            {
                usCurCol = usLineCol[ltA];

/* add any lines from file A that have matches, but not in this section */
                while (ltA <= ltBottomA
                       && usLineCol[ltA] == usCurCol)
                    Add (&ltA, &ltPrecLine);
            }

/* and do the same for file B */
            if (ltB <= ltBottomB)
            {
                usCurCol = usLineCol[ltB];

                while (ltB <= ltBottomB
                       && usLineCol[ltB] == usCurCol)
                    Add (&ltB, &ltPrecLine);
            }
        }

/* link the lines to the next section of the composite file */
        ltLineNext[ltPrecLine] = ltFolLine;
        ltLinePrev[ltFolLine] = ltPrecLine;
    }

    return;
} /* end of Composite () */


void Add (PSHORT pltIndex, PSHORT pltPrecLine)
/* expected parameters: pltIndex = pointer to line table entry to be
                                added to composite file (input/output)
                        pltPrecLine = preceding line in composite (input/output)
*/

{
    ltLinePrev[*pltIndex] = *pltPrecLine;
    ltLineNext[*pltPrecLine] = *pltIndex;

    *pltPrecLine = *pltIndex;
    (*pltIndex)++;

    sNumLines[2]++;
} /* end of Add () */


PVOID AllocMem (USHORT usSize)
/* allocates memory for 2nd (non-PM) thread */

{
    SEL Selector;

    if (DosAllocSeg (usSize, &Selector, 0))
    {
        strcpy (pWriteLine, "can't allocate memory");
        return NULL;
    }
    return MAKEP (Selector, 0);

} /* end of AllocMem () */
