# Cross-Language ConfigConsumer Examples

This document provides example code snippets for using the `ConfigConsumer` SHM‑based notification system from different languages.

## C++ (Header‑Only, Single Header)

```cpp
// config_consumer_example.cpp
#include <iostream>
#include "bs/app/sdk/config_consumer.hpp"

int main() {
    bs::sdk::ConfigConsumer consumer("myapp/config/v1");

    // Subscribe to changes
    consumer.subscribe([](std::span<const uint8_t> data, uint64_t version) {
        std::cout << "Config updated, version = " << version << ", data size = " << data.size() << std::endl;
        // decode data as needed …
    });

    while (true) {
        if (consumer.wait_and_read(1000)) {
            auto span = consumer.data();
            // process span …
        }
    }
    return 0;
}
```

## Python (via ctypes)

```python
import ctypes
import os
import sys
import time

# Load the shared library (adjust name as needed)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'build'))
lib = ctypes.CDLL('./libbs_app_sdk.so')

# Define structures and function prototypes
class BsConsumer(ctypes.Structure):
    _fields_ = []  # opaque for ctypes

lib.bs_consumer_create.argtypes = [ctypes.c_char_p, ctypes.c_int]
lib.bs_consumer_create.restype = ctypes.POINTER(BsConsumer)

lib.bs_consumer_destroy.argtypes = [ctypes.POINTER(BsConsumer)]
lib.bs_consumer_destroy.restype = None

lib.bs_consumer_wait.argtypes = [ctypes.POINTER(BsConsumer), ctypes.c_int]
lib.bs_consumer_wait.restype = ctypes.c_int

lib.bs_consumer_get_data.argtypes = [ctypes.POINTER(BsConsumer), ctypes.POINTER(ctypes.c_size_t)]
lib.bs_consumer_get_data.restype = ctypes.c_void_p

lib.bs_consumer_get_version.argtypes = [ctypes.POINTER(BsConsumer)]
lib.bs_consumer_get_version.restype = ctypes.c_ulonglong

# Usage
shm_path = "/bs_config_myapp/config/v1"
consumer = lib.bs_consumer_create(shm_path.encode(), -1)
if not consumer:
    print("Failed to create consumer")
    sys.exit(1)

while True:
    if lib.bs_consumer_wait(consumer, 1000) == 0:
        len_ = ctypes.c_size_t()
        data_ptr = lib.bs_consumer_get_data(consumer, ctypes.byref(len_))
        if data_ptr:
            data = ctypes.string_at(data_ptr, len_.value)
            print(f"Received {len_.value} bytes, version {lib.bs_consumer_get_version(consumer)}")
            # Parse data …
    time.sleep(0.1)

lib.bs_consumer_destroy(consumer)
```

## Go (using syscall.Mmap)

```go
package main

import (
    "fmt"
    "os"
    "syscall"
    "unsafe"
    "time"
)

type BsConsumer struct {
    fd   int
    shm  []byte
}

func createConsumer(shmPath string) (*BsConsumer, error) {
    // Open/create the shared memory object
    fd, err := syscall.Open(shmPath, syscall.O_RDWR, 0)
    if err != nil {
        return nil, err
    }

    // Stat to get size
    var stat syscall.Stat_t
    if err := syscall.Fstat(fd, &stat); err != nil {
        syscall.Close(fd)
        return nil, err
    }

    // Mmap
    shm, err := syscall.Mmap(fd, 0, int(stat.Size), syscall.PROT_READ|syscall.PROT_WRITE, syscall.MAP_SHARED)
    if err != nil {
        syscall.Close(fd)
        return nil, err
    }

    return &BsConsumer{fd: fd, shm: shm}, nil
}

func (c *BsConsumer) Close() {
    syscall.Munmap(c.shm)
    syscall.Close(c.fd)
}

func (c *BsConsumer) Wait(timeoutMs int) error {
    // Simple busy‑wait for demonstration – production code should use eventfd/poll
    time.Sleep(time.Duration(timeoutMs) * time.Millisecond)
    return nil
}

func (c *BsConsumer) Read() ([]byte, uint64_t) {
    // Layout parsing omitted – assume:
    //   offset 0x00  uint64 version_counter
    //   offset 0x08  uint64 active_buf_idx
    //   offset 0x10 uint64 data_len
    //   offset 0x40 bufA[64K] …
    if len(c.shm) < 0x40+8 { // minimal header size
        return nil, 0
    }
    // example read of version (little‑endian)
    ver := *(*uint64_t)(unsafe.Pointer(&c.shm[0]))
    // data_len
    dataLen := *(*uint64_t)(unsafe.Pointer(&c.shm[0x10]))
    start := 0x40 + (uint64(0) << 16) // active_buf_idx * SHM_BUF_SIZE
    data := c.shm[start : start+dataLen]
    return data, ver
}

func main() {
    const shmPath = "/bs_config_myapp/config/v1"
    consumer, err := createConsumer(shmPath)
    if err != nil {
        fmt.Fprintf(os.Stderr, "Failed to create consumer: %v\n", err)
        os.Exit(1)
    }
    defer consumer.Close()

    for {
        if err := consumer.Wait(1000); err == nil {
            data, ver := consumer.Read()
            fmt.Printf("Got config, version %d, %d bytes\n", ver, len(data))
            // Process data …
        }
    }
}
```

## Rust (using memfd_create + mmap)

```rust
use std::fs::File;
use std::io::Write;
use std::os::unix::fs::FileExt;
use std::os::unix::io::{FromRawFd, IntoRawFd};
use std::ptr::null_mut;
use std::slice;

#[repr(C)]
struct ShmLayout {
    version_counter: u64,   // +0x00
    active_buf_idx: u64,    // +0x08
    data_len: u64,          // +0x10
    reserved: [u8; 40],     // +0x18
    buf_a: [u8; 64 * 1024],
    buf_b: [u8; 64 * 1024],
}

struct ConfigConsumer {
    file: File,
    mapping: *mut ShmLayout,
}

impl ConfigConsumer {
    fn open(shmPath: &str) -> std::io::Result<Self> {
        // Open existing memfd
        let file = File::open(shmPath)?;
        let mapping = unsafe {
            libc::mmap(
                null_mut(),
                std::mem::size_of::<ShmLayout>(),
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_SHARED,
                file.as_raw_fd(),
                0,
            ) as *mut ShmLayout
        };
        if mapping == null_mut() {
            return Err(std::io::Error::last_os_error());
        }
        Ok(Self { file, mapping })
    }

    fn wait(&self, timeout_ms: i32) -> std::io::Result<()> {
        // Simplification – in production integrate with the eventfd_notifier
        std::thread::sleep(std::time::Duration::from_millis(timeout_ms as u64));
        Ok(())
    }

    fn read(&self) -> (u64, &[u8]) {
        let layout = unsafe { &*self.mapping };
        let ver = layout.version_counter;
        let len = layout.data_len;
        let buf_idx = layout.active_buf_idx;
        let slice = match buf_idx {
            0 => &layout.buf_a[..len as usize],
            1 => &layout.buf_b[..len as usize],
            _ => &[][..],
        };
        (ver, slice)
    }
}

fn main() {
    let consumer = match ConfigConsumer::open("/bs_config_myapp/config/v1") {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Failed to open consumer: {}", e);
            return;
        }
    };

    loop {
        if let Ok(()) = consumer.wait(1000) {
            let (ver, data) = consumer.read();
            println!("Received config, version = {}, {} bytes", ver, data.len());
            // Process data …
        }
    }
}
```

---

*To extend this document for additional languages, follow the same pattern: abstract the layout struct, use platform‑specific shared memory APIs, and implement the wait/read primitives using the C ABI (`bs_consumer_t`) or directly via SHM layout.*
