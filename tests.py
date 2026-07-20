import argparse
import difflib
import json
import os
import subprocess
import sys

# Configuration
EXECUTABLE = "build/grugtorio"
TESTS_DIR = "tests"
OUTPUT_FILE = "build/test-output.grugtorio.json"


def run_tests():
    parser = argparse.ArgumentParser(description="Run grugtorio tests.")
    parser.add_argument(
        "--test",
        metavar="NAME",
        help="Run only the specified test",
    )
    args = parser.parse_args()

    # Ensure executable exists
    if not os.path.exists(EXECUTABLE):
        sys.exit(f"Error: Executable not found at {EXECUTABLE}")

    # Ensure tests directory exists
    if not os.path.exists(TESTS_DIR):
        sys.exit(f"Error: Directory '{TESTS_DIR}' not found")

    # Get optional runner (e.g., Valgrind) from environment variable
    runner = os.environ.get("TEST_RUNNER", "").split()

    if args.test:
        dir_path = os.path.join(TESTS_DIR, args.test)
        if not os.path.isdir(dir_path):
            sys.exit(f"Error: Test '{args.test}' not found")
        test_dirs = [args.test]
    else:
        test_dirs = [
            d
            for d in os.listdir(TESTS_DIR)
            if os.path.isdir(os.path.join(TESTS_DIR, d))
        ]

    for test_name in sorted(test_dirs):
        dir_path = os.path.join(TESTS_DIR, test_name)
        input_save = os.path.join(dir_path, "save.grugtorio.json")
        expected_save = os.path.join(dir_path, "expected.grugtorio.json")
        settings_file = os.path.join(dir_path, "settings.json")

        # Check for missing files
        if not os.path.exists(input_save):
            sys.exit(f"Error: Test '{test_name}' is missing {input_save}")
        if not os.path.exists(expected_save):
            sys.exit(f"Error: Test '{test_name}' is missing {expected_save}")
        if not os.path.exists(settings_file):
            sys.exit(f"Error: Test '{test_name}' is missing {settings_file}")

        # Validate settings.json
        try:
            with open(settings_file, "r") as f:
                settings = json.load(f)
            ticks = settings.get("ticks")
            if ticks is None:
                sys.exit(
                    f"Error: Test '{test_name}' settings.json is missing the 'ticks' key"
                )
        except json.JSONDecodeError:
            sys.exit(f"Error: Test '{test_name}' has malformed JSON in settings.json")

        # Run the test with potential runner prefix
        cmd = runner + [
            EXECUTABLE,
            "--input-save",
            input_save,
            "--output-save",
            OUTPUT_FILE,
            "--ticks",
            str(ticks),
        ]

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            sys.exit(
                f"Error: Test '{test_name}' failed to execute (return code {result.returncode})\n"
                f"Stderr: {result.stderr}"
            )

        # Load data for comparison
        try:
            with open(OUTPUT_FILE, "r") as f:
                output_data = json.load(f)
            with open(expected_save, "r") as f:
                expected_data = json.load(f)
        except json.JSONDecodeError:
            sys.exit(f"Error: Failed to parse JSON for test '{test_name}'")

        # Prepare strings for diffing
        output_str = json.dumps(output_data, indent=2, sort_keys=True)
        expected_str = json.dumps(expected_data, indent=2, sort_keys=True)

        # Output mismatch check
        if output_str != expected_str:
            diff = difflib.unified_diff(
                expected_str.splitlines(),
                output_str.splitlines(),
                fromfile="expected",
                tofile="actual",
                lineterm="",
            )
            diff_text = "\n".join(diff)

            sys.exit(
                f"Test '{test_name}' failed.\n\n"
                f"Error: Output mismatch.\n\n"
                f"The full actual output file can be found at: {OUTPUT_FILE}\n\n"
                f"Diff:\n{diff_text}"
            )

        print(f"Test '{test_name}' passed.")

    # Cleanup temporary output file only after all tests succeed
    if os.path.exists(OUTPUT_FILE):
        os.remove(OUTPUT_FILE)

    print("All tests passed successfully.")


if __name__ == "__main__":
    run_tests()
