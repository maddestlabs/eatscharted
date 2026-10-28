import sys
import os

WIDTH = 64
HEIGHT = 64

# Grid initialized to transparent
# 0xAARRGGBB
pixels = [[0 for _ in range(WIDTH)] for _ in range(HEIGHT)]

def set_px(x, y, a, r, g, b):
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        if a <= 0:
            return
        # Alpha blend with existing pixel if necessary
        curr = pixels[y][x]
        curr_a = (curr >> 24) & 0xFF
        if curr_a == 0 or a >= 255:
            pixels[y][x] = ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)
        else:
            # Simple alpha compositing
            src_a = a / 255.0
            dst_a = curr_a / 255.0
            out_a = src_a + dst_a * (1.0 - src_a)
            curr_r = (curr >> 16) & 0xFF
            curr_g = (curr >> 8) & 0xFF
            curr_b = curr & 0xFF
            out_r = int((r * src_a + curr_r * dst_a * (1.0 - src_a)) / out_a)
            out_g = int((g * src_a + curr_g * dst_a * (1.0 - src_a)) / out_a)
            out_b = int((b * src_a + curr_b * dst_a * (1.0 - src_a)) / out_a)
            pixels[y][x] = ((int(out_a * 255) & 0xFF) << 24) | ((out_r & 0xFF) << 16) | ((out_g & 0xFF) << 8) | (out_b & 0xFF)

def fill_rect(x0, y0, x1, y1, a, r, g, b):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            set_px(x, y, a, r, g, b)

def draw_line(x0, y0, x1, y1, a, r, g, b):
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    x, y = x0, y0
    while True:
        set_px(x, y, a, r, g, b)
        if x == x1 and y == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x += sx
        if e2 < dx:
            err += dx
            y += sy

# --- Palette Definitions ---
# Timber / Wood
WOOD_DARKEST = (42, 24, 14)      # Submerged wet timber & deep shadows
WOOD_DARK    = (68, 40, 24)      # Planking shadows
WOOD_MID     = (112, 70, 42)     # Carvel oak hull planks
WOOD_WARM    = (148, 96, 58)     # Sunlight timber
WOOD_LIGHT   = (182, 126, 80)    # Gunwale rails & deck highlights
GOLD_TRIM    = (224, 175, 55)    # Gilded filigree & brass rivets
GOLD_WARM    = (210, 155, 45)    # Shaded brass/gold
GOLD_BRIGHT  = (255, 220, 90)    # Stern lantern & figurehead glint

# Canvas / Sails
SAIL_SHADOW  = (185, 165, 135)   # Under-belly sail shade
SAIL_MID     = (228, 214, 185)   # Parchment canvas body
SAIL_LIT     = (250, 244, 226)   # Wind-blown canvas highlight
CREST_RED    = (196, 48, 44)     # Heraldic caravel cross / paw
CREST_DARK   = (142, 32, 30)     # Cross shadow

# Rigging / Iron
INK_DARK     = (34, 25, 19)      # Masts, ropes, shrouds
IRON_BOLT    = (60, 55, 50)

# Waterline / Foam / Wake
FOAM_WHITE   = (248, 252, 255)   # Sea spray froth
FOAM_CYAN    = (165, 220, 240)   # Shallow water wake edge
WATER_DEEP   = (45, 95, 125)     # Submerged water line tint

# --- 1. Rigging Ropes (Back Layer) ---
# Shrouds and stays from crow's nest (32, 17) down to hull
for t in range(28):
    f = t / 27.0
    # Left shroud
    set_px(int(32 * (1 - f) + 16 * f), int(17 * (1 - f) + 44 * f), 210, *INK_DARK)
    set_px(int(32 * (1 - f) + 19 * f), int(17 * (1 - f) + 44 * f), 180, *INK_DARK)
    # Right shroud
    set_px(int(33 * (1 - f) + 46 * f), int(17 * (1 - f) + 44 * f), 210, *INK_DARK)
    set_px(int(33 * (1 - f) + 43 * f), int(17 * (1 - f) + 44 * f), 180, *INK_DARK)
    # Ratlines (horizontal cross-ropes) on shrouds
    if t % 5 == 0 and t > 8:
        y_r = int(17 * (1 - f) + 44 * f)
        xl = int(32 * (1 - f) + 16 * f)
        xr = int(32 * (1 - f) + 19 * f)
        for rx in range(xl, xr + 1):
            set_px(rx, y_r, 160, *INK_DARK)
        xl2 = int(33 * (1 - f) + 43 * f)
        xr2 = int(33 * (1 - f) + 46 * f)
        for rx in range(xl2, xr2 + 1):
            set_px(rx, y_r, 160, *INK_DARK)

# Forestay to bowsprit tip
for t in range(30):
    f = t / 29.0
    set_px(int(32 * (1 - f) + 58 * f), int(16 * (1 - f) + 44 * f), 190, *INK_DARK)

# Backstay to stern
for t in range(30):
    f = t / 29.0
    set_px(int(32 * (1 - f) + 11 * f), int(16 * (1 - f) + 43 * f), 180, *INK_DARK)

# --- 2. Mainmast & Foremast Structure ---
# Mainmast (center, running x=31..33, y=6..48)
for y in range(6, 48):
    set_px(31, y, 255, *WOOD_MID)
    set_px(32, y, 255, *WOOD_LIGHT)
    set_px(33, y, 255, *WOOD_DARK)

# Foremast (x=46..47, y=20..46)
for y in range(20, 46):
    set_px(46, y, 255, *WOOD_WARM)
    set_px(47, y, 255, *WOOD_DARK)

# Mizzenmast (x=18, y=26..42)
for y in range(26, 42):
    set_px(18, y, 255, *WOOD_MID)

# Crow's Nest at (30..34, 16..18)
for x in range(30, 35):
    set_px(x, 18, 255, *WOOD_DARKEST)
    set_px(x, 17, 255, *WOOD_WARM)
    set_px(x, 16, 255, *WOOD_LIGHT if (x % 2 == 0) else WOOD_DARK)
set_px(29, 16, 255, *WOOD_MID)
set_px(35, 16, 255, *WOOD_MID)

# Yardarms (horizontal spars for sails)
# Main upper yardarm (y=20, x=18..46)
for x in range(18, 47):
    set_px(x, 20, 255, *WOOD_DARK)
# Main lower yardarm (y=37, x=21..44)
for x in range(21, 45):
    set_px(x, 37, 255, *WOOD_DARK)

# --- 3. Billowing Sails ---
# Mainsail (billowing curved rectangle from y=21 to 36, x=19 to 45)
# In antique charts, sails belly out to the right (wind blowing from aft to fore)
for y in range(21, 37):
    # Width of sail section with wind belly bulge
    t = (y - 21) / 15.0 # 0.0 at top to 1.0 at bottom
    # Belly expands in the middle (t ~ 0.5)
    belly = int(2.5 * (1.0 - 4.0 * (t - 0.5) ** 2))
    x_left = 20 - (1 if t > 0.3 and t < 0.8 else 0)
    x_right = 44 + belly
    for x in range(x_left, x_right + 1):
        rel_x = (x - x_left) / float(max(1, x_right - x_left))
        # Crease shadows and billowing highlight
        shade = SAIL_MID
        if rel_x < 0.12 or rel_x > 0.90 or t > 0.85:
            shade = SAIL_SHADOW
        elif 0.35 <= rel_x <= 0.75 and t < 0.65:
            shade = SAIL_LIT
        set_px(x, y, 255, *shade)

# Foremast Jib / Lateen Sail (Triangular sail at bow, y=24..41, x=45..55)
for y in range(24, 42):
    t_jib = (y - 24) / 17.0
    x_start = 46
    x_end = int(46 + t_jib * 8)
    for x in range(x_start, x_end + 1):
        rel = (x - x_start) / float(max(1, x_end - x_start))
        col = SAIL_LIT if rel > 0.4 and t_jib < 0.7 else (SAIL_SHADOW if rel < 0.2 else SAIL_MID)
        set_px(x, y, 255, *col)

# Mizzen Lateen Sail (Aft triangle, y=27..38, x=12..18)
for y in range(27, 39):
    t_miz = (y - 27) / 11.0
    x_start = int(18 - t_miz * 6)
    x_end = 18
    for x in range(x_start, x_end + 1):
        col = SAIL_SHADOW if (x == x_start or y == 38) else SAIL_MID
        set_px(x, y, 255, *col)

# --- 4. Mainmast Pennant / Swallowtail Flag ---
# Arched wind streamer from (33, 6) fluttering rightward to (46, 10)
pennant_pts = [
    (33, 6), (34, 6), (35, 6), (36, 7), (37, 7), (38, 8), (39, 8), (40, 8), (41, 7), (42, 7), (43, 8), (44, 9), (45, 9), (46, 10),
    (33, 7), (34, 7), (35, 7), (36, 8), (37, 8), (38, 9), (39, 9), (40, 9), (41, 8), (42, 8), (43, 9), (44, 10)
]
for px, py in pennant_pts:
    set_px(px, py, 255, 220, 50, 45) # Crimson flag
# Swallowtail notch at end
set_px(45, 10, 0, 0, 0, 0)
set_px(33, 5, 255, *GOLD_TRIM) # Brass masthead finial ball

# --- 5. Bowsprit & Carved Figurehead ---
# Bowsprit timber angling up to the right from hull (46, 46) to (59, 41)
draw_line(46, 46, 59, 41, 255, *WOOD_MID)
draw_line(46, 47, 59, 42, 255, *WOOD_DARK)
# Carved Golden Cat Figurehead at prow (49..53, 44..48)
figurehead_px = [
    (50, 44, GOLD_TRIM), (51, 43, GOLD_BRIGHT), (52, 44, GOLD_TRIM), # Cat ears
    (50, 45, GOLD_BRIGHT), (51, 45, GOLD_BRIGHT), (52, 45, GOLD_WARM if True else GOLD_TRIM), # Face
    (51, 46, (200, 140, 30)), # Nose/chin
    (49, 46, GOLD_TRIM), (50, 46, GOLD_BRIGHT), (52, 46, GOLD_TRIM), # Paws gripping prow
    (49, 47, GOLD_TRIM), (50, 47, GOLD_WARM if True else GOLD_TRIM)
]
for fx, fy, fc in figurehead_px:
    set_px(fx, fy, 255, *fc)

# --- 6. Poop Deck / Quarterdeck & Blue-Cloaked Cat Captain at Helm ---
# Quarterdeck raised cabin on aft (x=13..23, y=38..44)
for y in range(39, 45):
    for x in range(13, 24):
        set_px(x, y, 255, *WOOD_WARM if y == 39 else WOOD_MID)
# Railing on quarterdeck
for x in range(13, 24, 2):
    set_px(x, 38, 255, *WOOD_LIGHT)
draw_line(13, 37, 23, 37, 255, *WOOD_DARK)

# Helm wheel (x=21..22, y=36..38)
set_px(21, 37, 255, *WOOD_DARKEST)
set_px(22, 37, 255, *WOOD_DARKEST)
set_px(21, 36, 255, *WOOD_MID)

# Captain Hero! (Blue hooded cloak, pointed cat ears, peering forward!)
# Cloak/body at helm (x=18..20, y=35..38)
set_px(19, 34, 255, 235, 185, 130) # Cat ear tip / face fur
set_px(18, 34, 255, 70, 110, 160)   # Blue hood left ear
set_px(20, 34, 255, 70, 110, 160)   # Blue hood right ear
set_px(18, 35, 255, 45, 80, 135)    # Blue hood
set_px(19, 35, 255, 245, 195, 140)  # Face
set_px(20, 35, 255, 45, 80, 135)    # Hood rim
set_px(19, 36, 255, 40, 25, 20)     # Little eye looking right
set_px(18, 37, 255, 55, 95, 155)    # Cloak shoulder
set_px(19, 37, 255, 65, 115, 175)   # Cloak body
set_px(20, 37, 255, 55, 95, 155)    # Cloak shoulder
set_px(18, 38, 255, 40, 70, 120)    # Cloak hem
set_px(19, 38, 255, 45, 80, 135)
set_px(20, 38, 255, 40, 70, 120)

# --- 7. Aft Stern Lantern ---
# Curved wrought-iron bracket off stern at x=8..11, y=38..42
set_px(12, 40, 255, *INK_DARK)
set_px(11, 39, 255, *INK_DARK)
set_px(10, 38, 255, *INK_DARK)
set_px(9, 38, 255, *INK_DARK)
set_px(9, 39, 255, *INK_DARK)
# Glowing amber lantern core (x=8..10, y=40..42)
set_px(9, 39, 255, *IRON_BOLT)      # Top cap
set_px(8, 40, 255, *IRON_BOLT)
set_px(10, 40, 255, *IRON_BOLT)
set_px(9, 40, 255, 255, 240, 140)   # Radiant white-gold core (emissive!)
set_px(8, 41, 255, 255, 180, 50)    # Amber warm glow
set_px(9, 41, 255, 255, 225, 90)    # Amber bright core
set_px(10, 41, 255, 255, 180, 50)
set_px(9, 42, 255, *IRON_BOLT)      # Base cap

# --- 8. The Main Wooden Caravel Hull (Above Waterline: y=44..56) ---
# Sweeping nautical sheer line from sterncastle (left, y=44) down to midships waist (y=47) and up to prow (right, y=45)
for y in range(44, 57):
    # Determine horizontal hull span at row y
    t_hull = (y - 44) / 12.0
    # Left bound (stern curve): starts at 12 on quarterdeck, cuts inward to 16 at keel
    x_start = int(12 + t_hull * 4.5)
    # Right bound (prow curve): starts at 54 at prow, curves slightly to 50 at keel
    x_end = int(54 - t_hull * 3.5)
    
    # Planking row style (carvel lapstrake strakes)
    strake_seam = (y % 3 == 0)
    for x in range(x_start, x_end + 1):
        rel_x = (x - x_start) / float(max(1, x_end - x_start))
        
        # Base plank tone
        col = WOOD_MID
        if strake_seam:
            col = WOOD_DARK # Shadow between horizontal plank strakes
        elif y == 44 or (y == 45 and x > 25):
            col = WOOD_LIGHT # Gunwale top rail highlight
        elif rel_x < 0.15:
            col = WOOD_DARK # Stern shadow
        elif rel_x > 0.82:
            col = WOOD_WARM # Sunlit prow
        elif (y % 3 == 1):
            col = WOOD_WARM # Planking face
        
        # Gold trim ribbon along the upper hull strake (y=46)
        if y == 46 and 14 <= x <= 50 and (x % 4 != 0):
            col = GOLD_TRIM
        
        # Gunports / black iron square ports with bronze studs at midships (y=49..50)
        for gx in [24, 32, 40]:
            if x in (gx, gx + 1) and y in (49, 50):
                col = INK_DARK if (x == gx and y == 50) else IRON_BOLT
        
        # Anchor resting on starboard bow (x=48..49, y=48..51)
        if (x, y) in [(48, 48), (48, 49), (48, 50), (47, 50), (49, 50), (47, 51)]:
            col = IRON_BOLT

        set_px(x, y, 255, *col)

# --- 9. LOWER HULL & CURVED CARVEL KEEL (y=57..63) ---
# Clean wooden hull timbers narrowing down to a rounded keel/rudder, designed to naturally
# submerge into the WebGPU 3D water plane without any artificial water pixels.
for y in range(57, 64):
    t_keel = (y - 57) / 6.0
    k_start = int(17 + t_keel * 6)
    k_end = int(49 - t_keel * 7)
    strake = (y % 2 == 0)
    for x in range(k_start, k_end + 1):
        rel = (x - k_start) / float(max(1, k_end - k_start))
        col = WOOD_DARKEST
        if not strake:
            col = WOOD_DARK if rel < 0.8 else WOOD_WARM
        set_px(x, y, 255, *col)

# Keel centerline & stern rudder fin (x=20..23, y=60..63)
for y in range(60, 64):
    set_px(20, y, 255, *WOOD_DARKEST)
    set_px(21, y, 255, *WOOD_DARKEST)
    set_px(22, y, 255, *WOOD_DARK)

# Export to C++ .inl file matching hero_sprite_data.inl format
out_path = "src/procgen/ship_sprite_data.inl"
with open(out_path, "w") as f:
    f.write("// 64x64 32-bit RGBA pixel art data (AARRGGBB format) for Nautical Flagship Caravel\n")
    f.write("// Features: Billowing parchment sails, heraldic cross, sterncastle, captain hero, glowing lantern, and submerged waterline keel\n")
    f.write("static constexpr uint32_t kShipSpriteData[64 * 64] = {\n")
    for y in range(HEIGHT):
        f.write("    ")
        row_strs = [f"0x{pixels[y][x]:08X}u" for x in range(WIDTH)]
        f.write(", ".join(row_strs))
        if y < HEIGHT - 1:
            f.write(",\n")
        else:
            f.write("\n")
    f.write("};\n")

print(f"Successfully generated {out_path} ({WIDTH}x{HEIGHT})")
