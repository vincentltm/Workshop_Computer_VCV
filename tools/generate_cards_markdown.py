#!/usr/bin/env python3
import json
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VCV_PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
JSON_PATH = os.path.join(VCV_PROJECT_DIR, "cards_registry.json")
MD_PATH = os.path.join(VCV_PROJECT_DIR, "CARDS.md")

def format_features(ext):
    feats = []
    if ext.get("web_ui") == "yes":
        feats.append("🌐 Web UI")
    if ext.get("usb_audio") == "yes":
        feats.append("🔊 USB Audio")
    if ext.get("midi_serial") == "yes":
        feats.append("🎹 MIDI/Serial")
    return ", ".join(feats) if feats else "None"

def generate_markdown():
    if not os.path.exists(JSON_PATH):
        print(f"Error: {JSON_PATH} not found!")
        return

    with open(JSON_PATH, "r") as f:
        cards = json.load(f)

    active_built = [c for c in cards if c.get("status") == "active_built" and c.get("viable", True)]
    whitelisted_inactive = [c for c in cards if c.get("status") == "whitelisted_inactive" and c.get("viable", True)]
    unallowed_unbuilt = [c for c in cards if c.get("status") == "unallowed_unbuilt" and c.get("viable", True)]
    not_viable_cards = [c for c in cards if not c.get("viable", True)]

    lines = []
    lines.append("# Music Thing Modular Workshop Cards Master Registry")
    lines.append("")
    lines.append("Master source of truth derived from [`cards_registry.json`](file:///Users/vmaurer/Music/Workshop_VCV_Dev/Workshop_Computer_VCV/cards_registry.json).")
    lines.append("")
    lines.append(f"**Total Registered Cards**: {len(cards)} | **Active / Built**: {len(active_built)} | **Whitelisted Inactive**: {len(whitelisted_inactive)} | **Waiting (Unallowed)**: {len(unallowed_unbuilt)} | **Hardware Not Viable**: {len(not_viable_cards)}")
    lines.append("")
    
    lines.append(f"## 1. Active Built Cards ({len(active_built)})")
    lines.append("Cards enabled, permission granted (author or license), built, and packaged into the VCV Rack plugin and Patchnotes web application.")
    lines.append("")
    lines.append("| Num | Card ID | Card Name | Version | Port Type | Special Features | License | Creator | Permission | Source Availability | Patchnotes | Repository | Tested | Status |")
    lines.append("| :---: | :--- | :--- | :---: | :---: | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |")

    for c in active_built:
        num = f"[{c['num'].zfill(2)}]"
        cid = c['id']
        name = c['name']
        ver = c.get('version', '1.0')
        ptype = c.get('port_type', 'direct_port')
        feats = format_features(c.get('external_connections', {}))
        lic = c.get('license', 'No License Specified')
        creator = c.get('creator', 'Community')
        perm = c.get('permission', 'granted')
        repo_url = c.get('repository_url', '')
        repo_str = f"[Link]({repo_url})" if repo_url else "Internal"
        src_avail = "🔗 External Repo" if repo_url else ("⚠️ No Source Available" if c.get("no_source_available") else "✅ Source Available")
        pn_str = "🌐 Yes" if c.get("patchnotes") else "❌ No"
        tested = "[x] Yes" if c.get("tested") else "[ ] No"
        lines.append(f"| {num} | `{cid}` | {name} | `{ver}` | `{ptype}` | {feats} | `{lic}` | {creator} | `{perm}` | {src_avail} | {pn_str} | {repo_str} | {tested} | ✅ Active Built |")

    lines.append("")
    lines.append(f"## 2. Whitelisted Inactive Cards ({len(whitelisted_inactive)})")
    lines.append("Cards whitelisted for testing/build preparation, but currently disabled.")
    lines.append("")
    lines.append("| Num | Card ID | Card Name | Version | Port Type | Special Features | License | Creator | Permission | Source Availability | Patchnotes | Repository | Tested | Status |")
    lines.append("| :---: | :--- | :--- | :---: | :---: | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |")

    if not whitelisted_inactive:
        lines.append("*No cards currently in whitelisted inactive state.*")
    else:
        for c in whitelisted_inactive:
            num = f"[{c['num'].zfill(2)}]"
            cid = c['id']
            name = c['name']
            ver = c.get('version', '1.0')
            ptype = c.get('port_type', 'direct_port')
            feats = format_features(c.get('external_connections', {}))
            lic = c.get('license', 'No License Specified')
            creator = c.get('creator', 'Community')
            perm = c.get('permission', 'granted')
            repo_url = c.get('repository_url', '')
            repo_str = f"[Link]({repo_url})" if repo_url else "Internal"
            src_avail = "🔗 External Repo" if repo_url else ("⚠️ No Source Available" if c.get("no_source_available") else "✅ Source Available")
            pn_str = "🌐 Yes" if c.get("patchnotes") else "❌ No"
            tested = "[x] Yes" if c.get("tested") else "[ ] No"
            lines.append(f"| {num} | `{cid}` | {name} | `{ver}` | `{ptype}` | {feats} | `{lic}` | {creator} | `{perm}` | {src_avail} | {pn_str} | {repo_str} | {tested} | ⏳ Whitelisted Inactive |")

    lines.append("")
    lines.append(f"## 3. Waiting / Unbuilt Cards ({len(unallowed_unbuilt)})")
    lines.append("Community release folders present in the repository where build permission is waiting / not yet granted.")
    lines.append("")
    lines.append("| Num | Card ID | Card Name | Version | Port Type | Special Features | License | Creator | Permission | Source Availability | Patchnotes | Repository | Tested | Status |")
    lines.append("| :---: | :--- | :--- | :---: | :---: | :--- | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |")

    for c in unallowed_unbuilt:
        num = f"[{c['num'].zfill(2)}]"
        cid = c['id']
        name = c['name']
        ver = c.get('version', '1.0')
        ptype = c.get('port_type', 'direct_port')
        feats = format_features(c.get('external_connections', {}))
        lic = c.get('license', 'No License Specified')
        creator = c.get('creator', 'Community')
        perm = c.get('permission', 'waiting')
        repo_url = c.get('repository_url', '')
        repo_str = f"[Link]({repo_url})" if repo_url else "Internal"
        src_avail = "🔗 External Repo" if repo_url else ("⚠️ No Source Available" if c.get("no_source_available") else "✅ Source Available")
        pn_str = "🌐 Yes" if c.get("patchnotes") else "❌ No"
        tested = "[x] Yes" if c.get("tested") else "[ ] No"
        lines.append(f"| {num} | `{cid}` | {name} | `{ver}` | `{ptype}` | {feats} | `{lic}` | {creator} | `{perm}` | {src_avail} | {pn_str} | {repo_str} | {tested} | ❌ Waiting / Unbuilt |")

    lines.append("")
    lines.append(f"## 4. Hardware Not Viable Cards ({len(not_viable_cards)})")
    lines.append("Cards requiring specific RP2040 physical hardware (RF radio transmission, low-level DMA IRQ channels) that are not viable in software VCV Rack.")
    lines.append("")
    lines.append("| Num | Card ID | Card Name | Version | Creator | Reason / Viability Notes | Repository | Viability |")
    lines.append("| :---: | :--- | :--- | :---: | :--- | :--- | :---: | :---: |")

    for c in not_viable_cards:
        num = f"[{c['num'].zfill(2)}]"
        cid = c['id']
        name = c['name']
        ver = c.get('version', '1.0')
        creator = c.get('creator', 'Community')
        notes = c.get('viability_notes', 'Hardware dependent')
        repo_url = c.get('repository_url', '')
        repo_str = f"[Link]({repo_url})" if repo_url else "Internal"
        lines.append(f"| {num} | `{cid}` | {name} | `{ver}` | {creator} | {notes} | {repo_str} | 🚫 Not Viable |")

    lines.append("")
    lines.append("## Detailed Card Directory")
    lines.append("")

    for c in cards:
        num = f"[{c['num'].zfill(2)}]"
        cid = c['id']
        name = c['name']
        is_viable = c.get("viable", True)
        st = c.get("status", "unallowed_unbuilt")
        no_src = c.get("no_source_available", False)

        if not is_viable:
            status_disp = "🚫 Hardware Not Viable"
        elif st == "active_built":
            status_disp = "✅ Active Built"
        elif st == "whitelisted_inactive":
            status_disp = "⏳ Whitelisted Inactive"
        else:
            status_disp = "❌ Waiting / Unbuilt"

        ext = c.get("external_connections", {})
        web_ui = ext.get("web_ui", "none")
        usb_aud = ext.get("usb_audio", "none")
        midi_ser = ext.get("midi_serial", "none")
        work_st = ext.get("working_status", "untested")
        repo_url = c.get('repository_url', 'None')
        pn_st = "supported" if c.get("patchnotes") else "unsupported"

        lines.append(f"### {num} {name} (`{cid}`)")
        lines.append(f"- **Build Status**: {status_disp}")
        lines.append(f"- **Patchnotes Web Client**: `{pn_st}`")
        lines.append(f"- **Repository URL**: `{repo_url}`")
        lines.append(f"- **Source Availability**: `{'external_repo_available' if repo_url != 'None' else ('no_source_available' if no_src else 'source_available')}`")
        lines.append(f"- **Viability**: `{'viable' if is_viable else 'not_viable'}` ({c.get('viability_notes', '')})")
        lines.append(f"- **Whitelisted**: `{c.get('whitelisted', False)}` | **Enabled**: `{c.get('enabled', False)}`")
        lines.append(f"- **Creator / Author**: {c.get('creator', 'Unknown')}")
        lines.append(f"- **License**: `{c.get('license', 'No License Specified')}`")
        lines.append(f"- **Permission**: `{c.get('permission', 'waiting')}` ({c.get('permission_notes', '')})")
        lines.append(f"- **Version**: `{c.get('version', '1.0')}` | **Port Type**: `{c.get('port_type', 'direct_port')}`")
        lines.append(f"- **External Connections**: Web UI: `{web_ui}` | USB Audio: `{usb_aud}` | MIDI/Serial: `{midi_ser}`")
        lines.append(f"- **Source Location**: `{c.get('dir', '')}` ({c.get('source_type', 'internal')})")
        lines.append(f"- **Source Files**: `{', '.join(c.get('sources', []))}`")
        lines.append(f"- **Description**: {c.get('description', '')}")
        lines.append("")

    with open(MD_PATH, "w") as f:
        f.write("\n".join(lines))

    print(f"Generated {MD_PATH} successfully!")

if __name__ == "__main__":
    generate_markdown()
