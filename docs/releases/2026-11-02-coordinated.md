# Coordinated release, 2026-11-02

All four releases were published after their actual archives were downloaded and verified. The runtime accepts Format 3 only. Legacy migration remains an offline tool.

| Repository | Version | Commit | CI run |
| --- | --- | --- | --- |
| [ssice-a/EFF](https://github.com/ssice-a/EFF/releases/tag/v2.0.3) | v2.0.3 | `7ae206c2a802ba872c101a5d9e99eefe7bf9f178` | 41366525878 |
| [ssice-a/AnimeStudio](https://github.com/ssice-a/AnimeStudio/releases/tag/v1.9.0) | v1.9.0 | `d06d905d4d8dae664a5042137be0a70fcb61f54d` | 41363165925 |
| [ssice-a/EFF-Mod-Manager](https://github.com/ssice-a/EFF-Mod-Manager/releases/tag/v0.2.0) | v0.2.0 | `21ca1be4596728df0b65f497db6e61438f8b2dd3` | 41360735135 |
| [ssice-a/EFF-blender](https://github.com/ssice-a/EFF-blender/releases/tag/v0.50.0) | v0.50.0 | `b81296cbbc110e558248eb76a29027a77ad5f104` | 41367676430 |

## Verified archives

| Asset | Bytes | SHA256 |
| --- | ---: | --- |
| eff-native-tools.json | 770 | `0ef8e6aec03d9e5a30851e3b1ec97ad3c9fcdb7c23955a6a3e0cd60a9f4be6c3` |
| EFF_Shared_Native_Tools_v2.0.3.zip | 96113689 | `124ef70997254e28a7c25d4082a5b615d536b04e2dbd3ca1d245f2f1b2350938` |
| EFF_v2.0.3_dll.zip | 1587437 | `b164af4b0503b4c59eb79a7fbc03d47bd406c87ee4633424ae2220fdc587a0ac` |
| AnimeStudio_v1.9.0_net10.0-windows.zip | 13643298 | `ea39408079c78859c8f85a7e70d51beeb61a15aea0c3f9596fc468b625490c5e` |
| AnimeStudio_v1.9.0_net9.0-windows.zip | 13635726 | `624fdd693a9ad9d3bc2f435f1d19d6edc3c12d53e0d446b61a7d6d85a61f3cf0` |
| EFF-Mod-Manager-v0.2.0-win-x64.zip | 807258 | `59ce76e98b9685837551bc9399193ea789f4c84b1654e499522fd3a4ae7984b3` |
| EFF_Blender_v0.50.0.zip | 366776 | `211c84ad8c1c0156f5f5dd28cd5c339dd8b0b3036b7ca0f39a9acd4e956eee41` |

The DLL archive passed production-marker, static-runtime-import, package-layout and ZIP integrity checks. The shared-tools archive matched its size/SHA256 receipt, launched the frozen migrator, and contained the pinned DirectXTex binary. Blender archive contents matched every build-receipt hash and contained no native executables or libraries. AnimeStudio packages and Mod Manager matched their immutable tag commits and runtime layouts.

## Local assets

The [body weight repair](../body-weight-repair-20261102.md) is a separate local deployment, not public release content. It patched 160 Mesh resources across eight installed Mods and verified eight saved projects. Only matched body weights changed; ambiguous and unmatched vertices retained their weights. Effects, materials, textures and controls were protected. Game-side visual acceptance of the new weight repairs remains pending; the lx effect fix and transformation/key-switch fix were previously accepted by the user.

The exact verified v2.0.3 DLL was deployed separately after an isolated LoadLibrary/control-lock check; its SHA256 is `4621CD4ED5F679ED524677B41F46310DB8C93583E8035C256BD139777848EC20`. The previous DLL is backed up, and 663 protected Mod/configuration/proxy files remained unchanged. Installed bytes and all 160 repaired Mesh hashes were rechecked. In-game acceptance of this release binary awaits a restart.

Original projects and installed Mesh files remain backed up. Ignored local reports, staging views and receipts are retained for diagnosis/rollback. File-size reductions are not measured game memory savings.

Tags v2.0.0 through v2.0.2 were not published; v2.0.3 incorporates the validated packaging environment and current diagnostic fixtures without moving those tags. The full machine-readable receipt is [2026-11-02-coordinated.json](2026-11-02-coordinated.json).
