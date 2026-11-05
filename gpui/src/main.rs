//! Fourpole with a GPUI front end.
//!
//! The instrument (its C++ engine, and its JUCE panel kept off-screen as controller and as the renderer of the
//! panel's photoreal parts) sits behind the C bridge in `bridge/FourpoleBridge.h`. This window lays the panel
//! out, composites the parts on the GPU, draws the live text and the patch bar, and plays: drag the knobs and
//! wheels, click the rockers, play the keys with the mouse or the computer keyboard.

mod ffi;

use gpui::{
    App, Application, Bounds, Context, FocusHandle, KeyDownEvent, KeyUpEvent, MouseButton, MouseDownEvent, MouseMoveEvent,
    MouseUpEvent, ObjectFit, RenderImage, ScrollWheelEvent, Timer, Window, WindowBounds, WindowOptions, div, img, prelude::*, px,
    rgb, rgba, size,
};
use serde::Deserialize;
use std::collections::HashMap;
use std::sync::Arc;
use std::time::Duration;

#[derive(Deserialize, Clone)]
struct Rect {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
}

#[derive(Deserialize, Clone)]
struct Part {
    index: i32,
    kind: String,
    id: String,
    x: f32,
    y: f32,
    w: f32,
    h: f32,
    min: f32,
    max: f32,
}

#[derive(Deserialize, Clone)]
struct Button {
    x: f32,
    y: f32,
    w: f32,
    h: f32,
    command: String,
    label: String,
}

#[derive(Deserialize)]
struct Layout {
    parts: Vec<Part>,
    #[serde(rename = "namePlate")]
    name_plate: Rect,
    buttons: Vec<Button>,
    width: f32,
    height: f32,
}

#[derive(Deserialize, Default)]
struct State {
    title: String,
    message: String,
    overload: bool,
    power: bool,
    recording: bool,
    #[serde(rename = "pitchWheel")]
    pitch_wheel: f32,
    #[serde(rename = "modWheel")]
    mod_wheel: f32,
    notes: Vec<i32>,
    values: HashMap<String, f32>,
    level: f32,
}

/// What the mouse is doing to a part.
enum Drag {
    Value { part: usize, start_y: f32, start: f32, fine: bool },
    Wheel { part: usize, start_y: f32, start: f32 },
    Key { note: i32 },
}

const KNOB_FRAMES: f32 = 120.0;
const WHEEL_FRAMES: f32 = 64.0;

struct Fourpole {
    panel: ffi::Panel,
    layout: Layout,
    keys: Vec<(i32, Rect, bool)>, // note, rectangle (in the keyboard), black
    state: State,
    background: Option<(u32, Arc<RenderImage>)>,
    sprites: HashMap<(usize, i32, u32), Arc<RenderImage>>,
    drag: Option<Drag>,
    focus: FocusHandle,
    frame: (f32, f32, f32), // scale, x and y offset of the panel in the window
}

impl Fourpole {
    fn new(cx: &mut Context<Self>) -> Self {
        let panel = ffi::Panel::new(1480, 860);
        let layout: Layout = serde_json::from_str(&panel.parts_json()).expect("the panel's layout");
        let mut keys = Vec::new();
        for note in 41..=84 {
            let r = panel.key_rect(note);
            keys.push((note, r, ffi::key_is_black(note)));
        }
        let mut this = Self {
            panel,
            layout,
            keys,
            state: State::default(),
            background: None,
            sprites: HashMap::new(),
            drag: None,
            focus: cx.focus_handle(),
            frame: (1.0, 0.0, 0.0),
        };
        this.refresh();
        if let Ok(report) = std::env::var("FOURPOLE_GPUI_SELFTEST") {
            cx.spawn(async move |this, cx| {
                Timer::after(Duration::from_millis(1500)).await;
                let mut lines = Vec::new();
                let _ = this.update(cx, |this, cx| this.self_test_controls(&mut lines, cx));
                // a key clicked with the mouse sounds
                let _ = this.update(cx, |this, cx| this.click_key(60, true, cx));
                Timer::after(Duration::from_millis(600)).await;
                let _ = this.update(cx, |this, cx| {
                    this.refresh();
                    let held = this.state.notes.contains(&60);
                    lines.push(format!("{}    the key is held: {}", if held { "ok  " } else { "FAIL" }, held));
                    let level = this.state.level;
                    lines.push(format!("{}    a key clicked plays: level {:.3}", if level > 0.01 { "ok  " } else { "FAIL" }, level));
                    this.click_key(60, false, cx);
                    let fails = lines.iter().filter(|l| l.starts_with("FAIL")).count();
                    lines.push(String::new());
                    lines.push(if fails == 0 { "ALL PASSED".into() } else { format!("{fails} FAILED") });
                    let _ = std::fs::write(&report, lines.join("\n") + "\n");
                    cx.quit();
                });
            })
            .detach();
        }
        // the panel's live state, 30 times a second
        cx.spawn(async move |this, cx| {
            loop {
                Timer::after(Duration::from_millis(33)).await;
                let ok = this.update(cx, |this, cx| {
                    this.refresh();
                    cx.notify();
                });
                if ok.is_err() {
                    break;
                }
            }
        })
        .detach();
        this
    }

    fn refresh(&mut self) {
        if let Ok(s) = serde_json::from_str::<State>(&self.panel.state_json()) {
            self.state = s;
        }
    }

    fn value(&self, p: &Part) -> f32 {
        match p.kind.as_str() {
            "lamp" => match p.id.as_str() {
                "overload" => self.state.overload as i32 as f32,
                _ => self.state.power as i32 as f32,
            },
            "wheel" => {
                if p.id == "pitchWheel" {
                    self.state.pitch_wheel
                } else {
                    self.state.mod_wheel
                }
            }
            _ => self.state.values.get(&p.id).copied().unwrap_or(0.0),
        }
    }

    /// The frame a value falls on (knobs and wheels are drawn at so many positions).
    fn quantise(p: &Part, v: f32) -> (i32, f32) {
        match p.kind.as_str() {
            "knob" => {
                let t = ((v - p.min) / (p.max - p.min)).clamp(0.0, 1.0);
                let q = (t * (KNOB_FRAMES - 1.0)).round();
                (q as i32, p.min + q / (KNOB_FRAMES - 1.0) * (p.max - p.min))
            }
            "wheel" => {
                let t = ((v - p.min) / (1.0 - p.min)).clamp(0.0, 1.0);
                let q = (t * (WHEEL_FRAMES - 1.0)).round();
                (q as i32, p.min + q / (WHEEL_FRAMES - 1.0) * (1.0 - p.min))
            }
            _ => (v.round() as i32, v.round()),
        }
    }

    fn image(w: usize, h: usize, bgra: Vec<u8>) -> Arc<RenderImage> {
        let buffer = image::RgbaImage::from_raw(w as u32, h as u32, bgra).expect("a sprite's pixels");
        Arc::new(RenderImage::new(smallvec::smallvec![image::Frame::new(buffer)]))
    }

    fn background(&mut self, scale: f32) -> Arc<RenderImage> {
        let key = (scale * 64.0).round() as u32;
        if let Some((k, img)) = &self.background {
            if *k == key {
                return img.clone();
            }
        }
        let (w, h) = ((self.layout.width * scale).ceil() as usize, (self.layout.height * scale).ceil() as usize);
        let img = Self::image(w, h, self.panel.render_background(scale, w, h));
        self.background = Some((key, img.clone()));
        img
    }

    /// A part at a value (for the keyboard: a note's key pressed, or -1 for the whole keyboard).
    fn sprite(&mut self, part: usize, value: f32, scale: f32) -> Arc<RenderImage> {
        let p = self.layout.parts[part].clone();
        let (q, v) = if p.kind == "keyboard" { (value as i32, value) } else { Self::quantise(&p, value) };
        let key = (part, q, (scale * 64.0).round() as u32);
        if let Some(img) = self.sprites.get(&key) {
            return img.clone();
        }
        let (w, h) = if p.kind == "keyboard" && value >= 0.0 {
            let r = &self.keys.iter().find(|k| k.0 == value as i32).unwrap().1;
            (r.w, r.h)
        } else {
            (p.w, p.h)
        };
        let (pw, ph) = ((w * scale).ceil() as usize, (h * scale).ceil() as usize);
        let img = Self::image(pw, ph, self.panel.render_part(p.index, v, scale, pw, ph));
        // (a window resize changes the scale: drop the sprites of the old one)
        if self.sprites.len() > 6000 {
            self.sprites.clear();
        }
        self.sprites.insert(key, img.clone());
        img
    }

    fn to_panel(&self, pos: gpui::Point<gpui::Pixels>) -> (f32, f32) {
        let (s, ox, oy) = self.frame;
        ((f32::from(pos.x) - ox) / s, (f32::from(pos.y) - oy) / s)
    }

    fn note_at(&self, kb: &Part, x: f32, y: f32) -> Option<i32> {
        let (lx, ly) = (x - kb.x, y - kb.y);
        let inside = |r: &Rect| lx >= r.x && lx < r.x + r.w && ly >= r.y && ly < r.y + r.h;
        self.keys.iter().find(|k| k.2 && inside(&k.1)).or_else(|| self.keys.iter().find(|k| !k.2 && inside(&k.1))).map(|k| k.0)
    }

    fn mouse_down(&mut self, part: usize, ev: &MouseDownEvent, cx: &mut Context<Self>) {
        let p = self.layout.parts[part].clone();
        let (x, y) = self.to_panel(ev.position);
        match p.kind.as_str() {
            "knob" | "selector" => {
                self.drag = Some(Drag::Value { part, start_y: y, start: self.value(&p), fine: ev.modifiers.shift });
            }
            "rocker" => {
                let v = self.value(&p);
                self.panel.set(&p.id, if v > 0.5 { 0.0 } else { 1.0 });
            }
            "wheel" => self.drag = Some(Drag::Wheel { part, start_y: y, start: self.value(&p) }),
            "keyboard" => {
                if let Some(n) = self.note_at(&p, x, y) {
                    // lower on the key is louder, as on the panel
                    self.panel.note_on(n, 100);
                    self.drag = Some(Drag::Key { note: n });
                }
            }
            _ => {}
        }
        self.refresh();
        cx.notify();
    }

    fn mouse_move(&mut self, ev: &MouseMoveEvent, cx: &mut Context<Self>) {
        let (x, y) = self.to_panel(ev.position);
        let kb = self.layout.parts.iter().find(|p| p.kind == "keyboard").cloned();
        match &mut self.drag {
            Some(Drag::Value { part, start_y, start, fine }) => {
                let p = self.layout.parts[*part].clone();
                let travel = if *fine { 720.0 } else { 180.0 };
                let mut v = *start + (*start_y - y) / travel * (p.max - p.min);
                if p.kind == "selector" {
                    v = (*start + ((*start_y - y) / 24.0).round()).clamp(p.min, p.max);
                }
                self.panel.set(&p.id, v.clamp(p.min, p.max));
            }
            Some(Drag::Wheel { part, start_y, start }) => {
                let p = self.layout.parts[*part].clone();
                let v = (*start + (*start_y - y) / 120.0 * (1.0 - p.min)).clamp(p.min, 1.0);
                self.panel.set_wheel(if p.id == "pitchWheel" { 0 } else { 1 }, v);
            }
            Some(Drag::Key { note }) => {
                // a glissando: dragging across the keys plays each in turn
                let held = *note;
                if let Some(n) = kb.as_ref().and_then(|kb| self.note_at(kb, x, y)) {
                    if n != held {
                        self.panel.note_off(held);
                        self.panel.note_on(n, 100);
                        self.drag = Some(Drag::Key { note: n });
                    }
                }
            }
            None => return,
        }
        self.refresh();
        cx.notify();
    }

    fn mouse_up(&mut self, cx: &mut Context<Self>) {
        match self.drag.take() {
            Some(Drag::Key { note }) => self.panel.note_off(note),
            Some(Drag::Wheel { part, .. }) => {
                // the pitch wheel springs back to the middle
                if self.layout.parts[part].id == "pitchWheel" {
                    self.panel.set_wheel(0, 0.0);
                }
            }
            _ => {}
        }
        self.refresh();
        cx.notify();
    }

    fn scroll(&mut self, part: usize, ev: &ScrollWheelEvent, cx: &mut Context<Self>) {
        let p = self.layout.parts[part].clone();
        if p.kind != "knob" && p.kind != "selector" {
            return;
        }
        let dy = f32::from(ev.delta.pixel_delta(px(16.0)).y);
        let v = self.value(&p);
        let nv = if p.kind == "selector" { v + dy.signum() } else { v + dy / 400.0 * (p.max - p.min) };
        self.panel.set(&p.id, nv.clamp(p.min, p.max));
        self.refresh();
        cx.notify();
    }

    fn key(&mut self, key: &str, chars: Option<&str>, down: bool, m: &gpui::Modifiers) -> bool {
        let name = match key {
            "left" | "right" | "escape" => key.to_string(),
            _ => chars.unwrap_or(key).to_string(),
        };
        self.panel.key_event(&name, down, m.platform, m.shift)
    }
}

fn fake_down(x: f32, y: f32, frame: (f32, f32, f32), shift: bool) -> MouseDownEvent {
    let (s, ox, oy) = frame;
    MouseDownEvent {
        button: MouseButton::Left,
        position: gpui::point(px(ox + x * s), px(oy + y * s)),
        modifiers: gpui::Modifiers { shift, ..Default::default() },
        click_count: 1,
        first_mouse: false,
    }
}

fn fake_move(x: f32, y: f32, frame: (f32, f32, f32)) -> MouseMoveEvent {
    let (s, ox, oy) = frame;
    MouseMoveEvent { position: gpui::point(px(ox + x * s), px(oy + y * s)), pressed_button: Some(MouseButton::Left), modifiers: Default::default() }
}

impl Fourpole {
    /// The self-test: every knob, selector and rocker worked through the same handlers a click goes through.
    fn self_test_controls(&mut self, lines: &mut Vec<String>, cx: &mut Context<Self>) {
        let (mut knobs, mut rockers, mut bad) = (0, 0, Vec::new());
        for i in 0..self.layout.parts.len() {
            let p = self.layout.parts[i].clone();
            let (cx0, cy0) = (p.x + p.w * 0.5, p.y + p.h * 0.5);
            match p.kind.as_str() {
                "knob" | "selector" => {
                    self.refresh();
                    let before = self.value(&p);
                    let dy = if before > (p.min + p.max) * 0.5 { 60.0 } else { -60.0 }; // drag towards the far end
                    self.mouse_down(i, &fake_down(cx0, cy0, self.frame, false), cx);
                    self.mouse_move(&fake_move(cx0, cy0 + dy, self.frame), cx);
                    self.mouse_up(cx);
                    self.refresh();
                    let after = self.value(&p);
                    if (after - before).abs() < 1.0e-3 {
                        bad.push(format!("{} {} didn't move ({before} -> {after})", p.kind, p.id));
                    }
                    self.panel.set(&p.id, before);
                    knobs += 1;
                }
                "rocker" => {
                    self.refresh();
                    let before = self.value(&p);
                    self.mouse_down(i, &fake_down(cx0, cy0, self.frame, false), cx);
                    self.mouse_up(cx);
                    self.refresh();
                    if (self.value(&p) - before).abs() < 0.5 {
                        bad.push(format!("rocker {} didn't flip", p.id));
                    }
                    self.mouse_down(i, &fake_down(cx0, cy0, self.frame, false), cx);
                    self.mouse_up(cx);
                    rockers += 1;
                }
                _ => {}
            }
        }
        lines.push(format!("ok      {knobs} knobs and selectors, {rockers} rockers worked through the window's handlers"));
        for b in bad {
            lines.push(format!("FAIL    {b}"));
        }
    }

    fn click_key(&mut self, note: i32, down: bool, cx: &mut Context<Self>) {
        let kb = self.layout.parts.iter().position(|p| p.kind == "keyboard").unwrap();
        let p = self.layout.parts[kb].clone();
        let k = self.keys.iter().find(|k| k.0 == note).unwrap().1.clone();
        if down {
            self.mouse_down(kb, &fake_down(p.x + k.x + k.w * 0.5, p.y + k.y + k.h * 0.8, self.frame, false), cx);
        } else {
            self.mouse_up(cx);
        }
    }
}

impl Render for Fourpole {
    fn render(&mut self, window: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let view = window.viewport_size();
        let (vw, vh) = (f32::from(view.width), f32::from(view.height));
        let s = (vw / self.layout.width).min(vh / self.layout.height);
        let (ox, oy) = ((vw - self.layout.width * s) * 0.5, (vh - self.layout.height * s) * 0.5);
        self.frame = (s, ox, oy);
        // the parts are drawn at the screen's own resolution
        let dev = (s * window.scale_factor() * 16.0).round() / 16.0;
        let place = move |x: f32, y: f32, w: f32, h: f32| div().absolute().left(px(ox + x * s)).top(px(oy + y * s)).w(px(w * s)).h(px(h * s));

        let mut root = div()
            .size_full()
            .bg(rgb(0x0b0b0b))
            .track_focus(&self.focus)
            .on_mouse_move(cx.listener(|this, ev: &MouseMoveEvent, _, cx| this.mouse_move(ev, cx)))
            .on_mouse_up(MouseButton::Left, cx.listener(|this, _: &MouseUpEvent, _, cx| this.mouse_up(cx)))
            .on_key_down(cx.listener(|this, ev: &KeyDownEvent, _, cx| {
                if this.key(&ev.keystroke.key, ev.keystroke.key_char.as_deref(), true, &ev.keystroke.modifiers) {
                    cx.stop_propagation();
                }
                this.refresh();
                cx.notify();
            }))
            .on_key_up(cx.listener(|this, ev: &KeyUpEvent, _, cx| {
                this.key(&ev.keystroke.key, ev.keystroke.key_char.as_deref(), false, &ev.keystroke.modifiers);
                this.refresh();
                cx.notify();
            }));

        let bg = self.background(dev);
        root = root.child(place(0.0, 0.0, self.layout.width, self.layout.height).child(img(bg).size_full().object_fit(ObjectFit::Fill)));

        // the parts, each at its value
        for i in 0..self.layout.parts.len() {
            let p = self.layout.parts[i].clone();
            let v = if p.kind == "keyboard" { -1.0 } else { self.value(&p) };
            let sprite = self.sprite(i, v, dev);
            let mut el = place(p.x, p.y, p.w, p.h).child(img(sprite).size_full().object_fit(ObjectFit::Fill));
            if p.kind == "keyboard" {
                // the keys held (by the mouse, the computer keyboard or MIDI) drawn pressed
                for &n in self.state.notes.clone().iter() {
                    if let Some(k) = self.keys.iter().find(|k| k.0 == n).cloned() {
                        let ks = self.sprite(i, n as f32, dev);
                        el = el.child(
                            div()
                                .absolute()
                                .left(px(k.1.x * s))
                                .top(px(k.1.y * s))
                                .w(px(k.1.w * s))
                                .h(px(k.1.h * s))
                                .child(img(ks).size_full().object_fit(ObjectFit::Fill)),
                        );
                    }
                }
            }
            if p.kind != "lamp" {
                el = el
                    .on_mouse_down(MouseButton::Left, cx.listener(move |this, ev: &MouseDownEvent, window, cx| {
                        this.focus.focus(window);
                        this.mouse_down(i, ev, cx);
                    }))
                    .on_scroll_wheel(cx.listener(move |this, ev: &ScrollWheelEvent, _, cx| this.scroll(i, ev, cx)));
            }
            root = root.child(el);
        }

        // the patch bar: the name plate and the buttons
        let np = self.layout.name_plate.clone();
        let text = if self.state.message.trim().is_empty() { self.state.title.clone() } else { self.state.message.clone() };
        root = root.child(
            place(np.x, np.y, np.w, np.h)
                .flex()
                .items_center()
                .px(px(10.0 * s))
                .bg(rgb(0x14110d))
                .border_1()
                .border_color(rgb(0x000000))
                .rounded(px(3.0 * s))
                .text_size(px(13.0 * s))
                .text_color(if self.state.message.trim().is_empty() { rgb(0xe9e2d0) } else { rgb(0xffb43a) })
                .child(text),
        );
        for b in self.layout.buttons.clone() {
            let lit = b.command == "rec" && self.state.recording;
            let cmd = b.command.clone();
            root = root.child(
                place(b.x, b.y, b.w, b.h)
                    .flex()
                    .items_center()
                    .justify_center()
                    .bg(if lit { rgb(0x8a1c14) } else { rgb(0x26272b) })
                    .hover(|st| st.bg(rgb(0x34363b)))
                    .border_1()
                    .border_color(rgba(0xffffff2e))
                    .rounded(px(4.0 * s))
                    .text_size(px(12.0 * s))
                    .text_color(rgb(0xd8d8d4))
                    .child(b.label.clone())
                    .on_mouse_down(MouseButton::Left, cx.listener(move |this, _: &MouseDownEvent, window, cx| {
                        this.focus.focus(window);
                        this.panel.command(&cmd);
                        this.refresh();
                        cx.notify();
                    })),
            );
        }
        root
    }
}

fn main() {
    Application::new().run(|cx: &mut App| {
        let bounds = Bounds::centered(None, size(px(1480.0 * 0.85), px(860.0 * 0.85)), cx);
        cx.open_window(
            WindowOptions { window_bounds: Some(WindowBounds::Windowed(bounds)), ..Default::default() },
            |window, cx| {
                let view = cx.new(Fourpole::new);
                view.read(cx).focus.focus(window);
                view
            },
        )
        .unwrap();
        cx.activate(true);
    });
}
