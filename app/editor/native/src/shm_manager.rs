/* ── shm_manager.rs ────────────────────────────────────────────────
 * 第38天 ⑧ · Windows Named Pipe 跨进程通知（监听端）
 *
 * 提供 napi-rs FFI: shm_manager_listen(pipe_name) → message_json
 * 阻塞等待一次连接，读取后返回消息内容。
 * 非 Windows 平台返回 "not supported" 降级。
 * ───────────────────────────────────────────────────────────────── */

#[napi]
pub fn shm_manager_listen(pipe_name: String) -> napi::Result<String> {
  #[cfg(not(windows))]
  {
    let _ = pipe_name;
    return Ok(r#"{"success":false,"error":"Named Pipe only supported on Windows"}"#.into());
  }

  #[cfg(windows)]
  {
    use std::io::Read;

    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::System::Pipes::{
      ConnectNamedPipe, CreateNamedPipeW, PIPE_ACCESS_INBOUND,
      PIPE_READMODE_BYTE, PIPE_TYPE_BYTE, PIPE_WAIT,
    };
    use windows_sys::Win32::Foundation::{
      CloseHandle, ERROR_PIPE_CONNECTED, INVALID_HANDLE_VALUE,
    };

    let wide: Vec<u16> = OsStr::new(&pipe_name)
      .encode_wide()
      .chain(std::iter::once(0))
      .collect();

    let handle = unsafe {
      CreateNamedPipeW(
        wide.as_ptr(),
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,    // 最大实例数
        4096, // 输出缓冲区大小
        4096, // 输入缓冲区大小
        5000, // 默认超时 5s
        std::ptr::null_mut(),
      )
    };

    if handle == INVALID_HANDLE_VALUE {
      return Ok(
        format!(
          r#"{{"success":false,"error":"Cannot create pipe: {}"}}"#,
          pipe_name
        )
        .into(),
      );
    }

    // 阻塞等待客户端连接
    let connected = unsafe { ConnectNamedPipe(handle, std::ptr::null_mut()) };
    if connected == 0 {
      let err = unsafe { windows_sys::Win32::Foundation::GetLastError() };
      if err != ERROR_PIPE_CONNECTED {
        unsafe { CloseHandle(handle) };
        return Ok(
          format!(
            r#"{{"success":false,"error":"Pipe connect failed: code {}"}}"#,
            err
          )
          .into(),
        );
      }
    }

    // 读取消息
    let mut buf = vec![0u8; 4096];
    let n = {
      let mut file = unsafe { std::fs::File::from_raw_handle(handle as _) };
      match file.read(&mut buf) {
        Ok(n) => n,
        Err(e) => {
          unsafe { CloseHandle(handle) };
          return Ok(format!(r#"{{"success":false,"error":"{}"}}"#, e).into());
        }
      }
    };

    unsafe { CloseHandle(handle) };

    let msg = String::from_utf8_lossy(&buf[..n]).to_string();
    Ok(
      format!(
        r#"{{"success":true,"message":{}}}"#,
        serde_json::to_string(&msg).unwrap_or_else(|_| "\"\"".into())
      )
      .into(),
    )
  }
}
