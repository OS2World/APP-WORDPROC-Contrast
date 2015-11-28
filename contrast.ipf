:userdoc.

.* ************ CONTRAST C Program Help Text File (.IPF) ***************
:h1 res=401.Contrast

:i1.Contrast Program

:p.:hp2.Contrast:ehp2. compares two files on disk and notes lines that are
common to both. A :link reftype=hd res=500.bar chart:elink. is drawn, giving an
overall view of which lines match, and the text of either file, or a
:link reftype=hd res=501.composite:elink. file, is displayed. The default view is
the composite file.

:h1 res=402.Open Dialog

:i1.Open dialog

:p.The :hp2.Open:ehp2. dialog supplies lists of files and directories from
which the source files can be selected. The full path of the file is made by
concatenating the file name to the directory, so you can type subdirectories
in the name field if you want. '..' means the parent directory. If the name of
File B is null, the name from File A is used.

:h1 res=403.Problem Message Box

:i1.Problem Message Box

:p.The :hp2.Problem:ehp2. message box informs the user of a problem in the
program. This may or may not be fatal. The user is required to acknowledge the
message; the program will take appropriate action.

:h1 res=404.File Menu

:i1.File menu

:p.The :hp2.File:ehp2. menu contains the following items:
:dl tsize=4 compact break=all.
:dt.Open
:dd.Allows new source files to be :link reftype=hd res=503.opened:elink..
:dt.Exit
:dd.Closes the :hp2.Contrast:ehp2. program.
:edl.

:h1 res=405.Help Menu

:i1.Help menu

:p.The :hp2.Help:ehp2. menu contains the following items:
:dl break=all tsize=4 compact.
:dt.Help for help
:dd.Explains the use of the help functions.
:dt.Extended help
:dd.Displays general help on the :hp2.Contrast:ehp2. program.
:dt.Keys help
:dd.Displays help on the keys that can be used in the :hp2.Contrast:ehp2.
program.
:dt.Help index
:dd.Displays a list of selectable help topics.
:dt.About
:dd.Displays the application logo.
:edl.

:h1 res=406.Keys for Contrast

:i1.Keys for Contrast

:p.The :hp2.Contrast:ehp2. program assigns the following special functions to
keys:
:dl tsize=4 compact break=all.
:dt.Alt+O
:dd.Bring up the Open dialog.
:dt.F3
:dd.Exit the Contrast program.
:dt.Alt+A
:dt.Alt+B
:dd.Display file A or B
:dt.Alt+C
:dd.Display the :link reftype=hd res=501.composite:elink. file
:dt.Alt+L
:dt.Alt+T
:dt.Alt+I
:dd.Ignore leading, trailing, or all blanks for line comparison. Use
the :link reftype=hd res=408.Options:elink. menu to see the current status.
:dt.Alt+U
:dd.Display lines unique to either source file interleaved, as opposed to
alternately.
:dt.Alt+D
:dd.Bring up the Define Colours dialog.
:dt.Alt+F
:dd.Bring up the Fonts dialog.
:dt.Arrow keys
:dd.Scroll the displayed file by one character.
:dt.Page Up
:dt.Page Down
:dd.Scroll the file by the height of the text window.
:edl.

:h1 res=407.View Menu

:i1.View menu

:p.The :hp2.View:ehp2. menu changes the text displayed. This can be either of
the source files, or the :link reftype=hd res=501.composite:elink. file.

:h1 res=408.Options Menu

:i1.Options menu

:p.The :hp2.Options:ehp2. menu contains the following items:
:dl tsize=4 compact break=all.
:dt.Ignore leading blanks
:dd.Re-contrasts the files, with white space (i.e. spaces, tabs or nulls)
at the beginning of lines ignored. Any formatting of programs will thus be
irrelevent.
:dt.Ignore trailing blanks
:dd.Ignores white space at the end of lines. Some editors can be inconsistent
about leaving this in files.
:dt.Ignore all blanks
:dd.Disregards all white space, wherever it is in the files. This is
useful for comparing programs in free-format languages, especially if differing
editors have been used.
:dt.Interleave unique lines
:dd.Orders unmatching blocks in the composite file so that lines from files A
and B alternate. If this option is off, unmatching blocks will displayed as
whole blocks from file A or B.
:dt.Define colours used
:dd.Allows user definition of the :link reftype=hd res=502.colours:elink. used
by :hp2.Contrast:ehp2..
:dt.Set font used
:dd.Changes the :link reftype=hd res=504.font:elink. and style used for the text
display.
:edl.

:h1 res=500.Bar Chart

:i1.Bar chart

:p.The bar chart is a graphical representation of the two source files. Blocks
of common lines are assigned a :link reftype=hd res=502.colour:elink. different from
those around them, and drawn on the bars representing the files; a line links
the matched blocks. The matched colours in the bar chart are used as the text
colour for the file display; the default text colour for unmatched lines is
black (red or yellow on the chart). Black marks by the bar chart show where
the current text display is in the files.

:h1 res=501.Composite

:i1.Composite

:p.The composite file is made from the two source files, using the common lines
where possible.
:p.This is the default colour scheme:
:p.Common lines have a white background.
:p.Lines unique to File A are shown in black on yellow.
:p.Lines unique to File B are shown in black on red.
:p.Where a block of lines in File B has been moved from where it was in File A
the program has to guess whether block 1 was moved up past block 2 or block 2
moved down past block 1. It shows the larger block as common and the other as
moved in each file. The text shown as moved is drawn in a dark colour (not
black) on yellow or red.
The :link reftype=hd res=500.bar chart:elink. shows the connections.

:h1 res=502.Colours

:i1.Colours

:p.Default colours are:
:p.Text Background
:dl tsize=4 break=all compact.
:dt.Yellow
:dd.Lines unique to File A (or moved in File A)
Used in :link reftype=hd res=500.bar chart:elink. and as text background.
:dt.Red
:dd.Lines unique to File B (or moved in File B)
Used in bar chart and as text background.
:dt.White
:dd.Lines common to File A and File B
Text background only.
:edl.
:p.See :link reftype=hd res=501.Composite:elink. for how moved blocks are coloured.

:p.Text Foreground
:dl tsize=4 break=all compact.
:dt.Blue
:dt.Dark pink
:dt.Dark green
:dt.Dark cyan
:dd.Text that has been matched in both files. The same colour is used for the
identical text in each file; the choice of colour is such that surrounding
blocks of matched lines use different ones. The same colour is used in the
bar chart to show the lines.

:dt.Black
:dd.Unmatched text.
:edl.

:p. The :link reftype=inform res=1.actual colours:elink. are defined by the user
in the :link reftype=hd res=503.Define colours:elink. dialog.

:h1 res=503.Colours dialog

:i1.Colours dialog

:p.The :hp2.Colours:ehp2. dialog allows the colours used by :hp2.Contrast:ehp2.
to be set to any of 16 colours provided by Presentation Manager. Five text and
three background colours need to be defined. They do not have to be all
different, but the display may be confusing if they are not.
:p.The :hp2.default:ehp2. button resets all the colours to the program-supplied
settings.

:h1 res=504.Font Dialog

:i1.Font dialog

:p.The :hp2.Font:ehp2. dialog allows any fixed-width font present on the system
to be used to display the file. If there is no suitable font, the program will
use the System Proportional font, but text alignment will suffer as a result.
The numbers given after the font names are point sizes.
:p.The :hp2.italic:ehp2. and :hp2.bold:ehp2. checkboxes allow the font to be
displayed in different styles.

:euserdoc.
