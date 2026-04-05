"""
╔═══════════════════════════════════════╗
║           HEX VIEWER v1.0             ║
╚═══════════════════════════════════════╝

A lightweight binary file viewer
Displays file contents in hex and ASCII format

Author  : Seungjun Kim
GitHub  : github.com/pyshit3,mokalover
License : MIT License (c) 2026

Written during one of the harder chapters of my life.
If you're reading this, I made it through.
Better days are coming — I'm sure of it.
"""

import ctypes
import sys
import struct
import os
import customtkinter as ctk
from tkinter import filedialog, messagebox, Text, Scrollbar, ttk, PanedWindow
import tkinter as tk

# ── Load C library ──────────────────────────────────────
bits = struct.calcsize("P") * 8

if sys.platform == "win32":
    lib = ctypes.CDLL("./hexcore_64.dll" if bits == 64 else "./hexcore_32.dll")
else:
    lib = ctypes.CDLL("./hexcore.so")

lib.initEnvironment.restype = None

lib.loadFile.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_long)]
lib.loadFile.restype  = ctypes.POINTER(ctypes.c_ubyte)

lib.getHexViewChunk.argtypes    = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_long,
                                    ctypes.c_long, ctypes.c_long, ctypes.c_char_p]
lib.getHexViewChunk.restype     = None

lib.getAsciiViewChunk.argtypes  = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_long,
                                    ctypes.c_long, ctypes.c_long, ctypes.c_char_p]
lib.getAsciiViewChunk.restype   = None

lib.getOffsetViewChunk.argtypes = [ctypes.c_long, ctypes.c_long,
                                    ctypes.c_long, ctypes.c_char_p]
lib.getOffsetViewChunk.restype  = None

lib.parseDosHeader.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_long, ctypes.c_char_p]
lib.parseDosHeader.restype  = None

lib.parseDosStub.argtypes = [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_long, ctypes.c_char_p]
lib.parseDosStub.restype  = None

lib.freeBuffer.argtypes = [ctypes.POINTER(ctypes.c_ubyte)]
lib.freeBuffer.restype  = None

lib.initEnvironment()

# ── VSCode color scheme ───────────────────────────────────────
BG_DARK      = "#1e1e1e"
BG_SIDEBAR   = "#252526"
BG_TAB       = "#2d2d2d"
BG_TAB_ACT   = "#1e1e1e"
FG_PRIMARY   = "#d4d4d4"
FG_SECONDARY = "#858585"
TAB_ACT_LINE = "#007acc"
TREE_SELECT  = "#094771"

# ── Lazy loading config ───────────────────────────────────────
CHUNK_SIZE = 4096
LOAD_AHEAD = 2

# ── Global State ──────────────────────────────────────────────
tabs        = {}
current_tab = None

# ── C function wrappers ───────────────────────────────────────
def c_load_file(filepath: str):
    size = ctypes.c_long(0)
    buf  = lib.loadFile(filepath.encode(), ctypes.byref(size))
    if not buf:
        return None, 0
    return buf, size.value

def c_free(buf):
    lib.freeBuffer(buf)

def c_get_hex_chunk(buf, size, offset, chunk_size) -> str:
    out_buf = ctypes.create_string_buffer(chunk_size * 4)
    lib.getHexViewChunk(buf, size, offset, chunk_size, out_buf)
    return out_buf.value.decode()

def c_get_ascii_chunk(buf, size, offset, chunk_size) -> str:
    out_buf = ctypes.create_string_buffer(chunk_size * 2)
    lib.getAsciiViewChunk(buf, size, offset, chunk_size, out_buf)
    return out_buf.value.decode()

def c_get_offset_chunk(size, offset, chunk_size) -> str:
    out_buf = ctypes.create_string_buffer((chunk_size // 16 + 1) * 9)
    lib.getOffsetViewChunk(size, offset, chunk_size, out_buf)
    return out_buf.value.decode()

def c_get_dos_header(buf, size) -> str:
    out_buf = ctypes.create_string_buffer(1024)
    lib.parseDosHeader(buf, size, out_buf)
    return out_buf.value.decode()

def c_get_dos_stub(buf, size) -> str:
    # stub_size * 5 + 256 bytes (as per parseDosStub comment)
    out_size = size * 5 + 256
    out_buf  = ctypes.create_string_buffer(out_size)
    lib.parseDosStub(buf, size, out_buf)
    return out_buf.value.decode()

# ── Lazy loading ──────────────────────────────────────────────
def load_chunk(filepath: str, offset: int):
    tab  = tabs[filepath]
    buf  = tab["buf"]
    size = tab["size"]

    if offset >= size:
        return

    for t in [offset_text, hex_text, ascii_text]:
        t.configure(state="normal")

    hex_text.insert("end",    c_get_hex_chunk(buf, size, offset, CHUNK_SIZE))
    ascii_text.insert("end",  c_get_ascii_chunk(buf, size, offset, CHUNK_SIZE))
    offset_text.insert("end", c_get_offset_chunk(size, offset, CHUNK_SIZE))

    for t in [offset_text, hex_text, ascii_text]:
        t.configure(state="disabled")

    tabs[filepath]["offset"] = offset + CHUNK_SIZE

def on_scroll_end(event=None):
    if not current_tab or current_tab not in tabs:
        return
    pos = hex_text.yview()
    if pos[1] >= 0.90:
        offset = tabs[current_tab]["offset"]
        for _ in range(LOAD_AHEAD):
            load_chunk(current_tab, offset)
            offset = tabs[current_tab]["offset"]

# ── Tab management ────────────────────────────────────────────
def open_file_in_tab(filepath: str):
    global current_tab

    if filepath in tabs:
        switch_tab(filepath)
        return

    buf, size = c_load_file(filepath)
    if not buf:
        messagebox.showerror("Error", f"Failed to open:\n{filepath}")
        return

    tabs[filepath] = {"buf": buf, "size": size, "offset": 0}
    create_tab_button(filepath)
    switch_tab(filepath)

def switch_tab(filepath: str):
    global current_tab
    current_tab = filepath

    buf  = tabs[filepath]["buf"]
    size = tabs[filepath]["size"]

    # Update tab bar highlight
    for path, tab in tabs.items():
        if "btn" in tab:
            is_active = (path == filepath)
            tab["btn"].configure(
                fg_color=BG_TAB_ACT if is_active else BG_TAB,
                text_color=FG_PRIMARY if is_active else FG_SECONDARY)
            tab["indicator"].configure(
                fg_color=TAB_ACT_LINE if is_active else BG_TAB)

    # Clear all text widgets
    for t in [offset_text, hex_text, ascii_text]:
        t.configure(state="normal")
        t.delete("1.0", "end")
        t.configure(state="disabled")

    # Reset offset and load first chunk
    tabs[filepath]["offset"] = 0
    load_chunk(filepath, 0)

    # Update PE panel tabs
    dos_header_text.configure(state="normal")
    dos_header_text.delete("1.0", "end")
    dos_header_text.insert("1.0", c_get_dos_header(buf, size))
    dos_header_text.configure(state="disabled")

    dos_stub_text.configure(state="normal")
    dos_stub_text.delete("1.0", "end")
    dos_stub_text.insert("1.0", c_get_dos_stub(buf, size))
    dos_stub_text.configure(state="disabled")

    # Update title and status bar
    app.title(f"Hex Viewer — {os.path.basename(filepath)}")
    status_left.configure(text=filepath)
    status_right.configure(text=f"{size:,} bytes")

def close_tab(filepath: str):
    global current_tab

    if filepath not in tabs:
        return

    c_free(tabs[filepath]["buf"])

    if "frame" in tabs[filepath]:
        tabs[filepath]["frame"].destroy()

    del tabs[filepath]

    if tabs:
        switch_tab(list(tabs.keys())[-1])
    else:
        current_tab = None
        app.title("Hex Viewer")
        status_left.configure(text="")
        status_right.configure(text="")
        for t in [offset_text, hex_text, ascii_text,
                  dos_header_text, dos_stub_text]:
            t.configure(state="normal")
            t.delete("1.0", "end")
            t.configure(state="disabled")

def create_tab_button(filepath: str):
    name = os.path.basename(filepath)

    frame = ctk.CTkFrame(tab_bar, fg_color=BG_TAB, corner_radius=0, width=160)
    frame.pack(side="left", fill="y", padx=(0, 1))
    frame.pack_propagate(False)

    indicator = ctk.CTkFrame(frame, fg_color=BG_TAB, height=2, corner_radius=0)
    indicator.pack(fill="x")

    row = ctk.CTkFrame(frame, fg_color="transparent", corner_radius=0)
    row.pack(fill="both", expand=True)

    btn = ctk.CTkButton(row, text=name, fg_color=BG_TAB, text_color=FG_SECONDARY,
                        hover_color="#2a2d2e", corner_radius=0, anchor="w",
                        border_width=0, font=("Segoe UI", 12),
                        command=lambda p=filepath: switch_tab(p))
    btn.pack(side="left", fill="both", expand=True, padx=(8, 0))

    close_btn = ctk.CTkButton(row, text="×", fg_color="transparent",
                               text_color=FG_SECONDARY, hover_color="#3c3c3c",
                               width=24, height=24, corner_radius=4,
                               font=("Segoe UI", 13),
                               command=lambda p=filepath: close_tab(p))
    close_btn.pack(side="right", padx=4)

    tabs[filepath]["frame"]     = frame
    tabs[filepath]["btn"]       = btn
    tabs[filepath]["indicator"] = indicator

# ── Folder explorer ───────────────────────────────────────────
def open_folder():
    folder = filedialog.askdirectory(title="Open Folder")
    if not folder:
        return
    folder_label.configure(text=os.path.basename(folder).upper())
    populate_tree(folder)

def populate_tree(folder: str):
    tree.delete(*tree.get_children())
    try:
        for entry in sorted(os.listdir(folder)):
            full = os.path.join(folder, entry)
            icon = "📁" if os.path.isdir(full) else get_icon(entry)
            tree.insert("", "end", iid=full, text=f"  {icon}  {entry}")
    except PermissionError:
        pass

def get_icon(filename: str) -> str:
    ext = os.path.splitext(filename)[1].lower()
    return {
        ".exe": "⚙️", ".dll": "🔧", ".sys": "🛡️",
        ".txt": "📄", ".py":  "🐍", ".c":   "📝",
        ".h":   "📝", ".bin": "📦", ".dat": "📦",
    }.get(ext, "📄")

def on_tree_double_click(event):
    item = tree.focus()
    if not item:
        return
    if os.path.isfile(item):
        open_file_in_tab(item)
    elif os.path.isdir(item):
        populate_tree(item)

# ── Scroll synchronization ────────────────────────────────────
def sync_scroll(*args):
    offset_text.yview_moveto(args[0])
    hex_text.yview_moveto(args[0])
    ascii_text.yview_moveto(args[0])
    scrollbar.set(*args)
    on_scroll_end()

def on_mousewheel(event):
    offset_text.yview_scroll(int(-1 * (event.delta / 120)), "units")
    hex_text.yview_scroll(int(-1 * (event.delta / 120)), "units")
    ascii_text.yview_scroll(int(-1 * (event.delta / 120)), "units")
    on_scroll_end()
    return "break"

def on_scrollbar(command, *args):
    offset_text.yview(command, *args)
    hex_text.yview(command, *args)
    ascii_text.yview(command, *args)

# ── PE panel tab switching ────────────────────────────────────
def show_dos_header():
    dos_header_text.pack(fill="both", expand=True)
    dos_stub_text.pack_forget()
    dos_header_btn.configure(fg_color=BG_TAB_ACT, text_color=FG_PRIMARY)
    dos_stub_btn.configure(fg_color="#2d2d2d", text_color=FG_SECONDARY)

def show_dos_stub():
    dos_stub_text.pack(fill="both", expand=True)
    dos_header_text.pack_forget()
    dos_stub_btn.configure(fg_color=BG_TAB_ACT, text_color=FG_PRIMARY)
    dos_header_btn.configure(fg_color="#2d2d2d", text_color=FG_SECONDARY)

# ── App setup ─────────────────────────────────────────────────
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.title("Hex Viewer")
app.geometry("1400x800")
app.configure(fg_color=BG_DARK)

# ── Top menu bar ──────────────────────────────────────────────
menubar = ctk.CTkFrame(app, fg_color="#3c3c3c", height=30, corner_radius=0)
menubar.pack(fill="x")
menubar.pack_propagate(False)

ctk.CTkLabel(menubar, text="Hex Viewer",
             text_color=FG_PRIMARY, font=("Segoe UI", 12)).pack(side="left", padx=12)

ctk.CTkButton(menubar, text="📂 Open Folder", fg_color="transparent",
              text_color=FG_SECONDARY, hover_color="#4c4c4c",
              height=28, corner_radius=4, font=("Segoe UI", 12),
              command=open_folder).pack(side="left", padx=4, pady=1)

# ── Status bar ────────────────────────────────────────────────
statusbar = ctk.CTkFrame(app, fg_color="#007acc", height=22, corner_radius=0)
statusbar.pack(fill="x", side="bottom")
statusbar.pack_propagate(False)

status_left = ctk.CTkLabel(statusbar, text="", text_color="white",
                            font=("Segoe UI", 11), anchor="w")
status_left.pack(side="left", padx=8)

status_right = ctk.CTkLabel(statusbar, text="", text_color="white",
                             font=("Segoe UI", 11), anchor="e")
status_right.pack(side="right", padx=8)

# ── Main PanedWindow (horizontal: sidebar | right) ────────────
main_pane = PanedWindow(app, orient=tk.HORIZONTAL, bg="#3c3c3c",
                        sashwidth=4, sashrelief="flat", bd=0)
main_pane.pack(fill="both", expand=True)

# ── Sidebar ───────────────────────────────────────────────────
sidebar = tk.Frame(main_pane, bg=BG_SIDEBAR, width=220)
main_pane.add(sidebar, minsize=100)

folder_label = ctk.CTkLabel(sidebar, text="NO FOLDER OPENED",
                              text_color=FG_SECONDARY,
                              font=("Segoe UI", 11, "bold"),
                              fg_color=BG_SIDEBAR, anchor="w")
folder_label.pack(fill="x", padx=12, pady=(12, 4))

style = ttk.Style()
style.theme_use("default")
style.configure("Custom.Treeview",
                background=BG_SIDEBAR, foreground=FG_PRIMARY,
                fieldbackground=BG_SIDEBAR, borderwidth=0,
                font=("Segoe UI", 12), rowheight=24)
style.map("Custom.Treeview", background=[("selected", TREE_SELECT)])

tree = ttk.Treeview(sidebar, style="Custom.Treeview", show="tree", selectmode="browse")
tree.pack(fill="both", expand=True)
tree.bind("<Double-1>", on_tree_double_click)

# ── Right area ────────────────────────────────────────────────
right = tk.Frame(main_pane, bg=BG_DARK)
main_pane.add(right, minsize=400)

# ── Tab bar ───────────────────────────────────────────────────
tab_bar = ctk.CTkFrame(right, fg_color=BG_TAB, height=35, corner_radius=0)
tab_bar.pack(fill="x")
tab_bar.pack_propagate(False)

# ── Column headers ────────────────────────────────────────────
header = tk.Frame(right, bg="#252526", height=24)
header.pack(fill="x")
header.pack_propagate(False)

tk.Label(header, text="OFFSET", fg=FG_SECONDARY, bg="#252526",
         font=("Courier", 11), width=10).pack(side="left", padx=(8, 0))
tk.Label(header, text="HEX", fg=FG_SECONDARY, bg="#252526",
         font=("Courier", 11)).pack(side="left", padx=16)
tk.Label(header, text="ASCII", fg=FG_SECONDARY, bg="#252526",
         font=("Courier", 11)).pack(side="right", padx=16)

# ── Vertical PanedWindow (hex area | pe panel) ────────────────
vert_pane = PanedWindow(right, orient=tk.VERTICAL, bg="#3c3c3c",
                        sashwidth=4, sashrelief="flat", bd=0)
vert_pane.pack(fill="both", expand=True)

# ── Hex view area ─────────────────────────────────────────────
hex_area = tk.Frame(vert_pane, bg=BG_DARK)
vert_pane.add(hex_area, minsize=100)

scrollbar = Scrollbar(hex_area, command=on_scrollbar,
                      bg="#3c3c3c", troughcolor=BG_DARK,
                      activebackground="#555555", width=12)
scrollbar.pack(side="right", fill="y")

TEXT_KWARGS = dict(bg=BG_DARK, fg=FG_PRIMARY, font=("Courier New", 12),
                   relief="flat", bd=0, wrap="none", state="disabled",
                   selectbackground=TREE_SELECT, insertbackground=FG_PRIMARY,
                   yscrollcommand=sync_scroll)

offset_text = Text(hex_area, width=10, **TEXT_KWARGS)
offset_text.pack(side="left", fill="y", padx=(8, 0))

hex_text = Text(hex_area, **TEXT_KWARGS)
hex_text.pack(side="left", fill="both", expand=True, padx=8)

ascii_text = Text(hex_area, width=18, **TEXT_KWARGS)
ascii_text.pack(side="right", fill="y", padx=(0, 0))

for t in [offset_text, hex_text, ascii_text]:
    t.bind("<MouseWheel>", on_mousewheel)

# ── PE panel ──────────────────────────────────────────────────
pe_panel = tk.Frame(vert_pane, bg="#252526")
vert_pane.add(pe_panel, minsize=80)

# PE panel header with tab buttons
pe_header = tk.Frame(pe_panel, bg="#2d2d2d", height=28)
pe_header.pack(fill="x")
pe_header.pack_propagate(False)

tk.Label(pe_header, text="PE STRUCTURE", fg=FG_SECONDARY, bg="#2d2d2d",
         font=("Segoe UI", 11, "bold")).pack(side="left", padx=12, pady=4)

dos_header_btn = ctk.CTkButton(pe_header, text="DOS Header",
                                fg_color=BG_TAB_ACT, text_color=FG_PRIMARY,
                                hover_color="#3c3c3c", height=22, corner_radius=4,
                                font=("Segoe UI", 11), command=show_dos_header)
dos_header_btn.pack(side="left", padx=4, pady=3)

dos_stub_btn = ctk.CTkButton(pe_header, text="DOS Stub",
                              fg_color="#2d2d2d", text_color=FG_SECONDARY,
                              hover_color="#3c3c3c", height=22, corner_radius=4,
                              font=("Segoe UI", 11), command=show_dos_stub)
dos_stub_btn.pack(side="left", padx=4, pady=3)

# PE panel content
PE_TEXT_KWARGS = dict(bg="#252526", fg=FG_PRIMARY, font=("Courier New", 12),
                      relief="flat", bd=0, wrap="none", state="disabled",
                      selectbackground=TREE_SELECT, padx=12, pady=4)

dos_header_text = Text(pe_panel, **PE_TEXT_KWARGS)
dos_header_text.pack(fill="both", expand=True)

dos_stub_text = Text(pe_panel, **PE_TEXT_KWARGS)

# ── Free memory on window close ───────────────────────────────
def on_close():
    for tab in tabs.values():
        c_free(tab["buf"])
    app.destroy()

app.protocol("WM_DELETE_WINDOW", on_close)
app.mainloop()