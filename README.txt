==========================================================================================
tiosmod - a computer-based unlocking and optimizing program aimed at official TI-68k
          calculators OS
Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
Portions Copyright (C) 2000-2010 Olivier Armand
==========================================================================================


------------------------------------------------------------------------------------------
Author's motivations
------------------------------------------------------------------------------------------
By chronological order of appearance: optimizing, unlocking, and fixing.
* the initial motivation of this work was scratching a very old itch of mine (before the
creation of FreeFlash): modding TI's OS to optimize several hunks of code that have
bothered me for more than six years.
* the motivation to remove TI's silly protections, while at it, came later, as a
consequence of TI's increasingly frequent and increasingly vicious attacks on our freedom
to tinker with the devices we own...
* in the future, fixing bugs would be nice, too. This motivation was not present either
when I first thought of fiddling with the OS, but it's a logical consequence of TI
abandoning the TI-68k line years ago:
    * the la(te)st OS upgrade for 89T and V200, AMS v3.10, was released in July 2005;
    * the la(te)st OS upgrade for 89 and 92+, AMS v2.09, was released in March 2003
      (and AMS 2.08 & 2.09 for 89 use the 0x340000-0x34FFFF sector, depriving users from
      10% of the archive memory they could use on earlier 2.xx versions...).
It's high time we programmers/users tackled the tasks that the manufacturer is no longer
doing !
And now that we have factored, in the summer of 2009, all interesting public RSA signing
keys used in TI-68k and TI-Z80 calculators, we don't even have to use FreeFlash, or the
older TIB-Receiver, any longer :-)


------------------------------------------------------------------------------------------
Usage of the patcher
------------------------------------------------------------------------------------------
`tiosmod [-h/--help]`
`tiosmod [+-options] base.xxu patched_base.xxu`
After patching the OS, if you want to transfer the OS to a real calculator (the usual
warranty disclaimers apply, see below):
    * (preferred nowadays) use RabbitSign to re-sign it, so as to be able to seamlessly
      transfer the OS;
    * "f-sign" the OS with FreeFlash and transfer it to your calculator as described in
      the FreeFlash readme.
See http://www.ticalc.org/archives/news/articles/14/145/145273.html for more information.

Needless to say, the program is - at least for now - aimed at those who know what they're
doing. You're on your own if you mess up calculators by using pristine or modified
versions of the patcher. No build scripts are distributed, and this is on purpose.

Please also note also that *redistributing* modified versions of TI's copyrighted OS is
illegal in most countries ;-)


------------------------------------------------------------------------------------------
Changelog
------------------------------------------------------------------------------------------

v0.2.2: fourth public release, posted on Cemetech, Omnimaga and TI-Bank on 20100914.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10 (all models), 2.08 (89 only for now,
      I don't have copies of 2.08 for 92+ and for V200 - but the patching code may work
      on those as well, seeing that 2.08 and 2.09 are very close to each other).
    * split the program into the building blocks for making a patch (tiosmod.c, under
      GPLv2) and the patch itself (amspatch.c, under WTFPLv2).
    * new capabilities: shrinking:
        * EXPERIMENTAL: shrink AMS 2.08 for 89, so as to make it fit in the same number
          of sectors as AMS 2.01-2.05 :-)
          (USE AT YOUR OWN RISK - though AFAICT, my testing 89 runs the modified version
          just fine)
    * now distributing binary diffs suitable for xdelta 1.x, xdelta 3.x and bsdiff.

v0.2.1: third public release, posted on Cemetech, Omnimaga and TI-Bank on 20100822.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10 (all models).
    * new unlocking capabilities:
        * remove artificial limitation of the size of ASM programs on AMS 2.xx;
        * remove "Invalid Program Reference" error, artificial limitation of AMS 2.xx and
          later when using ASM programs in expressions.

v0.2: second public release, posted on Cemetech, Omnimaga and TI-Bank on 20100813.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10 (all models).
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

v0.1: first public release, posted on #ti on 20100717.
    * supported AMS versions: 2.05, 2.09, 3.01, 3.10 (all models).
    * new unlocking capabilities:
        * skip FlashApp signature validation (Flashappy);

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
/   * (BrandonW) make the program into a generic (TI-Z80, TI-68k) patcher with some
      scripting language well suited to our needs. Several options:
        * (hardest) define some form of a scripting language and turn the program into a
          script interpreter;
        * use C as a scripting language, interpret it using e.g. TCC (warning, it seems
          to be somewhat dead) or the more powerful but heavier LLVM infrastructure ?
        * (easiest) don't go much beyond splitting the building blocks and the patch
          itself, and compile these files together. This was implemented in v0.2.2,
          before, perhaps, doing better some day :)
      NOTE: due to pagination, TI-Z80 OS support is harder than TI-68k OS support...
      
/   * shrink AMS binaries, so as to leave more Flashapp & archive room available to
      users and programmers.
      The only "easily" (-enough) reachable targets for shrinking are AMS 2.08 (done in
      v0.2.2) and 2.09 for 89, whose original version reduces the amount of available
      archive memory by 10% (compared to older AMS 2.xx versions), due to a spill of
      respectively only ~100 and ~800 bytes onto the next sector...
      All other versions must be shrunk by at least 16K - this is MUCH harder !
      NOTE: significant chunks of data at the end of the OS can be moved to the space
      wasted by TI (but used by PedroM) between ROM_base + 0x13000 (approximately) and
      ROM_base + 0x17FFF.
      NOTE: could the duplicated "y1-99" "y1-99'" strings be optimized ?

0   * fix TI's bugs for them. AMS has nothing of the magnitude of the bug that pleagues
      84+ OS 2.53MP, but it has bugs nevertheless. See the list at
      http://www.technicalc.org/buglist/bugs.pdf : fixing bugs 13, 53, 54, 65, 66 and 89
      may be reasonably easy. Fixing 25 and 50 (between others) would be nice, but is
      presumably harder than the aforementioned bugs.

0   * optimize more functions, such as (but not limited to):
        * graphical functions: the whole set is ripe for optimization...

        * some fairly low-hanging fruit in the CAS core: for his graphical expression
          editor Hail (and byproducts int2str and fastlist), Samuel Stearley re-coded
          several routines of the CAS core, e.g. next_expression_index, in a much
          faster way...

0   * customization of AMS behaviour, e.g.:
        * AMS Extender / EasyChar / ticonst;
        * AutoAlphaLock Off.

0   * (??) put new features into the OS ??
        * PreOS (hard)


------------------------------------------------------------------------------------------
License: buildling blocks: GPL version 2 only
------------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 2 (and only version 2) of the
License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA


------------------------------------------------------------------------------------------
License: OS patching proper: WTFPL version 2
------------------------------------------------------------------------------------------
This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.


------------------------------------------------------------------------------------------
Contact
------------------------------------------------------------------------------------------
lionel underscore debroux yahoo fr
