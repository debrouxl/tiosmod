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

// Include the file that contains the helper functions we're taking advantage of.
#include "tiosmod.c"

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


    // 1b) Disable Flash execution protection by setting a higher value in port 700012.
    {
        // * 1 direct write in the early reset code.
        temp = ROM_base + 0x12188;
        Seek(temp);
        temp = SearchLong(UINT32_C(0x700012));
        printf("Killing Flash execution protection initialization at %06" PRIX32 "\n", temp - 8);
        PutShort(0x003F, temp - 6);
        // * 1 reference in a subroutine of the trap #$B, function $10 handler.
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
        WriteShort(0x2040);
        WriteShort(0x508F);
        WriteShort(0x4E75);
    }


    // 1d) Hard-code Flashappy, for seamless install of unsigned FlashApps (e.g. some versions of GTC).
    {
        Seek(ROM_base + UINT32_C(0x12188));
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
            Seek(ROM_base + UINT32_C(0x12188));
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
        Seek(ROM_base + UINT32_C(0x12188));
        temp = SearchShort(0xA244);
        printf("Killing the \"Invalid Program Reference\" error at %06" PRIX32 ", ", temp - 2);
        PutShort(0x4E71, temp - 2);
        temp = SearchShort(0xA244);
        printf("%06" PRIX32 ", ", temp - 2);
        PutShort(0x4E71, temp - 2);
        temp = SearchShort(0xA244);
        printf("%06" PRIX32 "\n", temp - 2);
        PutShort(0x4E71, temp - 2);
    }
}


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


static void FixAMS(void) {
    // TODO: fix TI's bugs for them. They have abandoned the TI-68k calculator line in 2005...
}


static void ShrinkAMS(void) {
    uint8_t buffer[100];
    uint32_t temp;
    uint32_t src =  UINT32_C(0x33FEE0);
    uint32_t dest = UINT32_C(0x214000);

    if (I == 11 && ROM_base == 0x200000) {
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
        PutLong(UINT32_C(0x214000), UINT32_C(0x2B97E8));
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
    else if (I == 12 && ROM_base == 0x200000) {
        printf("TODO: shrink AMS 2.09 for 89, to make it fit into the same number of sectors as earlier AMS 2.xx versions.\n");
    }
}


void PatchAMS(void) {
    UnlockAMS();

    OptimizeAMS();

    FixAMS();

    ShrinkAMS();
}
