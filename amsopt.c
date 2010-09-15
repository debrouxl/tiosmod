/**
 * amsopt - a computer-based AMS (TI-68k official OS) unlocking and optimizing program.
 * Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
 * Portions Copyright (C) 2000-2010 Olivier Armand
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, 5th Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


#define EndAMSSign 73


// Calculator models
enum {TI89 = 3, TI92 = 1, V200 = 8, TI89T = 9};


// The indices of several ROM_CALLs used by this program.
#define DrawChar                     (0x1A4)
#define DrawClipChar                 (0x191)
#define DrawStr                      (0x1A9)
#define EM_GetArchiveMemoryBeginning (0x3CF)
#define EX_stoBCD                    (0x0C0)
#define HeapDeref                    (0x096)
#define memcmp                       (0x270)
#define ReleaseVersion               (0x440)
#define sf_width                     (0x4D3)
#define XR_stringPtr                 (0x293)

static uint32_t HEAD;
static uint32_t delta;
static uint32_t ROM_base;
static uint32_t jmp_tbl;
static uint32_t TIOS_entries;
static uint32_t AMS_Frame;
static uint32_t F_4x6_data;
static uint32_t F_6x8_data;
static uint32_t F_8x10_data;
static uint32_t TrapBFunctions;
static uint8_t  AMS_Major;

static FILE *input;
static FILE *output;


// Read data at the current file position.
static uint8_t ReadByte (void) {
    uint8_t temp_byte;
    temp_byte = fgetc(output);
    return temp_byte;
}

static uint16_t ReadShort (void) {
    uint16_t temp_short;
    temp_short  = fgetc(output) << 8;
    temp_short |= fgetc(output);
    return temp_short;
}

static uint32_t ReadLong (void) {
    uint32_t temp_long;
    temp_long  = fgetc(output) << 24;
    temp_long |= fgetc(output) << 16;
    temp_long |= fgetc(output) << 8;
    temp_long |= fgetc(output);
    return temp_long;
}


// Write data at the current file position.
static void WriteByte (uint8_t byte_in) {
    fputc (((int)byte_in) & 0xFF, output);
}

static void WriteShort (uint16_t short_in) {
    fputc (((int)(short_in >> 8)) & 0xFF, output);
    fputc (((int)(short_in     )) & 0xFF, output);
}

static void WriteLong (uint32_t long_in) {
    fputc (((int)(long_in >> 24)) & 0xFF, output);
    fputc (((int)(long_in >> 16)) & 0xFF, output);
    fputc (((int)(long_in >>  8)) & 0xFF, output);
    fputc (((int)(long_in      )) & 0xFF, output);
}


// Read data at the given absolute address.
static uint8_t GetByte (uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    return ReadByte();
}

static uint16_t GetShort (uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    return ReadShort();
}

static uint32_t GetLong (uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    return ReadLong();
}

static void GetNBytes (unsigned char *buffer, uint32_t n, uint32_t absaddr) {
    uint32_t i;
    fseek(output, absaddr - delta, SEEK_SET);
    for (i = 0; i < n; i++) {
        buffer[i] = ReadByte();
    }
}


// Write data at the given absolute address.
static void PutByte (uint8_t byte_in, uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    WriteByte(byte_in);
}

static void PutShort (uint16_t word_in, uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    WriteShort(word_in);
}

static void PutLong (uint32_t long_in, uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
    WriteLong(long_in);
}

static void PutNBytes (uint8_t *buffer, uint32_t n, uint32_t absaddr) {
    uint32_t i;
    fseek(output, absaddr - delta, SEEK_SET);
    for (i = 0; i < n; i++) {
        WriteByte(buffer[i]);
    }
}


// Seek in output file to point to given absolute address.
static void Seek (uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
}

static uint32_t Tell (void) {
    return (ftell(output) + delta);
}


// Search for values.
static uint32_t SearchByte (uint8_t value) {
    while (ReadByte() != value);
    return Tell();
}

static uint32_t SearchShort (uint16_t value) {
    while (ReadShort() != value);
    return Tell();
}

static uint32_t SearchLong (uint32_t value) {
    while (ReadLong() != value) {
        fseek(output, -2, SEEK_CUR);
    }
    return Tell();
}


// Get address of given ROM_CALL.
static uint32_t rom_call_addr (uint32_t idx) {
    return GetLong(jmp_tbl + 4 * idx);
}

// Get address of given vector.
static uint32_t GetVector (uint32_t absaddr) {
    fseek(output, HEAD + 0x88 + absaddr, SEEK_SET);
    return ReadLong();
}

// Get address of a PC-relative JSR or LEA.
static uint32_t GetPCRelativeValue(uint32_t absaddr) {
    Seek(absaddr);
    return absaddr + ((int32_t)(int16_t)ReadShort());
}

// Get address of given function of trap #$B.
static uint32_t GetTrapBFunction (uint32_t idx) {
    uint32_t temp;
    uint32_t temp2;
    
    if (TrapBFunctions == 0) {
        temp = GetVector(0xAC);
        Seek(temp);
        temp = SearchLong(0xC6FC0006);
        TrapBFunctions = GetPCRelativeValue(temp - 6);
    }
    return GetLong(TrapBFunctions + 6 * idx);
}

// Get attribute in OO_SYSTEM_FRAME.
static uint32_t GetAttribute (uint32_t attr) {
    uint32_t temp;
    uint32_t temp2;
    uint32_t limit;
    
    if (AMS_Frame == 0) {
        temp = GetVector(0xA8);
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


// Find **TIFL** in .xxu file.
uint32_t FindTIFL (uint32_t I, FILE *file) {
    char *point;
    point = malloc(0xA008);
    if (!point) {
        printf("\n    Error : not enough memory.\n");
        return UINT32_MAX;
    }
    fseek (file, 0, SEEK_SET);
    fread (point, 1, 0xA000, file);
    for (; I < 0xA000; I++) {
        if (point[I+0] == '*' &&
            point[I+1] == '*' &&
            point[I+2] == 'T' &&
            point[I+3] == 'I' &&
            point[I+4] == 'F' &&
            point[I+5] == 'L' &&
            point[I+6] == '*' &&
            point[I+7] == '*'
           ) {
            free (point);
            return I;
        }
    }
    free (point);
    return UINT32_MAX;
}


// Where all the fun begins...
int main (int argc, char *argv[])
{
    uint8_t buffer[30];
    uint32_t size;
    uint32_t count;
    uint8_t calc;
    uint32_t maxAMSsize;
    uint32_t I;
    uint32_t temp, temp2;

    printf ("\n- AMS Optimizer v0.1 by Lionel Debroux (derived from Flash Apps Installer v0.3 by ExtendeD & Lionel Debroux) -\n\n");
    if (argc < 3) {
        printf ("    Usage : amsopt base.xxu patched_base.xxu\n");
        return 1;
    }
    if ( (input = fopen (argv[1],"rb")) == NULL) {
        printf ("    Error : file '%s' not found.\n", argv[1]);
        return 2;
    }
    else {
        printf("    Opening '%s'...\n", argv[1]);
    }

    // Find our way into the file, several sanity checks.
    fread (buffer, 1, sizeof("**TIFL**")-1, input);
    if (strncmp (buffer, "**TIFL**", sizeof("**TIFL**")-1)) {
WrongType:
        printf ("\n    Error : wrong input file type.\n"\
        "    Use .89u, .9xu or .v2u ROM files.\n");
        fclose(input);
        return 3;
    }

    fseek (input, 0x11, SEEK_SET);
    fread (buffer, 1, sizeof("basecode")-1, input);

    if ( !strncmp (buffer, "License", sizeof("License")-1)) {
        I=0x11;
        if ((I = FindTIFL(I, input)) == UINT32_MAX) {
            goto WrongType;
        }
        HEAD = (uint32_t) I+17;
        fseek (input, I+17, SEEK_SET);
        printf("\tINFO: found %" PRIu32 " bytes of license at the beginning of the file\n",I);
        fread (buffer, 1, sizeof ("basecode")-1, input);
        if (strncmp (buffer, "basecode", sizeof ("basecode")-1)) {
            goto WrongType;
        }
    }
    else {
        HEAD = 17;
        if (strncmp (buffer, "basecode", sizeof("basecode")-1)) {
            goto WrongType;
        }
    }

    HEAD += 61;
    fseek (input, 0x16+HEAD, SEEK_SET);
    fread (buffer, 1, 29, input);
    if (strncmp (buffer, "Advanced Mathematics Software", sizeof("Advanced Mathematics Software")-1)) {
        goto WrongType;
    }

    fseek (input, 2+ HEAD, SEEK_SET);
    fread (buffer, 1, 4, input);
    size =   UINT32_C(0x0000001) * (unsigned char)buffer[3]
           + UINT32_C(0x0000100) * (unsigned char)buffer[2]
           + UINT32_C(0x0010000) * (unsigned char)buffer[1]
           + UINT32_C(0x1000000) * (unsigned char)buffer[0];
    printf("\tINFO: found AMS of size %" PRIu32 "\n", size);

    // Check calculator type
    fseek (input, 8+HEAD, SEEK_SET);
    calc=fgetc(input);
    printf("\tINFO: found calculator type %" PRIu8 "\n", calc);
    if ((calc != TI89) && (calc != TI92) && (calc != V200) && (calc != TI89T)) {
        printf("\n    Error: unknown calculator type, aborting...\n");
        fclose(input);
        return 4;
    }

    // Check size.
    maxAMSsize = (calc < V200) ? (2 * 1048576 - 3 * 65536) : (4 * 1048576 - 3 * 65536);
    if (size > maxAMSsize) {
        printf ("\n    Error: OS too large, aborting...\n");
        fclose(input);
        return 5;
    }

    // Check AMS version.
    fgetc(input);
    fgetc(input);
    I = fgetc(input);
    printf("\tINFO: found AMS version type %" PRIu32 "\n", I);
    if (I != 9 && I != 12 && I != 13 && I != 14) {
WrongVersion:
        printf ("\n    Error : unsupported AMS version.\n"\
        "    Use AMS 2.05, 2.09, 3.01 or 3.10; you can also help expanding this program.\n"
        );
        fclose (input);
        return 6;
    }

    // Create and fill output file.
    printf ("    Creating output file '%s'...\n", argv[2]);
    if ((output = fopen (argv[2], "rb")) != NULL) {
        printf ("\n    Error : file '%s' already exists. Refusing to overwrite it.", argv[2]);
        fclose(output);
        fclose(input);
        return 7;
    }

    if ((output = fopen (argv[2], "w+b"))==NULL) {
        printf ("\n    Error : can't create '%s'.\n", argv[2]);
        fclose (input);
        return 8;
    }

    printf("\tINFO: copying %" PRIu32 " bytes to output file\n\n", size + HEAD + EndAMSSign);
    fseek (input, 0, SEEK_SET);
    for (count=0; count<size + HEAD + EndAMSSign; count++) {
        fputc (fgetc (input), output);
    }

    fclose(input);


    // OK, we can now focus on unlocking and optimizing AMS :-)
    // Setup absolute address -> file offset translation.
    jmp_tbl = GetVector(0xC8);
    ROM_base = jmp_tbl & UINT32_C(0xE00000);
    delta = ROM_base + 0x12000 - HEAD;

    TIOS_entries = rom_call_addr(-1);
    TrapBFunctions = 0;
    AMS_Frame = 0;
    temp = rom_call_addr(ReleaseVersion);
    AMS_Major = GetByte(temp);


    // 1a) Hard-code HW2/3Patch: disable RAM execution protection.
    {
        temp = rom_call_addr(EX_stoBCD);
        printf("Killing RAM execution protection at %06" PRIx32 "\n", temp + 0x56);
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
        printf("Killing Flash execution protection initialization at %06" PRIx32 "\n", temp - 8);
        PutShort(0x003F, temp - 6);
        // * 1 reference in a subroutine of the trap #$B, function $10 handler.
        temp = GetTrapBFunction(0x10);
        printf("Killing Flash execution protection update at %06" PRIx32 "\n", temp);
        Seek(temp);
        WriteLong(0x33FC003F);
        WriteLong(0x700012);
        WriteShort(0x4E75);
    }


    // 1c) Hard-code a change equivalent to MaxMem and XPand: don't call the subroutine EM_GetArchiveMemoryBeginning
    //     calls before returning, after rounding up the result of OO_GetEndOfAllFlashApps, that both MaxMem and XPand modify.
    {
        temp = rom_call_addr(EM_GetArchiveMemoryBeginning);
        Seek(temp);
        temp = SearchLong(UINT32_C(0xFFFF0000));
        printf("Increasing amount of archive memory at %06" PRIx32 "\n", temp);
        WriteShort(0x2040);
        WriteShort(0x508F);
        WriteShort(0x4E75);
    }


    // 1d) Hard-code Flashappy, for seamless install of unsigned FlashApps.
    {
        Seek(ROM_base + 0x12188);
        temp = rom_call_addr(XR_stringPtr);
        temp2 = SearchLong(0x0000020E);
        if (ReadShort() != 0x4EB9 || ReadLong() != temp) {
            printf("Unexpected data, skipping the killing of FlashApp signature checking !\n");
        }
        temp = GetPCRelativeValue(temp2 - 0x0C);
        temp2 = rom_call_addr(memcmp);
        Seek(temp);
        temp = SearchLong(temp2);
        printf("Killing FlashApp signature checking at %06" PRIx32 "\n", temp - 0x0E);
        temp2 = GetShort(temp - 0x08);
        PutShort(temp2, temp - 0x0C);
    }


    // 2a) Rewrite HeapDeref
    {
        temp = rom_call_addr(HeapDeref);
        temp2 = GetShort(temp + 0x0A);
        if (temp2 == 0) {
            temp2 = GetShort(temp + 0x0C);
        }
        printf("Optimizing HeapDeref at %06" PRIx32 "\n", temp);
        Seek(temp);
        WriteLong(0x302F0004);
        WriteShort(0xE548);
        if (temp2 < 0x8000) {
            WriteShort(0x2078);
            WriteShort(temp2);
        }
        else {
            WriteShort(0x2079);
            WriteLong(temp2);
        }
        WriteLong(0x20700000);
        WriteShort(0x4E75);
    }


    // 2b) Hard-code OO_GetAttr(OO_SYSTEM_FRAME, OO_(S|L|H)FONT) into the subroutine of DrawStr/DrawChar/DrawClipChar.
    {
        uint32_t offset = 0;

        F_4x6_data  = GetAttribute(0x300);
        F_6x8_data  = GetAttribute(0x301);
        F_8x10_data = GetAttribute(0x302);

        temp = rom_call_addr(DrawChar);
        if (AMS_Major == '2') {
            temp = GetPCRelativeValue(temp + 0x26);
        }
        else {
            temp = GetLong(temp + 0x26);
            offset = 2;
        }
        printf("Optimizing character drawing in %06" PRIx32 "\n", temp);


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
    {
        temp = rom_call_addr(sf_width);
        printf("Optimizing sf_width at %06" PRIx32 "\n", temp);
        Seek(temp);
        WriteShort(0x41F9);
        WriteLong(F_4x6_data);
        WriteShort(0x7000);
        WriteLong(0x102F0005);
        WriteShort(0x3200);
        WriteShort(0xD040);
        WriteShort(0xD041);
        WriteShort(0xD040);
        WriteLong(0x10300000);
        WriteShort(0x4E75);
    }


    printf ("\n    Optimization successful.\n");
    fclose(output);
    return 0;
}
