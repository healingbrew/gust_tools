/*
  A18_Decrypt - Archive unpacker for Gust (Koei/Tecmo) PC games
  Copyright © 2019 VitaSmith
  Copyright © 2018 Yuri Hime (shizukachan)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#define ftell64 _ftelli64
#define fseek64 _fseeki64
#if !defined(S_ISDIR)
#define S_ISDIR(ST_MODE) (((ST_MODE) & _S_IFMT) == _S_IFDIR)
#endif
#define CREATE_DIR(path) CreateDirectoryA(path, NULL)
#define PATH_SEP '\\'
#else
#define ftell64 ftello64
#define fseek64 fseeko64
#define CREATE_DIR(path) (mkdir(path, 0755) == 0)
#define PATH_SEP '/'
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#pragma pack(push, 1)
typedef struct {
    uint32_t unknown1;
    uint32_t nb_entries;
    uint32_t unknown2;
    uint32_t unknown3;
} pak_header;

typedef struct {
    char     filename[128];
    uint32_t length;
    uint8_t  key[20];
    uint32_t data_offset;
    uint32_t dummy;
} pak_entry32;

typedef struct {
    char     filename[128];
    uint32_t length;
    uint8_t  key[20];
    uint64_t data_offset;
    uint64_t dummy;
} pak_entry64;
#pragma pack(pop)

static pak_header header;
static pak_entry64* entries64 = NULL;

#if defined(_WIN32)
static char* basename(const char* path)
{
    static char app_name[64];
    _splitpath_s(path, NULL, 0, NULL, 0, app_name, sizeof(app_name), NULL, 0);
    return app_name;
}
#endif

static bool create_path(char* path)
{
    bool result = true;
    struct stat st;
    if (stat(path, &st) != 0) {
        // Directory doesn't exist, create it
        size_t pos = 0;
        for (size_t n = strlen(path); n > 0; n--) {
            if (path[n] == PATH_SEP) {
                pos = n;
                break;
            }
        }
        if (pos > 0) {
            // Create parent dirs
            path[pos] = 0;
            char* new_path = malloc(strlen(path) + 1);
            if (new_path == NULL) {
                fprintf(stderr, "ERROR: Can't allocate path\n");
                return false;
            }
            strcpy(new_path, path);
            result = create_path(new_path);
            free(new_path);
            path[pos] = PATH_SEP;
        }
        // Create node
        if (result)
            result = CREATE_DIR(path);
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: '%s' exists but isn't a directory\n", path);
        return false;
    }

    return result;
}

static __inline void decode(uint8_t* a, uint8_t* k, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
        a[i] ^= k[i % 20];
}

// To handle either 32 and 64 bit PAK entries
#define entry(i, m) (is_pak32 ? (((pak_entry32*)entries64)[i]).m : entries64[i].m)

int main(int argc, char** argv)
{
    bool is_pak32 = false;
    if (argc != 2) {
        printf("%s (c) 2018-2019 Yuri Hime & VitaSmith\n\nUsage: %s <Gust PAK file>\n\n"
            "Dumps the Gust PAK format archive to the current directory.\n"
            "If unpacked to the game directory, you can remove the .pak file\n"
            "and it will use the unpacked assets. Have fun, modders!\n",
            basename(argv[0]), basename(argv[0]));
        return 0;
    }
    FILE* src = fopen(argv[1], "rb");
    if (src == NULL) {
        fprintf(stderr, "Can't open PAK file '%s'", argv[1]);
        return -1;
    }

    if (fread(&header, sizeof(header), 1, src) == 0) {
        fprintf(stderr, "Can't read header");
        return -1;
    }

    if ((header.unknown1 != 0x20000) || (header.unknown2 != 0x10) || (header.unknown3 != 0x0D)) {
        fprintf(stderr, "WARNING: Signature doesn't match expected PAK file format.\n");
    }

    if (header.nb_entries > 16384) {
        fprintf(stderr, "WARNING: More than 16384 entries, is this a supported archive?\n");
    }
    entries64 = calloc(header.nb_entries, sizeof(pak_entry64));
    if (entries64 == NULL) {
        fprintf(stderr, "ERROR: Can't allocate entries\n");
        return -1;
    }

    fread(entries64, sizeof(pak_entry64), header.nb_entries, src);

    // Detect if we are dealing with 32 or 64-bit pak entries by checking
    // the data_offsets at the expected 32 and 64-bit struct location and
    // adding the absolute value of the difference with last data_offset.
    // The sum that is closest to zero tells us if we are dealing with a
    // 32 or 64-bit PAK archive.
    uint64_t sum[2] = { 0, 0 };
    uint32_t val[2], last[2] = { 0, 0 };
    for (uint32_t i = 0; i < min(header.nb_entries, 64); i++) {
        val[0] = ((pak_entry32*)entries64)[i].data_offset;
        val[1] = (uint32_t)(entries64[i].data_offset >> 32);
        for (int j = 0; j < 2; j++) {
            sum[j] += (val[j] > last[j]) ? val[j] - last[j] : last[j] - val[j];
            last[j] = val[j];
        }
    }
    is_pak32 = (sum[0] < sum[1]);
    printf("Detected %s PAK format\n\n", is_pak32 ? "A17/32-bit" : "A18/64-bit");

    char path[256];
    uint8_t* buf;
    bool skip_decode;
    int64_t file_data_offset = sizeof(pak_header) + header.nb_entries * (is_pak32 ? sizeof(pak_entry32) : sizeof(pak_entry64));
    printf("OFFSET    SIZE     NAME\n");
    for (uint32_t i = 0; i < header.nb_entries; i++) {
        int j;
        for (j = 0; (j < 20) && (entry(i, key)[j] == 0); j++);
        skip_decode = (j >= 20);
        if (!skip_decode)
            decode((uint8_t*)entry(i, filename), entry(i, key), 128);
        for (size_t n = 0; n < strlen(entry(i, filename)); n++) {
            if (entry(i, filename)[n] == '\\')
                entry(i, filename)[n] = PATH_SEP;
        }
        printf("%09" PRIx64 " %08x %s%c\n", entry(i, data_offset) + file_data_offset,
            entry(i, length), entry(i, filename), skip_decode?'*':' ');
        strcpy(path, &entry(i, filename)[1]);
        for (size_t n = strlen(path); n > 0; n--) {
            if (path[n] == PATH_SEP) {
                path[n] = 0;
                break;
            }
        }
        if (!create_path(path)) {
            fprintf(stderr, "Can't create path '%s'\n", path);
            continue;
        }
        FILE* dst = NULL;
        dst = fopen(&entry(i, filename)[1], "wb");
        if (dst == NULL) {
            fprintf(stderr, "Can't create file '%s'\n", &entry(i, filename)[1]);
            continue;
        }
        fseek64(src, entry(i, data_offset) + file_data_offset, SEEK_SET);
        buf = malloc(entry(i, length));
        if (buf == NULL) {
            fprintf(stderr, "ERROR: Can't allocate entries\n");
            return -1;
        }
        fread(buf, 1, entry(i, length), src);
        if (!skip_decode)
            decode(buf, entry(i, key), entry(i, length));
        fwrite(buf, 1, entry(i, length), dst);
        free(buf);
        fclose(dst);
    }
    fclose(src);
    free(entries64);
    return 0;
}