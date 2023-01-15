#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
//#include <stat.h>
#include <io.h>
#include <doslib.h>
#include <iocslib.h>
#include <zlib.h>
#include "memory.h"

#define VERSION "0.1.0"

inline static void* _doscall_malloc(size_t size) {
  int addr = MALLOC(size);
  return (addr >= 0x81000000) ? NULL : (char*)addr;
}

inline static void _doscall_mfree(void* ptr) {
  if (ptr == NULL) return;
  MFREE((int)ptr);
}

static void show_help() {
  printf("usage: rsrx [options]\n");
  printf("options:\n");
  printf("     -d <download directory> (default:current directory)\n");
  printf("     -s <baud rate> (default:9600)\n");
  printf("     -t <timeout[sec]> (default:120)\n");
  printf("     -f ... overwrite existing file (default:no)\n");
  printf("     -r ... no CRC check\n");
  printf("     -h ... show version and help message\n");
}

int main(int argc, char* argv[]) {

  // default exit code
  int rc = 1;

  // program credit and version
  printf("RSRX.X - RS232C File Receiver " VERSION " 2023 by tantan\n");
   
  // default parameters
  int baud_rate = 9600;
  int timeout = 120;
  int overwrite = 0;
  int crc_check = 1;
  int buffer_size = 128 * 1024;
  unsigned char* chunk_data = NULL;

  // download path
  static unsigned char download_path [ 256 ];
  strcpy(download_path, ".");
  
  // command line options
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
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
  int path_len = strlen(download_path);
  unsigned char uc = download_path[ path_len - 1 ];
  unsigned char uc2 = path_len >= 2 ? download_path[ path_len - 2 ] : 0;
  if (uc != '\\' && uc != '/' && uc2 != ':') {
    strcat(download_path, "\\");
  }

  // convert baud rate speed to IOCS SET232C() speed value
  int speed = 9;
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
    printf("unsupported baud rate.\n");
    goto exit;      
  }

  // setup RS232C port
  SET232C( 0x4C00 + speed );    // 8bit, non-P, 1stop, no-XON

  // buffer memory allocation
  chunk_data = _doscall_malloc(buffer_size);

  // describe settings
  printf("--\n");
  printf("Download Path: %s\n", download_path);
  printf("Baud Rate: %d bps\n", baud_rate);
  printf("Timeout: %d sec\n", timeout);
  printf("Buffer Size: %d KB\n", buffer_size);

  // output file handle
  FILE* fp = NULL;

  // main loop
  for (;;) {

    // try to find eye catch (RSTX7650)
    static unsigned char eye_catch_start[9] = "RSTX7650";
    static unsigned char eye_catch_end[9]   = "RSTXDONE";

    int t0 = time(NULL);
    int t1 = t0;
    int ofs = 0;
    int found = 0;
    while ((t1 - t0) < timeout) {
      if (LOF232C() > 0) {
        unsigned char uc = INP232C() & 0xff;
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
      if (BITSNS(0) & 0x02) break;    // ESC key
      t1 = time(NULL);
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
    t0 = time(NULL);
    t1 = t0;
    found = 0;
    while ((t1 - t0) < timeout) {
      if (LOF232C() >= 72) {
        found = 1;
        break;
      }
      if (BITSNS(0) & 0x02) break;    // ESC key
      t1 = time(NULL);
    } 
    if (!found) {
      printf("error: Cannot get header information in time.\n");
      goto exit;
    }

    // parse header
    static unsigned char file_size[4];
    static unsigned char file_name[32 + 1];
    static unsigned char file_time[19 + 1];
    static unsigned char padding[17 + 1];
    for (int i = 0; i < 4; i++) {
      file_size[i] = INP232C() & 0xff;
    }
    for (int i = 0; i < 32; i++) {
      file_name[i] = INP232C() & 0xff;
    }
    for (int i = 0; i < 19; i++) {
      file_time[i] = INP232C() & 0xff;
    }
    for (int i = 0; i < 17; i++) {
      padding[i] = INP232C() & 0xff;
    }

    int file_sz = *((int*)file_size);
    file_name[32] = '\0';
    for (int i = 31; i > 0; i--) {      // trimming
      if (file_name[i] == ' ') {
        file_name[i] = '\0';
      } else {
        break;
      }
    }
    file_time[19] = '\0';
    padding[17] = '\0';

    static unsigned char out_path_name[ 256 ];
    strcpy(out_path_name, download_path);
    strcat(out_path_name, file_name);

    if (!overwrite) {
      int suffix = 0;
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

    printf("--\n");
    printf("Eye Catch: %s\n", eye_catch_start);
    printf("File Size: %d\n", file_sz);
    printf("File Name: %s\n", file_name);
    printf("File Time: %s\n", file_time);
    //printf("Padding: %s\n", padding);
    printf("--\n");

    // current time
    int file_start_time = time(NULL);

    // current crc
    unsigned int current_crc = 0;

    // total received size
    int received_size = 0;

    // total received time
    int received_time = 0;

    for (;;) {

      // chunk size
      t0 = time(NULL);
      t1 = t0;
      found = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() >= 4) {
          found = 1;
          break;
        }
        if (BITSNS(0) & 0x02) break;    // ESC key
        t1 = time(NULL);
      } 
      if (!found) {
        printf("error: Cannot get chunk size information in time.\n");
        goto exit;
      }
      static unsigned char chunk_size[4];
      for (int i = 0; i < 4; i++) {
        chunk_size[i] = INP232C() & 0xff;
      }
      int chunk_sz = *((int*)chunk_size);
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
      //printf("Chunk Size = %d\n", chunk_sz);

      // chunk data
      t0 = time(NULL);
      t1 = t0;
      ofs = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() > 0) {
          chunk_data[ofs++] = INP232C() & 0xff;
          if (ofs >= chunk_sz) {
            break;
          }
        }
        if (BITSNS(0) & 0x02) break;    // ESC key
        t1 = time(NULL);
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
      t0 = time(NULL);
      t1 = t0;
      found = 0;
      while ((t1 - t0) < timeout) {
        if (LOF232C() >= 4) {
          found = 1;
          break;
        }
        if (BITSNS(0) & 0x02) break;    // ESC key
        t1 = time(NULL);
      } 
      if (!found) {
        printf("error: Cannot get chunk crc information in time.\n");
        goto exit;
      }
      static unsigned char chunk_crc[4];
      for (int i = 0; i < 4; i++) {
        chunk_crc[i] = INP232C() & 0xff;
      }

      if (crc_check) {
        unsigned int expected_crc = *((unsigned int*)chunk_crc);
        unsigned int actual_crc = crc32(current_crc, chunk_data, chunk_sz);
        if (actual_crc != expected_crc) {
          printf("error: CRC error.\n");
          goto exit;
        }
        current_crc = actual_crc;
      }

      received_size += chunk_sz;

      int elapsed = time(NULL) - file_start_time;
      printf("Received %d/%d bytes (%4.2f%%) in %d sec.\n", received_size, file_sz, 100.0*received_size/file_sz, elapsed);
    }

    // close file
    fclose(fp);
    fp = NULL;

    // update file timestamp
    unsigned char year[] = "    ";
    unsigned char month[] = "  ";
    unsigned char day[] = "  ";
    unsigned char hour[] = "  ";
    unsigned char min[] = "  ";
    unsigned char sec[] = "  ";
    memcpy(year,file_time,4);
    memcpy(month,file_time+5,2);
    memcpy(day,file_time+8,2);
    memcpy(hour,file_time+11,2);
    memcpy(min,file_time+14,2);
    memcpy(sec,file_time+17,2);
    int dt = ((atoi(year) - 1980) << 25) | (atoi(month) << 21) | (atoi(day) << 16) | (atoi(hour) << 11) | (atoi(min) << 5) | (atoi(sec));
    //int fno = open(out_path_name, O_WRONLY | O_BINARY, S_IWRITE);
    int fno = open(out_path_name, O_WRONLY | O_BINARY, 1);              // to avoid conflict with zlib.h
    FILEDATE(fno, dt);
    close(fno);

    int elapsed = time(NULL) - file_start_time;
    printf("Received %d/%d bytes successfully in %d sec as %s\n", received_size, file_sz, elapsed, out_path_name);

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