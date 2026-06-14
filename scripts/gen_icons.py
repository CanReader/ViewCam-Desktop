#!/usr/bin/env python3
"""Generate the ViewCam icon set as standalone SVGs from the design mockup.

Each icon is the exact inline markup from 'ViewCam Desktop.html', wrapped in a
24x24 viewBox with white strokes so QML can tint via MultiEffect colorization.
"""
import os

OUT = os.path.join(os.path.dirname(__file__), "..", "resources", "icons")

# name -> (stroke_width, inner_markup)
ICONS = {
    "camera": (1.8, '<rect x="2" y="6" width="13" height="12" rx="2"/><path d="m15 10 6-3v10l-6-3"/>'),
    "camera-thin": (1.2, '<rect x="2" y="6" width="13" height="12" rx="2.5"/><path d="m15 10 6-3v10l-6-3"/>'),
    "mic-stand": (1.8, '<path d="M12 2a3 3 0 0 0-3 3v6a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z"/><path d="M19 10v1a7 7 0 0 1-14 0v-1M12 18v4M8 22h8"/>'),
    "gear": (1.8, '<path d="M12 15a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1Z"/>'),
    "battery": (1.8, '<rect x="2" y="7" width="18" height="10" rx="2.5"/><path d="M22 11v2"/><rect x="4" y="9.5" width="11" height="5" rx="1" fill="#FFFFFF" stroke="none"/>'),
    "battery-full": (1.7, '<rect x="2" y="7" width="18" height="10" rx="2.5"/><path d="M22 11v2"/><rect x="4" y="9.2" width="12" height="5.6" rx="1" fill="#FFFFFF" stroke="none"/>'),
    "wifi": (1.8, '<path d="M5 13a10 10 0 0 1 14 0M8.5 16.5a5 5 0 0 1 7 0"/><circle cx="12" cy="20" r="0.6" fill="#FFFFFF"/>'),
    "arrow-right": (2.2, '<path d="M5 12h14M13 6l6 6-6 6"/>'),
    "search": (2.0, '<circle cx="11" cy="11" r="7"/><path d="m21 21-4.3-4.3"/>'),
    "mic": (1.8, '<rect x="9" y="2" width="6" height="12" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/>'),
    "volume": (1.8, '<path d="M11 5 6 9H2v6h4l5 4V5Z"/>'),
    "volume-waves": (1.8, '<path d="M11 5 6 9H2v6h4l5 4V5Z"/><path d="M15.5 8.5a5 5 0 0 1 0 7M19 5a9 9 0 0 1 0 14"/>'),
    "wave": (1.8, '<path d="M2 12h4l3-8 4 16 3-8h4"/>'),
    "wave-mid": (1.8, '<path d="M3 12h4l3-8 4 16 3-8h4"/>'),
    "app-window": (1.8, '<rect x="3" y="4" width="18" height="14" rx="2"/><path d="M8 21h8M12 18v3"/>'),
    "loopback": (1.8, '<rect x="2" y="6" width="14" height="12" rx="2"/><path d="M16 10h6M19 7v6"/>'),
    "meters": (1.8, '<path d="M12 1v22M8 5v14M16 5v14M4 9v6M20 9v6"/>'),
    "buffer": (1.8, '<rect x="3" y="8" width="18" height="8" rx="2"/><path d="M7 8v8M11 8v8M15 8v8"/>'),
    "plus": (1.8, '<path d="M5 12h14M12 5v14"/>'),
    "mono-circle": (1.8, '<circle cx="12" cy="12" r="9"/><path d="M8 12h8"/>'),
    "gate": (1.8, '<path d="M2 12h4l3 8 6-16 3 8h4"/>'),
    "gpu": (1.8, '<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>'),
    "protocol": (1.8, '<path d="M4 6h16M4 12h16M4 18h10"/>'),
    "resolution": (1.8, '<rect x="2" y="5" width="20" height="14" rx="2"/><path d="M8 12h8"/>'),
    "clock": (1.8, '<circle cx="12" cy="12" r="9"/><path d="M12 8v4l3 2"/>'),
    "arch": (1.8, '<path d="M4 18V8a8 8 0 0 1 16 0v10M4 18h16"/>'),
    "port": (1.8, '<path d="M12 2v4M12 18v4M2 12h4M18 12h4M5 5l3 3M16 16l3 3M19 5l-3 3M8 16l-3 3"/>'),
    "reconnect": (1.8, '<path d="M3 12a9 9 0 1 0 9-9M3 12l3-3M3 12l3 3"/>'),
    "shield": (1.8, '<path d="M12 2 4 6v6c0 5 8 8 8 8s8-3 8-8V6Z"/>'),
    "sun": (1.8, '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2"/>'),
    "palette": (1.8, '<circle cx="13.5" cy="6.5" r="2.5"/><circle cx="17.5" cy="10.5" r="2.5"/><circle cx="8.5" cy="7.5" r="2.5"/><circle cx="6.5" cy="12.5" r="2.5"/><path d="M12 22a10 10 0 1 1 0-20c4 0 6 2.5 6 5s-2 3-4 3h-2c-2 0-2 2-1 3s1 4-2 4Z"/>'),
    "telemetry": (1.8, '<path d="M3 3v18h18"/><path d="M7 14l3-3 3 3 4-5"/>'),
    "compact": (1.8, '<path d="M4 6h16M4 12h10M4 18h16"/>'),
    "laptop": (1.8, '<rect x="3" y="4" width="18" height="14" rx="2"/><path d="M8 21h8"/>'),
    "info": (1.8, '<circle cx="12" cy="12" r="9"/><path d="M12 16v-5M12 8h.01"/>'),
    "code": (1.8, '<path d="m16 18 6-6-6-6M8 6l-6 6 6 6"/>'),
    "renderer": (1.8, '<path d="M4 17l6-6-6-6M12 19h8"/>'),
    "licenses": (1.8, '<path d="M8 3H6a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V5a2 2 0 0 0-2-2h-2M8 7h8M8 11h8M8 15h5"/>'),
    "chevron-down": (1.8, '<path d="M6 9l6 6 6-6"/>'),
    "address": (1.8, '<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M3 9h18"/>'),
    "signal-graph": (1.8, '<path d="M3 12h4l3 8 4-16 3 8h4"/>'),
    "latency": (1.8, '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>'),
    "bitrate": (1.8, '<path d="M12 3v18M5 10l7-7 7 7"/>'),
    "packet": (1.8, '<path d="M3 12h18M3 6h18M3 18h18"/>'),
    "phone": (1.8, '<rect x="6" y="3" width="12" height="18" rx="3"/><path d="M11 18h2"/>'),
    "system": (1.8, '<circle cx="12" cy="12" r="9"/><path d="M9 12l2 2 4-4"/>'),
    "capture": (1.8, '<path d="M4 6h16v12H4zM9 6V4M15 6V4"/>'),
    "flip": (1.8, '<path d="M3 7v4M3 11h4M3 11a9 9 0 0 1 15-3M21 17v-4M21 13h-4M21 13a9 9 0 0 1-15 3"/>'),
    "focus": (1.8, '<circle cx="12" cy="12" r="3"/><path d="M12 3v3M12 18v3M3 12h3M18 12h3"/>'),
    "power": (1.9, '<path d="M18.36 6.64A9 9 0 1 1 5.64 6.64M12 2v8"/>'),
}

TPL = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" '
       'stroke="#FFFFFF" stroke-width="{sw}" stroke-linecap="round" stroke-linejoin="round">{inner}</svg>\n')

os.makedirs(OUT, exist_ok=True)
for name, (sw, inner) in ICONS.items():
    with open(os.path.join(OUT, f"{name}.svg"), "w") as f:
        f.write(TPL.format(sw=sw, inner=inner))
print(f"wrote {len(ICONS)} icons to {os.path.abspath(OUT)}")
