# Notepad
*classic fast notepad with a few modern features*

## Goals

The first, foremost and only goal of this project is to implement extremely lean version of the classic Windows Notepad.
It's not intended to compete with Notepad++, PSPad, or any other intentional and great editors out there.
Instead it aims to provide classic old school Notepad, instant and compact, with the most common pain points fixed.

## TODO

* DONE: support Dark Mode
* remember window position per file
* don't choke on large files (memory mapping)
* prevent editing the same file twice (locking)
* do not hold up shutdown
* performance requirement: blink into existence
* respect user font scale settings and allow change

**Maybe:**

* autosave
* row numbers
* more than one undo step
* option to run in single process
* regex search and replace?
* simple lists management (Ctrl+Shift+L)
* tabs?

**Questions:**

* should try for it to be vcruntime-less binary?
* Windows 10+
