#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <io.h>
#include <doslib.h>
#include <iocslib.h>
#include <zlib.h>
#include "memory.h"

#define VERSION "0.2.0"

inline static void* _doscall_malloc(size_t size) {
  uint32_t addr = MALLOC(size);
  return (addr >= 0x81000000) ? NULL : (void*)addr;
}

inline static void _doscall_mfree(void* ptr) {
  if (ptr == NULL) return;
  MFREE((uint32_t)ptr);
}

static void show_help() {
  printf("usage: rsrx [options]\n");
  printf("options:\n");
  printf("     -d <download directory> (default:current directory)\n");
  printf("     -s <baud rate> (default:19200)\n");
  printf("     -t <timeout[sec]> (default:120)\n");
  printf("     -f ... overwrite existing file (default:no)\n");
  printf("     -r ... no CRC check\n");
  printf("     -h ... show version and help message\n");
}

int32_t main(int32_t argc, uint8_t* argv[]) {

  // default exit code
  int32_t rc = -1;

  // program credit and version
  printf("RSRX.X - RS232C File Receiver " VERSION " 2023 by tantan\n");
   
  // default parameters
  int32_t baud_rate = 19200;
  int16_t timeout = 120;
  int16_t overwrite = 0;
  int16_t crc_check = 1;
  size_t buffer_size = 128 * 1024;

  // data buffer
  uint8_t* chunk_data = NULL;

  // output file handle
  FILE* fp = NULL;

  // download path
  static uint8_t download_path [ 256 ];
  strcpy(download_path, ".");
  
  // command line options
  for (int16_t i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && strlen(argv[i]) >= 2) {
      if (argv[i][1] == 'd' && i+1 < argc) {
        strcpy(download_path, argv[i+1]);
      } else if (argv[i][1] == 's' && i+1 < argc) {
        baud_rate = atoi(argv[i+1]);
      } else if (argv[i][1] == 't' && i+1 < argc) {
        timeout = atoi(argv[i+1]);
      } else if (argv[i][1] == 'b' && i+1 < argc) {
        buffer_size = atoi(argv[i+1]);
      } else if (argv[i][1] == 'f') {
        overwrite = 1;
      } else if (argv[i][1] == 'r') {
        crc_check = 0;
      } else if (argv[i][1] == 'h') {
        show_help();
        goto exit;
      } else {
        printf("error: unknown option.\n");
        goto exit;
      }
    }
  }

  // add path delimitor if needed
  int16_t path_len = strlen(download_path);
  uint8_t uc = download_path[ path_len - 1 ];
  uint8_t uc2 = path_len >= 2 ? download_path[ path_len - 2 ] : 0;
  if (uc != '\\' && uc != '/' && uc2 != ':') {
    strcat(download_path, "\\");
  }

  // convert baud rate speed to IOCS SET232C() speed value
  int32_t speed = 8;
  switch (baud_rate) {
    case 9600:
      speed = 7;
      break;
    case 19200:
      speed = 8;
      break;
    case 38400:
      speed = 9;
      break;
    case 52080:
    case 57600:
      speed = 0x0d;
      break;
    case 76800:
    case 78125:
      speed = 0x0e;
      break;
    case 117180:
      speed = 0x0f;
      break;
    default:
    printf("error: unsupported baud rate.\n");
    goto exit;      
  }

  // setup RS232C port
  SET232C( 0x4C00 + speed );    // 8bit, non-P, 1stop, no-XON

  // buffer memory allocation
  chunk_data = _doscall_malloc(buffer_size);
  if (chunk_data == NULL) {
    printf("error: cannot allocate buffer memory.\n");
    goto exit;
  }

  // describe settings
  printf("--\n");
  printf("Download Path: %s\n", download_path);
  printf("Baud Rate: %d bps\n", baud_rate);
  printf("Timeout: %d sec\n", timeout);
  printf("Buffer Size: %d KB\n", buffer_size);

  // main loop
  for (;;) {

    // try to find eye catch (RSTX7650)
    static uint8_t eye_catch_start[9] = "RSTX7650";
    static uint8_t eye_catch_end[9]   = "RSTXDONE";

    uint32_t t0 = ONTIME();
    uint32_t t1 = t0;
    timeout *= 100;

    int32_t ofs = 0;
    int16_t found = 0;

    while ((t1 - t0) < timeout) {
      if (LOF232C() > 0) {
        uint8_t uc = INP232C() & 0xff;
        if (uc == eye_catch_start[ofs] || uc == eye_catch_end[ofs]) {
          if (ofs == 7) {
            found = (uc == eye_catch_start[ofs]) ? 1 : -1;      // found full 8 bytes!
            break;   
          }
          ofs++;
        } else {
          ofs = 0;                // need to rewind
        }
      }
      if (BITSNS(0) & 0x02) {
        printf("Closed communication.\n");
        rc = 1;
        goto exit;
      }
      t1 = ONTIME();
    }
    printf("--\n");
    if (found == 0) {
      printf("error: Communiation link failure. Cannot find eye catch.\n");
      goto exit;
    } else if (found == -1) {
      printf("Closed communication.\n");
      rc = 0;
      goto exit;
    }

    printf("Established communication link.\n");

    // file size (4 bytes) + file name (32 bytes) + file time (19 bytes) + padding (17 bytes)
    t0 = ONTIME();
    t1 = t0;
    found = 0;
    while ((t1 - t0) < timeout) {
      if (LOF232C() >= 72) {
        found = 1;
        break;
      }
      if (BITSNS(0) & 0x02) {
        // ESC key
        printf("Closed communication.\n");
        goto exit;
      }
      t1 = ONTIME();
    } 
    if (!found) {
      printf("error: Cannot get header information in time.\n");
      goto exit;
    }

    // parse header
    static uint8_t file_size[4];
    static uint8_t file_name[32 + 1];
    static uint8_t file_time[19 + 1];
    static uint8_t padding[17 + 1];
    for (int16_t i = 0; i < 4; i++) {
      file_size[i] = INP232C() & 0xff;
    }
    for (int16_t i = 0; i < 32; i++) {
      file_name[i] = INP232C() & 0xff;
    }
    for (int16_t i = 0; i < 19; i++) {
      file_time[i] = INP232C() & 0xff;
    }
    for (int16_t i = 0; i < 17; i++) {
      padding[i] = INP232C() & 0xff;
    }

    uint32_t file_sz = *((uint32_t*)file_size);
    file_name[32] = '\0';
    for (int16_t i = 31; i > 0; i--) {      // trimming
      if (file_name[i] == ' ') {
        file_name[i] = '\0';
      } else {
        break;
      }
    }
    file_time[19] = '\0';
    padding[17] = '\0';

    printf("--\n");
    printf("Eye Catch: %s\n", eye_catch_start);
    printf("File Size: %d\n", file_sz);
    printf("File Name: %s\n", file_name);
    printf("File Time: %s\n", file_time);
    //printf("Padding: %s\n", padding);
    printf("--\n");

    static uint8_t out_path_name[ 256 ];
    strcpy(out_path_name, download_path);
    strcat(out_path_name, file_name);

    if (!overwrite) {
      int16_t suffix = 0;
      for (suffix = 0; suffix < 10; suffix++) {
        static struct FILBUF filbuf;
        if (FILES(&filbuf, out_path_name, 0x33) >= 0) {
          out_path_name[ strlen(out_path_name) - 1 ] = '0' + suffix;
          printf("waring: Target file already exists. Storing to %s\n", out_path_name);
        } else {
          break;
        }
      }
      if (suffix >= 10) {
        printf("error: Too many duplications. Cannot write file.\n");
        goto exit;
      }
    }

    if ((fp = fopen(out_path_name, "wb")) == NULL) {
      printf("error: File write open error.\n");
      goto exit;
    }

    // current time
    uint32_t file_start_time = ONTIME();

    // current crc
    uint32_t current_crc = 0;

    // total received size
    uint32_t received_size = 0;

    // total received time
    uint32_t received_time = 0;

    for (;;) {

      // chunk size
      t0 = ONTIME();
      t1 = t0;
      found = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() >= 4) {
          found = 1;
          break;
        }
        if (BITSNS(0) & 0x02) {
          // ESC key
          printf("Closed communication.\n");
          goto exit;
        }
        t1 = ONTIME();
      } 
      if (!found) {
        printf("error: Cannot get chunk size information in time.\n");
        goto exit;
      }
      static uint8_t chunk_size[4];
      for (int16_t i = 0; i < 4; i++) {
        chunk_size[i] = INP232C() & 0xff;
      }
      uint32_t chunk_sz = *((uint32_t*)chunk_size);
      if (chunk_sz >= buffer_size) {
        printf("error: Chunk size (%d) is larger than buffer size (%d).\n", chunk_sz, buffer_size);
        goto exit;
      }
      if (chunk_sz == 0) {
        if (received_size != file_sz) {
          printf("error: file size unmatch. expected=%d, actual=%d\n", file_size, received_size);
          goto exit;
        }
        break;
      }
  
      // chunk data
      t0 = ONTIME();
      t1 = t0;
      ofs = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() > 0) {
          chunk_data[ofs++] = INP232C() & 0xff;
          if (ofs >= chunk_sz) {
            break;
          }
        }
        if (BITSNS(0) & 0x02) {
          printf("Closed communication.\n");
          goto exit;
        }
        t1 = ONTIME();
      } 
      if (ofs < chunk_sz) {
        printf("error: Cannot get chunk data in time.\n");
        goto exit;
      }
      if (fwrite(chunk_data, 1, chunk_sz, fp) < chunk_sz) {
        printf("error: File write error.\n");
        goto exit;
      }

      // chunk crc
      t0 = ONTIME();
      t1 = t0;
      found = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() >= 4) {
          found = 1;
          break;
        }
        if (BITSNS(0) & 0x02) {
          // ESC key
          printf("Closed communication.\n");
          goto exit;
        }
        t1 = ONTIME();
      } 
      if (!found) {
        printf("error: Cannot get chunk crc information in time.\n");
        goto exit;
      }
      static uint8_t chunk_crc[4];
      for (int16_t i = 0; i < 4; i++) {
        chunk_crc[i] = INP232C() & 0xff;
      }

      if (crc_check) {
        uint32_t expected_crc = *((uint32_t*)chunk_crc);
        uint32_t actual_crc = crc32(current_crc, chunk_data, chunk_sz);
        if (actual_crc != expected_crc) {
          printf("error: CRC error.\n");
          goto exit;
        }
        current_crc = actual_crc;
      }

      received_size += chunk_sz;

      uint32_t elapsed = ONTIME() - file_start_time;
      printf("\rReceived %d/%d bytes (%4.2f%%) in %4.2f sec.", received_size, file_sz, 100.0*received_size/file_sz, elapsed/100.0);
    }

    // close file
    fclose(fp);
    fp = NULL;

    // update file timestamp
    uint8_t year[] = "    ";
    uint8_t month[] = "  ";
    uint8_t day[] = "  ";
    uint8_t hour[] = "  ";
    uint8_t min[] = "  ";
    uint8_t sec[] = "  ";
    memcpy(year,file_time,4);
    memcpy(month,file_time+5,2);
    memcpy(day,file_time+8,2);
    memcpy(hour,file_time+11,2);
    memcpy(min,file_time+14,2);
    memcpy(sec,file_time+17,2);
    uint32_t dt = ((atoi(year) - 1980) << 25) | (atoi(month) << 21) | (atoi(day) << 16) | (atoi(hour) << 11) | (atoi(min) << 5) | (atoi(sec));
    //uint32_t fno = open(out_path_name, O_WRONLY | O_BINARY, S_IWRITE);
    uint32_t fno = open(out_path_name, O_WRONLY | O_BINARY, 1);              // to avoid conflict with zlib.h
    FILEDATE(fno, dt);
    close(fno);

    uint32_t elapsed = ONTIME() - file_start_time;
    printf("\rReceived %d/%d bytes successfully in %4.2f sec as %s\n", received_size, file_sz, elapsed/100.0, out_path_name);

  }

exit:

  // close output file handle
  if (fp != NULL) {
    fclose(fp);
    fp = NULL;
  }

  // discard unconsumed data
  while (LOF232C() > 0) {
    INP232C();
  }

  // free buffer
  if (chunk_data != NULL) {
    _doscall_mfree(chunk_data);
    chunk_data = NULL;
  }

  // flush key buffer
  while (B_KEYSNS() != 0) {
    B_KEYINP();
  }

  return rc;
}