"""Build exact author template tables and validate format-3 directories."""
from __future__ import annotations

import configparser
import hashlib
import json
import math
from pathlib import Path
import re
import struct

from .type_tree import require

HEADER = struct.Struct("<16sIIQQ32s")
MAGIC = b"EFF_AUTHOR_FILE3"
METADATA_STRUCTURE = "EFF_AUTHOR_RESOURCE_METADATA/3"
CONTROLS_METADATA_STRUCTURE = "EFF_AUTHOR_RESOURCE_METADATA/4"


class _MetadataWriter:
    def __init__(self):
        self.data = bytearray()

    def integer(self, value):
        require(isinstance(value, int) and 0 <= value <= 0xFFFFFFFFFFFFFFFF,
                "format-3 metadata integer is out of range")
        self.data.extend(value.to_bytes(8, "little"))

    def string(self, value):
        require(isinstance(value, str) and "\0" not in value,
                "format-3 metadata string is invalid")
        encoded = value.encode("utf-8", "strict")
        self.integer(len(encoded))
        self.data.extend(encoded)

    def finish(self):
        return bytes(self.data)


def encode_resource_file(kind, metadata, payload):
    """Encode one final Format 3 resource file, without a package intermediate."""
    require(isinstance(kind, int) and 1 <= kind <= 8,
            "format-3 resource kind is invalid")
    metadata, payload = bytes(metadata), bytes(payload)
    require(bool(metadata) or kind == 3,
            "format-3 resource metadata is required")
    require(not metadata or kind != 3,
            "Texture resource metadata belongs in mod.ini, not the scaffold")
    return HEADER.pack(MAGIC, 3, kind, len(metadata), len(payload),
                       hashlib.sha256(metadata).digest()) + metadata + payload


def encode_material_resource(material_id, resource_file, source_path,
                             source_asset, author_data):
    author_data = bytes(author_data)
    writer = _MetadataWriter()
    for value in ("EFF_AUTHOR_MATERIAL_EDITS/1", material_id, resource_file,
                  source_path, source_asset):
        writer.string(value)
    writer.integer(len(author_data))
    writer.string(hashlib.sha256(author_data).hexdigest().upper())
    return encode_resource_file(2, writer.finish(), author_data)


def encode_physics_resource(resource_id, resource_file, native_data,
                            author_data, targets):
    native_data, author_data = bytes(native_data), bytes(author_data)
    targets = sorted(set((str(path), str(asset)) for path, asset in targets))
    require(native_data and author_data and targets,
            "format-3 Physics resource requires native data, author data, and targets")
    writer = _MetadataWriter()
    writer.string(METADATA_STRUCTURE)
    writer.integer(4)
    writer.string(resource_id)
    writer.string(resource_file)
    writer.string(hashlib.sha256(native_data).hexdigest().upper())
    writer.integer(5)
    writer.integer(len(native_data))
    writer.integer(len(targets))
    for path, asset in targets:
        writer.string(path)
        writer.string(asset)
    writer.integer(len(author_data))
    writer.string(hashlib.sha256(author_data).hexdigest().upper())
    return encode_resource_file(4, writer.finish(), native_data + author_data)


def encode_effect_resource(resource_id, resource_file, native_data):
    native_data = bytes(native_data)
    require(native_data, "format-3 Effect resource is empty")
    writer = _MetadataWriter()
    writer.string(METADATA_STRUCTURE)
    writer.integer(5)
    writer.string(resource_id)
    writer.string(resource_file)
    writer.string(hashlib.sha256(native_data).hexdigest().upper())
    writer.integer(len(native_data))
    return encode_resource_file(5, writer.finish(), native_data)


def encode_effect_author_resource(resource_id, resource_file, author_data):
    author_data = bytes(author_data)
    require(author_data, "format-3 Effect author data is empty")
    writer = _MetadataWriter()
    writer.string(METADATA_STRUCTURE)
    writer.integer(7)
    writer.string(resource_id)
    writer.string(resource_file)
    writer.integer(len(author_data))
    writer.string(hashlib.sha256(author_data).hexdigest().upper())
    return encode_resource_file(7, writer.finish(), author_data)


def encode_skeleton_resource(resource_id, resource_file, skeleton_data):
    """Wrap a standalone author Skeleton payload for Format 3."""
    skeleton_data = bytes(skeleton_data)
    require(skeleton_data, "format-3 Skeleton data is empty")
    writer = _MetadataWriter()
    writer.string(METADATA_STRUCTURE)
    writer.integer(8)
    writer.string(resource_id)
    writer.string(resource_file)
    writer.integer(len(skeleton_data))
    writer.string(hashlib.sha256(skeleton_data).hexdigest().upper())
    return encode_resource_file(8, writer.finish(), skeleton_data)


class _ControlWriter(_MetadataWriter):
    def expression(self, node):
        if node is None:
            self.integer(0)
            return
        op, number, variable, left, right = node
        self.integer(1)
        self.integer(op)
        self.data.extend(struct.pack("<d", float(number)))
        self.string(variable)
        self.expression(left)
        self.expression(right)


_OPS = {"number": 0, "variable": 1, "not": 2, "negative": 3,
        "equal": 4, "not_equal": 5, "less": 6, "less_equal": 7,
        "greater": 8, "greater_equal": 9, "and": 10, "or": 11}


def _write_expr(writer, node):
    if node is None:
        writer.integer(0)
        return
    op, number, variable, left, right = node
    writer.integer(1)
    writer.integer(_OPS[op])
    writer.data.extend(struct.pack("<d", float(number)))
    writer.string(variable)
    _write_expr(writer, left)
    _write_expr(writer, right)


def _expression_for_bindings(bindings):
    expression = None
    for binding in bindings:
        variable = binding["variable"]
        choices = None
        for value in binding["visible"]:
            term = ("equal", 0.0, "", ("variable", 0.0, variable, None, None),
                    ("number", float(value), "", None, None))
            choices = term if choices is None else ("or", 0.0, "", choices, term)
        if choices is None:
            choices = ("number", 0.0, "", None, None)
        expression = choices if expression is None else ("and", 0.0, "", expression, choices)
    return expression or ("number", 1.0, "", None, None)


def _part_visibility(part):
    return part.get("visibilityExpression") or _expression_for_bindings(
        part["visibilityBindings"])


def _key_chord(value):
    names = {"INSERT": 0x2D, "DELETE": 0x2E, "HOME": 0x24, "END": 0x23,
             "PAGEUP": 0x21, "PAGEDOWN": 0x22, "LEFT": 0x25, "RIGHT": 0x27,
             "UP": 0x26, "DOWN": 0x28, "SPACE": 0x20, "ENTER": 0x0D,
             "ESC": 0x1B, "TAB": 0x09, "BACKSPACE": 0x08, "CAPSLOCK": 0x14,
             "TILDE": 0xC0, "NUMPADPLUS": 0x6B, "NUMPADMINUS": 0x6D,
             "NUMPADMULTIPLY": 0x6A, "NUMPADDIVIDE": 0x6F, "NUMPADDECIMAL": 0x6E}
    vk = modifiers = 0
    for token in value.split("+"):
        name = token.strip().upper()
        modifier = {"CTRL": 2, "SHIFT": 4, "ALT": 1}.get(name)
        if modifier is not None:
            require(not modifiers & modifier, "duplicate key modifier")
            modifiers |= modifier
        else:
            require(vk == 0, "key chord has multiple keys")
            if len(name) == 1 and name.isalnum():
                vk = ord(name)
            elif name.startswith("F") and name[1:].isdigit() and 1 <= int(name[1:]) <= 24:
                vk = 0x70 + int(name[1:]) - 1
            elif name.startswith("NUMPAD") and name[6:].isdigit() and int(name[6:]) <= 9:
                vk = 0x60 + int(name[6:])
            else:
                vk = names.get(name, 0)
    require(vk != 0, "invalid runtime key chord: " + value)
    return vk, modifiers


def _effect_runtime_part(effect, parts):
    source_part = effect.get("part", "")
    if not source_part:
        return ""
    declared = {_part_id(part["partId"]) for part in parts}
    if source_part in declared:
        return source_part
    runtime_part = _part_id(source_part)
    require(runtime_part in declared,
            "Format 3 Effect attachment Part is not declared: " + source_part)
    return runtime_part


def _decode_bone_animation_payload(payload, label):
    payload = bytes(payload)
    require(len(payload) >= 12 and payload[:8] == b"EFFBAN1\0",
            "invalid sampled Bone tracks: " + label)
    offset = 8
    version, = struct.unpack_from("<I", payload, offset); offset += 4
    require(version == 1, "unsupported sampled Bone tracks: " + label)

    def string16():
        nonlocal offset
        require(offset + 2 <= len(payload), "truncated sampled Bone track string: " + label)
        size, = struct.unpack_from("<H", payload, offset); offset += 2
        require(size and offset + size <= len(payload), "invalid sampled Bone track string: " + label)
        value = payload[offset:offset + size].decode("utf-8", "strict"); offset += size
        require("\0" not in value, "invalid sampled Bone track string: " + label)
        return value

    identity, skeleton = string16(), string16()
    require(offset + 13 <= len(payload), "truncated sampled Bone animation header: " + label)
    duration, sample_rate, loop, count = struct.unpack_from("<fIBI", payload, offset); offset += 13
    require(math.isfinite(duration) and duration > 0 and sample_rate > 0 and loop <= 1 and
            0 < count <= 16384, "invalid sampled Bone animation timing: " + label)
    tracks = []
    seen = set()
    for _ in range(count):
        path = string16()
        samples, = struct.unpack_from("<I", payload, offset) if offset + 4 <= len(payload) else (0,)
        require(offset + 4 <= len(payload), "truncated sampled Bone track: " + label)
        offset += 4
        require(_valid_bone_path(path) and path not in seen and 2 <= samples <= 1_000_000 and
                samples <= (len(payload) - offset) // 40,
                "invalid sampled Bone track extent or path: " + label + "/" + path)
        values = [struct.unpack_from("<10f", payload, offset + index * 40) for index in range(samples)]
        offset += samples * 40
        for sample in values:
            require(all(math.isfinite(value) for value in sample) and
                    abs(sum(value * value for value in sample[3:7]) - 1.0) <= .001 and
                    all(value > 0 for value in sample[7:]),
                    "invalid sampled Bone TRS: " + label + "/" + path)
        seen.add(path)
        tracks.append(dict(skeleton=skeleton, path=path, samples=values))
    require(offset == len(payload), "sampled Bone animation has trailing bytes: " + label)
    return dict(id=identity, skeleton=skeleton, duration=duration, sample_rate=sample_rate,
                loop=bool(loop), tracks=tracks)


def _controls_resource(mod_id, parts, switches, material_clips, effects,
                       state_defaults=None, persistent_states=None,
                       key_bindings=None, bone_animations=()):
    preserve_state = state_defaults is not None
    defaults = {key: float(value) for key, value in (state_defaults or {}).items()}
    persistent = set(persistent_states or ()) if preserve_state else set()
    controls, bindings = {}, []
    runtime_parts, animations, material_ids, material_tracks = {}, {}, set(), {}

    def merge_animation(identity, label, key, duration, speed, loop, autoplay,
                        sample_count=None, speed_variable=None, sample_rate=None):
        """Return one typed-track clip, enforcing one shared playback clock."""
        animation = animations.get(identity)
        if animation is None:
            variable = speed_variable or ("$anim_speed_" + _identifier(identity))
            default(variable, speed)
            animation = dict(id=identity, label=label, speedVariable=variable,
                key=key or "", duration=duration, speed=speed, loop=loop,
                autoplay=autoplay, tracks={}, boneTracks=[], sampleCount=sample_count,
                sampleRate=sample_rate)
            animations[identity] = animation
            if key:
                bindings.append(("KeyAnimation" + _identifier(identity), variable,
                    "press", *_key_chord(key), 1.0, label, [1.0]))
            return animation
        require(abs(float(animation["duration"]) - float(duration)) <= 1e-5 and
                animation["speed"] == speed and
                animation["loop"] == loop and animation["autoplay"] == autoplay and
                animation["key"] == (key or ""),
                "mixed Animation tracks must share playback settings: " + identity)
        if speed_variable:
            require(animation["speedVariable"] == speed_variable,
                    "mixed Animation tracks must share one playback speed control: " + identity)
        if sample_count is not None:
            require(animation["sampleCount"] in (None, sample_count),
                    "mixed Animation tracks must share one sample timeline: " + identity)
            animation["sampleCount"] = sample_count
        if sample_rate is not None:
            require(animation["sampleRate"] in (None, sample_rate),
                    "mixed Animation tracks must share one sample rate: " + identity)
            animation["sampleRate"] = sample_rate
        return animation

    def default(variable, value):
        value = float(value)
        require(math.isfinite(value), "invalid runtime default: " + variable)
        if variable in defaults:
            require(defaults[variable] == value,
                    "runtime default differs from preserved state: " + variable)
            return
        defaults[variable] = value
        persistent.add(variable)

    if key_bindings is None:
        for index, switch in enumerate(switches, 1):
            variable = switch["variable"]
            default(variable, switch["default"])
            values = switch.get("stateValues", list(range(switch["stateCount"])))
            bindings.append(("KeySwitch" + str(index), variable, "cycle",
                             *_key_chord(switch["key"]), 1.0, switch.get("label", ""),
                             list(map(float, values)) or [0.0, 1.0]))
    else:
        for binding in key_bindings:
            variable = binding["variable"]
            require(variable in defaults, "key binding state is undeclared: " + variable)
            bindings.append((binding["id"], variable, binding["type"],
                             *_key_chord(binding["key"]), float(binding["speed"]),
                             binding.get("label", ""), list(map(float, binding["values"]))))

    shape_bindings = {}
    shape_rows = []
    for part in parts:
        part_id = _part_id(part["partId"])
        shape_variables = {}
        for control in part.get("shapeControls", ()):
            if not control["enabled"]:
                continue
            token = re.sub(r"[^A-Za-z0-9_]", "_", part["object"])
            key = (token, control["identity"])
            variable = shape_bindings.get(key)
            if variable is None:
                variable = control.get("variable") or (
                    "$shape_" + token + "_" + _identifier(control["identity"]))
                shape_bindings[key] = variable
                default(variable, control["default"])
                controls[variable] = (control["label"], float(control["minimum"]),
                                      float(control["maximum"]))
                shape_rows.append((variable, control))
                for direction, chord, target, suffix in (
                    ("Increase", control["hotkey_increase"], control["maximum"], "Increase"),
                    ("Decrease", control["hotkey_decrease"], control["minimum"], "Decrease")):
                    if chord:
                        binding_key = "\0".join((token, control["identity"], variable))
                        binding_id = hashlib.sha256(binding_key.encode("utf-8")).hexdigest()[:16]
                        bindings.append(("KeyShape" + binding_id + suffix,
                                         variable, "hold", *_key_chord(chord),
                                         float(control["hotkey_speed"]), control["label"],
                                         [float(target)]))
            elif control.get("variable"):
                require(variable == control["variable"],
                        "shared shape identity maps to multiple state variables")
            shape_variables[control["shape"]] = variable
        runtime_parts[part_id] = dict(visible=_part_visibility(part),
                                      shapes=shape_variables)

    for part in parts:
        part_id = _part_id(part["partId"])
        for clip in part.get("shapeAnimations", ()):
            identity = clip["id"]
            variable = clip.get("speedVariable") or ("$anim_speed_" + _identifier(identity))
            sample_counts = {len(values) for values in clip["channels"].values()}
            require(len(sample_counts) <= 1,
                    "Animation tracks must share one sample timeline: " + identity)
            animation = merge_animation(identity, clip["label"], clip.get("key", ""),
                clip["duration"], clip["speed"], clip["loop"], clip["autoplay"],
                next(iter(sample_counts), None), variable, clip.get("sample_rate"))
            for shape, samples in clip["channels"].items():
                state = runtime_parts[part_id]["shapes"].get(shape)
                require(state is not None, "animated shape lacks a Format 3 control: " + shape)
                require(state not in animation["tracks"],
                        "duplicate mixed Animation target: " + identity + "/" + state)
                animation["tracks"][state] = samples

    for clip in material_clips:
        identity = clip["id"]
        speed = "$mat_speed_" + _identifier(identity)
        sample_counts = {len(target["samples"]) for target in clip["targets"]}
        require(len(sample_counts) <= 1,
                "Animation tracks must share one sample timeline: " + identity)
        animation = merge_animation(identity, clip["label"], clip.get("key", ""),
            clip["duration"], clip["speed"], clip["loop"], clip["autoplay"],
            next(iter(sample_counts), None), speed if identity not in animations else None,
            clip.get("sample_rate"))
        for target in clip["targets"]:
            variable = target["variable"]
            samples = target["samples"]
            require(samples, "material animation track has no samples")
            low, high = min(samples), max(samples)
            if low == high:
                high = low + max(1.0, abs(low) * 0.01)
            default(variable, samples[0])
            controls[variable] = (clip["label"] + " " + target["property"], low, high)
            require(variable not in animation["tracks"],
                    "duplicate mixed Animation target: " + identity + "/" + variable)
            animation["tracks"][variable] = samples
            material_tracks[variable] = dict(material=target["material"],
                property=target["property"], kind=target["kind"], component=target["component"])
        material_ids.add(identity)

    for source in bone_animations:
        decoded = _decode_bone_animation_payload(source["data"], source["id"])
        identity = _identifier(source["id"])
        require(decoded["id"] == identity and decoded["skeleton"] == _identifier(source["skeleton"]),
                "sampled Bone animation identity differs: " + identity)
        speed = float(source.get("speed", 1.0))
        key = source.get("key", "")
        autoplay = bool(source.get("autoplay", False))
        sample_count = len(decoded["tracks"][0]["samples"])
        animation = merge_animation(identity, source.get("label", identity), key,
            decoded["duration"], speed, decoded["loop"], autoplay,
            sample_count, source.get("speedVariable") if identity not in animations else None,
            decoded["sample_rate"])
        require(all(len(values) == len(decoded["tracks"][0]["samples"])
                    for values in animation["tracks"].values()) and
                all(len(track["samples"]) == len(decoded["tracks"][0]["samples"])
                    for track in animation["boneTracks"]),
                "mixed Animation tracks must share one sample timeline: " + identity)
        existing_bones = {(track["skeleton"], track["path"]) for track in animation["boneTracks"]}
        for track in decoded["tracks"]:
            key = (track["skeleton"], track["path"])
            require(key not in existing_bones, "duplicate mixed Animation Bone target: " + identity)
            existing_bones.add(key)
            animation["boneTracks"].append(track)
        sample_count = len(decoded["tracks"][0]["samples"])
        require(all(len(values) == sample_count for values in animation["tracks"].values()),
                "mixed Animation tracks must share one sample timeline: " + identity)

    effect_state = {}
    for effect in effects:
        prefab = effect.get("targetPrefab", "")
        renderer_path = effect.get("rendererPath", "")
        target_path = effect.get("targetPath", "")
        target_asset = effect.get("targetAsset", "")
        target_bones = effect.get("bones", [])
        require(prefab.startswith("assets/") and prefab.endswith(".prefab") and ".." not in prefab and
                not renderer_path.startswith("/") and "\\" not in renderer_path and ".." not in renderer_path and "//" not in renderer_path and
                target_path.startswith("assets/") and ".." not in target_path and target_asset,
                "Format 3 Effect requires exact Prefab/Renderer/Mesh source proof")
        require(not effect.get("bone") or target_bones,
                "Format 3 bone-mounted Effect requires a source bone palette")
        identity = effect["id"]
        variable = "$fx_" + _identifier(identity)
        default(variable, int(effect.get("autoplay", True)))
        controls[variable] = (effect["label"], 0.0, 1.0)
        keys = effect.get("keys") or ([effect["key"]] if effect.get("key") else [])
        for index, chord in enumerate(keys):
            bindings.append(("KeyRuntime" + _identifier(identity) + ("" if index == 0 else "_K" + str(index + 1)),
                variable, "cycle", *_key_chord(chord), 1.0, "", [0.0, 1.0]))
        native = effect["native"]
        runtime_part = _effect_runtime_part(effect, parts)
        row = dict(id=identity, label=effect["label"], part=runtime_part,
                   bone=effect.get("bone", ""), variable=variable,
                   autoplay=bool(effect.get("autoplay", True)),
                   position=effect["position"], rotation=effect["rotation"], scale=effect["scale"],
                   targetPrefab=effect.get("targetPrefab", ""),
                   rendererPath=effect.get("rendererPath", ""),
                   targetPath=effect.get("targetPath", ""), targetAsset=effect.get("targetAsset", ""),
                   targetBones=target_bones,
                   native=dict(key=native["key"], prefab=native["prefab"], rootName=native["rootName"],
                       root=native["root"], particles=native["particles"], bundles=[]))
        for index, bundle in enumerate(native["bundles"]):
            file = "effects/" + native["key"] + "/" + str(index) + ".effres"
            row["native"]["bundles"].append(dict(file=file, hash=bundle["hash"],
                                                  size=len(bundle["data"]),
                                                  dependencies=bundle["dependencies"]))
        effect_state[identity] = row

    writer = _ControlWriter()
    writer.string(CONTROLS_METADATA_STRUCTURE)
    writer.integer(6)
    writer.integer(1)
    writer.integer(len(defaults))
    for key in sorted(defaults):
        writer.string(key); writer.data.extend(struct.pack("<d", defaults[key]))
    writer.integer(len(persistent))
    for key in sorted(persistent): writer.string(key)
    writer.integer(len(controls))
    for key in sorted(controls):
        label, low, high = controls[key]
        writer.string(key); writer.string(label)
        writer.data.extend(struct.pack("<dd", low, high))
    writer.integer(len(bindings))
    for identity, variable, kind, vk, modifiers, speed, label, values in bindings:
        for value in (identity, variable, kind): writer.string(value)
        writer.integer(vk); writer.integer(modifiers)
        writer.data.extend(struct.pack("<d", speed)); writer.string(label)
        writer.integer(len(values))
        for value in values: writer.data.extend(struct.pack("<d", value))
    writer.integer(len(runtime_parts))
    for part in sorted(runtime_parts):
        state = runtime_parts[part]; writer.string(part); _write_expr(writer, state["visible"])
        writer.integer(len(state["shapes"]))
        for shape in sorted(state["shapes"]): writer.string(shape); writer.string(state["shapes"][shape])
    writer.integer(len(animations))
    for identity in sorted(animations):
        animation = animations[identity]
        writer.string(identity); writer.string(animation["label"]); writer.string(animation["speedVariable"])
        writer.integer(_key_chord(animation["key"])[0] if animation["key"] else 0)
        writer.integer(_key_chord(animation["key"])[1] if animation["key"] else 0)
        writer.data.extend(struct.pack("<dd", animation["duration"], animation["speed"]))
        writer.integer(int(animation["loop"])); writer.integer(int(animation["autoplay"]))
        writer.integer(len(animation["tracks"]))
        for variable in sorted(animation["tracks"]):
            writer.string(variable); values = animation["tracks"][variable]
            writer.integer(len(values))
            for value in values: writer.data.extend(struct.pack("<d", value))
        writer.integer(len(animation.get("boneTracks", ())))
        for track in animation.get("boneTracks", ()):
            writer.string(track["skeleton"]); writer.string(track["path"])
            writer.integer(len(track["samples"]))
            for sample in track["samples"]: writer.data.extend(struct.pack("<10f", *sample))
    writer.integer(len(material_ids))
    for identity in sorted(material_ids): writer.string(identity)
    writer.integer(len(material_tracks))
    for variable in sorted(material_tracks):
        track = material_tracks[variable]
        writer.string(variable); writer.string(track["material"]); writer.string(track["property"])
        writer.string(track["kind"]); writer.integer(track["component"])
    writer.integer(len(effect_state))
    for identity in sorted(effect_state):
        effect = effect_state[identity]
        for value in (effect["id"], effect["label"], effect["part"], effect["bone"], effect["variable"]): writer.string(value)
        writer.integer(int(effect["autoplay"]))
        for value in effect["position"] + effect["rotation"] + effect["scale"]:
            writer.data.extend(struct.pack("<d", value))
        for value in (effect["targetPrefab"], effect["rendererPath"], effect["targetPath"], effect["targetAsset"]): writer.string(value)
        writer.integer(len(effect["targetBones"]))
        for value in effect["targetBones"]: writer.string(value)
        native = effect["native"]
        for value in (native["key"], native["prefab"], native["rootName"]): writer.string(value)
        writer.integer(native["root"]); writer.integer(native["particles"]); writer.integer(len(native["bundles"]))
        for bundle in native["bundles"]:
            writer.string(bundle["file"]); writer.string(bundle["hash"]); writer.integer(bundle["size"])
            writer.integer(len(bundle["dependencies"]))
            for dependency in bundle["dependencies"]: writer.integer(dependency)
    return encode_resource_file(6, writer.finish(), b""), runtime_parts


def _identifier(value):
    value = str(value)
    require(re.fullmatch(r"[A-Za-z0-9_.-]+", value) is not None,
            "Format 3 identifier is invalid: " + value)
    return value


def _part_id(part_id):
    value = _identifier(part_id)
    return _identifier("Part" + value.removeprefix("Mesh"))


def part_resource_id(part_id):
    return _part_id(part_id)


def identifier(value):
    return _identifier(value)


def _dds_header(width, height, mips, format_id, linear):
    require(format_id in (4, 25, 27), "DDS Texture format is unsupported")
    header = bytearray(148)
    header[:4] = b"DDS "
    def put(offset, value):
        struct.pack_into("<I", header, offset, int(value))
    flags = 0x1007 | (8 if format_id == 4 else 0x80000) | (0x20000 if mips > 1 else 0)
    put(4, 124); put(8, flags); put(12, height); put(16, width)
    put(20, width * 4 if format_id == 4 else max(1, (width + 3) // 4) * 16)
    put(28, mips); put(76, 32); put(80, 4); put(84, int.from_bytes(b"DX10", "little"))
    put(108, 0x1000 | (0x400008 if mips > 1 else 0))
    put(128, 28 if format_id == 4 and linear else 29 if format_id == 4 else
         83 if format_id == 27 else 98 if linear else 99)
    put(132, 3); put(140, 1)
    return bytes(header)


def _write_ini(root, ini):
    import io
    output = io.StringIO()
    ini.write(output, space_around_delimiters=False)
    (Path(root) / "mod.ini").write_text(output.getvalue(), encoding="utf-8", newline="\n")


def write_format3_directory(root, mod_id, parts, materials, textures, rules,
                            switches, material_clips, effects, physics=(),
                            author_files=None, state_defaults=None,
                            persistent_states=None, key_bindings=None,
                            key_switch_enabled=True, target_bundles=None,
                            skeletons=(), bone_animations=()):
    """Write one final Format 3 author directory from typed Blender records."""
    import hashlib
    import os
    import re
    root = Path(root).resolve()
    require(root.is_dir(), "Format 3 output directory must exist")
    mod_id = _identifier(mod_id)
    author_files = dict(author_files or {})
    target_bundles = dict(target_bundles or {})
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), strict=True)
    ini.optionxform = str
    ini["Mod"] = {"format": "3", "id": mod_id,
                   "key_switch_enabled": "1" if key_switch_enabled else "0"}
    ini["Constants"] = {}
    texture_ids = {}
    for texture in textures:
        identity = _identifier(texture["id"])
        path, dds = "textures/" + identity + ".effres", "textures/" + identity + ".dds"
        texture_ids[identity] = identity
        values = {"path": path, "dds": dds}
        for key, value, default in (("linear", texture["colorSpace"] == 0, False),
                                    ("filter", texture["filter"], 1), ("wrap", texture["wrap"], 0),
                                    ("aniso", texture["aniso"], 1), ("mip_bias", texture["mipBias"], 0.0)):
            if value != default: values[key] = str(value).lower() if isinstance(value, bool) else str(value)
        ini[identity] = values
        relative = (root / dds).resolve()
        require(root in relative.parents, "Format 3 DDS path escapes Mod")
        relative.parent.mkdir(parents=True, exist_ok=True)
        source_path = texture.get("ddsSourcePath")
        if source_path:
            source = Path(source_path)
            data = source.read_bytes()
            require(hashlib.sha256(data).hexdigest().upper() == texture.get("ddsHash"),
                    "DDS input changed during export: " + str(source))
        else:
            data = _dds_header(texture["width"], texture["height"], texture["mipCount"],
                               texture["formatId"], texture["colorSpace"] == 0) + bytes(texture["pixels"])
        relative.write_bytes(data)

    material_ids = {}
    for material in materials.values():
        identity = _identifier(material["id"]); file = "materials/" + identity + ".effres"
        material_ids[identity] = identity
        bindings = {row["property"]: row["texture"] for row in material["textureEdits"]}
        ini[identity] = {"path": file, "source.path": material["sourcePath"],
                         "source.asset": material["sourceAsset"]}
        for prop, texture in bindings.items():
            require(texture in texture_ids, "Format 3 Material references missing Texture: " + texture)
            ini[identity]["texture." + prop] = texture
        data = bytes(material["authorData"])
        (root / file).parent.mkdir(parents=True, exist_ok=True)
        (root / file).write_bytes(encode_material_resource(identity, file,
            material["sourcePath"], material["sourceAsset"], data))

    ids_by_part = {}
    shape_variables = {}
    for part in parts:
        section = _part_id(part["partId"])
        ids_by_part[part["partId"]] = section
        shape_variables[section] = {}
        if part.get("shapeBindings") is not None:
            shape_variables[section].update(part["shapeBindings"])
            continue
        for control in part.get("shapeControls", ()):
            if control["enabled"]:
                token = re.sub(r"[^A-Za-z0-9_]", "_", part["object"])
                shape_variables[section][control["shape"]] = control.get("variable") or (
                    "$shape_" + token + "_" + _identifier(control["identity"]))
    for part in parts:
        section = ids_by_part[part["partId"]]
        mesh_id = _identifier("Mesh_" + section)
        mesh_file = "meshes/" + section + ".effres"
        mesh_values = {"path": mesh_file}
        skeleton_id = part.get("skeleton", "")
        if skeleton_id:
            mesh_values["skeleton"] = _identifier(skeleton_id)
        ini[mesh_id] = mesh_values
        visible = _part_visibility(part)
        # Format 3 retains the same small expression grammar in mod.ini.
        def show(node):
            op, number, variable, left, right = node
            if op == "number": return "true" if number else "false"
            if op == "variable": return variable
            if op == "not": return "!(" + show(left) + ")"
            if op == "negative": return "-" + show(left)
            operators = {"equal": "==", "not_equal": "!=", "less": "<",
                         "less_equal": "<=", "greater": ">",
                         "greater_equal": ">=", "or": "||", "and": "&&"}
            if op in operators:
                rendered = show(left) + " " + operators[op] + " " + show(right)
                return "(" + rendered + ")" if op in ("or", "and") else rendered
            raise ValueError("unsupported visibility expression")
        material_id = part.get("material", "")
        values = {"mesh": mesh_id, "visible": show(visible)}
        material_slot = part.get("materialSlot", 0)
        require(isinstance(material_slot, int) and not isinstance(material_slot, bool) and
                0 <= material_slot <= 0xFFFFFFFF,
                "Format 3 Part source Material slot is invalid: " + section)
        values["material_slot"] = str(material_slot)
        if material_id:
            material_id = _identifier(material_id)
            require(material_id in material_ids,
                    "Format 3 Part Material is not declared: " + section)
            values["material"] = material_id
        for shape, variable in shape_variables[section].items(): values["shape." + shape] = variable
        ini[section] = values

    rule_base_ids = {}
    base_counts = {}
    target_identities = set()
    for rule in rules:
        base = min(rule["targets"], key=lambda row: row["level"])
        section = _identifier("Replace" + base["sourceAsset"])
        rule_base_ids[id(rule)] = (section, base)
        base_counts[section] = base_counts.get(section, 0) + 1
        for target in rule["targets"]:
            identity = (target["sourcePath"], target["sourceAsset"])
            require(identity not in target_identities,
                    "Format 3 Replace target is duplicated: " + repr(identity))
            target_identities.add(identity)
    rule_sections = {}
    used_sections = set(ini)
    for rule in rules:
        preferred, base = rule_base_ids[id(rule)]
        section = preferred
        if base_counts[preferred] > 1 or section in used_sections:
            seed = (base["sourcePath"] + "\n" + base["sourceAsset"]).encode("utf-8")
            section = preferred + "_" + hashlib.sha256(seed).hexdigest()[:12].upper()
        suffix = 2
        candidate = section
        while candidate in used_sections:
            candidate = section + "_" + str(suffix)
            suffix += 1
        section = candidate
        used_sections.add(section)
        rule_sections[id(rule)] = section

    for rule in rules:
        base = min(rule["targets"], key=lambda row: row["level"])
        section = rule_sections[id(rule)]
        values = {"target.path": base["sourcePath"], "target.asset": base["sourceAsset"]}
        bundle = target_bundles.get((base["sourcePath"], base["sourceAsset"]))
        if bundle: values["target.bundle"] = bundle
        if rule["hidden"]: values["handling"] = "skip"
        else: values["parts"] = ",".join(ids_by_part[row] for row in rule["parts"])
        levels = set()
        for target in rule["targets"]:
            level = target["level"]
            require(level not in levels, "duplicate Replace LOD target")
            levels.add(level)
            if level:
                values["lod." + str(level) + ".path"] = target["sourcePath"]
                values["lod." + str(level) + ".asset"] = target["sourceAsset"]
                bundle = target_bundles.get((target["sourcePath"], target["sourceAsset"]))
                if bundle: values["lod." + str(level) + ".bundle"] = bundle
        require(0 in levels, "Format 3 Replace target lacks LOD0")
        ini[section] = values

    skeleton_ids = set()
    for skeleton in skeletons:
        resource_id = _identifier(skeleton["id"])
        require(resource_id not in skeleton_ids, "duplicate Format 3 Skeleton: " + resource_id)
        section_id = "Skeleton" + resource_id
        require(section_id not in ini, "Format 3 Skeleton section collides: " + section_id)
        skeleton_ids.add(resource_id)
        relative = "skeletons/" + resource_id + ".effres"
        payload = bytes(skeleton["data"])
        (root / relative).parent.mkdir(parents=True, exist_ok=True)
        (root / relative).write_bytes(encode_skeleton_resource(resource_id, relative, payload))
        ini[section_id] = {"type": "skeleton", "path": relative}

    referenced_skeletons = {
        values["skeleton"] for section, values in ini.items()
        if section.startswith("Mesh") and values.get("skeleton")
    }
    require(referenced_skeletons <= skeleton_ids,
            "Format 3 Mesh references an undeclared Skeleton: " +
            repr(sorted(referenced_skeletons - skeleton_ids)))
    require({ _identifier(animation["skeleton"]) for animation in bone_animations } <= referenced_skeletons,
            "Format 3 Animation Skeleton is not used by any Mesh")

    animation_ids = set()
    for animation in bone_animations:
        resource_id = _identifier(animation["id"])
        skeleton_id = _identifier(animation["skeleton"])
        contribution = (resource_id, skeleton_id)
        require(contribution not in animation_ids,
                "duplicate Format 3 Animation Bone contribution: " + resource_id + "/" + skeleton_id)
        require(skeleton_id in skeleton_ids,
                "Format 3 Animation references an undeclared Skeleton: " + skeleton_id)
        animation_ids.add(contribution)

    for resource in physics:
        resource_id = _identifier(resource["id"]); relative = "physics/" + resource_id + ".effres"
        author = bytes(resource["author"]); native = bytes(resource["native"])
        (root / relative).parent.mkdir(parents=True, exist_ok=True)
        (root / relative).write_bytes(encode_physics_resource(resource_id, relative, native,
            author, resource["targets"]))
        target_rules = set()
        wanted_targets = {tuple(target) for target in resource["targets"]}
        for rule in rules:
            if any((target["sourcePath"], target["sourceAsset"]) in wanted_targets
                   for target in rule["targets"]):
                target_rules.add(rule_sections[id(rule)])
        require(target_rules, "Format 3 Physics has no declared Mesh targets: " + resource_id)
        ini[resource_id] = {"type": "physics", "path": relative,
                            "targets": ",".join(sorted(target_rules))}

    effect_resources, effect_authors, effect_rules = {}, {}, {}
    for effect in effects:
        source = effect["native"]
        for index, bundle in enumerate(source["bundles"]):
            relative = "effects/" + source["key"] + "/" + str(index) + ".effres"
            file_id = "ResourceEffect_" + hashlib.sha256(relative.encode("utf-8")).digest()[:8].hex().upper()
            if relative not in effect_resources:
                data = bytes(bundle["data"]); digest = hashlib.sha256(data).hexdigest().upper()
                (root / relative).parent.mkdir(parents=True, exist_ok=True)
                (root / relative).write_bytes(encode_effect_resource(file_id, relative, data))
                effect_resources[relative] = file_id
            if effect.get("part"):
                for rule in rules:
                    if effect["part"] in rule["parts"]:
                        effect_rules.setdefault(relative, set()).add(rule_sections[id(rule)])
            else:
                for rule in rules:
                    if any((target["sourcePath"], target["sourceAsset"]) ==
                           (effect.get("targetPath"), effect.get("targetAsset")) for target in rule["targets"]):
                        effect_rules.setdefault(relative, set()).add(rule_sections[id(rule)])
        author_name = _identifier(effect["id"]) + ".effeffect"
        require(author_name in author_files, "Format 3 Effect author snapshot is absent: " + author_name)

    control_file = "controls.effres"
    control_data, _runtime_parts = _controls_resource(
        mod_id, parts, switches, material_clips, effects,
        state_defaults=state_defaults, persistent_states=persistent_states,
        key_bindings=key_bindings, bone_animations=bone_animations)
    (root / control_file).write_bytes(control_data)
    for relative, identity in effect_resources.items():
        values = {"type": "effect", "path": relative}
        targets = sorted(effect_rules.get(relative, ()))
        if targets: values["targets"] = ",".join(targets)
        ini[identity] = values
    for effect in effects:
        payload = bytes(author_files[_identifier(effect["id"]) + ".effeffect"])
        digest = hashlib.sha256(payload).hexdigest().upper()
        identity = "ResourceEffectAuthor_" + digest[:16]
        relative = "effects/author/" + digest + ".effres"
        if identity not in effect_authors:
            (root / relative).parent.mkdir(parents=True, exist_ok=True)
            (root / relative).write_bytes(encode_effect_author_resource(identity, relative, payload))
            effect_authors[identity] = relative
            ini[identity] = {"type": "effect-author", "path": relative}
        ini["EffectAttachment" + _identifier(effect["id"])] = {
            "path": control_file, "author": identity}
    for index, clip in enumerate(material_clips):
        ini["MaterialAnimation" + _identifier(clip["id"])] = {"path": control_file}
    for identity, animation in _controls_animation_ids(parts, material_clips,
                                                        bone_animations).items():
        if identity not in {clip["id"] for clip in material_clips}:
            ini["ShapeAnimation" + _identifier(identity)] = {"path": control_file}
    ini["ResourceControls"] = {"type": "controls", "path": control_file}
    _write_ini(root, ini)
    return dict(format=3, modId=mod_id, _ini=ini)


def _controls_animation_ids(parts, material_clips, bone_animations=()):
    result = {clip["id"]: clip for part in parts for clip in part.get("shapeAnimations", ())}
    result.update({clip["id"]: clip for clip in material_clips})
    result.update({clip["id"]: clip for clip in bone_animations})
    return result
def _digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest().upper()


def _template(source, kind, path, asset, resource, verified_snapshots):
    package = Path(source.package).resolve()
    files = source.rows
    require((resource.get("type"), resource.get("logicalPath"), resource.get("name")) ==
            (kind, path, asset), "selected EFFSOURCE template identity differs")
    native = resource["native"]
    file = files.get(native["cab"])
    require(file is not None, "EFFSOURCE native CAB is absent: " + native["cab"])
    snapshot = (package / file["snapshot"]["file"]).resolve()
    require(package in snapshot.parents and snapshot.is_file(), "EFFSOURCE snapshot path is invalid")
    key = snapshot, file["snapshot"]["sha256"]
    if key not in verified_snapshots:
        require(_digest(snapshot) == key[1], "EFFSOURCE snapshot checksum differs: " + native["cab"])
        verified_snapshots.add(key)
    return [kind, path, asset, native["cab"], str(native["pathId"]), str(snapshot), file["snapshot"]["sha256"]], file


def resolve_target_bundles(sources, rules):
    """Route exact Mesh identities, including dependency-only static meshes."""
    wanted = {(target["sourcePath"], target["sourceAsset"])
              for rule in rules for target in rule["targets"]}
    result = {}
    for source in sources:
        for resource in source.manifest["resources"]:
            identity = resource.get("logicalPath"), resource.get("name")
            if resource.get("type") != "Mesh" or identity not in wanted:
                continue
            bundle = str(resource.get("sourceBundle", "")).replace("\\", "/").lower()
            require(bundle.startswith("bundles/windows/") and bundle.endswith(".ab") and
                    all(part not in (".", "..") for part in bundle.split("/")),
                    "Mesh source lacks a valid logical Bundle route: " + repr(identity))
            prior = result.setdefault(identity, bundle)
            require(prior == bundle, "conflicting logical Mesh Bundle routes: " + repr(identity))
    return result


def resolve_templates(sources, rules, *, include_texture=False, additional=()):
    """Resolve templates for the explicitly selected Format 3 Mesh targets."""
    packages = {Path(source.package).resolve(): source for source in sources}
    packages = sorted(packages.items(), key=lambda row: str(row[0]).casefold())
    wanted = {
        ("Mesh", target["sourcePath"], target["sourceAsset"])
        for rule in rules if not rule["hidden"]
        for target in rule["targets"]
    }
    wanted.update(tuple(row) for row in additional)

    texture_template = None
    rows_by_identity = {}
    verified_snapshots = set()
    for _package, source in packages:
        wanted_rows = {}
        for identity in wanted:
            matches = source.find_all(*identity)
            require(len(matches) <= 1,
                    "duplicate exact EFFSOURCE template: " + repr(identity))
            if matches:
                wanted_rows[identity] = matches[0]
        if include_texture and texture_template is None:
            resource = source.first_of_type("Texture2D")
            if resource is not None:
                texture_template, _ = _template(
                    source, "Texture2D", resource["logicalPath"], resource["name"],
                    resource, verified_snapshots)
        for identity, resource in wanted_rows.items():
            kind, path, asset = identity
            if resource is None:
                continue
            row, _ = _template(source, kind, path, asset, resource,
                               verified_snapshots)
            identity = kind, path, asset
            prior = rows_by_identity.get(identity)
            if prior is None:
                rows_by_identity[identity] = row
            else:
                same_native_snapshot = prior[:5] == row[:5] and prior[6] == row[6]
                require(same_native_snapshot,
                        f"ambiguous exact {kind} template across selected source packages: {path} / {asset}")

    missing = wanted - rows_by_identity.keys()
    require(not missing, "selected EFFSOURCE packages lack exact author templates: " + repr(sorted(missing)))
    rows = [rows_by_identity[key] for key in sorted(rows_by_identity)]
    if include_texture:
        require(texture_template is not None, "Texture2D author template is absent from EFFSOURCE")
        rows.append(texture_template)
    return [dict(kind=row[0], path=row[1], asset=row[2], cab=row[3],
                 pathId=int(row[4]), file=row[5], sha256=row[6]) for row in rows]


class _MetadataReader:
    def __init__(self, data):
        self.data = data
        self.offset = 0

    def integer(self):
        require(self.offset + 8 <= len(self.data), "format-3 metadata is truncated")
        value = int.from_bytes(self.data[self.offset:self.offset + 8], "little")
        self.offset += 8
        return value

    def string(self):
        size = self.integer()
        require(size <= len(self.data) - self.offset, "format-3 metadata string extent differs")
        value = self.data[self.offset:self.offset + size].decode("utf-8", "strict")
        self.offset += size
        return value

    def end(self):
        require(self.offset == len(self.data), "format-3 metadata has trailing bytes")


class _AuthorPayloadReader:
    def __init__(self, data, label):
        self.data = bytes(data)
        self.offset = 0
        self.label = label

    def take(self, size):
        require(0 <= size <= len(self.data) - self.offset,
                self.label + " payload is truncated")
        value = self.data[self.offset:self.offset + size]
        self.offset += size
        return value

    def u8(self):
        return self.take(1)[0]

    def u16(self):
        return struct.unpack("<H", self.take(2))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def f32(self):
        value = struct.unpack("<f", self.take(4))[0]
        require(math.isfinite(value), self.label + " contains a non-finite value")
        return value

    def string16(self):
        size = self.u16()
        value = self.take(size).decode("utf-8", "strict")
        require(value and "\0" not in value, self.label + " contains an empty or invalid string")
        return value

    def end(self):
        require(self.offset == len(self.data), self.label + " payload has trailing bytes")


def _valid_bone_path(path):
    return (bool(path) and not path.startswith("/") and not path.endswith("/") and
            "\\" not in path and "//" not in path and
            all(part not in ("", ".", "..") for part in path.split("/")))


def _decode_author_skeleton(data, label):
    reader = _AuthorPayloadReader(data, label)
    require(reader.take(8) == b"EFFSKA1\0", label + " signature differs")
    require(reader.u32() == 1, label + " version differs")
    count = reader.u32()
    require(0 < count <= 16384, label + " node count is invalid")
    require(reader.string16() == "unity-y-up-left-handed",
            label + " coordinate space differs")
    nodes = []
    paths = set()
    for index in range(count):
        path = reader.string16()
        parent = reader.i32()
        position = tuple(reader.f32() for _ in range(3))
        rotation = tuple(reader.f32() for _ in range(4))
        scale = tuple(reader.f32() for _ in range(3))
        require(_valid_bone_path(path) and path not in paths,
                label + " path is invalid or duplicated: " + path)
        paths.add(path)
        if index == 0:
            require(parent == -1 and "/" not in path,
                    label + " root node is invalid")
        else:
            require(0 <= parent < index and path.rpartition("/")[0] == nodes[parent][0],
                    label + " node path/parent differs: " + path)
        require(abs(sum(value * value for value in rotation) - 1.0) <= .001 and
                all(value > 0 for value in scale),
                label + " node transform is invalid: " + path)
        nodes.append((path, parent, position, rotation, scale))
    reader.end()
    return {node[0] for node in nodes}


def _decode_author_bone_animation(data, label, skeleton_paths):
    reader = _AuthorPayloadReader(data, label)
    require(reader.take(8) == b"EFFBAN1\0", label + " signature differs")
    require(reader.u32() == 1, label + " version differs")
    identity, skeleton = reader.string16(), reader.string16()
    duration, sample_rate, loop, count = reader.f32(), reader.u32(), reader.u8(), reader.u32()
    require(duration > 0 and sample_rate > 0 and loop <= 1 and 0 < count <= 16384,
            label + " timing or track count is invalid")
    tracks = set()
    for _ in range(count):
        path, sample_count = reader.string16(), reader.u32()
        require(_valid_bone_path(path) and path in skeleton_paths and path not in tracks,
                label + " track references an unknown or duplicate bone: " + path)
        require(2 <= sample_count <= 1000000,
                label + " sample count is invalid: " + path)
        for _ in range(sample_count):
            sample = tuple(reader.f32() for _ in range(10))
            require(abs(sum(value * value for value in sample[3:7]) - 1.0) <= .001 and
                    all(value > 0 for value in sample[7:]),
                    label + " TRS sample is invalid: " + path)
        tracks.add(path)
    reader.end()
    return identity, skeleton


def _read_author_resource(root, relative, expected_kind, payload_bytes, label):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        require(len(header) == HEADER.size, label + " header is truncated")
        magic, version, kind, metadata_bytes, declared_payload, digest = HEADER.unpack(header)
        require(magic == MAGIC and version == 3 and kind == expected_kind and
                declared_payload == payload_bytes,
                label + " envelope differs")
        metadata = stream.read(metadata_bytes)
        payload = stream.read(payload_bytes)
    require(len(metadata) == metadata_bytes and hashlib.sha256(metadata).digest() == digest and
            len(payload) == payload_bytes,
            label + " metadata or payload is truncated")
    return _MetadataReader(metadata), payload


def _validate_skeleton_resource(root, relative, section, payload_bytes):
    metadata, payload = _read_author_resource(root, relative, 8, payload_bytes, section)
    require((metadata.string(), metadata.integer()) == (METADATA_STRUCTURE, 8),
            "format-3 Skeleton metadata structure differs: " + relative)
    resource_id, resource_file = metadata.string(), metadata.string()
    author_bytes, author_hash = metadata.integer(), metadata.string()
    metadata.end()
    require(section == "Skeleton" + resource_id and resource_file == relative and
            author_bytes == payload_bytes and len(author_hash) == 64 and
            author_hash.upper() == author_hash and
            hashlib.sha256(payload).hexdigest().upper() == author_hash,
            "format-3 Skeleton identity, extent, or checksum differs: " + section)
    return _decode_author_skeleton(payload, section)


def _effmat_values(data, label):
    try:
        text = data.decode("utf-8-sig", "strict")
    except UnicodeDecodeError as error:
        raise ValueError("Material author snapshot is not UTF-8: " + label) from error
    values = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith((";", "#")):
            continue
        require("=" in line, "Material author snapshot line is invalid: " + label)
        key, value = line.split("=", 1)
        key, value = key.strip(), value.strip()
        require(key and key not in values, "Material author snapshot key is empty or duplicated: " + label)
        values[key] = value
    require(values.get("format") == "EFFMAT" and values.get("version") == "1" and
            values.get("overrides", "").lower() == "true" and values.get("source", "").strip(),
            "Material author snapshot header differs: " + label)
    return values


def is_logical_resource_path(path):
    if (not isinstance(path, str) or "\0" in path or "\\" in path or ":" in path or
            not path.lower().startswith(("assets/", "packages/"))):
        return False
    return all(part not in ("", ".", "..") for part in path.split("/"))


def _validate_material_resource(root, relative, section, ini, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require(reader.string() == "EFF_AUTHOR_MATERIAL_EDITS/1",
                "format-3 Material metadata structure differs: " + relative)
        material_id, resource_file, source_path, source_asset = (
            reader.string() for _ in range(4))
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(material_id == section and resource_file and is_logical_resource_path(source_path) and
                source_path == ini[section].get("source.path") and
                source_asset == ini[section].get("source.asset") and
                len(author_hash) == 64 and author_hash.upper() == author_hash and
                author_bytes > 0 and author_bytes == payload_bytes,
                "format-3 Material identity/extents differ from mod.ini: " + section)
        author_data = stream.read(author_bytes)
    require(len(author_data) == author_bytes and hashlib.sha256(author_data).hexdigest().upper() == author_hash,
            "format-3 Material edit checksum differs: " + section)
    values = _effmat_values(author_data, section)
    require(values["source"] == source_path, "format-3 Material author source differs: " + section)
    snapshot_textures = {key: value for key, value in values.items() if key.startswith("texture.")}
    manifest_textures = {key: value for key, value in ini[section].items() if key.startswith("texture.")}
    require(snapshot_textures == manifest_textures,
            "format-3 Material author texture bindings differ from mod.ini: " + section)
    declared_textures = {name for name in ini.sections() if name.startswith("Texture")}
    require(all(value in declared_textures for value in snapshot_textures.values()),
            "format-3 Material author references an undeclared Texture: " + section)


def _validate_physics_resource(root, relative, section, ini, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require((reader.string(), reader.integer()) == (METADATA_STRUCTURE, 4),
                "format-3 Physics metadata structure differs: " + relative)
        resource_id, resource_file, native_hash = (reader.string() for _ in range(3))
        native_kind, native_bytes = reader.integer(), reader.integer()
        target_count = reader.integer()
        require(target_count <= (len(metadata) - reader.offset) // 16,
                "format-3 Physics target metadata is truncated: " + relative)
        targets = {(reader.string(), reader.string()) for _ in range(target_count)}
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(resource_id == section and resource_file and native_kind == 5 and
                len(native_hash) == len(author_hash) == 64 and
                native_hash.upper() == native_hash and author_hash.upper() == author_hash and
                native_bytes > 0 and author_bytes > 0 and native_bytes + author_bytes == payload_bytes and
                targets,
                "format-3 Physics identity/extents differ from mod.ini: " + section)
        native_digest = hashlib.sha256()
        remaining = native_bytes
        while remaining:
            block = stream.read(min(1024 * 1024, remaining))
            require(block, "format-3 Physics native payload is truncated: " + section)
            native_digest.update(block)
            remaining -= len(block)
        author = stream.read(author_bytes)
    require(native_digest.hexdigest().upper() == native_hash and len(author) == author_bytes and
            hashlib.sha256(author).hexdigest().upper() == author_hash,
            "format-3 Physics native/author checksum differs: " + section)
    require(len(author) >= 28 and author[:8] == b"EFFPHS3\0",
            "format-3 Physics author snapshot header differs: " + section)
    version, document_bytes, skeleton_bytes = struct.unpack_from("<IQQ", author, 8)
    require(version == 1 and document_bytes > 0 and skeleton_bytes > 0 and
            28 + document_bytes + skeleton_bytes == len(author) and
            author[28:36] == b"EFFPHYS\0" and
            author[28 + document_bytes:36 + document_bytes] == b"EFFSKEL\0",
            "format-3 Physics author snapshot dependencies/extents differ: " + section)
    rule_ids = {item.strip() for item in ini[section].get("targets", "").split(",") if item.strip()}
    declared_targets = set()
    for rule in rule_ids:
        if rule not in ini:
            continue
        values = ini[rule]
        declared_targets.add((values.get("target.path", ""), values.get("target.asset", "")))
        lod_levels = set()
        for key in values:
            if not key.startswith("lod."):
                continue
            match = re.fullmatch(r"lod\.([1-9][0-9]*)\.(path|asset|bundle)", key)
            require(match, "format-3 Physics rule has an invalid LOD target: " + rule)
            lod_levels.add(match.group(1))
        for level in lod_levels:
            path_key, asset_key = "lod." + level + ".path", "lod." + level + ".asset"
            require(path_key in values and asset_key in values,
                    "format-3 Physics rule has an incomplete LOD target: " + rule)
            declared_targets.add((values[path_key], values[asset_key]))
    require(targets == declared_targets,
            "format-3 Physics Mesh targets differ from mod.ini: " + section)


def _parse_dds(path):
    size = path.stat().st_size
    with path.open("rb") as stream:
        header = stream.read(148)
    require(len(header) >= 128 and size >= 128 and header[:4] == b"DDS " and
            struct.unpack_from("<I", header, 4)[0] == 124 and
            struct.unpack_from("<I", header, 76)[0] == 32,
            "DDS header is invalid: " + str(path))
    flags = struct.unpack_from("<I", header, 8)[0]
    require(flags & 0x1007 == 0x1007 and
            struct.unpack_from("<I", header, 112)[0] == 0 and
            struct.unpack_from("<I", header, 24)[0] == 0,
            "DDS must be one ordinary 2D texture: " + str(path))
    height, width = struct.unpack_from("<II", header, 12)
    mips = struct.unpack_from("<I", header, 28)[0] or 1
    pixel_flags, fourcc = struct.unpack_from("<II", header, 80)
    offset = 128
    fmt = 0
    if pixel_flags & 4 and fourcc == int.from_bytes(b"DX10", "little"):
        require(len(header) >= 148 and size >= 148,
                "DDS DX10 array/cube/dimension/alpha mode is invalid: " + str(path))
        dimension, misc_flag, array_size, alpha_mode = struct.unpack_from("<4I", header, 132)
        require((dimension, misc_flag, array_size) == (3, 0, 1) and alpha_mode <= 4,
                "DDS DX10 array/cube/dimension/alpha mode is invalid: " + str(path))
        dxgi = struct.unpack_from("<I", header, 128)[0]
        fmt = 4 if dxgi in (28, 29) else 27 if dxgi == 83 else 25 if dxgi in (98, 99) else 0
        offset = 148
    elif pixel_flags & 4 and fourcc in (int.from_bytes(b"ATI2", "little"), int.from_bytes(b"BC5U", "little")):
        fmt = 27
    elif pixel_flags & 0x41 == 0x41 and struct.unpack_from("<5I", header, 88) == (
            32, 0xff, 0xff00, 0xff0000, 0xff000000):
        fmt = 4
    require(fmt in (4, 25, 27), "DDS format is unsupported: " + str(path))
    require(0 < width <= 16384 and 0 < height <= 16384,
            "DDS dimensions are invalid: " + str(path))
    max_mips = max(width, height).bit_length()
    require(0 < mips <= max_mips, "DDS mip count is invalid: " + str(path))
    total = 0
    mip_width, mip_height = width, height
    for _ in range(mips):
        total += (mip_width * mip_height * 4 if fmt == 4 else
                  max(1, (mip_width + 3) // 4) * max(1, (mip_height + 3) // 4) * 16)
        mip_width, mip_height = max(1, mip_width // 2), max(1, mip_height // 2)
    require(offset + total == size, "DDS mip data extent differs: " + str(path))
    return width, height, mips, fmt, offset, total


def _validate_effres(path, expected_kind):
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        require(len(header) == HEADER.size, "format-3 resource header is truncated: " + str(path))
        magic, version, kind, metadata_bytes, payload_bytes, digest = HEADER.unpack(header)
        require(magic == MAGIC and version == 3 and kind == expected_kind and
                (metadata_bytes > 0 or expected_kind == 3),
                "format-3 resource envelope differs: " + str(path))
        require(HEADER.size + metadata_bytes + payload_bytes == path.stat().st_size,
                "format-3 resource extent differs: " + str(path))
        metadata = stream.read(metadata_bytes)
    require(len(metadata) == metadata_bytes and hashlib.sha256(metadata).digest() == digest,
            "format-3 resource metadata checksum differs: " + str(path))
    return metadata, payload_bytes


def read_format3_directory(root):
    """Return publication metadata after checking relationships, DDS and envelopes."""
    root = Path(root).resolve()
    ini = configparser.ConfigParser(interpolation=None, delimiters=("=",), strict=True)
    ini.optionxform = str
    with (root / "mod.ini").open(encoding="utf-8-sig") as stream:
        ini.read_file(stream)
    require(ini.get("Mod", "format", fallback="") == "3", "format-3 manifest is absent")
    require(ini.has_option("Mod", "id") and ini.get("Mod", "id").strip(), "format-3 Mod id is absent")
    referenced = {"mod.ini"}
    resources = {}
    dds_rows = {}
    for section in ini.sections():
        if section in ("Mod", "Constants") or section.startswith(("Part", "Replace", "Key", "ShapeControl")):
            continue
        expected_kind = None
        if section.startswith("Mesh"):
            expected_kind = 1
        elif section.startswith(("ShapeAnimation", "MaterialAnimation", "EffectAttachment")):
            expected_kind = 6
        elif section.startswith("Skeleton"):
            expected_kind = 8
        elif section.startswith("Material"):
            expected_kind = 2
        elif section.startswith("Texture"):
            expected_kind = 3
        elif section.startswith("Resource"):
            resource_type = ini[section].get("type", "")
            expected_kind = {"physics": 4, "effect": 5, "controls": 6,
                             "effect-author": 7, "skeleton": 8}.get(resource_type)
            require(expected_kind is not None, "format-3 Resource type is invalid: " + section)
        require(expected_kind is not None, "unknown format-3 resource section: " + section)
        path = ini[section].get("path")
        require(path, "format-3 resource path is absent: " + section)
        candidate = (root / path).resolve()
        require(root in candidate.parents and candidate.is_file(), "format-3 resource is missing or escapes Mod: " + path)
        relative = candidate.relative_to(root).as_posix()
        prior = resources.setdefault(relative, expected_kind)
        require(prior == expected_kind, "format-3 resource path has conflicting kinds: " + relative)
        referenced.add(relative)
        dds = ini[section].get("dds")
        if dds:
            candidate = (root / dds).resolve()
            require(root in candidate.parents and candidate.is_file(), "format-3 DDS is missing or escapes Mod: " + dds)
            dds_relative = candidate.relative_to(root).as_posix()
            referenced.add(dds_relative)
            dds_rows[relative] = (dds_relative, section)
    resource_metadata = {}
    texture_semantics = {}
    for relative, kind in resources.items():
        metadata, payload_bytes = _validate_effres(root / relative, kind)
        resource_metadata[relative] = (metadata, payload_bytes)
    for section in ini.sections():
        if section.startswith("Mesh"):
            skeleton_id = ini[section].get("skeleton", "")
            if skeleton_id:
                require("Skeleton" + skeleton_id in ini and
                        ini["Skeleton" + skeleton_id].get("type") == "skeleton",
                        "format-3 Mesh Skeleton is unresolved: " + section)
        if not section.startswith("Texture"):
            continue
        values = ini[section]
        relative = values["path"]
        linear = values.get("linear", "false").lower()
        require(linear in ("true", "false", "1", "0"),
                "format-3 Texture linear setting is invalid: " + section)
        try:
            settings = (values["dds"], linear in ("true", "1"),
                        int(values.get("filter", "1")), int(values.get("wrap", "0")),
                        int(values.get("aniso", "1")), float(values.get("mip_bias", "0")))
        except ValueError as error:
            raise ValueError("format-3 Texture sampling settings are invalid: " + section) from error
        require(0 <= settings[2] <= 2 and 0 <= settings[3] <= 3 and 0 <= settings[4] <= 16 and
                math.isfinite(settings[5]), "format-3 Texture sampling settings are out of range: " + section)
        prior = texture_semantics.setdefault(relative, settings)
        require(prior == settings,
                "format-3 Texture path has conflicting DDS or sampling semantics: " + relative)
    for relative, (dds_relative, section) in dds_rows.items():
        metadata, _ = resource_metadata[relative]
        require(not metadata and resource_metadata[relative][1] > 0,
                "format-3 Texture must contain only a native Texture2D scaffold: " + relative)
        _parse_dds(root / dds_relative)
    skeleton_paths = {}
    for section in ini.sections():
        if section.startswith("Material"):
            _validate_material_resource(root, ini[section]["path"], section, ini,
                                        resource_metadata[ini[section]["path"]][1])
        elif section.startswith("Resource") and ini[section].get("type") == "physics":
            _validate_physics_resource(root, ini[section]["path"], section, ini,
                                       resource_metadata[ini[section]["path"]][1])
        elif section.startswith("Resource") and ini[section].get("type") == "effect-author":
            _validate_effect_author_resource(root, ini[section]["path"], section,
                                             resource_metadata[ini[section]["path"]][1])
        elif section.startswith("Skeleton"):
            skeleton_id = section[len("Skeleton"):]
            require(skeleton_id and skeleton_id not in skeleton_paths,
                    "format-3 Skeleton id is empty or duplicated: " + section)
            skeleton_paths[skeleton_id] = _validate_skeleton_resource(
                root, ini[section]["path"], section,
                resource_metadata[ini[section]["path"]][1])
        elif section.startswith("EffectAttachment"):
            author_id = ini[section].get("author", "")
            require(author_id in ini and ini[author_id].get("type") == "effect-author",
                    "format-3 EffectAttachment author resource is unresolved: " + section)

    files = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
    return dict(format=3, modId=ini["Mod"]["id"], _files=files)


def _validate_effect_author_resource(root, relative, section, payload_bytes):
    path = root / relative
    with path.open("rb") as stream:
        header = stream.read(HEADER.size)
        _, _, _, metadata_bytes, _, _ = HEADER.unpack(header)
        metadata = stream.read(metadata_bytes)
        reader = _MetadataReader(metadata)
        require((reader.string(), reader.integer()) == (METADATA_STRUCTURE, 7),
                "format-3 Effect author metadata structure differs: " + relative)
        resource_id, resource_file = reader.string(), reader.string()
        author_bytes, author_hash = reader.integer(), reader.string()
        reader.end()
        require(resource_id == section and resource_file and
                author_bytes == payload_bytes and len(author_hash) == 64 and
                author_hash.upper() == author_hash,
                "format-3 Effect author identity/extents differ: " + section)
        author = stream.read(author_bytes)
    require(len(author) == author_bytes and hashlib.sha256(author).hexdigest().upper() == author_hash,
            "format-3 Effect author checksum differs: " + section)
    require(len(author) >= 92 and author[:8] == b"EFFFXA3\0",
            "format-3 Effect author snapshot header differs: " + section)
    version, archive_bytes, state_bytes = struct.unpack_from("<IQQ", author, 8)
    archive_hash, state_hash = author[28:60], author[60:92]
    require(version == 1 and archive_bytes > 0 and state_bytes > 0 and
            archive_bytes + state_bytes == len(author) - 92 and
            hashlib.sha256(author[92:92 + archive_bytes]).digest() == archive_hash and
            hashlib.sha256(author[92 + archive_bytes:]).digest() == state_hash,
            "format-3 Effect author snapshot extents/checksum differ: " + section)
    try:
        value = json.loads(author[92 + archive_bytes:].decode("utf-8", "strict"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("format-3 Effect author document is invalid: " + section) from error
    require(value.get("format") == "EFF_EFFECT_AUTHOR3" and value.get("version") == 1 and
            isinstance(value.get("workspace"), dict) and isinstance(value.get("attachment"), dict),
            "format-3 Effect author document structure differs: " + section)
