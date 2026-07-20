# grugtorio

TODO: Add intro

## Running

```sh
cmake -B build
cmake --build build
build/grugtorio
```

## Examples

Every test serves as an example:

```sh
build/grugtorio --input-save tests/belt-zigzag/save.grugtorio.json
```

### grugtorio options

| Option                                 | Description                                 |
| -------------------------------------- | ------------------------------------------- |
| `--input-save [.grugtorio.json path]`  | Path to load a grugtorio save from.         |
| `--output-save [.grugtorio.json path]` | Path to write a grugtorio save to on exit.  |
| `--ticks [N]`                          | Number of ticks to simulate before exiting. |

## Recipes

| Item | Method | Ingredients |
|---|---|---|
| **Automation** | Craft | Copper plate + Iron gear wheel |
| Iron gear wheel | Craft | 2 Iron plate |
| Iron plate | Smelt | 1 Iron ore |
| Copper plate | Smelt | 1 Copper ore |
| **Logistic** | Craft | Iron gear wheel + Transport belt |
| Transport belt | Craft | Iron plate + Iron gear wheel |
| **Chemical** | Craft | Engine unit + Advanced circuit |
| Engine unit | Craft | Iron gear wheel + Pipe + Steel plate |
| Pipe | Craft | 1 Iron plate |
| Steel plate | Smelt | 5 Iron plate |
| Advanced circuit | Craft | Electronic circuit + Copper cable + Plastic bar |
| Electronic circuit | Craft | Iron plate + Copper cable |
| Copper cable | Craft | 1 Copper plate |
| Plastic bar | Craft | 1 Coal |
| **Production** | Craft | Electric furnace + Productivity module + Rail |
| Electric furnace | Craft | Steel plate + Advanced circuit + Stone brick |
| Stone brick | Smelt | 1 Stone |
| Productivity module | Craft | Advanced circuit + Electronic circuit |
| Rail | Craft | Steel plate + Stone + Iron stick |
| Iron stick | Craft | 1 Iron plate |
| **Utility** | Craft | Flying robot frame + Low density structure + Processing unit |
| Flying robot frame | Craft | Steel plate + Electronic circuit + Iron gear wheel + Battery |
| Battery | Craft | Iron plate + Copper plate |
| Low density structure | Craft | Copper plate + Steel plate + Plastic bar |
| Processing unit | Craft | Electronic circuit + Advanced circuit |
| **Space** | Launch | Rocket part + Satellite |
| Rocket part | Craft | Low density structure + Processing unit |
| Satellite | Craft | Low density structure + Processing unit + Solar panel + Accumulator + Radar |
| Solar panel | Craft | Copper plate + Electronic circuit + Steel plate |
| Accumulator | Craft | Iron plate + Battery |
| Radar | Craft | Electronic circuit + Iron gear wheel + Steel plate |

## Testing

To run the test suite, execute:

```bash
python tests.py
```

You can also run tests with Valgrind by setting the `TEST_RUNNER` environment variable:

```bash
TEST_RUNNER="valgrind --leak-check=full" python tests.py
```

The `tests` directory its tests contain `TODO.md` files whenever their behavior is known to not match Factorio _yet_.

## Running with valgrind

```sh
valgrind --leak-check=full build/grugtorio
```

## Running with AddressSanitizer and UBSan

```sh
cmake -B build -DENABLE_ASAN=ON
```

If you get leaks from Raylib then you should set this environment variable before configuring:

```sh
export LSAN_OPTIONS="suppressions=lsan.supp:print_suppressions=0"
```
