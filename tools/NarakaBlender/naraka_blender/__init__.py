bl_info = {
    "name": "Naraka Prefab Importer / 永劫无间时装",
    "author": "EIEM",
    "version": (0, 3, 0),
    "blender": (4, 2, 0),
    "location": "File > Import > Naraka Prefab package",
    "description": "Import Naraka meshes, skinning and lightweight editable PBR / animated emission",
    "category": "Import-Export",
}

import bpy
from bpy_extras.io_utils import ImportHelper
from bpy.props import BoolProperty, StringProperty, EnumProperty
from .readable import PREVIEW_MODES


def active_material(context):
    material = getattr(context, "material", None)
    return material or (context.object.active_material if context.object else None)


class NARAKA_OT_focus_texture(bpy.types.Operator):
    bl_idname = "node.naraka_focus_texture"
    bl_label = "定位这张贴图与通道"
    node_name: StringProperty()

    def execute(self,context):
        material=active_material(context)
        if material is None or self.node_name not in material.node_tree.nodes:return {"CANCELLED"}
        space=context.space_data
        while len(space.path)>1:
            bpy.ops.node.tree_path_parent()
        tree=material.node_tree
        for node in tree.nodes:node.select=False
        image=tree.nodes[self.node_name]
        image.select=True;tree.nodes.active=image
        for n in tree.nodes:
            if n.get('naraka_texture_source')==image.name:n.select=True
        region=next(r for r in context.area.regions if r.type=='WINDOW')
        with context.temp_override(region=region):
            bpy.ops.node.view_selected()
            sidebar=next((r for r in context.area.regions if r.type=='UI'),None)
            if space.show_region_ui and sidebar and sidebar.width>1:
                # The sidebar overlays the canvas; keep the selected pair clear.
                bpy.ops.view2d.zoom_out(zoomfacx=.18,zoomfacy=.18)
                bpy.ops.view2d.pan(deltax=sidebar.width//2,deltay=0)
        return {"FINISHED"}


class NARAKA_PT_textures(bpy.types.Panel):
    bl_label = "贴图与通道用途"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "永劫材质"

    @classmethod
    def poll(cls,context):
        material=active_material(context)
        return context.space_data.tree_type=='ShaderNodeTree' and material is not None and material.get('naraka_readable_layout',0)>=2

    def draw(self,context):
        material=active_material(context)
        for node in material.node_tree.nodes:
            if node.type!='TEX_IMAGE':continue
            box=self.layout.box()
            row=box.row();row.label(text=node.get('naraka_binding','贴图'))
            op=row.operator(NARAKA_OT_focus_texture.bl_idname,text='定位');op.node_name=node.name
            box.label(text=node.image.name if node.image else '缺少图片')
            box.label(text=node.get('naraka_uv_usage',''))
            for line in node.get('naraka_texture_usage','').split('；'):box.label(text=line)


class NARAKA_OT_preview(bpy.types.Operator):
    bl_idname = "node.naraka_preview"
    bl_label = "观察永劫材质阶段"
    bl_options = {"REGISTER", "UNDO"}
    mode: EnumProperty(items=PREVIEW_MODES, default="NORMAL")

    @classmethod
    def poll(cls,context):
        material=active_material(context)
        return material is not None and bool(material.get("naraka_readable_layout"))

    def execute(self,context):
        from .readable import set_preview
        try:
            set_preview(active_material(context),self.mode)
        except (ValueError,StopIteration) as error:
            self.report({"ERROR"},str(error) or "当前材质没有这一层流光")
            return {"CANCELLED"}
        return {"FINISHED"}


class NARAKA_PT_material(bpy.types.Panel):
    bl_label = "通道与流光"
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "永劫材质"

    @classmethod
    def poll(cls,context):
        return context.space_data.tree_type=="ShaderNodeTree" and NARAKA_OT_preview.poll(context)

    def draw(self,context):
        m=active_material(context);layout=self.layout
        if m.get('naraka_material_profile') == 'LIGHTWEIGHT':
            layout.label(text="轻量 PBR · 相位流光 / UV 滚动")
        else:
            layout.operator('material.naraka_simplify',text='将所选永劫材质简化为轻量 PBR')
        current=m.get('naraka_preview','NORMAL')
        label=next((label for key,label,_ in PREVIEW_MODES if key==current),'完整材质')
        layout.operator_menu_enum(NARAKA_OT_preview.bl_idname,'mode',text=label)
        waves=sorted([n for n in m.node_tree.nodes if n.get('naraka_role')=='flow_layer'],key=lambda n:n['naraka_flow_layer'])
        if waves:
            layout.label(text="UV 固定，亮区沿相位图推进")
            for node in waves:
                box=layout.box();box.label(text=node.label)
                for key in ('相位频率','速度 rad/s','阈值','锐度','强度','颜色'):
                    box.prop(node.inputs[key],'default_value',text=key)
        else:
            layout.label(text="当前材质没有周期流光")


class NARAKA_OT_import(bpy.types.Operator, ImportHelper):
    bl_idname = "import_scene.naraka_prefab"
    bl_label = "Import Naraka Prefab"
    bl_options = {"REGISTER", "UNDO"}
    filename_ext = ".json"
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})
    highest_lod: BoolProperty(name="Show highest detail LOD", default=True,
        description="Import all meshes; hide lower LOD objects using the source LODGroup")

    def execute(self, context):
        from .importer import import_package
        try:
            result = import_package(self.filepath, highest_lod=self.highest_lod)
            self.report({"INFO"}, "%d meshes, %d materials, %d textures" % (
                result["meshes"], result["materials"], result["textures"]))
            return {"FINISHED"}
        except Exception as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}


class NARAKA_OT_arrange_material(bpy.types.Operator):
    bl_idname = "node.naraka_arrange_material"
    bl_label = "Arrange Naraka shader nodes / 整理永劫无间节点"
    bl_description = "Arrange the active Naraka material in labeled lanes without changing shader connections"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        material = getattr(context, "material", None)
        if material is None and context.active_object:
            material = context.active_object.active_material
        return material is not None and bool(material.get("naraka_source"))

    def execute(self, context):
        from .layout import arrange_material
        material = getattr(context, "material", None)
        if material is None:
            material = context.active_object.active_material
        count = arrange_material(material)
        self.report({"INFO"}, "Arranged %d material sections" % count)
        return {"FINISHED"}


class NARAKA_OT_simplify(bpy.types.Operator):
    bl_idname = "material.naraka_simplify"
    bl_label = "Simplify Naraka materials / 简化永劫材质"
    bl_description = "Convert imported Naraka materials to lightweight PBR; back up original materials separately"
    scope: EnumProperty(items=[('SELECTED','所选物体','只简化所选物体的永劫材质'),
                               ('SCENE','当前场景','简化当前场景正在使用的永劫材质')], default='SELECTED')

    def execute(self, context):
        from .simplify import simplify_materials, cleanup_unused_legacy
        objects = list(context.scene.objects) if self.scope == 'SCENE' else list(context.selected_objects)
        if not objects and context.object:
            objects = [context.object]
        try:
            result = simplify_materials(objects)
            if self.scope == 'SCENE':
                result['unusedLegacyCleanup'] = cleanup_unused_legacy()
        except Exception as error:
            self.report({'ERROR'}, str(error))
            return {'CANCELLED'}
        self.report({'INFO'}, '已简化 %d 个材质；原材质备份：%s' % (result['materials'],result.get('backup','无需转换')))
        return {'FINISHED'}


def menu_import(self, context):
    self.layout.operator(NARAKA_OT_import.bl_idname, text="Naraka Prefab package / 永劫无间 (naraka.json)")


def register():
    bpy.utils.register_class(NARAKA_OT_import)
    bpy.utils.register_class(NARAKA_OT_arrange_material)
    bpy.utils.register_class(NARAKA_OT_simplify)
    bpy.utils.register_class(NARAKA_OT_preview)
    bpy.utils.register_class(NARAKA_PT_material)
    bpy.utils.register_class(NARAKA_OT_focus_texture)
    bpy.utils.register_class(NARAKA_PT_textures)
    bpy.types.TOPBAR_MT_file_import.append(menu_import)


def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    bpy.utils.unregister_class(NARAKA_OT_import)
    bpy.utils.unregister_class(NARAKA_OT_arrange_material)
    bpy.utils.unregister_class(NARAKA_OT_simplify)
    bpy.utils.unregister_class(NARAKA_PT_material)
    bpy.utils.unregister_class(NARAKA_PT_textures)
    bpy.utils.unregister_class(NARAKA_OT_focus_texture)
    bpy.utils.unregister_class(NARAKA_OT_preview)
