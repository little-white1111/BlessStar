/* ── shm_notify.rs ─────────────────────────────────────────────────
 * 第38天 ⑧ · Windows Named Pipe 跨进程通知（发送端）
 *
 * 提供 napi-rs FFI: shm_notify_send(pipe_name, message) → result_json
 * 非 Windows 平台返回 "not supported" 降级。
 * ───────────────────────────────────────────────────────────────── */

#[cfg(windows)]
use std::io::Write;

#[cfg(windows)]
use std::os::windows::io::FromRawHandle;

#[napi]
pub fn shm_notify_send(pipe_name: String, message: String) -> napi::Result<String> {
  #[cfg(not(windows))]
  {
    let _ = (pipe_name, message);
    return Ok(r#"{"success":false,"error":"Named Pipe only supported on Windows"}"#.into());
  }

  #[cfg(windows)]
  {
    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::Storage::FileSystem::{
      CreateFileW, FILE_GENERIC_WRITE, OPEN_EXISTING,
    };
    use windows_sys::Win32::Foundation::{CloseHandle, INVALID_HANDLE_VALUE};

    let wide: Vec<u16> = OsStr::new(&pipe_name)
      .encode_wide()
      .chain(std::iter::once(0))
      .collect();

    let handle = unsafe {
      CreateFileW(
        wide.as_ptr(),
        FILE_GENERIC_WRITE,
        0,
        std::ptr::null_mut(),
        OPEN_EXISTING,
        0,
        0,
      )
    };

    if handle == INVALID_HANDLE_VALUE || handle == 0 {
      return Ok(
        format!(
          r#"{{"success":false,"error":"Cannot connect to pipe: {}"}}"#,
          pipe_name
        )
        .into(),
      );
    }

    let result = {
      let mut file = unsafe { std::fs::File::from_raw_handle(handle as _) };
      match file.write_all(message.as_bytes()) {
        Ok(()) => {
          let _ = file.flush();
          r#"{"success":true,"sent":true}"#.to_string()
        }
        Err(e) => format!(r#"{{"success":false,"error":"{}"}}"#, e),
      }
    };

    unsafe { CloseHandle(handle) };
    Ok(result.into())
  }
}
