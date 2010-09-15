==========================================================================================
tiosfix - a computer-based unlocking and optimizing program aimed at official TI-68k
          calculators OS
Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
Portions Copyright (C) 2000-2010 Olivier Armand
==========================================================================================


------------------------------------------------------------------------------------------
Author's motivations
------------------------------------------------------------------------------------------
By chronological order of appearance: optimizing, unlocking, fixing.
* the initial motivation of this work was scratching a very old itch of mine (before the
creation of FreeFlash): modding TI's OS to optimize several hunks of code that have
bothered me for more than six years.
* the motivation to remove TI's silly protections, while at it, came later, as a
consequence of TI's increasingly frequent and increasingly vicious attacks on our freedom
to tinker with the devices we own...
* the motivation to fix bugs was not present either when I first thought of optimizing
the OS, but it's a logical consequence of TI abandoning the TI-68k line years ago:
    * the la(te)st OS upgrade for 89T and V200, AMS v3.10, was released in July 2005;
    * the la(te)st OS upgrade for 89 and 92+, AMS v2.09, was released in March 2003.
We users need to tackle the tasks that the manufacturer is no longer doing !
Now that we have factored, in the summer of 2009, all interesting public RSA signing keys
used in TI-68k and TI-Z80 calculators, we don't even have to use FreeFlash, or the older
TIB-Receiver, any longer :-)


------------------------------------------------------------------------------------------
Usage
------------------------------------------------------------------------------------------
`tiosfix [-h/--help]`
`tiosfix [+-options] base.xxu patched_base.xxu`
After patching the OS, if you want to transfer the OS to a real calculator (the usual
warranty disclaimers apply, see below):
    * (preferred nowadays) use RabbitSign to re-sign it, so as to be able to seamlessly
      transfer the OS;
    * "f-sign" the OS with FreeFlash and transfer it to your calculator as described in
      the FreeFlash readme.
See http://www.ticalc.org/archives/news/articles/14/145/145273.html for more information.

Needless to say, the program is aimed at those who know what they're doing. You're on
your own if you screw up.

Note also that *redistributing* modified versions of TI's copyrighted OS is illegal in
most countries ;-)


------------------------------------------------------------------------------------------
Changelog
------------------------------------------------------------------------------------------

v0.2: second public release on 20100813.
    * supported AMS versions: unchanged.
    * now named "tiosfix", todo/wish list expanded.
    * tiosfix now checks and updates the AMS ("basecode") checksum.
    * new optimization capabilities:
        * OPTIONALLY hard-code English language in XR_stringPtr. After this, language
          localizations don't quite work properly...
          They have always been a half-baked thing anyway (many TI-BASIC programs work for
          English only, it's insanely hard to use a number of TI-BASIC functions in such
          a way that the program is portable across all locales), and they make
          XR_stringPtr (used, besides the GUI, in the parsing) more than an order of
          magnitude slower.

v0.1: first somewhat public release, posted on #ti on 20100717.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10.
    * unlocking capabilities:
        * RAM execution protection (HW2/3Patch);
        * Flash execution protection (latest XPand, extended to all models).
        * remove artificial limitation of the amount of archive memory (MaxMem & XPand);
        * skip FlashApp signature validation (Flashappy);
    * optimization capabilities:
        * rewrite HeapDeref to match the AMS 1.xx code (the many inline versions are
          untouched, obviously);
        * hard-code the standard fonts in DrawStr/DrawChar/DrawClipChar and sf_width:
          the ability to redefine fonts is seldom used, but it slows down (asymptotically)
          by ~20% on F_6x8 and F_8x10, by ~80% on F_4x6 (stupidly implemented).

v0.0: sent privately to several persons.
    * supported AMS versions: 2.05, 2.09.
    * Flashappy equivalent not coded yet.

------------------------------------------------------------------------------------------
Todo / wish list
------------------------------------------------------------------------------------------
    * (BrandonW) make the program into a generic (TI-Z80, TI-68k) patcher with some
      scripting language well suited to our needs.
      Define some form of a scripting language and turn the program into a script
      interpreter... or maybe just use C as a scripting language and interpret it using
      e.g. TCC (warning, it seems to be somewhat dead) or the more powerful but heavier
      LLVM infrastructure ?

    * shrink AMS binaries, so as to leave more Flashapp & archive room available to
      users and programmers.
      The only "easily" (-enough) reachable target for shrinking is AMS 2.09 for 89,
      whose original version reduces the amount of available archive memory by 10%
      (compared to older AMS 2.0x versions), due to a spill of only ~800 bytes onto the
      next sector...
      All other versions must be shrunk by at least 16K - this is MUCH harder !
      Idea: significant chunks of data at the end of the OS can be moved to the space
      wasted by TI (but used by PedroM) between ROM_base + 0x13000 (approximately) and
      ROM_base + 0x17FFF.

    * fix TI's bugs for them. AMS has nothing of the magnitude of the bug that pleagues
      84+ OS 2.53MP, but it has bugs nevertheless. Fixing several bugs listed in
      http://www.technicalc.org/buglist/bugs.pdf , such as 13, 53, 54, 65, 66 and 89,
      look feasible without too much hassle. 25 and 50 (between others) would be nice,
      but the fixes are presumably harder.

    * optimize more functions, such as (but not limited to):
        * graphical functions: the whole set is ripe for optimization...

        * some fairly low-hanging fruit in the CAS core: for his graphical expression
          editor Hail (and byproducts int2str and fastlist), Samuel Stearley re-coded
          several routines of the CAS core, e.g. next_expression_index, in a much
          faster way...

    * customization of AMS behaviour, e.g.:
        * AMS Extender / EasyChar / ticonst;
        * AutoAlphaLock Off.

    * (??) put new features into the OS ??
        * PreOS (hard)


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
