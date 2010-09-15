==========================================================================================
amsopt - a computer-based AMS (TI-68k official OS) unlocking and optimizing program.
Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
Portions Copyright (C) 2000-2010 Olivier Armand
==========================================================================================


------------------------------------------------------------------------------------------
Author's motivations
------------------------------------------------------------------------------------------
Scratching a very old itch of mine: modding TI's OS to optimize several hunks of code that
have bothered me for more than seven years. Now that we have factored all interesting
public RSA signing keys used in TI-68k (and TI-Z80) calculators, we don't even have to
resort to TIB-Receiver (or the newer FreeFlash) any longer :-)
The motivation to remove TI's silly protections, while at it, came later, as a consequence
of TI's increasingly frequent and increasingly vicious attacks on our freedom to tinker
with the devices we own...

Contributions welcome ;-)


------------------------------------------------------------------------------------------
Usage
------------------------------------------------------------------------------------------
`amsopt base.xxu patched_base.xxu`
After patching the OS, if you want to transfer the OS to a real calculator (the usual
warranty disclaimers apply, see below):
    * (preferred nowadays) use RabbitSign to re-sign it, so as to be able to seamlessly
      transfer the OS;
    * "f-sign" the OS with FreeFlash and transfer it to your calculator as described in
      the FreeFlash readme.
See http://www.ticalc.org/archives/news/articles/14/145/145273.html for more information.

Note that redistributing modified versions of TI's copyrighted OS is illegal in most
countries.


------------------------------------------------------------------------------------------
Changelog
------------------------------------------------------------------------------------------

v0.1: first public release.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10.
    * unlocking capabilities:
        * RAM execution protection (HW2/3Patch);
        * Flash execution protection (latest XPand, extended to all models).
        * remove artificial limitation of the amount of archive memory (MaxMem & XPand);
        * skip FlashApp signature validation (Flashappy);
    * optimization capabilities:
        * rewrite HeapDeref to match the AMS 1.xx code (inline versions are untouched);
        * hard-code the standard fonts in DrawStr/DrawChar/DrawClipChar and sf_width:
          the ability to redefine fonts is seldom used, but it slows down (asymptotically)
          by ~20% on F_6x8 and F_8x10, by ~80% on F_4x6 (stupidly implemented).

v0.0: sent privately to several persons.
    * supported AMS versions: 2.05, 2.09 (89 / 92+).
    * unlocking capabilities:
        * RAM execution protection (HW2/3Patch);
        * Flash execution protection (latest XPand, extended to all models).
        * remove artificial limitation of the amount of archive memory (MaxMem & XPand);
    * optimization capabilities:
        * rewrite HeapDeref to match the AMS 1.xx code (the many inline versions are
          untouched, obviously);
        * hard-code the standard fonts in DrawStr/DrawChar/DrawClipChar and sf_width:
          the ability to redefine fonts is seldom used, but it slows down (asymptotically)
          by ~20% on F_6x8 and F_8x10, by ~80% on F_4x6 (stupidly implemented).


------------------------------------------------------------------------------------------
Todo / wish list
------------------------------------------------------------------------------------------
    * kill more protections if necessary (which ones ??).

    * optimize more functions, such as (but not limited to):
        * graphical functions: the whole set is ripe for optimization...

        * some fairly low-hanging fruit in the CAS core: for his graphical expression
          editor Hail, Samuel Stearley re-coded several routines of the CAS core, e.g.
          next_expression_index, in a much faster way...

    * fix TI's bugs for them. AMS has nothing of the magnitude of the bug that pleagues
      84+ OS 2.53MP, but it has bugs nevertheless. We might find some simple things to
      grind on in http://www.technicalc.org/buglist/bugs.pdf .

    * (??) put new features into the OS ??


------------------------------------------------------------------------------------------
License: GPL version 2 only
------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA


------------------------------------------------------------------------------------------
Contact
------------------------------------------------------------------------------------------
lionel underscore debroux yahoo fr
