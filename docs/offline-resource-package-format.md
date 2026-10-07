# 独立 Mod 编译结果与资源契约

`mod.ini` 是唯一作者规则入口，独立 `.mesh/.mat/.tex` 是作者载荷。Blender 的配套工具生成 `source-inputs.bin` 和 `compiled.bin`；DLL 读取后者，不调用作者工具。兼容性依据 `eiem_package_protocol.h` 的实际字段与 `eiem_compiled_mod.h` 的结构声明判断，不按版本号。

## 输出布局

```text
YourMod/
  mod.ini
  source-inputs.bin
  compiled.bin
  meshes/*.mesh
  materials/*.mat
  textures/*.tex
  state.ini              # 游戏生成的玩家数值，可保留
```

`compiled.bin` 保存独立 Mod 清单、原游戏相对文件路径/范围/SHA、成员复制规则、去重的字段结构字典、对象布局、精确流偏移和命名重定位位置、原根与作者标记。外层 Xpress-Huffman 压缩，尾部 SHA256；作者大段几何和像素通过资源文件范围引用，不再次嵌入。不保存完整 PFB 或游戏依赖。LZY 样本约 11.25 MiB；不存在人为大小/Mod 数量上限。

原游戏相对路径在读取时绑定当前安装并校验范围/摘要。作者根重新绑定 Mod 当前目录；移动到另一安装不依赖作者机器绝对路径。来源更新、资源或替换声明变化需要重新导出。仅按键、数值、显隐、形态控制修改不改变编译资源契约，可由 F10 读取；源资源字节变化不可用旧编译规则。

## 编译与发布

Blender 在暂存目录写选中的作者资源，复用或更新来源描述，再使用自身安装目录的 `eff_resource_pack.exe --export-mod <staged Mod> <game>` 编译一个独立 Mod。作者侧解析 Mesh/材质/贴图、寻找消费者、生成对象、校验规则回放。全部成功后才发布该 Mod；失败不覆盖此前完整结果。其他 Mod 不参与这次作者编译。

来源工具 `eff_source_prepare.exe --prepare-static-sources <game> <index> <Mod> <compiler>` 复用现有 AnimeStudio 索引，只读取相关原始范围。索引与模板留在作者机器。游戏安装只含 DLL、代理、设置及字段声明。

## DLL 应用规则

启动和 F10 读取当前完整 0/1/N Mod 集合，校验作者资源契约。独占输入直接回放编译复制规则；共用输入合并已编译对象记录，对不同原对象保留各自修改，对共用 Transform、LOD/helper、AssetBundle preload 列表按原槽位合并，对新增流调整偏移。重复原 Mesh 目标或同一字段的冲突修改拒绝。没有重新生成几何、材质或贴图，也没有重新查找 Prefab 消费者。

原对象未被声明修改时保持原字节。删除 Mod 使用此前来源证明恢复 pristine 候选；空集合走同一事务。集合候选与作者文件完整核验后才提交，导出过程中变化的快照拒绝，失败保持当前运行时代际。

运行时仍需读取原范围、解码容器并应用规则，生成 Unity 可加载的内存包。这些开销与规则文件大小不同。后台准备隔离输入；Unity 线程执行加载、核验和原生 owner 重建，动画、武器、音频、源租约及回滚沿现有链路处理。未加载目标随后使用当前规则。

`current.tsv` 和旧全量输入只保留只读兼容路径；新导出不生成全局作者集合缓存，DLL 不启动编译器，不读取索引，不主动查询游戏来源。旧非复制规则清单需要重新导出。

验证边界见[热重载测试](hot-reload-testing.md)。离线回放/模拟事务通过不表示所有模型的实机画面已验收。
