// The Rust side is only a pipe: it finds the knob's vendor HID interface, forwards every
// 64-byte input report to the UI as a `hid-report` event, and writes the UI's 64-byte reports.
// The protocol itself lives in the UI (src/proto.ts), so the same UI runs over WebHID too.

use hidapi::{HidApi, HidDevice};
use serde::Serialize;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use tauri::{AppHandle, Emitter, State};

const VENDOR_USAGE_PAGE: u16 = 0xFF00;
const VENDOR_USAGE: u16 = 0x01;
const PRODUCT: &str = "Quadra";
const REPORT_SIZE: usize = 64;

// `generation` changes on every open and close, so a reader thread left over from an earlier
// connection stops (and never clears the new one's device).
#[derive(Default)]
struct Link {
    device: Arc<Mutex<Option<HidDevice>>>,
    generation: Arc<AtomicU64>,
}

#[derive(Serialize, Clone)]
struct DeviceInfo {
    path: String,
    product: String,
    serial: String,
}

fn api() -> Result<HidApi, String> {
    HidApi::new().map_err(|e| e.to_string())
}

#[tauri::command]
fn hid_list() -> Result<Vec<DeviceInfo>, String> {
    let api = api()?;
    Ok(api
        .device_list()
        .filter(|d| {
            d.usage_page() == VENDOR_USAGE_PAGE
                && d.usage() == VENDOR_USAGE
                && d.product_string().map_or(false, |p| p.starts_with(PRODUCT))
        })
        .map(|d| DeviceInfo {
            path: d.path().to_string_lossy().into_owned(),
            product: d.product_string().unwrap_or_default().to_string(),
            serial: d.serial_number().unwrap_or_default().to_string(),
        })
        .collect())
}

#[tauri::command]
fn hid_open(path: String, link: State<Link>, app: AppHandle) -> Result<(), String> {
    let api = api()?;
    let cpath = std::ffi::CString::new(path).map_err(|e| e.to_string())?;
    let dev = api.open_path(&cpath).map_err(|e| e.to_string())?;
    let gen = link.generation.fetch_add(1, Ordering::SeqCst) + 1;
    *link.device.lock().unwrap() = Some(dev);

    // Reader: short timeouts so a write never waits long for the lock.
    let device = link.device.clone();
    let generation = link.generation.clone();
    thread::spawn(move || {
        let mut buf = [0u8; REPORT_SIZE];
        while generation.load(Ordering::SeqCst) == gen {
            let res = {
                let guard = device.lock().unwrap();
                match guard.as_ref() {
                    Some(d) => d.read_timeout(&mut buf, 5),
                    None => break,
                }
            };
            match res {
                Ok(0) => thread::sleep(Duration::from_millis(1)),
                Ok(n) => {
                    let _ = app.emit("hid-report", buf[..n].to_vec());
                }
                Err(e) => {
                    // Unplugged (or reset): drop this connection unless a newer one replaced it.
                    let mut guard = device.lock().unwrap();
                    if generation.load(Ordering::SeqCst) == gen {
                        *guard = None;
                        let _ = app.emit("hid-closed", e.to_string());
                    }
                    break;
                }
            }
        }
    });
    Ok(())
}

#[tauri::command]
fn hid_write(data: Vec<u8>, link: State<Link>) -> Result<(), String> {
    // hidapi wants the report ID first: 0 for an interface without report IDs.
    let mut out = [0u8; REPORT_SIZE + 1];
    let n = data.len().min(REPORT_SIZE);
    out[1..1 + n].copy_from_slice(&data[..n]);
    let guard = link.device.lock().unwrap();
    let dev = guard.as_ref().ok_or("not connected")?;
    dev.write(&out).map_err(|e| e.to_string())?;
    Ok(())
}

#[tauri::command]
fn hid_close(link: State<Link>) {
    link.generation.fetch_add(1, Ordering::SeqCst);
    *link.device.lock().unwrap() = None;
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(Link::default())
        .invoke_handler(tauri::generate_handler![hid_list, hid_open, hid_write, hid_close])
        .run(tauri::generate_context!())
        .expect("error while running the Quadra companion");
}
