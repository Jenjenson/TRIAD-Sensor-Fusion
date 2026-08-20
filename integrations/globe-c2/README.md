# Globe-C2 integration

The adapter consumes the fused `triad.layered_detection_snapshot.v1` document. It does not consume `latest_rf_snapshot.json` directly.

## Run

Start Globe-C2 on loopback, then:

```powershell
py bridge\triad_bridge.py `
  --snapshot "<TRIAD_PROJECT>\Saved\SingaporeSensorFusion\latest_layered_snapshot.json" `
  --c2 http://127.0.0.1:3000 `
  --api-profile globe
```

Environment alternatives:

- `TRIAD_UNREAL_PROJECT`: host Unreal project directory;
- `TRIAD_LAYERED_SNAPSHOT`: exact layered snapshot path;
- `TRIAD_C2_URL`: C2 base URL.

`--api-profile standard` sends only the basic supplied C2 contract. `--api-profile globe` enables the richer TRIAD context, evidence, radar/PTZ, and frame fields expected by the current Globe-C2 integration.

The default stale threshold is five seconds. If the snapshot becomes stale or forwarding fails, the bridge clears active cues instead of keeping an old operational picture.

## Test

```powershell
python -m unittest bridge.test_triad_bridge -v
```

Globe-C2's local mutation endpoints currently have no authentication. Bind the development server to `127.0.0.1`, not a LAN or public interface.
