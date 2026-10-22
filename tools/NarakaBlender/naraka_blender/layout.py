"""Deterministic, non-overlapping shader lanes; never change shader links."""


SECTIONS = {
    "time": ("01 · Time / 时间", (0.22, 0.28, 0.36)),
    "base": ("02 · Base color / 基础色", (0.30, 0.24, 0.16)),
    "normal": ("03 · Normal decoding / 法线解码", (0.20, 0.24, 0.38)),
    "surface": ("04 · Metallic & roughness / 金属度与粗糙度", (0.22, 0.30, 0.25)),
    "detail": ("04a · Detail array & masks / 细节数组与蒙版", (0.20, 0.29, 0.30)),
    "uv2": ("04b · UV2 overlay / 第二UV叠加", (0.25, 0.29, 0.24)),
    "cloth": ("05 · Cloth regions / 布料分区", (0.30, 0.22, 0.32)),
    "optical": ("05a · Diamond & glitter / 钻石与闪片", (0.31, 0.24, 0.36)),
    "effect": ("05b · Effect layers / 特效图层", (0.32, 0.27, 0.19)),
    "emission": ("06 · Emission inputs / 发光输入", (0.35, 0.25, 0.15)),
    "wave0": ("07 · Periodic2 wave 1 / 第一层流光", (0.32, 0.23, 0.14)),
    "wave1": ("08 · Periodic2 wave 2 / 第二层流光", (0.32, 0.23, 0.14)),
    "emission_mix": ("09 · Combine emission / 合成发光", (0.35, 0.25, 0.15)),
    "alpha": ("10 · Alpha & face culling / 透明与面剔除", (0.25, 0.28, 0.30)),
    "retained": ("11 · Unconverted source textures / 尚未接入的原始贴图", (0.32, 0.18, 0.17)),
    "output": ("12 · Surface & output / 着色与输出", (0.20, 0.32, 0.25)),
}


def node_size(node):
    """Conservative sizes also work before Blender's first UI draw."""
    sizes = {
        "TEX_IMAGE": (240, 350), "MAPPING": (220, 460), "UVMAP": (190, 130),
        "VALUE": (190, 110), "MATH": (190, 170), "VECT_MATH": (210, 245),
        "MIX_RGB": (210, 220), "SEPARATE_COLOR": (190, 190),
        "COMBINE_COLOR": (190, 205), "NEW_GEOMETRY": (210, 270),
        "NORMAL_MAP": (210, 230), "BSDF_PRINCIPLED": (310, 1000),
        "OUTPUT_MATERIAL": (210, 190),
    }
    width, height = sizes.get(node.type, (210, 300))
    if node.type == "GROUP":
        width, height = 380, 100 + 34 * sum(not s.hide for s in list(node.inputs) + list(node.outputs))
    # Re-layout uses measured dimensions when the UI has already drawn nodes.
    if not node.hide:
        # dimensions are UI pixels; locations/width use node-space units.
        # Taking raw dimensions makes every re-layout grow with Windows DPI.
        scale = node.dimensions.x / node.width if node.dimensions.x > 0 else 1
        height = max(height, node.dimensions.y / scale)
    return width, height


def arrange_material(material):
    """Pack DAG columns inside separate labeled frames, leaving generous gaps.

    Only positions, widths and frame parents change. Drivers, parameters,
    sockets, links and material evaluation remain untouched.
    """
    if hasattr(material, "get") and material.get("naraka_material_profile") == "LIGHTWEIGHT":
        from .lightweight import arrange_lightweight
        return arrange_lightweight(material)
    if hasattr(material, "get") and material.get("naraka_readable_layout"):
        from .readable import arrange_root
        arrange_root(material)
        return sum(n.type == "GROUP" for n in material.node_tree.nodes)
    tree = material.node_tree
    if tree is None:
        return 0
    nodes = [n for n in tree.nodes if n.type != "FRAME"]
    if not nodes:
        return 0
    # This operator owns only its own frames; keep user-created frames intact.
    for node in nodes:
        if node.parent and node.parent.get("naraka_layout_frame"):
            node.parent = None
    for frame in list(tree.nodes):
        if frame.type == "FRAME" and frame.get("naraka_layout_frame"):
            tree.nodes.remove(frame)
    groups = {}
    for node in nodes:
        group = node.get("naraka_section", "output" if node.type in ("BSDF_PRINCIPLED", "OUTPUT_MATERIAL") else "surface")
        groups.setdefault(group, []).append(node)
    sizes = {node: node_size(node) for node in nodes}
    predecessors = {node: [] for node in nodes}
    for link in tree.links:
        if link.from_node in sizes and link.to_node in sizes:
            predecessors[link.to_node].append(link.from_node)
    y, content_width, frame_count = 0, 0, 0
    order = list(SECTIONS) + sorted(set(groups) - set(SECTIONS))
    # Lanes are independently ranked: a shared normal/time input does not
    # push the two wave graphs halfway off the screen.
    for key in order:
        group = groups.get(key)
        if not group or key == "output":
            continue
        frame_count += 1
        frame = tree.nodes.new("NodeFrame")
        frame.label, color = SECTIONS.get(key, (key, (0.25, 0.25, 0.25)))
        frame.name = "Naraka · " + key
        frame["naraka_layout_frame"] = True
        frame.use_custom_color, frame.color = True, color
        frame.label_size = 24
        frame.shrink = True
        members, ranks, visiting = set(group), {}, set()

        def rank(node):
            if node in ranks:
                return ranks[node]
            if node in visiting:
                return 0  # User edits can introduce cycles; layout stays safe.
            visiting.add(node)
            ranks[node] = max((rank(p) + 1 for p in predecessors[node] if p in members and p not in visiting), default=0)
            visiting.remove(node)
            return ranks[node]

        columns = {}
        for node in group:
            columns.setdefault(rank(node), []).append(node)
        x, height = 0, 0
        for index in sorted(columns):
            column = columns[index]
            cursor = 0
            width = max(sizes[node][0] for node in column)
            for node in column:
                node.parent = frame
                node.width = sizes[node][0]
                node.location = (40 + x, -70 - cursor)
                node.select = False
                cursor += sizes[node][1] + 65
            x += width + 95
            height = max(height, cursor)
        # Use estimates rather than the deferred frame dimensions. Every lane
        # fits its tallest column, including Mapping and expanded Principled.
        frame.location = (0, y)
        frame.select = False
        y -= height + 170
        content_width = max(content_width, x)
    output = groups.get("output", [])
    for index, node in enumerate(output):
        node.parent = None
        node.width = sizes[node][0]
        node.location = (content_width + 220 + index * 440, -180)
        node.select = False
    material["naraka_node_layout"] = 1
    return frame_count
