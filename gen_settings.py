#!/usr/bin/env python3
"""Generate Android settings XML from default_config.toml.

Reads the TOML config and generates:
  1. res/xml/emulator_settings.xml   - PreferenceScreen hierarchy (ALL items, no skips)
  2. res/values/arrays.xml           - String array resources
  3. Updates string resources in res/values/strings.xml (es_ prefixed)
  4. Updates Java key arrays in EmulatorSettings.java
"""
import toml
import os
import re
from collections import OrderedDict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOML_PATH = os.path.join(SCRIPT_DIR, 'app/src/main/assets/config/default_config.toml')
XML_DIR = os.path.join(SCRIPT_DIR, 'app/src/main/res/xml')
VALUES_DIR = os.path.join(SCRIPT_DIR, 'app/src/main/res/values')
JAVA_PATH = os.path.join(SCRIPT_DIR, 'app/src/main/java/aenu/ax360e/EmulatorSettings.java')

# ── Preference type tags ──
LIST_TAG = "aenu.preference.ListPreference"
CHECK_TAG = "aenu.preference.CheckBoxPreference"
SEEK_TAG = "aenu.preference.SeekBarPreference"
EDIT_TAG = "androidx.preference.EditTextPreference"

# ── gen_list: keys with predefined options ──
# Each entry is (display_label, stored_value). If label==value, only one array needed.

GEN_LIST = OrderedDict([
    ("APU|apu",                             [("nop","nop"),("aaudio","aaudio"),("opensles","opensles")]),
    ("APU|xma_decoder",                     [("fake","fake"),("master","master"),("old","old"),("new","new")]),
    ("CPU|cpu",                             [("any","any"),("a64","a64")]),
    ("Content|license_mask",                [("disable","0"),("first","1"),("all","-1")]),
    ("Display|postprocess_antialiasing",    [("none","none"),("fxaa","fxaa"),("fxaa_extreme","fxaa_extreme")]),
    ("Display|postprocess_scaling_and_sharpening", [("bilinear","bilinear"),("cas","cas"),("fsr","fsr")]),
    ("GPU|anisotropic_override",            [("No override","-1"),("Disable anisotropic filtering","0"),("Force 1x anisotropic filtering","1"),("Force 2x anisotropic filtering","2"),("Force 4x anisotropic filtering","3"),("Force 8x anisotropic filtering","4"),("Force 16x anisotropic filtering","5")]),
    ("GPU|gpu",                             [("vulkan","vulkan"),("null","null")]),
    ("GPU|occlusion_query",                 [("fake","fake"),("fast","fast"),("fast-alt","fast-alt"),("strict","strict")]),
    ("GPU|readback_resolve",                [("fast","fast"),("full","full"),("none","none")]),
    ("GPU|render_target_path_vulkan",       [("any","any"),("fbo","fbo"),("fsi","fsi")]),
    ("HID|hid",                             [("android","android"),("nop","nop")]),
    ("Kernel|console_type",                 [("Retail","-1"),("Development Kit","0"),("Test Kit","1")]),
    ("Kernel|kernel_display_gamma_type",    [("linear","0"),("sRGB(CRT)","1"),("BT.709(HDTV)","2")]),
    ("Logging|log_level",                   [("error","0"),("warning","1"),("info","2"),("debug","3")]),
    ("UI|fps_overlay_position",             [("top-left","0"),("top-right","1"),("bottom-left","2"),("bottom-right","3")]),
    ("Video|avpack",                        [("PAL-60 Component (SD)","0"),("Unused","1"),("PAL-60 SCART","2"),
                                             ("480p Component (HD)","3"),("HDMI+A","4"),("PAL-60 Composite/S-Video","5"),
                                             ("VGA","6"),("TV PAL-60","7"),("HDMI","8")]),
    ("Video|video_standard",                [("NTSC","1"),("NTSC-J","2"),("PAL-60","3")]),
    ("Video|internal_display_resolution",   [("640x480","0"),("640x576","1"),("720x480","2"),("720x576","3"),
                                             ("800x600","4"),("848x480","5"),("1024x768","6"),("1152x864","7"),
                                             ("1280x720","8"),("1280x768","9"),("1280x960","10"),("1280x1024","11"),
                                             ("1360x768","12"),("1440x900","13"),("1680x1050","14"),
                                             ("1920x540","15"),("1920x1080","16")]),
    ("XConfig|user_country",                [("AE","1"),("AL","2"),("AM","3"),("AR","4"),("AT","5"),("AU","6"),
                                             ("AZ","7"),("BE","8"),("BG","9"),("BH","10"),("BN","11"),("BO","12"),
                                             ("BR","13"),("BY","14"),("BZ","15"),("CA","16"),("CH","18"),("CL","19"),
                                             ("CN","20"),("CO","21"),("CR","22"),("CZ","23"),("DE","24"),("DK","25"),
                                             ("DO","26"),("DZ","27"),("EC","28"),("EE","29"),("EG","30"),("ES","31"),
                                             ("FI","32"),("FO","33"),("FR","34"),("GB","35"),("GE","36"),("GR","37"),
                                             ("GT","38"),("HK","39"),("HN","40"),("HR","41"),("HU","42"),("ID","43"),
                                             ("IE","44"),("IL","45"),("IN","46"),("IQ","47"),("IR","48"),("IS","49"),
                                             ("IT","50"),("JM","51"),("JO","52"),("JP","53"),("KE","54"),("KG","55"),
                                             ("KR","56"),("KW","57"),("KZ","58"),("LB","59"),("LI","60"),("LT","61"),
                                             ("LU","62"),("LV","63"),("LY","64"),("MA","65"),("MC","66"),("MK","67"),
                                             ("MN","68"),("MO","69"),("MV","70"),("MX","71"),("MY","72"),("NI","73"),
                                             ("NL","74"),("NO","75"),("NZ","76"),("OM","77"),("PA","78"),("PE","79"),
                                             ("PH","80"),("PK","81"),("PL","82"),("PR","83"),("PT","84"),("PY","85"),
                                             ("QA","86"),("RO","87"),("RU","88"),("SA","89"),("SE","90"),("SG","91"),
                                             ("SI","92"),("SK","93"),("SV","95"),("SY","96"),("TH","97"),("TN","98"),
                                             ("TR","99"),("TT","100"),("TW","101"),("UA","102"),("US","103"),
                                             ("UY","104"),("UZ","105"),("VE","106"),("VN","107"),("YE","108"),("ZA","109")]),
    ("XConfig|user_language",              [("en","1"),("ja","2"),("de","3"),("fr","4"),("es","5"),("it","6"),
                                             ("ko","7"),("zh","8"),("pt","9"),("pl","11"),("ru","12"),("sv","13"),
                                             ("tr","14"),("nb","15"),("nl","16"),("zh-TW","17")]),
])
# ── gen_seekbar: int keys with (min, max) ──
GEN_SEEKBAR = OrderedDict([
    ("GPU|texture_cache_memory_limit_hard", (512, 4096)),
    ("GPU|texture_cache_memory_limit_soft", (512, 4096)),
    ("Memory|mmap_address_high",            (2, 63)),
    ("APU|apu_max_queued_frames",           (4, 64)),
    ("APU|xmp_default_volume",              (0, 100)),
    ("General|time_scalar",                 (1, 8)),
    ("Video|custom_internal_display_resolution_x", (0, 1920)),
    ("Video|custom_internal_display_resolution_y", (0, 1080)),
    ("UI|font_size",                        (8, 48)),
    ("UI|window_size_x",                    (320, 3840)),
    ("UI|window_size_y",                    (240, 2160)),
    ("Memory|scribble_heap_value",          (0, 255)),
    ("General|priority_class",              (0, 2)),
    ("General|recent_titles_entry_amount",  (1, 50)),
])

def convert_to_name(key):
    """Convert snake_case key to Title Case display name."""
    parts = key.split('_')
    return ' '.join(p.capitalize() if p else p for p in parts)


def escape_xml(s):
    """Escape special XML/Android string characters."""
    s = s.replace('\\', '\\\\')  # Escape backslashes for Android (must be first)
    s = s.replace("'", "\\'")     # Escape apostrophes Android-style (not &apos;)
    s = s.replace('&', '&amp;')
    s = s.replace('<', '&lt;')
    s = s.replace('>', '&gt;')
    s = s.replace('"', '&quot;')
    return s


def escape_xml_hint(s):
    """Escape for XML string resource, converting newlines to \\n."""
    s = escape_xml(s)
    s = s.replace('\n', '\\n')
    return s


_KV_RE = re.compile(r'^\s*([A-Za-z0-9_]+)\s*=\s*([^#\n]*?)(?:\s+#.*)?$')


def parse_toml_hints(path):
    """Extract per-key description comments from TOML file.
    Returns {section: {key: comment_text}}.
    """
    hints = {}
    current_section = None
    current_key = None

    with open(path, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    for raw_line in lines:
        line = raw_line.rstrip('\n').rstrip('\r')
        stripped = line.strip()

        if not stripped:
            # Empty line ends comment continuation after a key
            if current_key is not None:
                current_key = None
            continue

        if stripped.startswith('[') and stripped.endswith(']'):
            current_section = stripped[1:-1].strip()
            hints[current_section] = {}
            current_key = None
            continue

        if stripped.startswith('#'):
            # Pure comment line
            if current_key is not None and current_section is not None:
                # Continuation of previous key's comment
                text = stripped.lstrip('#').strip()
                hints[current_section][current_key] += '\n' + text
            # else: standalone / section comment, ignore
            continue

        m = _KV_RE.match(line)
        if m and current_section is not None:
            key = m.group(1)
            inline_comment = line.split('#', 1)[1].strip() if '#' in line else ''
            if inline_comment:
                hints[current_section][key] = inline_comment
                current_key = key
            else:
                current_key = None
        else:
            current_key = None

    return hints


def has_separate_values(entries):
    """Check if list entries have separate display/value pairs."""
    return any(label != value for label, value in entries)


def gen_pref_element(table_name, key_name, val, table_name_l, hint=None):
    """Generate XML lines for a single preference element."""
    full_key = f"{table_name}|{key_name}"
    title_ref = f"@string/es_{table_name_l}_{key_name}"
    summary_line = f'            app:summary="@string/es_hint_{table_name_l}_{key_name}"' if hint else None
    lines = []

    if full_key in GEN_LIST:
        entries = GEN_LIST[full_key]
        sep = has_separate_values(entries)
        lines.append(f'        <{LIST_TAG} app:title="{title_ref}"')
        if sep:
            lines.append(f'            app:entryValues="@array/es_arr_v_{table_name_l}_{key_name}"')
        else:
            lines.append(f'            app:entryValues="@array/es_arr_{table_name_l}_{key_name}"')
        lines.append(f'            app:entries="@array/es_arr_{table_name_l}_{key_name}"')
        if summary_line:
            lines.append(summary_line)
        lines.append(f'            app:iconSpaceReserved="false"')
        lines.append(f'            app:key="{full_key}" />')

    elif full_key in GEN_SEEKBAR:
        mn, mx = GEN_SEEKBAR[full_key]
        lines.append(f'        <{SEEK_TAG} app:title="{title_ref}"')
        lines.append(f'            app:min="{mn}"')
        lines.append(f'            android:max="{mx}"')
        lines.append(f'            app:showSeekBarValue="true"')
        if summary_line:
            lines.append(summary_line)
        lines.append(f'            app:iconSpaceReserved="false"')
        lines.append(f'            app:key="{full_key}" />')

    elif isinstance(val, bool):
        lines.append(f'        <{CHECK_TAG} app:title="{title_ref}"')
        if summary_line:
            lines.append(summary_line)
        lines.append(f'            app:iconSpaceReserved="false"')
        lines.append(f'            app:key="{full_key}" />')

    else:
        # string, int, float -> EditTextPreference
        #lines.append(f'        <{EDIT_TAG} app:title="{title_ref}"')
        lines.append(f'        <PreferenceScreen app:title="{title_ref}"')
        if summary_line:
            lines.append(summary_line)
        lines.append(f'            app:iconSpaceReserved="false"')
        lines.append(f'            app:key="{full_key}" />')

    return lines


def generate_emulator_settings_xml(config, hints):
    """Generate res/xml/emulator_settings.xml"""
    lines = []
    lines.append('<?xml version="1.0" encoding="utf-8"?>')
    lines.append('<PreferenceScreen')
    lines.append('    xmlns:android="http://schemas.android.com/apk/res/android"')
    lines.append('    xmlns:app="http://schemas.android.com/apk/res-auto">')

    for table_name in config:
        table = config[table_name]
        if not isinstance(table, dict):
            continue
        table_name_l = table_name.lower()
        table_hints = hints.get(table_name, {})

        lines.append(f'    <PreferenceScreen app:title="@string/es_{table_name_l}"')
        lines.append(f'        app:iconSpaceReserved="false"')
        lines.append(f'        app:key="{table_name}" >')

        for key_name in table:
            val = table[key_name]
            hint = table_hints.get(key_name)
            lines.extend(gen_pref_element(table_name, key_name, val, table_name_l, hint))

        lines.append(f'    </PreferenceScreen>')

    lines.append('</PreferenceScreen>')

    out_path = os.path.join(XML_DIR, 'emulator_settings.xml')
    os.makedirs(XML_DIR, exist_ok=True)
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f"Generated {out_path}")


def generate_arrays_xml(config):
    """Generate res/values/arrays.xml"""
    lines = []
    lines.append('<?xml version="1.0" encoding="utf-8"?>')
    lines.append('<resources>')

    for full_key, entries in GEN_LIST.items():
        table_name, key_name = full_key.split('|')
        table_name_l = table_name.lower()
        sep = has_separate_values(entries)

        if sep:
            lines.append(f'    <string-array name="es_arr_v_{table_name_l}_{key_name}">')
            for label, value in entries:
                lines.append(f'        <item>{escape_xml(value)}</item>')
            lines.append(f'    </string-array>')

        lines.append(f'    <string-array name="es_arr_{table_name_l}_{key_name}">')
        for label, value in entries:
            lines.append(f'        <item>{escape_xml(label)}</item>')
        lines.append(f'    </string-array>')

    lines.append('</resources>')

    out_path = os.path.join(VALUES_DIR, 'arrays.xml')
    with open(out_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f"Generated {out_path}")


def generate_strings(config, hints):
    """Update res/values/strings.xml: replace es_ and es_hint_ strings, keep others."""
    strings_path = os.path.join(VALUES_DIR, 'strings.xml')

    with open(strings_path, 'r', encoding='utf-8') as f:
        existing_content = f.read()

    # Separate: keep non-es_ lines, discard es_ and es_hint_ elements (including multi-line)
    non_es_lines = []
    skipping_multiline = False
    for line in existing_content.split('\n'):
        stripped = line.strip()
        if stripped in ('<resources>', '</resources>'):
            continue

        # If we're skipping a multi-line es_ string, check if this line ends it
        if skipping_multiline:
            if '</string>' in line:
                skipping_multiline = False
            continue

        # Check if this line starts an es_ string element
        if 'name="es_' in line:
            # If the element doesn't close on this line, enter multi-line skip
            if '</string>' not in line:
                skipping_multiline = True
            continue

        non_es_lines.append(line)

    # Collect generated string names for dedup check
    generated_names = set()
    for table_name in config:
        table = config[table_name]
        if not isinstance(table, dict):
            continue
        table_name_l = table_name.lower()
        generated_names.add(f'es_{table_name_l}')
        for key_name in table:
            generated_names.add(f'es_{table_name_l}_{key_name}')

    # Build output
    out = []
    out.append('<resources>')
    for line in non_es_lines:
        out.append(line)

    out.append('')
    out.append('    <!-- Auto-generated setting strings -->')

    for table_name in config:
        table = config[table_name]
        if not isinstance(table, dict):
            continue
        table_name_l = table_name.lower()
        table_hints = hints.get(table_name, {})
        out.append(f'    <string name="es_{table_name_l}">{escape_xml(table_name)}</string>')
        for key_name in table:
            display_name = convert_to_name(key_name)
            out.append(f'    <string name="es_{table_name_l}_{key_name}">{escape_xml(display_name)}</string>')
            # Generate hint string from TOML comment
            hint = table_hints.get(key_name)
            if hint:
                out.append(f'    <string name="es_hint_{table_name_l}_{key_name}">{escape_xml_hint(hint)}</string>')

    out.append('')
    out.append('</resources>')

    with open(strings_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out) + '\n')
    print(f"Updated {strings_path}")


def classify_key(table_name, key_name, val):
    """Classify a config key into bool/seekbar/list/edit."""
    full_key = f"{table_name}|{key_name}"
    if full_key in GEN_LIST:
        return 'list'
    if full_key in GEN_SEEKBAR:
        return 'seekbar'
    if isinstance(val, bool):
        return 'bool'
    return 'edit'


def generate_java_arrays(config):
    """Generate Java key arrays."""
    bool_keys = []
    int_keys = []
    string_arr_keys = []
    node_keys = []

    for table_name in config:
        table = config[table_name]
        if not isinstance(table, dict):
            continue
        node_keys.append(table_name)
        for key_name in table:
            val = table[key_name]
            full_key = f"{table_name}|{key_name}"
            cat = classify_key(table_name, key_name, val)
            if cat == 'bool':
                bool_keys.append(full_key)
            elif cat == 'seekbar':
                int_keys.append(full_key)
            elif cat == 'list':
                string_arr_keys.append(full_key)
            # 'edit' keys are not iterated in Java

    return bool_keys, int_keys, string_arr_keys, node_keys


def format_java_array(name, keys, indent='            '):
    """Format a Java String array declaration."""
    lines = [f'{indent}final String[] {name}={{']
    for k in keys:
        lines.append(f'{indent}    "{k}",')
    lines.append(f'{indent}}};')
    return '\n'.join(lines)


def update_java_arrays(bool_keys, int_keys, string_arr_keys, node_keys):
    """Update EmulatorSettings.java with new key arrays."""
    with open(JAVA_PATH, 'r', encoding='utf-8') as f:
        content = f.read()

    # Normalize line endings for regex matching
    content = content.replace('\r\n', '\n')

    # Replace each array block
    for arr_name, keys in [('BOOL_KEYS', bool_keys), ('INT_KEYS', int_keys),
                            ('STRING_ARR_KEYS', string_arr_keys), ('NODE_KEYS', node_keys)]:
        pattern = rf'final String\[\] {arr_name}=\{{\n.*?\n\s*\}};'
        indent = '            '
        arr_lines = [f'{indent}final String[] {arr_name}={{'] 
        for k in keys:
            arr_lines.append(f'{indent}    "{k}",')
        arr_lines.append(f'{indent}}};')
        replacement = '\n'.join(arr_lines)
        content, count = re.subn(pattern, replacement, content, flags=re.DOTALL)
        print(f"  {arr_name}: {count} replacements")

    with open(JAVA_PATH, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    print(f"Updated {JAVA_PATH}")


def main():
    print(f"Reading {TOML_PATH}")
    with open(TOML_PATH, 'r', encoding='utf-8') as f:
        config = toml.load(f)

    print(f"Parsed {len(config)} sections from TOML")

    # Extract comments/hints from TOML
    hints = parse_toml_hints(TOML_PATH)
    total_hints = sum(len(v) for v in hints.values())
    print(f"Extracted {total_hints} hint comments")

    # 1. Generate emulator_settings.xml
    generate_emulator_settings_xml(config, hints)

    # 2. Generate arrays.xml
    generate_arrays_xml(config)

    # 3. Update strings.xml
    #generate_strings(config, hints)

    # 4. Update Java arrays
    bool_keys, int_keys, string_arr_keys, node_keys = generate_java_arrays(config)
    update_java_arrays(bool_keys, int_keys, string_arr_keys, node_keys)

    # Print summary
    print(f"\nSummary:")
    print(f"  BOOL_KEYS:        {len(bool_keys)} entries")
    print(f"  INT_KEYS:         {len(int_keys)} entries")
    print(f"  STRING_ARR_KEYS:  {len(string_arr_keys)} entries")
    print(f"  NODE_KEYS:        {len(node_keys)} entries")
    print(f"\nDone! All files generated successfully.")


if __name__ == '__main__':
    main()
