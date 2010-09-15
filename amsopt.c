// Derived from my (Lionel Debroux) expanded version (0.3) of ExtendeD's "flapinst".

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#define EndAMSSign 73

enum {TI89 = 3, TI92 = 1, V200 = 8, TI89T = 9};

enum {AMS205 = 9, AMS209 = 12};


#define DrawChar                     (0x1A4)
#define DrawClipChar                 (0x191)
#define DrawStr                      (0x1A9)
#define EM_GetArchiveMemoryBeginning (0x3CF)
#define EX_stoBCD                    (0x0C0)
#define HeapDeref                    (0x096)
#define ReleaseDate                  (0x43F)
#define ReleaseVersion               (0x440)
#define sf_width                     (0x4D3)

static uint32_t HEAD;
static uint32_t delta;
static uint32_t ROM_base;
static uint32_t jmp_tbl;
static uint32_t TIOS_entries;
static uint32_t AMS_frame;
static uint32_t F_4x6_data;
static uint32_t F_6x8_data;
static uint32_t F_8x10_data;

FILE *input;
FILE *output;


// Read data at the current file position
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


// Write data at the current file position
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


// Read data at the given absolute address
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


// Write data at the given absolute address
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


static uint32_t rom_call_addr (uint32_t idx) {
    return GetLong(jmp_tbl + 4 * idx);
}


uint32_t FindTIFL(uint32_t I, FILE *file) {
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


int main(int argc, char *argv[])
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
        "            use .89u, .9xu or .v2u ROM files.\n");
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
    size =   0x0000001L * (unsigned char)buffer[3]
           + 0x0000100L * (unsigned char)buffer[2]
           + 0x0010000L * (unsigned char)buffer[1]
           + 0x1000000L * (unsigned char)buffer[0];
    printf("\tINFO: found AMS of size %" PRIu32 "\n", size);

    // Calculator type ?
    fseek (input, 8+HEAD, SEEK_SET);
    calc=fgetc(input);
    if ((calc != TI89) && (calc != TI92) && (calc != V200) && (calc != TI89T)) {
        printf("Unknown calculator type %" PRIu8 "\n", calc);
        fclose(input);
        return 4;
    }
    printf("\tINFO: found calculator type %" PRIu8 "\n", calc);

    // Check AMS version.
    fgetc(input);
    fgetc(input);
    I = fgetc(input);
    if (I != AMS205 && I != AMS209) {
WrongVersion:
        printf ("\n"
        "      Error : unsupported AMS version.\n"\
        "              Use AMS 2.05 or 2.09; you can also help expanding this program.\n"
        );
        fclose (input);
        return 5;
    }
    printf("\tINFO: found AMS version type %" PRIu32 "\n", I);

    // Check size.
    maxAMSsize = (I == AMS205) ? 0x140000UL-0x12000UL : 0x150000UL-0x12000UL;
    printf("\tINFO: maximum expected size for that AMS version type is %" PRIu32 "\n", maxAMSsize);

    if (size > maxAMSsize) {
        printf ("\n\n    Error: OS larger than expected, aborting...\n");
        fclose(input);
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
    fseek(output, HEAD + 0x150, SEEK_SET);
    jmp_tbl = ReadLong();
    ROM_base = jmp_tbl & UINT32_C(0xE00000);
    delta = ROM_base + 0x12000 - HEAD;

    TIOS_entries = rom_call_addr(-1);


    // 1a) Hard-code HW2/3Patch: disable RAM execution protection.
    temp = rom_call_addr(EX_stoBCD);
    printf("Killing RAM execution protection at %06" PRIx32 "\n", temp + 0x56);
    PutShort(0x0000, temp + 0x58);
    PutShort(0x0000, temp + 0x5C);
    PutShort(0x0000, temp + 0x62);
    PutShort(0x0000, temp + 0x68);


    // 1b) Disable Flash execution protection by setting a higher value in port 700012.
    //     * 1 direct write in the early reset code.
    fseek(output, HEAD + 0x188, SEEK_SET);
    while (ReadLong() != UINT32_C(0x700012)) {
        fseek(output, -2, SEEK_CUR);
    }
    fseek(output, -6, SEEK_CUR);
    printf("Killing Flash execution protection (1) at %06" PRIx32 "\n", ftell(output) + delta - 2);
    WriteShort(0x003F);
    //     * 1 reference in a subroutine of the trap #$B, function $10 handler.
    fseek(output, HEAD + 0x134, SEEK_SET);
    temp = ReadLong();
    fseek(output, temp - delta, SEEK_SET);
    while (ReadShort() != 0xC6FC) {
        temp += 2;
    }
    fseek(output, -4, SEEK_CUR);
    temp2 = (int32_t)(int16_t)ReadShort();
    temp = temp - 2 + temp2;
    temp = GetLong(temp + 0x60);
    temp2 = (int32_t)(int16_t)GetShort(temp + 0x08);
    temp = (temp + 0x08) + temp2;
    printf("Killing Flash execution protection (2) at %06" PRIx32 "\n", temp);
    PutShort(0x4E75, temp);


    // 1c) Hard-code a change equivalent to MaxMem and XPand: don't call the subroutine EM_GetArchiveMemoryBeginning
    //     calls before returning, after rounding up the result of OO_GetEndOfAllFlashApps, that both MaxMem and XPand modify.
    temp = rom_call_addr(EM_GetArchiveMemoryBeginning);
    temp2 = 0;
    while (temp2 != UINT32_C(0xFFFF0000)) {
        temp2 = GetLong(temp);
        temp += 2;
    }
    printf("Increasing amount of archive memory at %06" PRIx32 "\n", temp + 0x02);
    PutShort(0x2040, temp + 0x02);
    PutShort(0x508F, temp + 0x04);
    PutShort(0x4E75, temp + 0x06);
    /* // Hard-code MaxMem.
    temp2 = (int32_t)(int16_t)GetShort(temp + 0x24);
    temp = (temp + 0x24) + temp2;
    temp2 = 0;
    while (temp2 != 0x0C80) {
        temp2 = GetShort(temp);
        temp += 2;
    }
    printf("Unleashing more archive memory for HW1 calcs at %06" PRIx32 "\n", temp + 0x04);
    PutByte(0x60, temp + 0x04);*/


    // 1d) TODO: Hard-code Flashappy, for seamless install of unsigned FlashApps.
    //     AFAICS, Flashappy screws with a subroutine of trap #$B, function $9 handler.


    // 2a) Rewrite HeapDeref
    // AMS 1.xx: 30 2F 00 04 E5 48 20 78  xx xx 20 70 00 00 4E 75
    // AMS 2.xx: 70 00 30 2F 00 04 E5 88  20 78 xx xx 20 70 08 00  4E 75
    // AMS 3.xx: 70 00 30 2F 00 04 E5 88  20 79 xx xx xx xx 20 70  08 00 4E 75
    temp = rom_call_addr(HeapDeref);
    printf("Rewriting HeapDeref at %06" PRIx32 "\n", temp);
    GetNBytes(buffer, 16, temp);
    buffer[0]  = 0x30; buffer[1]  = 0x2F; buffer[2]  = 0x00; buffer[3]  = 0x04;
    buffer[4]  = 0xE5; buffer[5]  = 0x48; buffer[6]  = 0x20; buffer[7]  = 0x78;
    buffer[8]  = buffer[10];
    buffer[9]  = buffer[11];
    buffer[10] = 0x20; buffer[11] = 0x70; buffer[12] = 0x00; buffer[13] = 0x00;
    buffer[14] = 0x4E; buffer[15] = 0x75;
    PutNBytes(buffer, 16, rom_call_addr(HeapDeref));


    // 2b) Hard-code OO_GetAttr(OO_SYSTEM_FRAME, OO_(S|L|H)FONT) into the subroutine of DrawStr/DrawChar/DrawClipChar.
    fseek(output, HEAD + 0x130, SEEK_SET);
    temp = ReadLong();
    AMS_frame   = GetLong(temp + 10);
    F_4x6_data  = GetLong(AMS_frame + 0x96);
    F_6x8_data  = GetLong(AMS_frame + 0x9E);
    F_8x10_data = GetLong(AMS_frame + 0xA6);

    temp = rom_call_addr(DrawChar);
    temp2 = (int32_t)(int16_t)GetShort(temp + 0x26);
    temp = (temp + 0x26) + temp2;

    printf("Optimizing character drawing in %06" PRIx32 "\n", temp);
    PutShort(0x41F9, temp + 0x7A);
    PutLong(F_8x10_data, temp + 0x7C);
    PutShort(0x602A, temp + 0x80);
    PutShort(0x41F9, temp + 0xC2);
    PutLong(F_6x8_data, temp + 0xC4);
    PutShort(0x602A, temp + 0xC8);
    PutShort(0x41F9, temp + 0x112);
    PutLong(F_4x6_data, temp + 0x114);
    PutShort(0x602A, temp + 0x118);


    // 2c) Hard-code OO_GetAttr(OO_SYSTEM_FRAME, OO_SFONT) in rewritten sf_width.
    temp = rom_call_addr(sf_width);
    printf("Rewriting sf_width at %06" PRIx32 "\n", temp);
    PutShort(0x41F9, temp + 0x00);
    PutLong(F_4x6_data, temp + 0x02);
    PutShort(0x7000, temp + 0x06);
    PutLong(0x102F0005, temp + 0x08);
    PutShort(0x3200, temp + 0x0C);
    PutShort(0xD040, temp + 0x0E);
    PutShort(0xD041, temp + 0x10);
    PutShort(0xD040, temp + 0x12);
    PutLong(0x10300000, temp + 0x14);
    PutShort(0x4E75, temp + 0x18);


    printf ("\n    Optimization successful.\n");
    fclose(output);
    return 0;
}
