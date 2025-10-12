#!/usr/bin/env python3
"""
Test-framework for .shiro files.
Reads test instructions from comments in the format: //! <instruction>
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Dict, Optional, Tuple

# Global configuration
COMPILER = "shiroc"       # can be overriden by args
COMPILE_TIMEOUT = 10      # in seconds
RUN_TIMEOUT = 0.5         # in seconds
USE_VALGRIND = False
VALGRIND_TIME_FACTOR = 5  # timeouts multiplied by this
VALGRIND_CMD = ["valgrind", "--error-exitcode=1", "--leak-check=full",
                "--show-leak-kinds=all", "--errors-for-leak-kinds=all",
                "--track-origins=yes", "--read-var-info=yes",
                "--expensive-definedness-checks=yes", "--exit-on-first-error=yes"]


class TestInstruction:
    """Base class for test instructions"""
    def __init__(self, args: str):
        self.args = args

    def execute(self, context: 'TestContext') -> bool:
        """Execute the instruction. Returns True if successful."""
        raise NotImplementedError


class CompileInstruction(TestInstruction):
    """Compile the file"""
    def execute(self, context: 'TestContext') -> bool:
        return context.compile()


class RunInstruction(TestInstruction):
    """Compile and run the file"""
    def execute(self, context: 'TestContext') -> bool:
        if not context.compile():
            return False
        return context.run()


class OptionsInstruction(TestInstruction):
    """Set compiler options"""
    def execute(self, context: 'TestContext') -> bool:
        context.compiler_options = self.args.strip()
        return True


class ErrorInstruction(TestInstruction):
    """Expect a compiler error matching the regex"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"').strip("'")
        if not re.search(pattern, context.compile_output):
            context.error_message = f"Expected error pattern not found: {pattern}"
            return False
        return True


class WarningInstruction(TestInstruction):
    """Expect a compiler warning matching the regex"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"').strip("'")
        if not re.search(pattern, context.compile_output):
            context.error_message = f"Expected warning pattern not found: {pattern}"
            return False
        return True


class StdoutInstruction(TestInstruction):
    """Expect program output matching the regex"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"').strip("'")
        if not re.search(pattern, context.run_output):
            context.error_message = f"Expected stdout pattern not found: {pattern}"
            return False
        return True


# Instruction registry
INSTRUCTION_REGISTRY = {
    'compile': CompileInstruction,
    'run': RunInstruction,
    'options': OptionsInstruction,
    'error': ErrorInstruction,
    'warning': WarningInstruction,
    'stdout': StdoutInstruction,
}

# Instructions that require arguments
REQUIRED_ARGS_INSTRUCTIONS = {'options', 'error', 'warning', 'stdout'}


class TestContext:
    """Context for executing a test"""
    def __init__(self, filepath: Path):
        self.filepath = filepath
        self.compiler_options = ""
        self.compile_output = ""
        self.compile_returncode = None
        self.run_output = ""
        self.run_returncode = None
        self.error_message = ""
        self.executable = None
        self.has_error_instruction = False

    def compile(self) -> bool:
        """Compile the file. Returns True if compilation succeeded as expected."""
        with tempfile.NamedTemporaryFile(suffix='', delete=False) as tmp:
            self.executable = tmp.name

        cmd = [COMPILER, str(self.filepath), "-o", self.executable]
        if self.compiler_options:
            cmd.extend(self.compiler_options.split())
        if USE_VALGRIND:
            cmd = VALGRIND_CMD + cmd

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=COMPILE_TIMEOUT
            )
            self.compile_output = result.stdout + result.stderr
            self.compile_returncode = result.returncode

            # If we have error instructions, we expect compilation to fail
            if self.has_error_instruction:
                return True  # We'll validate the error later

            # Otherwise, compilation should succeed
            if result.returncode != 0:
                self.error_message = "Compilation failed unexpectedly"
                return False

            return True
        except subprocess.TimeoutExpired:
            self.error_message = "Compilation timed out"
            return False
        except Exception as e:
            self.error_message = f"Compilation error: {e}"
            return False

    def run(self) -> bool:
        """Run the compiled executable. Returns True if run succeeded as expected."""
        if not self.executable or not os.path.exists(self.executable):
            self.error_message = "No executable to run"
            return False

        try:
            cmd = [self.executable]
            if USE_VALGRIND:
                cmd = VALGRIND_CMD + cmd

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=RUN_TIMEOUT
            )
            self.run_output = result.stdout
            self.run_returncode = result.returncode

            if result.returncode != 0:
                self.error_message = f"Program exited with non-zero code: {result.returncode}"
                return False

            return True
        except subprocess.TimeoutExpired:
            self.error_message = f"Program execution timed out (>{RUN_TIMEOUT*1000}ms)"
            return False
        except Exception as e:
            self.error_message = f"Execution error: {e}"
            return False
        finally:
            # Clean up executable
            if self.executable and os.path.exists(self.executable):
                try:
                    os.remove(self.executable)
                except:
                    pass

    def cleanup(self):
        """Clean up any temporary files"""
        if self.executable and os.path.exists(self.executable):
            try:
                os.remove(self.executable)
            except:
                pass


def parse_instructions(filepath: Path) -> List[Tuple[str, str]]:
    """Parse test instructions from a file. Returns list of (instruction_name, args) tuples."""
    instructions = []

    with open(filepath, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            if '//!' in line:
                # Extract instruction after "//! "
                idx = line.find('//!')
                instruction_text = line[idx + 3:].strip()
                if not instruction_text:
                    continue

                # Check if instruction has a colon
                has_colon = ':' in instruction_text

                if has_colon:
                    # Split by colon to separate instruction name and args
                    colon_idx = instruction_text.index(':')
                    instruction_name = instruction_text[:colon_idx].strip().lower()
                    args = instruction_text[colon_idx + 1:].strip()
                else:
                    # No colon means no arguments expected
                    instruction_name = instruction_text.split()[0].lower() if instruction_text else ""
                    args = ""

                if not instruction_name:
                    continue

                # Validate that required-args instructions have arguments
                if instruction_name in REQUIRED_ARGS_INSTRUCTIONS:
                    if not args:
                        print(f"  WARNING: {filepath}:{line_num} - Instruction '{instruction_name}' requires arguments but none provided. Ignoring instruction.")
                        continue

                instructions.append((instruction_name, args))

    return instructions


def run_test(filepath: Path) -> bool:
    """Run a single test file. Returns True if test passed."""
    instructions = parse_instructions(filepath)

    if not instructions:
        print(f"  SKIP: {filepath} (no instructions)")
        return True

    # Group instructions by type
    instruction_objects = {
        'compile': [],
        'run': [],
        'options': [],
        'error': [],
        'warning': [],
        'stdout': [],
    }

    for inst_name, args in instructions:
        if inst_name not in INSTRUCTION_REGISTRY:
            print(f"  ERROR: {filepath} - Unknown instruction: {inst_name}")
            return False

        inst_class = INSTRUCTION_REGISTRY[inst_name]
        inst_obj = inst_class(args)
        instruction_objects[inst_name].append(inst_obj)

    # Validate single-instance instructions
    for inst_type in ['compile', 'run', 'options']:
        if len(instruction_objects[inst_type]) > 1:
            print(f"  ERROR: {filepath} - Multiple '{inst_type}' instructions not allowed")
            return False

    # Create test context
    context = TestContext(filepath)
    context.has_error_instruction = len(instruction_objects['error']) > 0

    try:
        # Execute options first (if present)
        if instruction_objects['options']:
            if not instruction_objects['options'][0].execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                return False

        # Execute compile or run (run implies compile)
        if instruction_objects['run']:
            if not instruction_objects['run'][0].execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                print(f"    Run output:\n{context.run_output}")
                return False
        elif instruction_objects['compile']:
            if not instruction_objects['compile'][0].execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Validate error expectations
        for error_inst in instruction_objects['error']:
            if not error_inst.execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Validate warning expectations
        for warning_inst in instruction_objects['warning']:
            if not warning_inst.execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Validate stdout expectations
        for stdout_inst in instruction_objects['stdout']:
            if not stdout_inst.execute(context):
                print(f"  FAIL: {filepath}")
                print(f"    {context.error_message}")
                print(f"    Program output:\n{context.run_output}")
                return False

        print(f"  PASS: {filepath}")
        return True

    finally:
        context.cleanup()


def find_test_files(directory: Path, recursive: bool) -> List[Path]:
    """Find all .shiro files in the directory."""
    if recursive:
        return sorted(directory.rglob("*.shiro"))
    else:
        return sorted(directory.glob("*.shiro"))


def main():
    parser = argparse.ArgumentParser(
        description="Test framework for shiro files"
    )
    parser.add_argument(
        "directory",
        type=Path,
        help="Directory containing test files"
    )
    parser.add_argument(
        "-r", "--recursive",
        action="store_true",
        help="Search recursively for test files"
    )
    parser.add_argument(
        "-c", "--compiler",
        type=Path,
        help="Path to compiler, default assumes `shiroc` findable via $PATH"
    )
    parser.add_argument(
        "--valgrind",
        action="store_true",
        help="Rune compiler and executable under valgrind"
    )

    args = parser.parse_args()

    if args.compiler:
        global COMPILER
        COMPILER = args.compiler

    if args.valgrind:
        global USE_VALGRIND, COMPILE_TIMEOUT, RUN_TIMEOUT
        USE_VALGRIND = True
        COMPILE_TIMEOUT *= VALGRIND_TIME_FACTOR
        RUN_TIMEOUT *= VALGRIND_TIME_FACTOR

    if not args.directory.is_dir():
        print(f"Error: {args.directory} is not a directory")
        sys.exit(1)

    # Find all test files
    test_files = find_test_files(args.directory, args.recursive)

    if not test_files:
        print(f"No .shiro files found in {args.directory}")
        sys.exit(0)

    print(f"Running {len(test_files)} test(s)...\n")

    # Run tests
    passed = 0
    for test_file in test_files:
        if run_test(test_file):
            passed += 1
        else:
            # Stop on first failure
            print(f"\nStopping after first failure.")
            print(f"Results: {passed}/{len(test_files)} passed")
            sys.exit(1)

    print(f"\nAll tests passed! ({passed}/{len(test_files)})")
    sys.exit(0)


if __name__ == "__main__":
    main()
