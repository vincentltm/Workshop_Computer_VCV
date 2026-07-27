#!/usr/bin/env python3
"""
conftest.py — Shared utilities, data types, and card registry loader
for the Workshop Computer unified test harness.
"""
import os
import re
import json
import sys
import platform
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Optional, Any

# ─── Path Constants ───────────────────────────────────────────────────────────
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
PROJECT_DIR = os.path.dirname(TOOLS_DIR)           # Workshop_Computer_VCV/
WORKSPACE_DIR = os.path.dirname(PROJECT_DIR)        # Workshop_VCV_Dev/
DEPS_DIR = os.path.join(PROJECT_DIR, "deps", "Workshop_Computer")
EXTERNAL_DIR = os.path.join(PROJECT_DIR, "deps", "external")
CARDS_SRC_DIR = os.path.join(PROJECT_DIR, "src", "cards")
REGISTRY_FILE = os.path.join(PROJECT_DIR, "cards_registry.json")
PATCHNOTES_DIR = os.path.join(WORKSPACE_DIR, "patchnotes", "patch_notes")
CARD_DEFINITIONS_JS = os.path.join(PATCHNOTES_DIR, "js", "cards", "CardDefinitions.js")
WASM_CARD_WRAPPER_JS = os.path.join(PATCHNOTES_DIR, "js", "cards", "WasmCardWrapper.js")
WASM_DIR = os.path.join(PATCHNOTES_DIR, "js", "cards", "wasm")
WASM_BINARY = os.path.join(WASM_DIR, "patchnotes_cards.wasm")
WASM_JS = os.path.join(WASM_DIR, "patchnotes_cards.js")
EXTENDED_METADATA_HPP = os.path.join(PROJECT_DIR, "src", "shared", "ExtendedMetadata.hpp")

# ─── ANSI Colors ──────────────────────────────────────────────────────────────
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
BLUE   = "\033[94m"
CYAN   = "\033[96m"
BOLD   = "\033[1m"
DIM    = "\033[2m"
RESET  = "\033[0m"

# ─── Namespace Special Cases (from port_all_cards.py) ─────────────────────────
SPECIAL_NS = {
    "usb_audio_bridge": "Card_USBAudio",
    "ca_sequencer": "Card_CASequencer",
    "cosmik_c1zzl3": "Card_CosmikC1zzl3",
    "fr330hfr33": "Card_Fr330hFr33",
    "duo_midi": "Card_DuoMidi",
    "simple_midi": "Card_SimpleMidi",
    "bitphase": "Card_BitPhase",
    "eighties_bass": "Card_EightiesBass",
    "cirpy_wavetable": "Card_CirpyWavetable",
    "backyard_rain": "Card_BackyardRain",
    "utility_pair": "Card_UtilityPair",
    "turing_machine": "Card_TuringMachine",
    "resonator": "Card_Resonator",
    "byo_benjolin": "Card_ByoBenjolin",
    "computer_grids": "Card_ComputerGrids",
    "dual_quant": "Card_DualQuant",
    "tapegrade": "Card_Tapegrade",
    "wild_pebble": "Card_WildPebble",
    "castle_process": "Card_CastleProcess",
    "west_coast_lpg": "Card_WestCoastLPG",
    "turing_clouds": "Card_TuringClouds",
    "sense_of_space": "Card_SenseOfSpace",
}

# ─── Data Classes ─────────────────────────────────────────────────────────────

@dataclass
class CardInfo:
    id: str
    num: str
    name: str
    dir: str
    ns: str
    sources: List[str]
    enabled: bool
    patchnotes: bool
    category: str
    creator: str
    license: str
    description: str


@dataclass
class TestResult:
    name: str
    passed: bool
    message: str
    details: Optional[Dict[str, Any]] = None

    def to_dict(self) -> Dict[str, Any]:
        d = {"name": self.name, "passed": self.passed, "message": self.message}
        if self.details:
            d["details"] = self.details
        return d


@dataclass
class CardTestReport:
    card_id: str
    card_num: str
    card_name: str
    platform: str       # "vcv", "patchnotes", "metadata", "build"
    status: str         # "pass", "degraded", "fail", "skip"
    tests: List[TestResult] = field(default_factory=list)
    error: Optional[str] = None
    duration_ms: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
        return {
            "card_id": self.card_id,
            "card_num": self.card_num,
            "card_name": self.card_name,
            "platform": self.platform,
            "status": self.status,
            "tests": [t.to_dict() for t in self.tests],
            "error": self.error,
            "duration_ms": self.duration_ms,
        }


# ─── Card Registry Loader ────────────────────────────────────────────────────

def _compute_namespace(card_id: str, explicit_ns: Optional[str] = None) -> str:
    """Compute the C++ namespace for a card, matching port_all_cards.py logic."""
    if explicit_ns:
        return explicit_ns
    if card_id in SPECIAL_NS:
        return SPECIAL_NS[card_id]
    return "Card_" + "".join(x.capitalize() for x in card_id.split("_"))


def _load_registry_raw() -> List[Dict]:
    """Load the raw JSON registry."""
    if not os.path.exists(REGISTRY_FILE):
        print(f"{RED}Error: cards_registry.json not found at {REGISTRY_FILE}{RESET}",
              file=sys.stderr)
        return []
    with open(REGISTRY_FILE, "r") as f:
        return json.load(f)


def _card_from_dict(entry: Dict) -> CardInfo:
    """Convert a registry dict entry to a CardInfo object."""
    cid = entry.get("id", "")
    ns = _compute_namespace(cid, entry.get("ns"))
    return CardInfo(
        id=cid,
        num=entry.get("num", "??"),
        name=entry.get("name", cid),
        dir=entry.get("dir", ""),
        ns=ns,
        sources=entry.get("sources", []),
        enabled=entry.get("enabled", False),
        patchnotes=entry.get("patchnotes", False),
        category=entry.get("category", ""),
        creator=entry.get("creator", ""),
        license=entry.get("license", ""),
        description=entry.get("description", ""),
    )


def load_card_registry() -> List[CardInfo]:
    """Load enabled cards from cards_registry.json."""
    raw = _load_registry_raw()
    return [_card_from_dict(e) for e in raw if e.get("enabled", False)]


def load_all_cards_registry() -> List[CardInfo]:
    """Load all cards (enabled + disabled) from cards_registry.json."""
    raw = _load_registry_raw()
    return [_card_from_dict(e) for e in raw]


# ─── Path Resolution ─────────────────────────────────────────────────────────

def resolve_card_source_dir(card: CardInfo) -> str:
    """Resolve the actual filesystem path for a card's source directory."""
    d = card.dir
    if d.startswith("deps/external/"):
        return os.path.join(PROJECT_DIR, d)
    elif d.startswith("releases/"):
        return os.path.join(DEPS_DIR, d)
    else:
        return os.path.join(DEPS_DIR, d)


def find_info_yaml(card: CardInfo) -> Optional[str]:
    """Find the info.yaml for a card, checking the source dir and parent."""
    src_dir = resolve_card_source_dir(card)
    # Direct path
    p = os.path.join(src_dir, "info.yaml")
    if os.path.exists(p):
        return p
    # Check parent directory (for cards like Turing_Machine/Rev_1_5_Code)
    parent = os.path.dirname(src_dir)
    p2 = os.path.join(parent, "info.yaml")
    if os.path.exists(p2):
        return p2
    return None


def get_dylib_extension() -> str:
    """Get the dynamic library extension for the current platform."""
    system = platform.system()
    if system == "Darwin":
        return ".dylib"
    elif system == "Windows":
        return ".dll"
    else:
        return ".so"


def get_card_dylib_path(card: CardInfo) -> str:
    """Return the expected path to the card's dynamic library."""
    ext = get_dylib_extension()
    return os.path.join(PROJECT_DIR, "res", "cards", f"libcard_{card.id}{ext}")


# ─── Metadata Parsers ────────────────────────────────────────────────────────

def parse_card_definitions_js() -> Dict[str, Dict]:
    """Parse CardDefinitions.js to extract card metadata.
    Returns dict keyed by card id."""
    if not os.path.exists(CARD_DEFINITIONS_JS):
        return {}

    with open(CARD_DEFINITIONS_JS, "r", encoding="utf-8") as f:
        content = f.read()

    cards = {}
    # Match each card object block by finding id: '...' patterns
    # Extract id, name, num, creator, license, repository
    id_pattern = re.compile(r"id:\s*'([^']+)'")
    name_pattern = re.compile(r"name:\s*'([^']*)'")
    num_pattern = re.compile(r"num:\s*'([^']*)'")
    creator_pattern = re.compile(r"creator:\s*'([^']*)'")
    license_pattern = re.compile(r"license:\s*'([^']*)'")
    repo_pattern = re.compile(r"repository:\s*'([^']*)'")

    # Split by top-level object boundaries (looking for { id: '...')
    blocks = re.split(r'\n\s*\{', content)
    for block in blocks:
        id_match = id_pattern.search(block)
        if not id_match:
            continue
        card_id = id_match.group(1)
        entry = {"id": card_id}

        for key, pat in [("name", name_pattern), ("num", num_pattern),
                         ("creator", creator_pattern), ("license", license_pattern),
                         ("repository", repo_pattern)]:
            m = pat.search(block)
            if m:
                entry[key] = m.group(1)

        cards[card_id] = entry

    return cards


WASM_ALIASES = {
    "simple_midi": "midi",
    "usb_audio_bridge": "usb_audio",
}


def parse_wasm_card_map() -> Dict[str, int]:
    """Parse WasmCardWrapper.js to extract the WASM_CARD_MAP mapping."""
    if not os.path.exists(WASM_CARD_WRAPPER_JS):
        return {}

    with open(WASM_CARD_WRAPPER_JS, "r", encoding="utf-8") as f:
        content = f.read()

    card_map = {}
    pattern = re.compile(r"'(\w+)':\s*(\d+)")
    start = content.find("WASM_CARD_MAP")
    if start == -1:
        return {}
    end = content.find("};", start)
    if end == -1:
        end = len(content)
    block = content[start:end]

    for m in pattern.finditer(block):
        card_map[m.group(1)] = int(m.group(2))

    # Add reverse alias lookup entries
    for registry_id, wasm_alias in WASM_ALIASES.items():
        if wasm_alias in card_map and registry_id not in card_map:
            card_map[registry_id] = card_map[wasm_alias]

    return card_map


# ─── Card Classification ─────────────────────────────────────────────────────

# Known Lua/configurable cards
LUA_CARDS = {"blackbird", "duo_midi", "krell"}
CONFIGURABLE_CARDS = {"flux", "utility_pair", "toolbox", "cvmod", "byo_benjolin"}

# ID-based heuristics for classification
OSC_KEYWORDS = {"vco", "organ", "twists", "birds", "bytebeat", "goldfish",
                "siren", "eighties_bass", "sheep", "chirp",
                "cirpy", "braids", "plaits", "fm_sine", "sine", "chord",
                "dronebox", "polysynth", "harmonic", "wavetable", "fifths",
                "lochovibes", "bitphase", "acid", "chorgan", "alloy",
                "voices_of_sid", "cosmik", "fr330h",
                "wild_pebble", "drumdrum", "noisebox"}

EFFECT_KEYWORDS = {"reverb", "delay", "filter", "dist", "stomp", "tape",
                   "flux", "grain", "glitter", "tapegrade", "glitch",
                   "stretchcore", "offair", "pantograph", "lens",
                   "castle_process", "sense_of_space", "fragments", "od",
                   "degenerator", "hot_fuzz"}

MIDI_KEYWORDS = {"midi", "simple_midi", "duo_midi"}

CLOCK_KEYWORDS = {"clock", "turing", "div", "grids", "motorik", "sequencer",
                  "polyrhythm", "markov", "two_tracks", "arpeggio",
                  "computer_grids", "turing_clouds", "turing_matrix",
                  "ca_sequencer", "tesserae", "gate_sequencer"}


def _parse_info_yaml_tags(yaml_path: Optional[str]) -> List[str]:
    """Extract tags from info.yaml if present."""
    if not yaml_path or not os.path.exists(yaml_path):
        return []
    tags = []
    in_tags = False
    try:
        with open(yaml_path, "r", encoding="utf-8") as f:
            for line in f:
                stripped = line.strip()
                if stripped.startswith("tags:"):
                    in_tags = True
                    continue
                if in_tags:
                    if stripped.startswith("- "):
                        tags.append(stripped[2:].strip().lower())
                    elif ":" in stripped:
                        break
    except Exception:
        pass
    return tags


def classify_card(card: CardInfo, info_yaml_path: Optional[str] = None) -> Dict[str, bool]:
    """Classify card behavior based on tags and ID heuristics."""
    tags = _parse_info_yaml_tags(info_yaml_path)
    cid = card.id.lower()

    is_lua = card.id in LUA_CARDS
    is_configurable = card.id in CONFIGURABLE_CARDS

    if is_lua or is_configurable:
        return {
            "is_oscillator": False,
            "is_effect": False,
            "is_midi": False,
            "is_clock": False,
            "is_lua": is_lua,
            "is_configurable": is_configurable,
        }

    is_oscillator = ("oscillator" in tags or "synth" in tags or
                     any(kw in cid for kw in OSC_KEYWORDS))
    is_effect = ("effect" in tags or "fx" in tags or
                 any(kw in cid for kw in EFFECT_KEYWORDS))
    is_midi = ("midi" in tags or "midi-host" in tags or
               any(kw in cid for kw in MIDI_KEYWORDS))
    is_clock = ("clock" in tags or "sequencer" in tags or
                any(kw in cid for kw in CLOCK_KEYWORDS))

    return {
        "is_oscillator": is_oscillator,
        "is_effect": is_effect,
        "is_midi": is_midi,
        "is_clock": is_clock,
        "is_lua": False,
        "is_configurable": False,
    }


# ─── Simple YAML Parser (no external deps) ───────────────────────────────────

def parse_simple_yaml(path: str) -> Dict[str, str]:
    """Lightweight YAML parser for info.yaml files.
    Handles basic Key: Value pairs. Returns flat dict of top-level keys."""
    result = {}
    if not os.path.exists(path):
        return result

    try:
        with open(path, "r", encoding="utf-8") as f:
            lines = f.readlines()
    except Exception:
        return result

    i = 0
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Skip empty lines and comments
        if not stripped or stripped.startswith("#"):
            i += 1
            continue

        # Check for key: value pattern (top-level only = no leading whitespace)
        if not line[0].isspace() and ":" in stripped:
            colon_pos = stripped.index(":")
            key = stripped[:colon_pos].strip()
            value = stripped[colon_pos + 1:].strip()

            if value == "|" or value == ">":
                # Multiline block — collect indented lines
                block_lines = []
                i += 1
                while i < len(lines):
                    if lines[i].strip() == "" or lines[i][0].isspace():
                        block_lines.append(lines[i].rstrip())
                        i += 1
                    else:
                        break
                result[key] = "\n".join(block_lines).strip()
                continue
            elif value:
                # Strip quotes
                if (value.startswith("'") and value.endswith("'")) or \
                   (value.startswith('"') and value.endswith('"')):
                    value = value[1:-1]
                result[key] = value

        i += 1

    return result


# ─── Flash Binary Helpers ─────────────────────────────────────────────────────

def find_flash_bin(card: CardInfo) -> Optional[str]:
    """Find the flash .bin file for a card, if one exists."""
    flash_path = os.path.join(PROJECT_DIR, f"flash_{card.id}.bin")
    if os.path.exists(flash_path):
        return flash_path
    return None

