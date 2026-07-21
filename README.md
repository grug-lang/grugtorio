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
| `--input-save <.grugtorio.json path>`  | Path to load a grugtorio save from.         |
| `--output-save <.grugtorio.json path>` | Path to write a grugtorio save to on exit.  |
| `--ticks <N>`                          | Number of ticks to simulate before exiting. |

## 7 buildings

- Electric mining drill
- Transport belt
- Inserter
- Stone furnace
- Assembling machine 1
- Lab
- Rocket silo

## 33 recipes

| Item | Method | Ingredients |
|---|---|---|
| **Automation** | Craft | 1 Copper plate + 1 Iron gear wheel |
| Copper plate | Smelt | 1 Copper ore |
| Iron gear wheel | Craft | 2 Iron plate |
| Iron plate | Smelt | 1 Iron ore |
| **Logistic** | Craft | 1 Iron gear wheel + 1 Transport belt |
| Transport belt | Craft | 1 Iron gear wheel + 1 Iron plate |
| Inserter | Craft | 1 Electronic circuit + 1 Iron gear wheel + 1 Iron plate |
| Electronic circuit | Craft | 3 Copper cable + 1 Iron plate |
| Copper cable | Craft | 0.5 Copper plate |
| **Chemical** | Craft | 3 Advanced circuit + 2 Engine unit |
| Advanced circuit | Craft | 4 Copper cable + 2 Electronic circuit + 2 Plastic bar |
| Plastic bar | Craft | 0.5 Coal |
| Engine unit | Craft | 1 Iron gear wheel + 2 Pipe + 1 Steel plate |
| Pipe | Craft | 1 Iron plate |
| Steel plate | Smelt | 5 Iron plate |
| **Production** | Craft | 30 Rail + 1 Electric furnace + 1 Productivity module |
| Rail | Craft | 0.5 Iron stick + 0.5 Steel plate + 0.5 Stone |
| Iron stick | Craft | 0.5 Iron plate |
| Electric furnace | Craft | 5 Advanced circuit + 10 Steel plate + 10 Stone brick |
| Stone brick | Smelt | 2 Stone |
| Productivity module | Craft | 5 Advanced circuit + 5 Electronic circuit |
| **Utility** | Craft | 2 Processing unit + 1 Flying robot frame + 3 Low density structure |
| Processing unit | Craft | 2 Advanced circuit + 20 Electronic circuit |
| Flying robot frame | Craft | 2 Battery + 1 Electric engine unit + 3 Electronic circuit + 1 Steel plate |
| Electric engine unit | Craft | 2 Electronic circuit + 1 Engine unit |
| Battery | Craft | 1 Copper plate + 1 Iron plate |
| Low density structure | Craft | 20 Copper plate + 5 Plastic bar + 2 Steel plate |
| **Space** | Launch | 100 Rocket part + 1 Satellite |
| Rocket part | Craft | 10 Low density structure + 10 Processing unit |
| Satellite | Craft | 100 Accumulator + 100 Low density structure + 100 Processing unit + 5 Radar + 100 Solar panel |
| Accumulator | Craft | 5 Battery + 2 Iron plate |
| Solar panel | Craft | 5 Copper plate + 15 Electronic circuit + 5 Steel plate |
| Radar | Craft | 5 Electronic circuit + 5 Iron gear wheel + 10 Iron plate |

## Testing

To run the test suite, execute:

```bash
python tests.py
```

You can also run tests with Valgrind by setting the `TEST_RUNNER` environment variable:

```bash
TEST_RUNNER="valgrind --leak-check=full" python tests.py
```

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
