/**
 * \file amspatch.c
 * \brief patcher part of a computer-based unlocking and optimizing
 *        program aimed at official TI-68k calculators OS.
 * Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */ 

#define PATCHDESC "amspatch-debrouxl-v11"

// Include the file that contains the helper functions we're taking advantage of.
#include "tiosmod.c"


//! Get address of given vector.
static uint32_t GetAMSVector (uint32_t absaddr) {
    fseek(output, HEAD + 0x88 + absaddr, SEEK_SET);
    return ReadLong();
}

//! Replace given vector with given address.
static void SetAMSVector (uint32_t absaddr, uint32_t newval) {
    fseek(output, HEAD + 0x88 + absaddr, SEEK_SET);
    WriteLong(newval);
}

//! Replace given vector with given address.
static void SetAMSrom_call (uint32_t idx, uint32_t newval) {
    PutLong(newval, jmp_tbl + 4 * idx);
}

//! Get address of a PC-relative JSR or LEA.
static uint32_t Get68kPCRelativeValue (uint32_t absaddr) {
    Seek(absaddr);
    return absaddr + ((int32_t)(int16_t)ReadShort());
}

//! Get address of given item of trap #9.
static uint32_t GetAMSTrap9Item (uint32_t idx) {
    uint32_t temp;

    if (Trap9Pointers == 0) {
        temp = GetAMSVector(0xA4);
        Seek(temp);
        Trap9Pointers = GetLong(temp + 2);
    }
    return GetLong(Trap9Pointers + 4 * idx);
}

//! Get address of given function of trap #$B.
static uint32_t GetAMSTrapBFunction (uint32_t idx) {
    uint32_t temp;

    if (TrapBFunctions == 0) {
        temp = GetAMSVector(0xAC);
        Seek(temp);
        temp = SearchLong(UINT32_C(0xC6FC0006));
        TrapBFunctions = Get68kPCRelativeValue(temp - 6);
    }
    return GetLong(TrapBFunctions + 6 * idx);
}

//! Get attribute in OO_SYSTEM_FRAME.
static uint32_t GetAMSAttribute (uint32_t attr) {
    uint32_t temp;
    uint32_t temp2;
    int32_t limit;
    
    if (AMS_Frame == 0) {
        temp = GetAMSVector(0xA8);
        AMS_Frame = GetLong(temp + 10);
    }
    Seek(AMS_Frame + 0x0E);
    limit = ReadLong();
    while (limit >= 0) {
        temp = ReadLong();
        temp2 = ReadLong();
        if (temp == attr) {
            return temp2;
        }
        limit--;
    }
    return UINT32_C(0xFFFFFFFF);
}


//! Kill the protections set by TI.
static void UnlockAMS(void) {
    uint32_t temp, temp2;

    // 1a) Hard-code HW2/3Patch: disable RAM execution protection.
    {
        temp = rom_call_addr(EX_stoBCD);
        printf("Killing RAM execution protection at %06" PRIX32 "\n", temp + 0x56);
        PutShort(0x0000, temp + 0x58);
        PutShort(0x0000, temp + 0x5C);
        PutShort(0x0000, temp + 0x62);
        PutShort(0x0000, temp + 0x68);
    }


    // 1b) Disable Flash execution protection:
    //         * on HW2+, by setting a higher value in port 700012;
    //         * on HW1, by turning reads from three stealth I/O ranges to writes to those ranges.
    {
        // * HW2+: 1 direct write early in the reset code.
        temp = ROM_base + 0x12188;
        Seek(temp);
        temp = SearchLong(UINT32_C(0x700012));
        printf("Killing Flash execution protection initialization at %06" PRIX32 "\n", temp - 8);
        PutShort(0x003F, temp - 6);
        // * HW1: three references to the stealth I/O ports in the early reset code
        PutShort(0x33C0, temp - 26);
        PutShort(0x33C0, temp - 20);
        PutShort(0x33C0, temp - 14);
        // * HW2+: 1 reference in a subroutine of the trap #$B, function $10 handler.
        temp = GetAMSTrapBFunction(0x10);
        printf("Killing Flash execution protection update at %06" PRIX32 "\n", temp);
        Seek(temp);
        WriteLong(UINT32_C(0x33FC003F));
        WriteLong(UINT32_C(0x700012));
        WriteShort(0x4E75);
    }


    // 1c) Hard-code a change equivalent to MaxMem and XPand: don't call the subroutine EM_GetArchiveMemoryBeginning
    //     calls before returning, after rounding up the result of OO_GetEndOfAllFlashApps, that both MaxMem and XPand modify.
    {
        temp = rom_call_addr(EM_GetArchiveMemoryBeginning);
        Seek(temp);
        temp = SearchLong(UINT32_C(0xFFFF0000));
        printf("Killing the limitation of the available amount of archive memory at %06" PRIX32 "\n", temp);
        Seek(temp);
        WriteShort(0x2040);
        WriteShort(0x508F);
        WriteShort(0x4E75);
    }


    // 1d) Hard-code Flashappy, for seamless install of unsigned FlashApps (e.g. some versions of GTC).
    {
        Seek(ROM_base + UINT32_C(0x20000));
        temp = rom_call_addr(XR_stringPtr);
        temp2 = SearchLong(UINT32_C(0x0000020E));
        if (ReadShort() != 0x4EB9 || ReadLong() != temp) {
            printf("Unexpected data, skipping the killing of FlashApp signature checking !\n");
        }
        else {
            temp = Get68kPCRelativeValue(temp2 - 0x0C);
            temp2 = rom_call_addr(memcmp);
            Seek(temp);
            temp = SearchLong(temp2);
            printf("Killing FlashApp signature checking at %06" PRIX32 "\n", temp - 0x0E);
            temp2 = GetShort(temp - 0x08);
            PutShort(temp2, temp - 0x0C);
        }
    }


    // 1e) Disable artificial limitation of the size of ASM programs on AMS 2.xx
    //     (TI made the check ineffective in 3.xx, without removing it...).
    {
        if (AMS_Major == 2) {
            Seek(ROM_base + UINT32_C(0x20000));
            if (AMS_Minor == 5) {
                temp = SearchLong(0x0C526000);
                printf("Killing the limitation of the size of ASM programs at %06" PRIX32 "\n", temp - 4);
                PutShort(0xFFFF, temp - 2);
            }
            else if (AMS_Minor == 8 || AMS_Minor == 9) {
                temp = SearchLong(0x0C536000);
                printf("Killing the limitation of the size of ASM programs at %06" PRIX32 "\n", temp - 4);
                PutShort(0xFFFF, temp - 2);
            }
            else {
                // Do nothing. This block should be unreachable anyway, due to the version type check.
            }
        }
    }


    // 1f) Remove "Invalid Program Reference" artificial limitation.
    {
        Seek(ROM_base + UINT32_C(0x20000));
        temp = SearchShort(0xA244);
        printf("Killing the \"Invalid Program Reference\" error at %06" PRIX32 ", ", temp - 2);
        PutShort(0x4E71, temp - 2);
        Seek(temp);
        temp = SearchShort(0xA244);
        printf("%06" PRIX32 ", ", temp - 2);
        PutShort(0x4E71, temp - 2);
        Seek(temp);
        temp = SearchShort(0xA244);
        printf("%06" PRIX32 "\n", temp - 2);
        PutShort(0x4E71, temp - 2);
    }
}


//! Optimize several sore spots of the OS.
static void OptimizeAMS(void) {
    uint32_t offset, limit;
    uint32_t temp, temp2, temp3, temp4, temp5;

    // 2a) Rewrite HeapDeref
    {
        temp = rom_call_addr(HeapDeref);
        temp2 = GetShort(temp + 0x0A);
        if (temp2 == 0) {
            temp2 = GetShort(temp + 0x0C);
        }
        printf("Optimizing HeapDeref at %06" PRIX32 "\n", temp);
        Seek(temp);
        WriteLong(UINT32_C(0x302F0004));
        WriteShort(0xE548);
        if (temp2 < 0x8000) {
            WriteShort(0x2078);
            WriteShort(temp2);
        }
        else {
            WriteShort(0x2079);
            WriteLong(temp2);
        }
        WriteLong(UINT32_C(0x20700000));
        WriteShort(0x4E75);
    }


    // 2b) Hard-code OO_GetAttr(OO_SYSTEM_FRAME, OO_(S|L|H)FONT) into the subroutine of DrawStr/DrawChar/DrawClipChar.
    if (enabled_changes & AMS_HARDCODE_FONTS_FLAG)
    {
        F_4x6_data  = GetAMSAttribute(0x300);
        F_6x8_data  = GetAMSAttribute(0x301);
        F_8x10_data = GetAMSAttribute(0x302);

        temp = rom_call_addr(DrawChar);
        if (AMS_Major == 2) {
            temp = Get68kPCRelativeValue(temp + 0x26);
            offset = 0;
        }
        else {
            temp = GetLong(temp + 0x26);
            offset = 2;
        }
        printf("Optimizing character drawing in %06" PRIX32 "\n", temp);

        Seek(temp + 0x7A + offset);
        WriteShort(0x41F9);
        WriteLong(F_8x10_data);
        WriteShort(0x602A);
        Seek(temp + 0xC2 + offset);
        WriteShort(0x41F9);
        WriteLong(F_6x8_data);
        WriteShort(0x602A);
        Seek(temp + 0x112 + offset);
        WriteShort(0x41F9);
        WriteLong(F_4x6_data);
        WriteShort(0x602A);
    }


    // 2c) Hard-code OO_GetAttr(OO_SYSTEM_FRAME, OO_SFONT) in rewritten sf_width.
    if (enabled_changes & AMS_HARDCODE_FONTS_FLAG)
    {
        temp = rom_call_addr(sf_width);
        printf("Optimizing sf_width at %06" PRIX32 "\n", temp);
        Seek(temp);
        WriteShort(0x41F9);
        WriteLong(F_4x6_data);
        WriteShort(0x7000);
        WriteLong(UINT32_C(0x102F0005));
        WriteShort(0x3200);
        WriteShort(0xD040);
        WriteShort(0xD041);
        WriteShort(0xD040);
        WriteLong(UINT32_C(0x10300000));
        WriteShort(0x4E75);
    }


    // 2d) Hard-code English language in XR_stringPtr.
    // WARNING, language localizations won't work properly after this...
    if (enabled_changes & AMS_HARDCODE_ENGLISH_LANGUAGE_FLAG)
    {
        temp = rom_call_addr(XR_stringPtr);
        temp2 = GetLong(AMS_Frame + 0x04);
        limit = GetLong(temp2 + 0x0E);
        temp3 = rom_call_addr(EV_runningApp);
        temp4 = rom_call_addr(HeapTable);
        temp5 = rom_call_addr(OO_CondGetAttr);


        printf("Optimizing XR_stringPtr at %06" PRIX32 "\n", temp);
        Seek(temp);
        WriteLong(UINT32_C(0x302F0006)); // WriteLong(0x202F0004);
        WriteShort(0x0C40); // WriteShort(0x0C80); 
        WriteShort(limit); // WriteLong(limit);
        WriteShort(0x620E);
        WriteShort(0x41F9);
        WriteLong(temp2);
        WriteShort(0xE548);
        WriteLong(UINT32_C(0x20700012)); // WriteLong(0x2070812);
        WriteShort(0x4E75);
        
        WriteShort(0x42A7);
        WriteShort(0x4857);
        WriteLong(UINT32_C(0x06400800));
        WriteShort(0x3F00);
        WriteShort(0x4267);
        if (temp3 < 0x8000) {
            WriteShort(0x3238);
            WriteShort(temp3);
        }
        else {
            WriteShort(0x3239);
            WriteLong(temp3);
        }
        if (temp4 < 0x8000) {
            WriteShort(0x41F8);
            WriteShort(temp4);
        }
        else {
            WriteShort(0x41F9);
            WriteLong(temp4);
        }
        WriteShort(0xE549);
        WriteLong(UINT32_C(0x20701000));
        WriteLong(UINT32_C(0x2F280014));
        WriteShort(0x4EB9);
        WriteLong(temp5);
        WriteLong(UINT32_C(0x4FEF000C));
        WriteShort(0x205F);
        WriteShort(0x4E75);
    }
}


//! Fix TI's bugs: they have abandoned the TI-68k calculator line in 2005...
static void FixAMS(void) {
    uint32_t temp, temp2, temp3, temp4;
    
    // 3a) Idea by Martial Demolins (Folco): on trap #3, wire a new routine that does a UniOS/PreOS/PedroM-style HeapDeref.
    //     Pristine AMS copies have OSenqueue wired, but that won't work at all.
    {
        printf("Replacing buggy trap #3 by UniOS/PreOS/PedroM-style HeapDeref\n");
        temp = rom_call_addr(HeapTable);
        temp2 = ROM_base + 0x13100;
        Seek(temp2);
        WriteShort(0xD0C8);
        WriteShort(0xD0C8);
        if (temp < 0x8000) {
            WriteShort(0xD0FC);
            WriteShort(temp);
        }
        else {
            WriteShort(0xD1FC);
            WriteLong(temp);
        }
        WriteShort(0x2050);
        WriteShort(0x4E73);
        SetAMSVector(0x8C, temp2);
    }

    // 3b) Fix bug #53 of http://www.technicalc.org/buglist/bugs.pdf , worked around in TIGCC & GCC4TI:
    //     OSContrastUp and OSContrastDn destroy the contents of registers d3 and d4, which they are not allowed to do.
    {
        temp = rom_call_addr(OSContrastUp);
        temp2 = rom_call_addr(OSContrastDn);

        temp3 = GetShort(temp);
        if (temp3 == 0x48A7) {

            temp3 = GetShort(temp2);
            if (temp3 == 0x48A7) {

                temp3 = SearchShort(0x4C9F);

                temp4 = SearchShort(0x48A7);
                if (temp4 - temp3 <= 0x10) {
                    PutShort(0x48E7, temp);
                    PutShort(0x48E7, temp2);
                    PutShort(0x4CDF, temp3 - 2);
                    PutShort(0x48E7, temp4 - 2);
                    printf("Fixing buggy OSContrastUp & OSContrastDn at %06" PRIX32 ", %06" PRIX32 ", %06" PRIX32 ", %06" PRIX32 "\n",
                           temp, temp2, temp3 - 2, temp4 - 2);
                }
            }
        }
    }

    // 3c) Fix the bug that can occur when changing batteries (HW3Patch fixes it).
    {
        temp = GetAMSVector(0xAC);
        Seek(temp);
        temp2 = SearchShort(0x4E68);
        temp2 += 4;
        Seek(temp2);
        WriteShort(0x4600);
        WriteLong(UINT32_C(0x020000FF));
        WriteLong(UINT32_C(0x0A000000));
        WriteShort(0x4600);
        WriteLong(UINT32_C(0x00000000));
        printf("Fixing bug that can occur when changing batteries at %06" PRIX32 "\n", temp2);
    }
}


//! Shrink two AMS versions that are just slightly too large and deprive users from 64 KB of archive memory available on older versions...
static void ShrinkAMS(void) {
    uint8_t buffer[256];
    uint32_t temp;
    uint32_t src;
    uint32_t dest;

    // 4a) Shrink AMS 2.08 and 2.09 for 89.
    if (I == 11 && CalculatorType == TI89) {
        src  = UINT32_C(0x33FEE0);
        dest = UINT32_C(0x214000);
        temp = dest;

        printf("Shrinking AMS 2.08 for 89, to make it fit into the same number of sectors as earlier AMS 2.xx versions...");
        // 33FEE0: 10 bytes: BITMAP( 5, 5) referenced by GD_Eraser.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x237362));
        src += 10, dest += 10;
        // 33FEEA: 10 bytes: BITMAP( 5, 5) referenced by GD_Eraser.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x237368));
        src += 10, dest += 10;
        // 33FEF4: 26 bytes: BITMAP( B, B) referenced by GT_WinCursor.
        GetNBytes(buffer, 26, src);
        PutNBytes(buffer, 26, dest);
        PutLong(dest, UINT32_C(0x2A8B06));
        src += 26, dest += 26;
        // 33FF0E:  8 bytes: BITMAP( 3, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        PutLong(dest, UINT32_C(0x2A8B1C));
        src += 8, dest += 8;
        // 33FF16: 10 bytes: BITMAP( 5, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2A8B32));
        src += 10, dest += 10;
        // 33FF20: 10 bytes: BITMAP( 5, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2A8B48));
        src += 10, dest += 10;
        // 33FF2A: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        // It is the same value as the first BITMAP referenced by GD_Eraser, let's optimize it away.
        //GetNBytes(buffer, 10, src);
        //PutNBytes(buffer, 10, dest);
        //PutLong(dest, UINT32_C(0x2B97E8));
        PutLong(temp, UINT32_C(0x2B97E8));
        //src += 10, dest += 10;
        src += 10;
        // 33FF34: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B97EC));
        src += 10, dest += 10;
        // 33FF3E: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array and GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B97F0));
        PutLong(dest, UINT32_C(0x2A8AEE));
        src += 10, dest += 10;
        // 33FF48: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B97F8));
        src += 10, dest += 10;
        // 33FF52: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B97F4));
        src += 10, dest += 10;
        // 33FF5C: 68 bytes: BITMAP(15,15) referenced by GT_WinCursor.
        GetNBytes(buffer, 68, src);
        PutNBytes(buffer, 68, dest);
        PutLong(dest, UINT32_C(0x2A8B5C));
        src += 68, dest += 68;
        // 33FFA0: 12 bytes: BITMAP( 7, 7) referenced by GT_WinCursor.
        GetNBytes(buffer, 12, src);
        PutNBytes(buffer, 12, dest);
        PutLong(dest, UINT32_C(0x2A8B76));
        src += 12, dest += 12;
        // 33FFAC: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B97FC));
        src += 16, dest += 16;
        // 33FFBC: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B9800));
        src += 16, dest += 16;
        // 33FFCC: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B9804));
        src += 16, dest += 16;
        // 33FFDC: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B9808));
        src += 16, dest += 16;
        // 33FFEC: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B980C));
        src += 16, dest += 16;
        // 33FFFC:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 O / box
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 340004:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 +
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 34000C:  8 bytes: BITMAP( 3, 3) without any absolute references... a dot in the middle
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 340014:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 X
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;

        // 34001C:  4 bytes: basecode checksum.
        temp = GetLong(UINT32_C(0x34001C));
        PutLong(temp, UINT32_C(0x33FEE0));
        // 340020: 67 bytes: end signature.
        GetNBytes(buffer, 67, UINT32_C(0x340020));
        PutNBytes(buffer, 67, UINT32_C(0x33FEE4)); // 33FF27

        // Decrease length of field 8000.
        SizeShrunk = src - UINT32_C(0x33FEE0);
        temp = GetLong(UINT32_C(0x212002));
        temp -= SizeShrunk;
        PutLong(temp, UINT32_C(0x212002));
        // Decrease length of field 8070 as well (spotted by RabbitSign).
        temp -= 126;
        PutLong(temp, UINT32_C(0x212080));
        printf(" shrunk by %" PRIu32 " bytes.\n", SizeShrunk);
    }
    else if (I == 12 && CalculatorType == TI89) {
        src  = UINT32_C(0x33FFB0);
        dest = UINT32_C(0x214000);

        printf("Shrinking AMS 2.09 for 89, to make it fit into the same number of sectors as earlier AMS 2.xx versions...");
        // 33FFB0: 90 bytes: TITABLED menu.
        GetNBytes(buffer, 90, src);
        PutNBytes(buffer, 90, dest);
        PutLong(dest, UINT32_C(0x2F5DD2));
        src += 90, dest += 90;

        // 34000A: 126 bytes: Table formats dialog. Needs patching.
        GetNBytes(buffer, 126, src);
        PutNBytes(buffer, 126, dest);
        PutLong(dest + 38, dest + 16);
        PutLong(dest, UINT32_C(0x2F7FE8));
        src += 126, dest += 126;

        // 340088: 150 bytes: Table setup dialog. Needs patching.
        GetNBytes(buffer, 150, src);
        PutNBytes(buffer, 150, dest);
        PutLong(dest + 86, dest + 40);
        PutLong(dest + 118, dest + 52);
        PutLong(dest, UINT32_C(0x2F870A));
        src += 150, dest += 150;

        // 34011E: 38 bytes: "WARNING - Graph screen size unknown, so Graph <-> Table setting not supported" dialog. Does not need patching.
        GetNBytes(buffer, 38, src);
        PutNBytes(buffer, 38, dest);
        PutLong(dest, UINT32_C(0x2FA8D8));
        src += 38, dest += 38;

        // 340144: 128 bytes: TITEXTED menu.
        GetNBytes(buffer, 128, src);
        PutNBytes(buffer, 128, dest);
        PutLong(dest, UINT32_C(0x224312));
        src += 128, dest += 128;

        // 3401C4: 10 bytes: BITMAP( 5, 5) referenced by GD_Eraser.
        temp = dest;
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x237362));
        src += 10, dest += 10;
        // 3401CE: 10 bytes: BITMAP( 5, 5) referenced by GD_Eraser.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x237368));
        src += 10, dest += 10;
        // 3401D8: 26 bytes: BITMAP( B, B) referenced by GT_WinCursor.
        GetNBytes(buffer, 26, src);
        PutNBytes(buffer, 26, dest);
        PutLong(dest, UINT32_C(0x2A8CCE));
        src += 26, dest += 26;
        // 3401F2:  8 bytes: BITMAP( 3, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        PutLong(dest, UINT32_C(0x2A8CE4));
        src += 8, dest += 8;
        // 3401FA: 10 bytes: BITMAP( 5, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2A8CFA));
        src += 10, dest += 10;
        // 340204: 10 bytes: BITMAP( 5, 3) referenced by GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2A8D10));
        src += 10, dest += 10;
        // 34020E: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        // It is the same value as the first BITMAP referenced by GD_Eraser, let's optimize it away.
        //GetNBytes(buffer, 10, src);
        //PutNBytes(buffer, 10, dest);
        //PutLong(dest, UINT32_C(0x2B97E8));
        PutLong(temp, UINT32_C(0x2B99B0));
        //src += 10, dest += 10;
        src += 10;
        // 340218: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B99B4));
        src += 10, dest += 10;
        // 340222: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array and GT_WinCursor.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B99B8));
        PutLong(dest, UINT32_C(0x2A8CB6));
        src += 10, dest += 10;
        // 34022C: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B99C0));
        src += 10, dest += 10;
        // 340236: 10 bytes: BITMAP( 5, 5) referenced by a pointer in an array.
        GetNBytes(buffer, 10, src);
        PutNBytes(buffer, 10, dest);
        PutLong(dest, UINT32_C(0x2B99BC));
        src += 10, dest += 10;
        // 340240: 68 bytes: BITMAP(15,15) referenced by GT_WinCursor.
        GetNBytes(buffer, 68, src);
        PutNBytes(buffer, 68, dest);
        PutLong(dest, UINT32_C(0x2A8D24));
        src += 68, dest += 68;
        // 340284: 12 bytes: BITMAP( 7, 7) referenced by GT_WinCursor.
        GetNBytes(buffer, 12, src);
        PutNBytes(buffer, 12, dest);
        PutLong(dest, UINT32_C(0x2A8D3E));
        src += 12, dest += 12;
        // 340290: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B99C4));
        src += 16, dest += 16;
        // 3402A0: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B99C8));
        src += 16, dest += 16;
        // 3402B0: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B99CC));
        src += 16, dest += 16;
        // 3402C0: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B99D0));
        src += 16, dest += 16;
        // 3402D0: 16 bytes: BITMAP( 6, B) referenced by a pointer in an array.
        GetNBytes(buffer, 16, src);
        PutNBytes(buffer, 16, dest);
        PutLong(dest, UINT32_C(0x2B99D4));
        src += 16, dest += 16;
        // 3402E0:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 O / box
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 3402E8:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 +
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 3402F0:  8 bytes: BITMAP( 3, 3) without any absolute references... a dot in the middle
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;
        // 3402F8:  8 bytes: BITMAP( 3, 3) without any absolute references... 3x3 X
        GetNBytes(buffer, 8, src);
        PutNBytes(buffer, 8, dest);
        src += 8, dest += 8;

        // 340300:  4 bytes: basecode checksum.
        temp = GetLong(UINT32_C(0x340300));
        PutLong(temp, UINT32_C(0x33FFB0));
        // 340304: 67 bytes: end signature.
        GetNBytes(buffer, 67, UINT32_C(0x340304));
        PutNBytes(buffer, 67, UINT32_C(0x33FFB4)); // 33FFF7

        // Decrease length of field 8000.
        SizeShrunk = src - UINT32_C(0x33FFB0);
        temp = GetLong(UINT32_C(0x212002));
        temp -= SizeShrunk;
        PutLong(temp, UINT32_C(0x212002));
        // Decrease length of field 8070 as well (spotted by RabbitSign).
        temp -= 126;
        PutLong(temp, UINT32_C(0x212080));
        printf(" shrunk by %" PRIu32 " bytes.\n", SizeShrunk);
    }
}


//! Add functionality to AMS.
static void ExpandAMS(void) {
    uint32_t temp, temp2, temp3, temp4, temp5, temp6;

    // 5a) Reintegrate OSVRegisterTimer/OSVFreeTimer functionality.
    {
        printf("Reintegrating OSVRegisterTimer/OSVFreeTimer functionality\n");
        // Add a new AI5 handler and modify the original one.
        temp = GetAMSVector(0x74);
        Seek(temp);
        temp2 = ReadLong();
        temp3 = SearchShort(0x4E73);
        temp4 = GetLong(temp3 - 6);
        Seek(temp3 - 6);
        WriteShort(0x4E75);
        temp5 = GetAMSTrap9Item(3);
        temp6 = ROM_base + 0x13110;
        Seek(temp6);
        WriteLong(temp2);
        WriteShort(0x4EB9);
        WriteLong(temp + 4);
        WriteShort(0x45F8);
        WriteShort(temp5);
        WriteShort(0x76FF);
        WriteShort(0xB69A);
        WriteShort(0x6718);
        WriteShort(0x5392);
        WriteShort(0x6614);
        WriteLong(UINT32_C(0x24EAFFFC));
        WriteShort(0x205A);
        WriteShort(0x4E90);
        WriteShort(0xB4FC);
        WriteShort(temp5 + 24);
        WriteShort(0x6DEA);
        WriteLong(temp4);
        WriteShort(0x4E73);
        WriteShort(0x508A);
        WriteShort(0x60F0);
        SetAMSVector(0x74, temp6);

        // Rewrite the timer-related reset (init) code entirely.
        // It's easy enough to end up with code smaller than TI's code, despite the new code providing more functionality...
        temp = rom_call_addr(FiftyMsecTick);
        temp2 = GetAMSTrap9Item(4);
        temp6 = rom_call_addr(OSRegisterTimer);
        Seek(temp6);
        temp6 = SearchBackwardsShort(0x48E7);
        WriteShort(0x7000);
        if (temp < 0x8000) {
            WriteShort(0x21C0);
            WriteShort(temp);
        }
        else {
            WriteShort(0x23C0);
            WriteLong(temp);
        }
        WriteShort(0x41F8);
        WriteShort(temp2 + 0xA);
        WriteShort(0x43F8);
        temp3 = 8;
        if (AMS_Major == 2) {
            if (AMS_Minor == 5) {
                temp3 = 7;
            }
        }
        else {
            if (CalculatorType == TI89T) {
                temp3 = 9;
            }
        }
        WriteShort(temp5 - 2 * temp3);
        WriteByte(0x74);
        WriteByte(temp3 - 1);
        WriteShort(0x72FF);
        WriteShort(0x20C1);
        WriteShort(0x20C0);
        WriteShort(0x32C0);
        WriteLong(UINT32_C(0x51CAFFF8));
        WriteShort(0x22C1);
        WriteShort(0x22C0);
        WriteShort(0x22C0);
        WriteShort(0x22C1);
        WriteShort(0x22C0);
        WriteShort(0x22C0);
        WriteShort(0x4E75);

        // OSVRegisterTimer.
        temp6 = ROM_base + 0x13140;
        Seek(temp6);
        WriteShort(0x7000);
        WriteLong(UINT32_C(0x322F0004));
        WriteShort(0x5341);
        WriteShort(0x74FF);
        WriteLong(UINT32_C(0x0C410002));
        WriteShort(0x641C);
        WriteShort(0x41F8);
        WriteShort(temp5);
        WriteLong(UINT32_C(0xC2FC000C));
        WriteShort(0xD1C1);
        WriteShort(0xB490);
        WriteShort(0x660E);
        WriteLong(UINT32_C(0x242F0006));
        WriteShort(0x20C2);
        WriteShort(0x20C2);
        WriteLong(UINT32_C(0x20AF000A));
        WriteShort(0x5240);
        WriteShort(0x4E75);
        SetAMSrom_call(OSVRegisterTimer, temp6);

        // OSVFreeTimer.
        temp6 = ROM_base + 0x13170;
        Seek(temp6);
        WriteShort(0x7000);
        WriteLong(UINT32_C(0x322F0004));
        WriteShort(0x5341);
        WriteShort(0x74FF);
        WriteLong(UINT32_C(0x0C410002));
        WriteShort(0x6412);
        WriteShort(0x41F8);
        WriteShort(temp5);
        WriteLong(UINT32_C(0xC3FC000C));
        WriteShort(0xD1C1);
        WriteShort(0x20C2);
        WriteShort(0x4298);
        WriteShort(0x4290);
        WriteShort(0x5240);
        WriteShort(0x4E75);
        SetAMSrom_call(OSVFreeTimer, temp6);
    }
}


void PatchAMS(void) {
    UnlockAMS();

    OptimizeAMS();

    FixAMS();

    ShrinkAMS();

    ExpandAMS();
}
