#!/usr/bin/env python3
import sys
import os
import ftplib
import pathlib
import time

def upload_dir(ftp, local_dir, remote_dir):
    try:
        ftp.mkd(remote_dir)
    except ftplib.error_perm:
        pass

    for item in os.listdir(local_dir):
        lpath = os.path.join(local_dir, item)
        rpath = f"{remote_dir}/{item}".replace("//", "/")
        if os.path.isdir(lpath):
            upload_dir(ftp, lpath, rpath)
        else:
            size = os.path.getsize(lpath)
            print(f"Uploading {item} ({size / (1024*1024):.1f} MB)...", flush=True)
            t0 = time.time()
            with open(lpath, "rb") as f:
                ftp.storbinary(f"STOR {rpath}", f, blocksize=65536)
            elapsed = max(0.001, time.time() - t0)
            print(f"  done in {elapsed:.1f}s ({(size/1024/elapsed):.1f} KB/s)", flush=True)

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/3ds/upload_ftp.py <3ds-ip> [port=5000]")
        sys.exit(1)

    ip = sys.argv[1]
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 5000
    source = pathlib.Path("sdcard/3ds")
    if not source.exists():
        print(f"Error: {source} not found")
        sys.exit(1)

    print(f"Connecting to {ip}:{port}...")
    ftp = ftplib.FTP()
    ftp.connect(ip, port, timeout=30)
    ftp.login()
    print("Connected. Uploading sdcard/3ds to /3ds/ ...")
    upload_dir(ftp, str(source), "/3ds")
    ftp.quit()
    print("Upload complete!")

if __name__ == "__main__":
    main()
