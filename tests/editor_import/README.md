# Editor import regression

Build the addon with the pinned binding API and provide its matching private Steam runtime. Run from the checkout:

```text
python tests/editor_import/run.py --engine <REDOT_BIN> --addon <built addon directory> --scratch <new scratch directory>
```

The addon directory must contain `godotsteam.gdextension`, the built native library, the editor plugin, and the runtime dependencies. Use a fresh build output when changing files under `plugin/editor/`; incremental directory installation may retain old nested scripts.

Run separately with Redot 26.2, official 26.3 and the patched 26.3 diagnostic build, using separate scratch directories. Add `--library-target template_release` to select the release library in an editor process. Each engine process uses headless mode, Dummy audio, a 120-frame bound and a 60-second wall-clock timeout.

The runner checks a no-addon control, plugin-only partial initialization without the extension, cold imports with the plugin disabled and enabled, editor restarts, the native game fixture, missing-App-ID failure, and Steam access from an autoload's `_init()` with automatic initialization disabled and enabled. The latter uses App ID 480 and only initializes, pumps callbacks and shuts down. It neither launches a Steam client nor writes account data.

Any nonzero exit, timeout, error/warning diagnostic or missing completion marker fails the run. Logs and structured results stay in the scratch directory. Official 26.3 can fail even its no-addon editor control; inspect those results to classify its independent engine shutdown problem. Such a failure is never converted to PASS.

On first discovery in an already-running editor, native classes register internally and class documentation waits for an editor restart to avoid Redot's queued documentation shutdown crash. Steam, project settings and the dock are available during discovery; normal class exposure and help return after restart. Cached editor startup and game startup retain CORE registration, settings and auto-init before autoload creation. The plugin checks availability and safely tolerates partial initialization.

This is an integration test requiring built native libraries and restricted SDK inputs for the build. It is intentionally outside SDK-free source-check CI.
