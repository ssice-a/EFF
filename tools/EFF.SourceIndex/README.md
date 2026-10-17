# EFF offline source index updater

Blender invokes this helper only after its compiler reports a stale game source
index. It shares AnimeStudio's incremental index builder and Unity parser, but
does not include or open its UI, refresh author source packages, touch a `.blend`,
or publish a Mod. Unchanged Bundle records are reused; changed/new Bundles are
parsed and retired Bundles are removed under the workspace index lock. Failed or
incomplete updates do not proceed to Mod publication.

Build from the repository root with `tools\build_source_index.cmd`. The bundled
Windows x64 executable includes its .NET runtime, so author machines do not need
AnimeStudio or a separate .NET installation. Only the executable and notices are
included in Blender. The same canonical index sources are linked into
`AnimeStudio.IndexTests` for new/changed/retired content, current-index reuse,
location rebinding, unavailable payload, checkpoint and concurrent update tests.

Commands:

```text
eff_source_index.exe --protocol
eff_source_index.exe --update GAME WORKSPACE/index/endfield_assets.eidx
```

The full source index stays in the author's existing workspace; it is not copied
into the Blender add-on or distributed Mod. The helper is about 43 MiB in the
current self-contained build. Normal unchanged-game exports do not launch it.
