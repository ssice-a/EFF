# EFF quick tutorial: installation and Mod creation

[中文](Mod%20Tutorial.md)

EFF has three parts: AnimeStudio extracts source assets, EFF Blender edits and compiles a Mod, and the EFF DLL loads it in the game.

## Install the game DLL

1. Download the DLL archive from [EFF Releases](https://github.com/ssice-a/EFF/releases/latest).
2. Quit the game. Extract the archive into the folder containing `Endfield.exe`, for example:

   ```text
   D:\Hypergryph Launcher\games\Endfield Game
   ```

   The two proxy DLLs go beside `Endfield.exe`; the `plugin` folder goes in the same directory.

   ![Choose the game installation directory](image/安装0.png)
   ![Extract the DLL package into the game directory](image/安装1.png)

3. Put each complete Mod folder under `plugin/mods/`. Keep `mod.ini`, `compiled.bin` and the Mod's resource files together:

   ```text
   plugin/mods/YourMod/
     mod.ini
     compiled.bin
     source-inputs.bin
     meshes/
     materials/
     textures/
   ```

4. Start the game normally.

When updating EFF, quit the game first and preserve your `plugin/eff.ini`, Mod folders and their `state.ini`. F10 reloads Mod data; replacing the DLL itself requires a restart.

## In-game controls

- **F10** reloads the complete current contents of `plugin/mods/`, including added, removed and edited Mods. Finish exporting before pressing it. If a model is still loading, wait for the load to finish and retry.
- **Ins** opens the control panel. Each Mod has a checkbox enabling its keyboard controls. All are enabled by default; several Mods may respond to the same key. Close the panel to use the assigned keys.
- The dropdown chooses which Mod's manual controls are displayed. It does not disable the other Mods' keys.
- Checkbox choices are saved to `key_switch_enabled` in the Mod's `mod.ini`. Disabling keyboard controls keeps the Mod loaded and preserves its current appearance.

You can also edit the flag yourself, then press F10:

```ini
[Mod]
key_switch_enabled=1
```

Use `0` to disable keyboard controls. An omitted flag defaults to enabled. Global shortcuts are configured in `plugin/eff.ini`.

## Install the author tools

Download and install [EFF Blender](https://github.com/ssice-a/EFF-blender/releases/latest) and [AnimeStudio](https://github.com/ssice-a/AnimeStudio/releases/latest). EFF Blender includes its own author core and compiler.

The tested release targets Windows x64 and Blender 5.0.1. Save your project and close Blender before replacing an existing add-on installation.

## Extract source assets

1. Open AnimeStudio and choose **Open VFS**.
2. Select the game's VFS directory, for example:

   ```text
   D:\Hypergryph Launcher\games\Endfield Game\Endfield_Data\StreamingAssets\VFS
   ```

   ![Open VFS in AnimeStudio](image/解包0.png)
   ![Select the game's VFS directory](image/解包1.png)

3. Select the model Prefab you want to edit and export it as an EFF source package. Choose an output directory. The first extraction may take longer.

   ![Choose source assets for export](image/解包3.png)
   ![Export the EFF source package](image/解包4.png)

Keep this source package separately from your game Mod output. Its metadata and field templates are needed when authoring.

## Import and edit in Blender

1. Choose **File → Import → EFF Mod package** and open the extracted package's `mod.ini`.

   ![Open the EFF importer](image/blender导入0.png)
   ![Import the source model](image/blender导入1.png)

2. Edit the imported Meshes. Their EFF properties record the source targets. To use an external model, join your geometry into the imported Mesh that it should replace. Several author parts can replace one source: duplicate the imported target before editing, or separate parts after joining, preserving their EFF source properties.

   ![Edit geometry while retaining EFF source properties](image/blender制作0.png)

3. Check UVs and skinning before export. Use `UV0` for the main UV map, remove unnecessary color attributes, and use vertex group names matching the imported EFF skeleton. Every exported skinned vertex needs valid weights.
4. Configure materials through the **EFF material** properties. Those properties determine the in-game material; Blender preview nodes alone do not. Set texture paths to existing files on your computer. **Import EFF Material** lets you use a material from another extracted character package.
5. Editing a shared Blender material changes every Mesh using it. Use **New Material** to create independent variants with different parameters or textures. Variants may share the same original game material as their template.

   ![Configure EFF materials and texture paths](image/blender制作1.png)

EFF supports visibility switches, shape controls and mapping all target LODs to the author's LOD0 geometry. This LOD option does not generate reduced geometry. Texture encoding inherits the source format, so you do not need to choose a DDS compression format manually.

## Export selected Meshes

1. Select the EFF Meshes you want to export.
2. Open the **EFF** sidebar and choose **Export selected Mod**. Set the Mod folder name and select the game's `plugin/mods/` as the parent directory.
3. For a new Mod, export **All resources**. For an existing complete Mod, use **Mesh only**, **Materials and textures**, or **Textures only** as appropriate.
4. Wait for export and offline compilation to finish. The add-on publishes the staged result only after validation. `compiled.bin` must be distributed with the author resources.

   ![Export selected Meshes from the EFF panel](image/blender导出0.png)

Objects disabled for rendering in Blender are also hidden in the exported game appearance. Check this when a part is missing.

The full export dialog enables **Export key switches** by default. This includes the configured style and shape shortcuts; the DLL's Ins checkboxes let each player decide which Mods receive keys.

Exports directly under `plugin/mods/` locate the game automatically. For another output location, set `EFF_RELOAD_GAME` to the game installation directory.

Keep your `.blend` project and extracted source package. After a successful export, press **F10** in the game and check the updated model and controls.
