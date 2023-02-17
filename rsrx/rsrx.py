import argparse
import os
import datetime
import time
import serial
import binascii

def read_in_chunks(file_object, chunk_size):
    """Lazy function (generator) to read a file piece by piece.
    Default chunk size: 1k."""
    while True:
        data = file_object.read(chunk_size)
        if not data:
            break
        yield data

def receive_files(download_path, device, speed, timeout, force):

  port = serial.Serial( device, speed,
                        bytesize = serial.EIGHTBITS,
                        parity = serial.PARITY_NONE,
                        stopbits = serial.STOPBITS_ONE,
                        timeout = timeout,
                        xonxoff = False,
                        rtscts = True,
                        dsrdtr = False )
  # open port
  if port.isOpen() is False:
    port.open()

  print("--")
  print(f"Download Path: {download_path}")
  print(f"Baud Rate: {speed} bps")
  print(f"Timeout: {timeout} sec")

  if download_path[-1] != '/':
    download_path += '/'

  try:

    while True:

      eye_catch_start = "RSTX7650".encode('ascii')
      eye_catch_end   = "RSTXDONE".encode('ascii')

      eye_catch = []
      ofs = 0
      found = 0

      # read eye catch
      while True:
        c = port.read()
        if c[0] == eye_catch_start[ofs]:
          if ofs == 7:
            found = 1
            break
          else:
            ofs += 1
        elif c[0] == eye_catch_end[ofs]:
          if ofs == 7:
            found = -1
            break
          else:
            ofs += 1

      print("--")

      if found == 0:
        raise Exception("error: communiation link failure. cannot find eye catch.")
      elif found == -1:
        print("Closed communication.")
        return

      print("Established communication link.")

      # file size (4 bytes) + file name (32 bytes) + file time (19 bytes) + padding (17 bytes)
      header = port.read(72)
      file_size = header[0:4]
      file_name = header[4:36].decode('cp932').rstrip()
      file_time = header[36:55].decode('ascii')
      padding = header[55:72]

      file_sz = int.from_bytes(file_size, 'big')

      print("--")
      print(f"Eye Catch: {eye_catch_start.decode('ascii')}")
      print(f"File Size: {file_sz}")
      print(f"File Name: {file_name}")
      print(f"File Time: {file_time}")
      print("--")

      download_name = download_path + file_name
      if force is not True:
        for suffix in range(10):
          if os.path.exists(download_name):
            download_name = download_path + file_name + "." + str(suffix)

        if os.path.exists(download_name):
          raise Exception("error: too many file duplications.")

      print(f"Downloading as {download_name} ...")

      with open(download_name, "wb") as f:

        total_size = 0
        crc = 0

        while True:

          chunk_len = port.read(4)
          chunk_sz = int.from_bytes(chunk_len, 'big')
          if chunk_sz == 0:
            break

          chunk_data = port.read(chunk_sz)
          chunk_crc = port.read(4)
          
          expected_crc = int.from_bytes(chunk_crc, 'big')
          chunk_crc  = binascii.crc32(chunk_data, crc)
          if chunk_crc != expected_crc:
            raise Exception(f"error: CRC error. (expected:{expected_crc:X},actual:{chunk_crc:X})")
          crc = chunk_crc

          f.write(chunk_data)

          total_size += chunk_sz

          print(f"\rReceived {total_size} bytes. (CRC={crc:X})", end="")

      mtime = datetime.datetime(year=int(file_time[0:4]), month=int(file_time[5:7]), day=int(file_time[8:10]), hour=int(file_time[11:13]), minute=int(file_time[14:16]), second=int(file_time[17:19]), microsecond=0)
      os.utime(path=download_name, times=(mtime.timestamp(), mtime.timestamp()))
      print("\nCompleted receiving operation successfully.")

  except Exception as e:
    print(e)

  finally:
    if port.isOpen():
      port.close()

def main():

    parser = argparse.ArgumentParser()

    parser.add_argument("--device", help="serial device name", default='/dev/tty.usbserial-AQ028S08')
    parser.add_argument("-d","--download_path", help="download path name", default=".")
    parser.add_argument("-s","--baudrate", help="baud rate", type=int, default=19200)
    parser.add_argument("-t","--timeout", help="time out[sec]", type=int, default=120)
    parser.add_argument("-f","--force", help="overwrite existing file", action='store_true')

    args = parser.parse_args()

    receive_files(args.download_path, args.device, args.baudrate, args.timeout, args.force)


if __name__ == "__main__":
    main()
