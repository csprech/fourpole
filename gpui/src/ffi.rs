//! The C bridge (bridge/FourpoleBridge.h), wrapped. Everything runs on the main thread.

use std::ffi::{CStr, CString, c_char, c_int};

#[repr(C)]
pub struct FpPanel {
    _private: [u8; 0],
}

unsafe extern "C" {
    fn fp_create(width: c_int, height: c_int) -> *mut FpPanel;
    fn fp_destroy(p: *mut FpPanel);
    fn fp_parts_json(p: *mut FpPanel) -> *const c_char;
    fn fp_render_background(p: *mut FpPanel, scale: f32, bgra: *mut u8, w: c_int, h: c_int) -> c_int;
    fn fp_render_part(p: *mut FpPanel, index: c_int, value: f32, scale: f32, bgra: *mut u8, w: c_int, h: c_int) -> c_int;
    fn fp_key_rect(p: *mut FpPanel, note: c_int, x: *mut f32, y: *mut f32, w: *mut f32, h: *mut f32);
    fn fp_key_is_black(note: c_int) -> c_int;
    fn fp_set(p: *mut FpPanel, id: *const c_char, value: f32);
    fn fp_note_on(p: *mut FpPanel, note: c_int, velocity: c_int);
    fn fp_note_off(p: *mut FpPanel, note: c_int);
    fn fp_set_wheel(p: *mut FpPanel, which: c_int, value: f32);
    fn fp_key_event(p: *mut FpPanel, chars: *const c_char, down: c_int, command: c_int, shift: c_int) -> c_int;
    fn fp_state_json(p: *mut FpPanel) -> *const c_char;
    fn fp_command(p: *mut FpPanel, name: *const c_char);
}

pub struct Panel(*mut FpPanel);

impl Panel {
    pub fn new(width: i32, height: i32) -> Self {
        Self(unsafe { fp_create(width, height) })
    }
    fn string(ptr: *const c_char) -> String {
        unsafe { CStr::from_ptr(ptr) }.to_string_lossy().into_owned()
    }
    pub fn parts_json(&self) -> String {
        Self::string(unsafe { fp_parts_json(self.0) })
    }
    pub fn state_json(&self) -> String {
        Self::string(unsafe { fp_state_json(self.0) })
    }
    pub fn render_background(&self, scale: f32, w: usize, h: usize) -> Vec<u8> {
        let mut px = vec![0u8; w * h * 4];
        unsafe { fp_render_background(self.0, scale, px.as_mut_ptr(), w as c_int, h as c_int) };
        px
    }
    pub fn render_part(&self, index: i32, value: f32, scale: f32, w: usize, h: usize) -> Vec<u8> {
        let mut px = vec![0u8; w * h * 4];
        unsafe { fp_render_part(self.0, index, value, scale, px.as_mut_ptr(), w as c_int, h as c_int) };
        px
    }
    pub fn key_rect(&self, note: i32) -> super::Rect {
        let (mut x, mut y, mut w, mut h) = (0.0, 0.0, 0.0, 0.0);
        unsafe { fp_key_rect(self.0, note, &mut x, &mut y, &mut w, &mut h) };
        super::Rect { x, y, w, h }
    }
    pub fn set(&self, id: &str, value: f32) {
        let id = CString::new(id).unwrap();
        unsafe { fp_set(self.0, id.as_ptr(), value) }
    }
    pub fn note_on(&self, note: i32, velocity: i32) {
        unsafe { fp_note_on(self.0, note, velocity) }
    }
    pub fn note_off(&self, note: i32) {
        unsafe { fp_note_off(self.0, note) }
    }
    pub fn set_wheel(&self, which: i32, value: f32) {
        unsafe { fp_set_wheel(self.0, which, value) }
    }
    pub fn key_event(&self, chars: &str, down: bool, command: bool, shift: bool) -> bool {
        let c = CString::new(chars).unwrap_or_default();
        unsafe { fp_key_event(self.0, c.as_ptr(), down as c_int, command as c_int, shift as c_int) != 0 }
    }
    pub fn command(&self, name: &str) {
        let n = CString::new(name).unwrap();
        unsafe { fp_command(self.0, n.as_ptr()) }
    }
}

pub fn key_is_black(note: i32) -> bool {
    unsafe { fp_key_is_black(note) != 0 }
}

impl Drop for Panel {
    fn drop(&mut self) {
        unsafe { fp_destroy(self.0) }
    }
}
