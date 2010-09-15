/**
 * \file tiosfix.c
 *
 * tiosfix - a computer-based unlocking and optimizing program aimed at official
 *           TI-68k calculators OS
 * Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
 * Portions Copyright (C) 2000-2010 Olivier Armand (ExtendeD)
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


// The program's command-line switches
#define AMS_HARDCODE_FONTS_STR             "ams-hardcode-fonts"
#define AMS_HARDCODE_FONTS_FLAG            (0x00000001)
#define AMS_HARDCODE_ENGLISH_LANGUAGE_STR  "ams-hardcode-english-language"
#define AMS_HARDCODE_ENGLISH_LANGUAGE_FLAG (0x00000002)


// Calculator models
enum {TI89 = 3, TI92P = 1, V200 = 8, TI89T = 9};


// The indices of ROM_CALLs used by this program
#define DrawChar                     (0x1A4)
#define DrawClipChar                 (0x191)
#define DrawStr                      (0x1A9)
#define EM_GetArchiveMemoryBeginning (0x3CF)
#define EV_runningApp                (0x45D)
#define EX_stoBCD                    (0x0C0)
#define HeapDeref                    (0x096)
#define HeapTable                    (0x441)
#define memcmp                       (0x270)
#define OO_Deref                     (0x3FB)
#define OO_CondGetAttr               (0x3FA)
#define ReleaseVersion               (0x440)
#define sf_width                     (0x4D3)
#define XR_stringPtr                 (0x293)


#define AdditionalSize (4 + 2 + 3 + 64)


// Internal variables, shared memory style.
static uint32_t I;
static uint32_t HEAD;
static uint32_t delta;
static uint32_t ROM_base;
static uint32_t BasecodeSize;
static uint32_t jmp_tbl;
static uint32_t TIOS_entries;
static uint32_t AMS_Frame;
static uint32_t F_4x6_data;
static uint32_t F_6x8_data;
static uint32_t F_8x10_data;
static uint32_t TrapBFunctions;
static uint8_t  AMS_Major;

static uint32_t enabled_changes;

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


//! Seek in output file in order to point to given absolute address.
static void Seek (uint32_t absaddr) {
    fseek(output, absaddr - delta, SEEK_SET);
}

//! Get the absolute address currently pointed to.
static uint32_t Tell (void) {
    return (ftell(output) + delta);
}


// Search for values, return the absolute address of the first byte after the value.
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


//! Get address of given ROM_CALL.
static uint32_t rom_call_addr (uint32_t idx) {
    return GetLong(jmp_tbl + 4 * idx);
}

//! Get address of given vector.
static uint32_t GetVector (uint32_t absaddr) {
    fseek(output, HEAD + 0x88 + absaddr, SEEK_SET);
    return ReadLong();
}

//! Get address of a PC-relative JSR or LEA.
static uint32_t GetPCRelativeValue(uint32_t absaddr) {
    Seek(absaddr);
    return absaddr + ((int32_t)(int16_t)ReadShort());
}

//! Get address of given function of trap #$B.
static uint32_t GetTrapBFunction (uint32_t idx) {
    uint32_t temp;

    if (TrapBFunctions == 0) {
        temp = GetVector(0xAC);
        Seek(temp);
        temp = SearchLong(UINT32_C(0xC6FC0006));
        TrapBFunctions = GetPCRelativeValue(temp - 6);
    }
    return GetLong(TrapBFunctions + 6 * idx);
}

//! Get attribute in OO_SYSTEM_FRAME.
static uint32_t GetAttribute (uint32_t attr) {
    uint32_t temp;
    uint32_t temp2;
    int32_t limit;
    
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

//! Compute checksum.
static uint32_t ComputeChecksum(uint32_t size, uint32_t start) {
    uint16_t temp;
    uint32_t temp2 = 0;
    Seek(start);
    while (size > 0) {
        temp = ReadShort();
        temp2 += (uint32_t)temp;
        size -= 2;
    }
    return temp2;
}


//! Find **TIFL** in .xxu file.
static int FindTIFL (FILE *file) {
    char *point;
    point = malloc(0xA008);
    if (!point) {
        printf("\n    ERROR : not enough memory.\n");
        return 1;
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
            return 0;
        }
    }
    free (point);
    return 1;
}


static int SkipLicense(void) {
    char buffer[30];

    // Find our way into the file, several sanity checks.
    fread (buffer, 1, sizeof("**TIFL**")-1, input);
    if (strncmp (buffer, "**TIFL**", sizeof("**TIFL**")-1)) {
WrongType:
        printf ("\n    ERROR : wrong input file type.\n"\
        "    Use .89u, .9xu or .v2u ROM files.\n");
        fclose(input);
        return 3;
    }

    fseek (input, 0x11, SEEK_SET);
    fread (buffer, 1, sizeof("basecode")-1, input);

    if (!strncmp (buffer, "License", sizeof("License") - 1)) {
        I = 0x11;
        if (FindTIFL(input)) {
            goto WrongType;
        }
        HEAD = I + 17;
        fseek (input, I + 17, SEEK_SET);
        printf("\tINFO: found %" PRIu32 " bytes of license at the beginning of the file\n",I);
        fread (buffer, 1, sizeof ("basecode")-1, input);
        if (strncmp (buffer, "basecode", sizeof ("basecode") - 1)) {
            goto WrongType;
        }
    }
    else {
        HEAD = 17;
        if (strncmp (buffer, "basecode", sizeof("basecode") - 1)) {
            goto WrongType;
        }
    }

    HEAD += 61;
    fseek (input, 0x16+HEAD, SEEK_SET);
    fread (buffer, 1, 29, input);
    if (strncmp (buffer, "Advanced Mathematics Software", sizeof("Advanced Mathematics Software") - 1)) {
        goto WrongType;
    }

    fseek (input, 2+ HEAD, SEEK_SET);
    fread (buffer, 1, 4, input);
    BasecodeSize =   UINT32_C(0x0000001) * (unsigned char)buffer[3]
                   + UINT32_C(0x0000100) * (unsigned char)buffer[2]
                   + UINT32_C(0x0010000) * (unsigned char)buffer[1]
                   + UINT32_C(0x1000000) * (unsigned char)buffer[0];
    printf("\tINFO: found AMS of size %" PRIu32 " (0x%" PRIX32 ")\n", BasecodeSize, BasecodeSize);

    return 0;
}


static int AMSSanityChecks(void) {
    uint8_t calc;
    uint32_t maxAMSsize;
    uint32_t range;

    // Check calculator type
    fseek (input, 8+HEAD, SEEK_SET);
    calc = fgetc(input);
    printf("\tINFO: found calculator type %" PRIu8 "\n", calc);
    if ((calc != TI89) && (calc != TI92P) && (calc != V200) && (calc != TI89T)) {
        printf("\n    ERROR: unknown calculator type, aborting...\n");
        fclose(input);
        return 4;
    }

    // Check AMS version.
    fgetc(input);
    fgetc(input);
    I = fgetc(input);
    printf("\tINFO: found AMS version type %" PRIu32 "\n", I);
    if (I != 9 && I != 12 && I != 13 && I != 14) {
        printf ("\n    ERROR : unsupported AMS version.\n"
                "    Use AMS 2.05, 2.09, 3.01 or 3.10.\n"
                "    If you really need another version, please contact the author.\n"
               );
        fclose (input);
        return 5;
    }

    // Check size.
    // Starting at 12, TI didn't bother incrementing the version number properly...
    maxAMSsize = UINT32_C(0x140000) - UINT32_C(0x12000);
    if (I >= 12) {
        // 89 OS is larger than 92+/V200 OS.
        if (calc != TI92P) {
            maxAMSsize += UINT32_C(0x10000);
        }
        if (I >= 13) {
            maxAMSsize += UINT32_C(0x10000);
            if (I == 14) {
                maxAMSsize += UINT32_C(0x10000);
            }
        }
    }
    range = UINT32_C(0x10000);
    if (calc == V200 && I == 12) {
        range += UINT32_C(0x10000);
    }

    printf("\tINFO: expecting a size between 0x%" PRIX32 " and 0x%" PRIX32 ":", maxAMSsize - range, maxAMSsize);
    if ((BasecodeSize + 67 < maxAMSsize - range) || (BasecodeSize + 67 >= maxAMSsize)) {
        printf ("\n    ERROR: unexpected size in file, aborting...\n");
        fclose(input);
        return 6;
    }
    printf(" correct.\n");

    return 0;
}


static int CreateFillOutputFile(int argc, char *argv[]) {
    uint32_t i;

    printf ("    Creating output file '%s'...\n", argv[argc - 1]);
    if ((output = fopen (argv[argc - 1], "rb")) != NULL) {
        printf ("\n    ERROR : file '%s' already exists. Refusing to overwrite it.", argv[argc - 1]);
        fclose(output);
        fclose(input);
        return 7;
    }

    if ((output = fopen (argv[argc - 1], "w+b"))==NULL) {
        printf ("\n    ERROR : can't create '%s'.\n", argv[argc - 1]);
        fclose (input);
        return 8;
    }

    printf("\tINFO: copying %" PRIu32 " bytes to output file\n", BasecodeSize + HEAD + AdditionalSize);
    fseek (input, 0, SEEK_SET);
    for (i = 0; i < BasecodeSize + HEAD + AdditionalSize; i++) {
        fputc (fgetc (input), output);
    }

    fclose(input);

    return 0;
}


static int SetupAMS(int argc, char *argv[]) {
    uint32_t temp, temp2;
    int i;

    i = SkipLicense();
    if (i) {
        return i;
    }

    i = AMSSanityChecks();
    if (i) {
        return i;
    }
    
    i = CreateFillOutputFile(argc, argv);
    if (i) {
        return i;
    }


    // Setup internal variables.
    jmp_tbl = GetVector(0xC8);
    ROM_base = jmp_tbl & UINT32_C(0xE00000);
    delta = ROM_base + UINT32_C(0x12000) - HEAD;

    TIOS_entries = rom_call_addr(-1);
    TrapBFunctions = 0;
    AMS_Frame = 0;
    temp = rom_call_addr(ReleaseVersion);
    AMS_Major = GetByte(temp);


    // One last check: the basecode checksum.
    BasecodeSize = GetLong(ROM_base + UINT32_C(0x12000) + 2) + 2;
    temp = GetLong(BasecodeSize + ROM_base + UINT32_C(0x12000));
    temp2 = ComputeChecksum(BasecodeSize, ROM_base + UINT32_C(0x12000));
    printf("\tINFO: embedded basecode checksum is %08" PRIX32 ".\n"
           "\t      computed basecode checksum is %08" PRIX32 ".\n\n", temp, temp2);
    if (temp != temp2) {
        printf ("    ERROR : computed checksum does not match the checksum embedded into AMS.\n"
                "    Refusing to modify the file, please use a pristine copy of AMS.");
        fclose(output);
        fclose(input);
        return 9;
    }

    return 0;
}


static void KillAMSProtections(void) {
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
        temp = GetTrapBFunction(0x10);
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
            temp = GetPCRelativeValue(temp2 - 0x0C);
            temp2 = rom_call_addr(memcmp);
            Seek(temp);
            temp = SearchLong(temp2);
            printf("Killing FlashApp signature checking at %06" PRIX32 "\n", temp - 0x0E);
            temp2 = GetShort(temp - 0x08);
            PutShort(temp2, temp - 0x0C);
        }
    }
}


void OptimizeAMS(void) {
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


void CleanupAMS(void) {
    uint32_t temp;

    // Update basecode checksum.
    temp = ComputeChecksum(BasecodeSize, ROM_base + UINT32_C(0x12000));
    printf("\n\tINFO: new basecode checksum is %08" PRIX32 ".\n", temp);
    PutLong(temp, BasecodeSize + ROM_base + UINT32_C(0x12000));

    printf ("\n    Fix successful.\n");
    fclose(output);
}


//! Where all the fun begins...
int main (int argc, char *argv[])
{
    int i;

    printf ("\n- TIOS Fixer v0.2 by Lionel Debroux (portions from TI-68k Flash Apps Installer v0.3 by Olivier Armand & Lionel Debroux) -\n\n");
    if ((argc < 3) || (!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help"))) {
        printf ("    Usage : tiosfix [+/-options] base.xxu patched_base.xxu\n"
                "    options: * " AMS_HARDCODE_FONTS_STR " (defaults to enabled)\n"
                "             * " AMS_HARDCODE_ENGLISH_LANGUAGE_STR " (defaults to disabled)\n"
                "    (prefix name option with '+' to force enable it, with '-' to force disable it)\n"
               );
        return 1;
    }
    if ( (input = fopen (argv[argc - 2],"rb")) == NULL) {
        printf ("    ERROR : file '%s' not found.\n", argv[argc - 2]);
        return 2;
    }
    else {
        printf("    Opening '%s'...\n", argv[argc - 2]);
    }

    // Changes enabled by default.
    enabled_changes = AMS_HARDCODE_FONTS_FLAG;

    // Adjust the list of enabled changes according to the program's parameters.
    for (i = 1; i < argc - 2; i++) {
        if (!strcmp(argv[i]+1, AMS_HARDCODE_FONTS_STR)) {
            if (argv[i][0] == '-') {
                enabled_changes &= ~AMS_HARDCODE_FONTS_FLAG;
            }
        }
        else if (!strcmp(argv[i]+1, AMS_HARDCODE_ENGLISH_LANGUAGE_STR)) {
            if (argv[i][0] == '+') {
                enabled_changes |= AMS_HARDCODE_ENGLISH_LANGUAGE_FLAG;
            }
        }
    }


    // Setup the program for the modification stage.
    i = SetupAMS(argc, argv);
    if (i) {
        return i;
    }


    // Fiddle with AMS :-)
    KillAMSProtections();

    OptimizeAMS();


    // Cleanup and return.
    CleanupAMS();

    return 0;
}
