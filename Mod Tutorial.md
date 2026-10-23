# EFF Mod制作流程及插件的约定

[English](Mod%20Tutorial_EN.md)

## 概览
这是一个插件,旨在帮助用户更快捷方便地加载以及制作明日方舟终末地的mod.插件主要分为解包,blender制作以及插件加载的三个部分.
## 功能
- mod可以使用游戏中任意材质,贴图无需dds压缩选择格式,会自动继承游戏的对应贴图的格式
- 按下**F10**热重载mod
- 通过按键切换显隐mod的部件,按下**INS**选择是否运行某个mod的按键切换
- 可装配形态键,可按键控制形态键
- 可直接指定多LOD都使用LOD0级别网格,无需额外制作LOD
## 制作流程

- 插件下载与安装
  - 下载[dll插件](https://github.com/ssice-a/EFF/releases/tag/v1.3.4),解压并复制里面的内容到游戏路径:如D:\Hypergryph Launcher\games\Endfield Game
    ![](image/安装0.png)
    ![](image/安装1.png)
  - 下载[blender插件](https://github.com/ssice-a/EFF-blender/releases/tag/v0.40.3)以及[animestudio](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.3.1)并自行安装

- 解包

    - 打开animestudio,选择open VFS然后选择游戏路径如:D:\Hypergryph Launcher\games\Endfield Game\Endfield_Data\StreamingAssets\VFS
        ![](image/解包0.png)
        ![](image/解包1.png)
    然后选择资源工作区。已有工作区直接选择原目录；新版会优先提示同一游戏安装已登记且可用的工作区，不必重复复制索引。
    没有可用索引时，可选择“下载预建索引”“本地构建”或“导入索引包”。下载需要发布方提供匹配当前游戏内容的索引附件，只省去全游戏索引构建；角色、贴图和特效仍从本机游戏按需提取。本地首次构建可能较久，后续复用未变记录。准备过程在后台执行，可用 File > Abort 取消。
        ![](image/解包3.png)
        ![](image/解包4.png)

- blender修改

    - 先导入模型
        ![](image/blender导入0.png)
        ![](image/blender导入1.png)
    - blender侧修改模型
        主要有以下规则
    1. 以游戏的数据为准则,所有的数据都写在导入时的mesh内,如果是将外部模型导入替换游戏内模型,请将你的mesh合并到你想替换的mesh上,**如有多个模型都想替换一个模型**,可以多**复制几份导入的模型一一替换,或者替换后分离网格也是可以的**
    ![](image/blender制作0.png)
    2. 请检查合并前你的mesh UV是否正确且主要的UV名称为UV0,不要有颜色属性,顶点组的名称是否是EFF内的骨骼名称
    3. 关于材质,游戏内使用的材质取决于该网格内材质属性的EFF材质,你可以在里面修改贴图为你自己磁盘内的贴图路径,可以点击导入EFF材质导入其他角色的材质
    4. **注意,各个网格共享的材质中你修改其中一个材质的内容,其余的网格也同步修改,如需新建材质,请点击新材质**
    ![](image/blender制作1.png)

- 导出

   选中你想导出的网格,点击侧边栏导出按钮即可导出,注意相机渲染为隐藏的物体会在游戏内也表现为隐藏.注意导出的文件夹要放在mods文件夹下
   ![](image/blender导出0.png)
## 索引与作者源文件怎么保留

- AnimeStudio、Blender 和 EFF Repair 现在可以共用一份索引。打开当前游戏与原 AnimeStudio 工作区一次，通过校验后会自动登记；索引保留在原位置，不强制搬家。默认公共登记位于 `%LOCALAPPDATA%/EFF/Resources`。
- Blender 当前仍需要全量索引，但不需要每个工程复制一份。作者源包记录的旧索引绝对路径不存在时，会继续查找同一游戏安装的共享索引和父目录工作区；无需为了找回索引重新解包已有角色。
- 完整工作区的 `index/` 内应保留 `endfield_assets.eidx`、`endfield_assets.eidx.bundles` 和 `endfield-index-manifest.json`。只有一个 `.eidx` 不等于完整工作区。
- **索引不能代替作者源包。** 请保留导入的 `.eff` / `.efffx`、`source/manifest.json`、清单引用的源快照与模板，以及自己制作的模型和替换贴图。删掉解包源包后，重新下载索引不能恢复它们；需恢复备份或重新提取来源并核对绑定。
- 游戏更新或插件升级不会仅凭全局指纹判旧源包失效。相关 Bundle 内容和依赖未变时继续复用；缺少可靠逐 Bundle 记录的老包自动保留，跳过原因见工作区 `source-update-skipped.log`，需要时可从 EFF 菜单手动更新原解包来源。
- 成品 Mod 的游戏运行不需要作者的全局索引或解包目录。本次共享索引功能无需更新游戏 DLL。下载功能需新版 AnimeStudio，工程搬移后的自动定位需新版 Blender 插件。

完整规则及发布源配置见[共享索引说明](docs/shared-index-resource-management.md)。

## 注意事项
- 游戏内按ins打开按键切换控制面板
- F10热重载
