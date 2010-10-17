/**
 * \file tiosmod.c
 * \brief building blocks for a computer-based unlocking and optimizing
 *        program aimed at official TI-68k calculators OS
 * Copyright (C) 2010 Lionel Debroux (lionel underscore debroux yahoo fr)
 * Portions Copyright (C) 2000-2010 Olivier Armand (ExtendeD)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 (and only version 2) of the
 * License.
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
#include <unistd.h>


// The program's command-line switches
#define AMS_HARDCODE_FONTS_STR             "ams-hardcode-fonts"
#define AMS_HARDCODE_FONTS_FLAG            (0x00000001)
#define AMS_HARDCODE_ENGLISH_LANGUAGE_STR  "ams-hardcode-english-language"
#define AMS_HARDCODE_ENGLISH_LANGUAGE_FLAG (0x00000002)


//! Calculator models
enum {TI89 = 3, TI92P = 1, V200 = 8, TI89T = 9};


// The indices of ROM_CALLs used by this program
#define DrawChar                     (0x1A4)
#define DrawClipChar                 (0x191)
#define DrawStr                      (0x1A9)
#define EM_GetArchiveMemoryBeginning (0x3CF)
#define EV_runningApp                (0x45D)
#define EX_stoBCD                    (0x0C0)
#define FiftyMsecTick                (0x4FC)
#define HeapDeref                    (0x096)
#define HeapTable                    (0x441)
#define memcmp                       (0x270)
#define OO_Deref                     (0x3FB)
#define OO_CondGetAttr               (0x3FA)
#define ReleaseVersion               (0x440)
#define sf_width                     (0x4D3)
#define XR_stringPtr                 (0x293)
#define OSContrastDn                 (0x297)
#define OSContrastUp                 (0x296)
#define OSRegisterTimer              (0x0F0)
#define OSVFreeTimer                 (0x285)
#define OSVRegisterTimer             (0x284)


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
static uint32_t Trap9Pointers;
static uint32_t TrapBFunctions;
static uint8_t  AMS_Major;
static uint8_t  AMS_Minor;
static uint8_t  CalculatorType;

static uint32_t enabled_changes;

static FILE *input;
static FILE *output;
static char * OutputFileName;
static uint32_t OutputFileSize;
static uint32_t SizeShrunk;


// The function called by main() after opening an AMS update file and setting the base internal variables.
void PatchAMS(void);


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

static void GetNBytes (uint8_t * buffer, uint32_t n, uint32_t absaddr) {
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


// Search forward for values, return the absolute address of the first byte after the value.
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


// Search backwards for values, return the absolute address of the value.
static uint32_t SearchBackwardsByte (uint8_t value) {
    while (ReadByte() != value) {
        fseek(output, -2, SEEK_CUR);
    }
    return Tell() - 1;
}

static uint32_t SearchBackwardsShort (uint16_t value) {
    while (ReadShort() != value) {
        fseek(output, -4, SEEK_CUR);
    }
    return Tell() - 2;
}

static uint32_t SearchBackwardsLong (uint32_t value) {
    while (ReadLong() != value) {
        fseek(output, -6, SEEK_CUR);
    }
    return Tell() - 4;
}



//! Get address of given ROM_CALL.
static uint32_t rom_call_addr (uint32_t idx) {
    return GetLong(jmp_tbl + 4 * idx);
}

//! Compute checksum.
static uint32_t ComputeAMSChecksum(uint32_t size, uint32_t start) {
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
    uint32_t expectedSize;

    // Check calculator type
    fseek (input, 8+HEAD, SEEK_SET);
    CalculatorType = fgetc(input);
    printf("\tINFO: found calculator type %" PRIu8 "\n", CalculatorType);
    if ((CalculatorType != TI89) && (CalculatorType != TI92P) && (CalculatorType != V200) && (CalculatorType != TI89T)) {
        printf("\n    ERROR: unknown calculator type, aborting...\n");
        fclose(input);
        return 4;
    }

    // Get AMS version.
    fgetc(input);
    fgetc(input);
    I = fgetc(input);
    printf("\tINFO: found AMS version type %" PRIu32 "\n", I);

    // Check size.
    // Starting at 12, TI didn't bother incrementing the version number properly...
    if (I == 9) {
        if (CalculatorType == TI89) expectedSize = 0x124772;
        else if (CalculatorType == TI92P) expectedSize = 0x123F8E;
        else goto SizeError;
    }
    else if (I == 11) {
        if (CalculatorType == TI89) expectedSize = 0x12E01A;
        else if (CalculatorType == TI92P) expectedSize = 0x12D96A;
        else if (CalculatorType == V200) expectedSize = 0x12DBEE;
        else goto SizeError;
    }
    else if (I == 12) {
        if (CalculatorType == TI89) expectedSize = 0x12E2FE;
        else if (CalculatorType == TI92P) expectedSize = 0x12DC4E;
        else if (CalculatorType == V200) expectedSize = 0;
        else goto SizeError;
    }
    else if (I == 13) {
        if (CalculatorType == TI89T) expectedSize = 0x14565A;
        else if (CalculatorType == V200) expectedSize = 0x148D3A;
        else goto SizeError;
    }
    else if (I == 14) {
        if (CalculatorType == TI89T) expectedSize = 0x155C3E;
        else goto SizeError;
    }
    else {
        printf ("\n    ERROR : unsupported AMS version.\n"
                "    Use AMS 2.05, 2.08, 2.09, 3.01 or 3.10.\n"
                "    If you really need another version, please contact the author.\n"
               );
        fclose (input);
        return 5;
    }

    // Special case: multiple versions bear the same version number.
    if (CalculatorType == V200 && I == 12) {
        if (BasecodeSize != 0x12DECA && BasecodeSize != 0x1393F6) {
            goto SizeError;
        }
    }
    else {
        if (BasecodeSize != expectedSize) {
SizeError:
            printf ("\n    ERROR: unexpected size 0x%" PRIX32 " in file, aborting...\n", BasecodeSize);
            fclose(input);
            return 6;
        }
    }

    return 0;
}


static int CreateFillOutputFileAMS(int argc, char *argv[]) {
    uint32_t i;

    OutputFileName = argv[argc - 1];
    printf ("    Creating output file '%s'...\n", OutputFileName);
    if ((output = fopen (OutputFileName, "rb")) != NULL) {
        printf ("\n    ERROR : file '%s' already exists. Refusing to overwrite it.", OutputFileName);
        fclose(output);
        fclose(input);
        return 7;
    }

    if ((output = fopen (OutputFileName, "w+b"))==NULL) {
        printf ("\n    ERROR : can't create '%s'.\n", OutputFileName);
        fclose (input);
        return 8;
    }

    OutputFileSize = BasecodeSize + HEAD + AdditionalSize;
    printf("\tINFO: copying %" PRIu32 " bytes to output file\n", OutputFileSize);
    fseek (input, 0, SEEK_SET);
    for (i = 0; i < OutputFileSize; i++) {
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
    
    i = CreateFillOutputFileAMS(argc, argv);
    if (i) {
        return i;
    }


    // Setup internal variables.
    fseek(output, HEAD + 0x88 + 0xC8, SEEK_SET);
    jmp_tbl = ReadLong();
    ROM_base = jmp_tbl & UINT32_C(0xE00000);
    delta = ROM_base + UINT32_C(0x12000) - HEAD;

    TIOS_entries = rom_call_addr(-1);
    Trap9Pointers = 0;
    TrapBFunctions = 0;
    AMS_Frame = 0;
    temp = rom_call_addr(ReleaseVersion);
    AMS_Major = GetByte(temp) - '0';
    AMS_Minor = ((GetByte(temp + 2) - '0') * 10) + (GetByte(temp + 3) - '0');

    SizeShrunk = 0;

    // One last check: the basecode checksum.
    BasecodeSize = GetLong(ROM_base + UINT32_C(0x12000) + 2) + 2;
    temp = GetLong(BasecodeSize + ROM_base + UINT32_C(0x12000));
    temp2 = ComputeAMSChecksum(BasecodeSize, ROM_base + UINT32_C(0x12000));
    printf("\tINFO: embedded basecode checksum is %08" PRIX32 ".\n"
           "\t      computed basecode checksum is %08" PRIX32 ".\n\n", temp, temp2);
    if (temp != temp2) {
        printf ("    ERROR : computed checksum does not match the checksum embedded into AMS.\n"
                "            Refusing to modify the file, please use a pristine copy of AMS.");
        fclose(output);
        fclose(input);
        return 9;
    }

    return 0;
}


static void FinishAMS(void) {
    uint32_t temp;

    // Update basecode checksum.
    temp = ComputeAMSChecksum(BasecodeSize - SizeShrunk, ROM_base + UINT32_C(0x12000));
    printf("\n\tINFO: new basecode checksum is %08" PRIX32 ".\n", temp);
    PutLong(temp, BasecodeSize - SizeShrunk + ROM_base + UINT32_C(0x12000));

    printf ("\n    Fix successful.\n");

    OutputFileSize -= SizeShrunk;
    // Change little-endian size bytes if necessary.
    if (SizeShrunk != 0) {
        OutputFileSize -= HEAD;
        temp =   ((OutputFileSize & UINT32_C(0x000000FF)) << 24) 
               | ((OutputFileSize & UINT32_C(0x0000FF00)) << 8) 
               | ((OutputFileSize & UINT32_C(0x00FF0000)) >> 8) 
               | ((OutputFileSize & UINT32_C(0xFF000000)) >> 24);
        OutputFileSize += HEAD;
        PutLong(temp, ROM_base + UINT32_C(0x12000) - 4); // Slightly dirty.
    }

    fclose(output);

    // Truncate file if necessary.
    if (SizeShrunk != 0) {
        printf("\n\tINFO: final file size is %" PRIu32 " (0x%" PRIX32 ")\n", OutputFileSize, OutputFileSize);
        if (truncate(OutputFileName, OutputFileSize) != 0) {
            printf("ERROR truncating file, OS will probably be invalid\n");
        }
    }
}


//! Where all the fun begins...
int main (int argc, char *argv[])
{
    int i;

    printf ("\n- TIOS Modder v0.2.5 by Lionel Debroux (portions from TI-68k Flash Apps Installer v0.3 by Olivier Armand & Lionel Debroux) -\n");
    printf ("- Using patchset: " PATCHDESC "\n\n");
    if ((argc < 3) || (!strcmp(argv[1], "-h")) || (!strcmp(argv[1], "--help"))) {
        printf ("    Usage : tiosmod [+/-options] base.xxu patched_base.xxu\n"
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
    PatchAMS();


    // Cleanup and return.
    FinishAMS();

    return 0;
}
