/******************************/
/*    CONTRAST header file    */
/******************************/

/******************************/
/* ID_*   resource identifier */
/* DID_*    dialog identifier */
/* MID_*      menu identifier */
/* HID_*      help identifier */
/******************************/

#define ID_CONTRAST       256

#define MID_FILE        1
#define MID_OPEN        10
#define MID_EXIT        11

#define MID_VIEW        2
#define MID_FILEA       20             /* |                     */
#define MID_FILEB       21             /* } must be consecutive */
#define MID_COMPOSITE   22             /* |                     */

#define MID_OPTIONS                 3
#define MID_IGNORE_LEADING_BLANKS   30
#define MID_IGNORE_TRAILING_BLANKS  31
#define MID_IGNORE_ALL_BLANKS       32
#define MID_INTERLEAVE_UNIQUE_LINES 33
#define MID_SET_COLOURS             34
#define MID_SET_FONT                35

#define MID_HELP          9
#define MID_HELP_FOR_HELP 90
#define MID_ABOUT         91

#define DID_HELP        3

#define DID_OPEN        100
#define DID_FILEA       101
#define DID_FILEB       102
#define DID_CUR_DIR_A   103
#define DID_CUR_DIR_B   104
#define DID_FILES_A     105
#define DID_FILES_B     106
#define DID_DIRS_A      107
#define DID_DIRS_B      108

#define DID_ABOUT       110

#define DID_COLOURS             120
#define DID_COLOURS_USED        121
#define DID_POSSIBLE_COLOURS    122
#define DID_DEFAULT             123

#define DID_F1_BA               124     /* reserve 14 after this */

#define DID_SET_FONT            140
#define DID_FONTS_AVAILABLE     141
#define DID_ITALIC              142
#define DID_BOLD                143

#define DID_SHOW_COLOURS        150
#define DID_ACTUAL_COLOURS      151     /* reserve 7 after this */


#define ID_PROBLEM      201

#define SID_USED_BASE      16           /* divisible by 16 for performance */
#define SID_POSSIBLE_BASE  32           /*     "     "  "   "       "      */

/* these must be kept up to date with the values in CONTRAST.IPF */
#define HID_CONTRAST       401
#define HID_OPEN           402
#define HID_PROBLEM        403
#define HID_FILE           404
#define HID_HELP           405
#define HID_KEYS           406
#define HID_VIEW           407
#define HID_OPTIONS        408

#define HID_BAR_CHART      500
#define HID_COMPOSITE      501
#define HID_COLOURS        502
#define HID_SET_COLOURS    503
#define HID_SET_FONT       504

