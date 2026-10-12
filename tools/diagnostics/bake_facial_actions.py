"""Bake verified SkeletalMorph resources onto the user's source armature.

Run in Blender with EIEM_Blender enabled. This writes a separate Action library,
never saves the user's current .blend or publishes any game Mod.
"""
import bisect
import json
import math
import pathlib
import sys

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector


POSE_FIELDS = ('_browValueL', '_browValueR', '_eyeValueL', '_eyeValueR',
               '_mouthValue', '_otherValue')
CURVE_FIELDS = ('_browCurveL', '_browCurveR', '_eyeCurveL', '_eyeCurveR',
                '_mouthCurve', '_otherCurve')


def curve_value(curve, time):
    keys = curve['m_Curve']
    if not keys:
        return 0.0
    if any(k['weightedMode'] for k in keys):
        raise ValueError('Weighted curve requires a verified evaluator')
    index = bisect.bisect_right([k['time'] for k in keys], time)
    if index == 0:
        return keys[0]['value']
    if index == len(keys):
        return keys[-1]['value']
    left, right = keys[index - 1:index + 1]
    length = right['time'] - left['time']
    if length <= 0:
        raise ValueError('Unordered source curve keys')
    if not math.isfinite(left['outSlope']) or not math.isfinite(right['inSlope']):
        return left['value']
    t = (time - left['time']) / length
    return ((2*t**3 - 3*t*t + 1) * left['value']
            + (t**3 - 2*t*t + t) * length * left['outSlope']
            + (-2*t**3 + 3*t*t) * right['value']
            + (t**3 - t*t) * length * right['inSlope'])


def controls(resource, time=0):
    result = {}
    if '_pose' in resource:
        for field in POSE_FIELDS:
            for item in resource['_pose'].get(field, []):
                name = item['_ctrlName']
                if name in result:
                    raise ValueError('Repeated controller: ' + name)
                result[name] = item['_value']
    else:
        for field in CURVE_FIELDS:
            for item in resource.get(field, []):
                name = item['_ctrlName']
                if name in result:
                    raise ValueError('Repeated controller: ' + name)
                result[name] = curve_value(item['_curve'], time)
    if not all(math.isfinite(v) for v in result.values()):
        raise ValueError('Nonfinite facial control')
    return result


def vector(value):
    return Vector(tuple(value[c] for c in 'xyz'))


def maya_rotation(value):
    # The actual source rest quaternions validate XYZ and (+X,-Y,-Z).
    return Euler((math.radians(value[0]), -math.radians(value[1]),
                  -math.radians(value[2])), 'XYZ').to_quaternion()


def bake(source_path, rig_name, output_path, replace_library=False):
    source = json.loads(pathlib.Path(source_path).read_text(encoding='utf-8'))
    rig = bpy.data.objects.get(rig_name)
    if rig is None or rig.type != 'ARMATURE':
        raise ValueError('Target source armature is absent')
    original_context_object = bpy.context.object
    original_mode = original_context_object.mode if original_context_object else 'OBJECT'
    addon = sys.modules['EIEM_Blender.eiem_blender_addon']
    data = source['avatar']['data']
    refs = {r['rid']: r for r in source['avatar']['references']['RefIds']}
    mapping = {name: refs[pointer['rid']]['data']
               for name, pointer in zip(data['morphMappingNames'], data['morphMappingCfgs'])}
    base = {name: item for name, item in zip(data['allBoneNames'], data['basePoseConfig'])}
    id_name = {item['boneID']: name for name, item in base.items()}
    if len(id_name) != len(base):
        raise ValueError('Ambiguous avatar bone IDs')
    for cfg in mapping.values():
        for item in cfg['bones']:
            if item['boneID'] not in id_name:
                raise ValueError('Morph references an unknown bone ID')
            if item['boneNameHash'] != base[id_name[item['boneID']]]['boneNameHash']:
                raise ValueError('Morph bone identity disagrees with its avatar')
    if pathlib.Path(output_path).exists() and not replace_library:
        raise ValueError('Action library already exists: ' + output_path)

    # Source EFF omits two unweighted helper bones. Their original game
    # transforms, rather than guessed transforms, can complete this facial rig.
    needed = [name for name in base if name not in rig.data.bones]
    physics_path = pathlib.Path(source['source_physics'])
    source_transforms = json.loads(physics_path.read_text(encoding='utf-8'))['transforms']
    transforms_by_name = {}
    for transform in source_transforms:
        transforms_by_name.setdefault(transform['path'].rsplit('/', 1)[-1], []).append(transform)
    helpers = []
    for name in needed:
        candidates = transforms_by_name.get(name, [])
        if len(candidates) != 1:
            raise ValueError('Cannot resolve missing source helper: ' + name)
        transform = candidates[0]
        path = transform['path'].split('/', 1)[1]
        parent = path.rsplit('/', 1)[0].rsplit('/', 1)[-1]
        if parent not in rig.data.bones:
            raise ValueError('Missing helper parent: ' + parent)
        helpers.append((name, transform, path, parent))

    selected = []
    skipped = []
    for resource in source['resources']:
        if '_pose' in resource:
            names = list(controls(resource))
        else:
            names = [c['_ctrlName'] for field in CURVE_FIELDS for c in resource.get(field, [])]
        unknown = sorted(set(names) - mapping.keys())
        if unknown:
            skipped.append({'source': resource['m_Name'], 'reason': 'Unmapped source controllers',
                            'controllers': unknown})
            continue
        selected.append(resource)
    if not selected:
        raise ValueError('No facial resources match this avatar')

    basis = addon.unity_transform_matrix_to_blender_basis()
    basis_inv = basis.inverted()
    original_selected = list(bpy.context.selected_objects)
    original_active = bpy.context.view_layer.objects.active
    original_action = rig.animation_data.action if rig.animation_data else None
    original_slot = rig.animation_data.action_slot if rig.animation_data else None
    scene = bpy.context.scene
    original_frame, original_subframe = scene.frame_current, scene.frame_subframe
    original_pose = {p.name: (p.location.copy(), p.rotation_mode, p.rotation_quaternion.copy(),
                              p.rotation_euler.copy(), p.scale.copy()) for p in rig.pose.bones}
    actions = []
    new_helpers = []
    try:
        if original_mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        if helpers:
            for obj in bpy.context.selected_objects:
                obj.select_set(False)
            rig.select_set(True)
            bpy.context.view_layer.objects.active = rig
            bpy.ops.object.mode_set(mode='EDIT')
            for name, transform, path, parent in helpers:
                parent_bone = rig.data.edit_bones[parent]
                p, q, s = transform['localPosition'], transform['localRotation'], transform['localScale']
                local = Matrix.LocRotScale(Vector(p), Quaternion((q[3], q[0], q[1], q[2])), Vector(s))
                world = parent_bone.matrix @ basis @ local @ basis_inv
                bone = rig.data.edit_bones.new(name)
                bone.parent = parent_bone
                bone.length = 0.01
                bone.matrix = world
                bone.use_deform = False
                bone['eiem_path'] = path
                bone['eiem_skeleton_source'] = True
                bone['eiem_source_parent'] = path.rsplit('/', 1)[0]
                bone['eiem_local_position'] = p
                bone['eiem_local_rotation'] = q
                bone['eiem_local_scale'] = s
                bone['eiem_facial_source_helper'] = True
                new_helpers.append(name)
            bpy.ops.object.mode_set(mode='OBJECT')
            for name in new_helpers:
                bone = rig.data.bones[name]
                bone['eiem_rest_display'] = [bone.matrix_local[r][c] for r in range(4) for c in range(4)]

        ordered = sorted(rig.data.bones, key=lambda b: len(b.parent_recursive))
        source_local = {}
        source_world = {}
        for bone in ordered:
            p, q, s = bone.get('eiem_local_position'), bone.get('eiem_local_rotation'), bone.get('eiem_local_scale')
            if p is None or q is None or s is None:
                raise ValueError('Missing source rest metadata: ' + bone.name)
            local = Matrix.LocRotScale(Vector(p), Quaternion((q[3], q[0], q[1], q[2])), Vector(s))
            source_local[bone.name] = local
            source_world[bone.name] = source_world[bone.parent.name] @ local if bone.parent else local
        affected = set(base)
        for name in base:
            affected.update(b.name for b in rig.data.bones[name].parent_recursive)
        rest_error = max(abs((basis @ source_world[b.name] @ basis_inv)[r][c] - b.matrix_local[r][c])
                         for b in ordered if b.name in affected for r in range(4) for c in range(4))
        if rest_error > 0.0001:
            raise ValueError('Armature rest was edited; source mapping requires retargeting: ' + str(rest_error))
        neutral_error = 0.0
        for name, item in base.items():
            expected = Matrix.LocRotScale(vector(item['position']), maya_rotation(vector(item['rotation'])),
                                         vector(item['scale']))
            neutral_error = max(neutral_error, max(abs(expected[r][c]-source_local[name][r][c])
                                                  for r in range(4) for c in range(4)))
        # The avatar stores rounded Maya Euler values while the PostModel
        # stores quaternions. Actual neutral positions agree below 5e-7 m;
        # their measured angular discrepancy is below 0.06 degrees.
        if neutral_error > 0.001:
            raise ValueError('Avatar neutral TRS disagrees with source armature: ' + str(neutral_error))

        def evaluate(weights):
            pose = {name: [vector(item['position']), vector(item['rotation']), vector(item['scale'])]
                    for name, item in base.items()}
            for ctrl, weight in weights.items():
                cfg = mapping[ctrl]
                if cfg['partType'] != 32:
                    weight = max(0.0, min(1.0, weight))
                for delta in cfg['bones']:
                    result = pose[id_name[delta['boneID']]]
                    for index, field in enumerate(('position', 'rotation', 'scale')):
                        result[index] += vector(delta[field]) * weight
            unity_world, blender_world, values = {}, {}, {}
            for bone in ordered:
                if bone.name in pose:
                    p, e, s = pose[bone.name]
                    local = Matrix.LocRotScale(p, maya_rotation(e), s)
                else:
                    local = source_local[bone.name]
                unity_world[bone.name] = unity_world[bone.parent.name] @ local if bone.parent else local
                world = basis @ unity_world[bone.name] @ basis_inv
                blender_world[bone.name] = world
                if bone.name not in base:
                    continue
                kwargs = {'invert': True}
                if bone.parent:
                    kwargs.update(parent_matrix=blender_world[bone.parent.name],
                                  parent_matrix_local=bone.parent.matrix_local)
                local_basis = bone.convert_local_to_pose(world, bone.matrix_local, **kwargs)
                location, rotation, scale = local_basis.decompose()
                reconstructed = bone.convert_local_to_pose(Matrix.LocRotScale(location, rotation, scale),
                    bone.matrix_local, **dict(kwargs, invert=False))
                error = max(abs(reconstructed[r][c]-world[r][c]) for r in range(4) for c in range(4))
                if error > 0.0001:
                    raise ValueError('Blender transform round trip failed: ' + bone.name)
                values[bone.name] = (tuple(location), tuple(rotation), tuple(scale))
            return values

        rows = []
        fps = scene.render.fps / scene.render.fps_base
        for resource in selected:
            kind = 'Emotion' if '_blinkInterval' in resource else ('Pose' if '_pose' in resource else 'Anim')
            suffix = resource['m_Name'].removeprefix('data_facialmorph_').removeprefix(kind.lower() + '_')
            action_name = 'EIEM_Face_Liino_' + kind + '_' + suffix
            if action_name in bpy.data.actions:
                raise ValueError('Facial Action already exists: ' + action_name)
            if '_pose' in resource:
                times = [0.0]
            else:
                duration = float(resource['_duration'])
                if not 0 < duration < 600:
                    skipped.append({'source': resource['m_Name'], 'reason': 'Empty or invalid duration'})
                    continue
                times = sorted(set([0.0, duration] + [i / 60 for i in range(1, math.ceil(duration * 60))]
                    + [k['time'] for field in CURVE_FIELDS for ctrl in resource.get(field, [])
                       for k in ctrl['_curve']['m_Curve'] if 0 <= k['time'] <= duration]))
            samples = [evaluate(controls(resource, time)) for time in times]
            action = bpy.data.actions.new(action_name)
            actions.append(action)
            action.use_fake_user = True
            action['eiem_facial_source'] = resource['m_Name']
            action['eiem_facial_kind'] = kind
            action['eiem_facial_avatar'] = source['avatar']['m_Name']
            action['eiem_facial_samples_per_second'] = 60 if kind == 'Anim' else 0
            slot = action.slots.new(id_type='OBJECT', name=rig.name)
            layer = action.layers.new('Facial bones')
            strip = layer.strips.new(type='KEYFRAME')
            curves = strip.channelbag(slot, ensure=True).fcurves
            frames = [1.0 + time * fps for time in times]
            for name in base:
                pb = rig.pose.bones[name]
                for sample_index in range(1, len(samples)):
                    a, b = samples[sample_index-1][name][1], samples[sample_index][name][1]
                    if sum(a[i] * b[i] for i in range(4)) < 0:
                        loc, quat, scale = samples[sample_index][name]
                        samples[sample_index][name] = (loc, tuple(-v for v in quat), scale)
                for component, field in enumerate(('location', 'rotation_quaternion', 'scale')):
                    width = 4 if component == 1 else 3
                    path = pb.path_from_id(field)
                    for index in range(width):
                        fc = curves.new(data_path=path, index=index)
                        channel = [sample[name][component][index] for sample in samples]
                        if len(frames) > 2 and max(channel)-min(channel) <= 1e-8:
                            points = [(frames[0], channel[0]), (frames[-1], channel[-1])]
                        else:
                            points = list(zip(frames, channel))
                        fc.keyframe_points.add(len(points))
                        fc.keyframe_points.foreach_set('co', [v for point in points for v in point])
                        for key in fc.keyframe_points:
                            key.interpolation = 'LINEAR' if kind == 'Anim' else 'CONSTANT'
                        fc.update()
            action.asset_mark()
            action.asset_data.description = ('Liino game facial ' + kind.lower() + ': ' + resource['m_Name'])
            rows.append({'action': action.name, 'source': resource['m_Name'], 'kind': kind,
                         'frames': len(times), 'duration': times[-1], 'bones': len(base)})

        # Evaluate Blender's actual Action/slot pipeline against baked values.
        rig.animation_data_create()
        validation = []
        for row in [r for r in rows if r['source'] in ['data_facialmorph_emotion_happyl',
                    'data_facialmorph_emotion_angryl', 'data_facialmorph_emotion_sadl',
                    'data_facialmorph_anim_blink_02']]:
            action = bpy.data.actions[row['action']]
            rig.animation_data.action = action
            rig.animation_data.action_slot = action.slots[0]
            for name in base:
                rig.pose.bones[name].rotation_mode = 'QUATERNION'
            resource = next(r for r in selected if r['m_Name'] == row['source'])
            time = row['duration'] / 2
            expected = evaluate(controls(resource, time))
            target_frame = 1 + time * fps
            scene.frame_set(math.floor(target_frame), subframe=target_frame % 1)
            bpy.context.view_layer.update()
            error = max(abs(actual-v) for name, values in expected.items()
                        for field, target in zip(('location', 'rotation_quaternion', 'scale'), values)
                        for actual, v in zip(getattr(rig.pose.bones[name], field), target))
            if error > 0.001:
                raise ValueError('Action evaluation disagrees with source pose: ' + row['action'] + ' ' + str(error))
            validation.append({'action': row['action'], 'maximum_channel_error': error})
        rig.animation_data.action = original_action
        if original_action is not None:
            rig.animation_data.action_slot = original_slot
        for name, values in original_pose.items():
            pb = rig.pose.bones[name]
            pb.location, pb.rotation_mode, pb.rotation_quaternion, pb.rotation_euler, pb.scale = values
        scene.frame_set(original_frame, subframe=original_subframe)
        preview = next((a for a in actions if a.get('eiem_facial_source') == 'data_facialmorph_emotion_happyl'), None)
        if preview is not None:
            for name in base:
                rig.pose.bones[name].rotation_mode = 'QUATERNION'
            rig.animation_data.action = preview
            rig.animation_data.action_slot = preview.slots[0]
            scene.frame_set(original_frame, subframe=original_subframe)
            bpy.context.view_layer.update()
        bpy.data.libraries.write(output_path, set(actions), fake_user=True, compress=True)
        report = {'target': rig.name, 'library': output_path, 'actions': rows, 'skipped': skipped,
                  'added_source_helpers': new_helpers, 'rest_matrix_error': rest_error,
                  'avatar_neutral_matrix_error': neutral_error, 'validation': validation,
                  'preview': preview.name if preview else None,
                  'limitations': ['Only bone animation is baked; shader expression parameters are excluded.',
                                  'Emotion base poses do not fabricate random blink or micro-expression timing.']}
        pathlib.Path(output_path).with_suffix('.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        return report
    except Exception:
        if bpy.context.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        if rig.animation_data:
            rig.animation_data.action = original_action
            if original_action is not None:
                rig.animation_data.action_slot = original_slot
        for name, values in original_pose.items():
            if name in rig.pose.bones:
                pb = rig.pose.bones[name]
                pb.location, pb.rotation_mode, pb.rotation_quaternion, pb.rotation_euler, pb.scale = values
        scene.frame_set(original_frame, subframe=original_subframe)
        for action in actions:
            bpy.data.actions.remove(action)
        if new_helpers:
            bpy.context.view_layer.objects.active = rig
            bpy.ops.object.mode_set(mode='EDIT')
            for name in new_helpers:
                rig.data.edit_bones.remove(rig.data.edit_bones[name])
            bpy.ops.object.mode_set(mode='OBJECT')
        raise
    finally:
        for obj in bpy.context.selected_objects:
            obj.select_set(False)
        for obj in original_selected:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = original_active
        if original_mode != 'OBJECT' and original_active is not None:
            bpy.ops.object.mode_set(mode=original_mode)
