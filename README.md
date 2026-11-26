# Challenge Bolt Streak

For OpenRAC Going Commando NTSC-U, PAL or NTSC-U v2.00 on Windows x64. Import the ZIP with the OpenRAC launcher and place this add-on after NG++ when using both.

## Damage effect

- **Default** resets the multiplier when you take damage.
- **Lenient** removes one multiplier step when you take nonlethal damage.
- **On Death** keeps the multiplier and kill progress through nonlethal damage. Death resets the streak to ×1.

## Growth curve

| Setting | Cap by Challenge Mode run | Gain rate by run |
| --- | --- | --- |
| Default | 20 each run | 1× each run |
| Linear | 20, 40, 60, 80, … | 1×, 2×, 3×, 4×, … |
| Proposed | 20, 50, 100, 200, 300, 400, 500, … | 1×, 1.75×, 2.5×, 4×, 6×, 8×, 10×, … |

After run 7, Proposed adds 100 to the cap and 2× to the gain rate each run. One qualifying kill adds credit at the selected rate. Each step uses the game's threshold through ×20 and ten stock kills' worth of credit after that. On Death and Proposed are selected on first installation.

Above ×20, the add-on shows its own streak on the HUD. It passes the extra streak value into enemy bursts and keeps OpenRAC and NG++ in charge of coin creation, payout limits and rare-coin handling. The extended portion of the streak is held for the ongoing play session.

Challenge Environment Bolts 0.2.0 reads the full active streak from this version on Windows.

PAL was tested in-game. The two NTSC-U revisions use the matching OpenRAC data addresses. In-game tests for those revisions are pending.

## Build

The release uses OpenRAC mod API 1.36. The build requires CMake 3.24 or newer, a C++17 compiler, and Python 3 for fetching and packaging the SDK.

```sh
python tools/fetch_sdk.py
cmake -S . -B build
cmake --build build --config Release
python .sdk/openrac-mod-sdk-1.36.0/tools/package_mod.py build/package/rac2_challenge_bolt_streak -o dist
```

Run the tests with `ctest --test-dir build -C Release --output-on-failure`.
