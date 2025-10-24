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
from typing import List, Dict, Optional, Tuple, Set

# Global configuration
COMPILER = "shiro"        # can be overriden by args
COMPILE_TIMEOUT = 10      # in seconds
RUN_TIMEOUT = 0.5         # in seconds
USE_VALGRIND = False
VALGRIND_TIME_FACTOR = 5  # timeouts multiplied by this
VALGRIND_CMD = ["valgrind", "--error-exitcode=1", "--leak-check=full",
                "--show-leak-kinds=all", "--errors-for-leak-kinds=all",
                "--track-origins=yes", "--read-var-info=yes",
                "--expensive-definedness-checks=yes", "--exit-on-first-error=yes",
                "--suppressions=src/tests/valgrind.supp"]


class TestInstruction:
    """Base class for test instructions"""
    def __init__(self, args: str, line_number: int = 0):
        self.args = args
        self.line_number = line_number

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
        # Extract custom executable path if provided
        custom_path = self.args.strip().strip('"').strip("'") if self.args else None

        if not context.compile(custom_path):
            return False
        return context.run(custom_path)


class OptionsInstruction(TestInstruction):
    """Set compiler options"""
    def execute(self, context: 'TestContext') -> bool:
        context.compiler_options = self.args.strip()
        return True


class ErrorInstruction(TestInstruction):
    """Expect a compiler error matching the regex at the instruction's line"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"')

        # Find all error messages at this line number
        matching_errors = []
        for line_num, msg in context.errors_by_line.get(self.line_number, []):
            if re.search(pattern, msg):
                matching_errors.append((line_num, msg))

        if not matching_errors:
            context.error_message = (
                f"Expected error pattern `{pattern}` at line {self.line_number} not found"
            )
            return False

        # Mark these errors as covered
        for line_num, msg in matching_errors:
            context.covered_errors.add((line_num, msg))

        return True


class WarningInstruction(TestInstruction):
    """Expect a compiler warning matching the regex at the instruction's line"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"')

        # Find all warning messages at this line number
        matching_warnings = []
        for line_num, msg in context.warnings_by_line.get(self.line_number, []):
            if re.search(pattern, msg):
                matching_warnings.append((line_num, msg))

        if not matching_warnings:
            context.error_message = (
                f"Expected warning pattern `{pattern}` at line {self.line_number} not found"
            )
            return False

        # Mark these warnings as covered
        for line_num, msg in matching_warnings:
            context.covered_warnings.add((line_num, msg))

        return True


class StdoutInstruction(TestInstruction):
    """Expect program output matching the regex on the next line"""
    def execute(self, context: 'TestContext') -> bool:
        pattern = self.args.strip().strip('"').strip("'")

        # Check if there are more lines available
        if context.stdout_cursor >= len(context.stdout_lines):
            context.error_message = (
                f"Expected stdout pattern '{pattern}' but program produced no more output"
            )
            return False

        # Get the next line
        actual_line = context.stdout_lines[context.stdout_cursor]

        # Match pattern against this line
        if not re.search(pattern, actual_line):
            context.error_message = (
                f"Expected stdout pattern '{pattern}' but got: {repr(actual_line)}"
            )
            return False

        # Move to next line
        context.stdout_cursor += 1
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
    def __init__(self, test_target: Path):
        self.test_target = test_target
        self.is_directory = test_target.is_dir()
        self.directory_path = test_target if self.is_directory else test_target.parent
        self.compiler_options = ""
        self.compile_output = ""
        self.compile_returncode = None
        self.run_output = ""
        self.run_returncode = None
        self.error_message = ""
        self.executable = None  # Temp file path when no custom path is specified
        self.custom_executable_path = None  # User-specified executable path from !run
        self.has_error_instruction = False

        # Track errors and warnings by line number
        self.errors_by_line: Dict[int, List[Tuple[int, str]]] = {}
        self.warnings_by_line: Dict[int, List[Tuple[int, str]]] = {}
        self.all_errors: List[Tuple[int, str]] = []
        self.all_warnings: List[Tuple[int, str]] = []
        self.covered_errors: Set[Tuple[int, str]] = set()
        self.covered_warnings: Set[Tuple[int, str]] = set()

        # Track stdout lines for sequential validation
        self.stdout_lines: List[str] = []
        self.stdout_cursor: int = 0

    def parse_compiler_messages(self):
        """Parse compiler output for errors and warnings"""
        # Remove color from compiler output
        ansi_escape = re.compile(r'\x1b\[[0-9;]*m')
        clean_output = ansi_escape.sub('', self.compile_output)

        # Pattern: path/to/filename:line:col error: message
        # or: path/to/filename:line:col warning: message
        if self.is_directory:
            # Match any .shiro file in the directory
            error_pattern = re.compile(
                r'([^/\\]+\.shiro):(\d+):\d+:\s+error:\s*(.*)$',
                re.MULTILINE
            )
            warning_pattern = re.compile(
                r'([^/\\]+\.shiro):(\d+):\d+:\s+warning:\s*(.*)$',
                re.MULTILINE
            )
        else:
            # Match specific filename
            filename = self.test_target.name
            error_pattern = re.compile(
                rf'(?:^|[/\\]){re.escape(filename)}:(\d+):\d+:\s+error:\s*(.*)$',
                re.MULTILINE
            )
            warning_pattern = re.compile(
                rf'(?:^|[/\\]){re.escape(filename)}:(\d+):\d+:\s+warning:\s*(.*)$',
                re.MULTILINE
            )

        # Find all errors
        for match in error_pattern.finditer(clean_output):
            if self.is_directory:
                # Groups: (filename, line_num, message)
                line_num = int(match.group(2))
                message = match.group(3).strip()
            else:
                # Groups: (line_num, message)
                line_num = int(match.group(1))
                message = match.group(2).strip()
            self.all_errors.append((line_num, message))
            if line_num not in self.errors_by_line:
                self.errors_by_line[line_num] = []
            self.errors_by_line[line_num].append((line_num, message))

        # Find all warnings
        for match in warning_pattern.finditer(clean_output):
            if self.is_directory:
                # Groups: (filename, line_num, message)
                line_num = int(match.group(2))
                message = match.group(3).strip()
            else:
                # Groups: (line_num, message)
                line_num = int(match.group(1))
                message = match.group(2).strip()
            self.all_warnings.append((line_num, message))
            if line_num not in self.warnings_by_line:
                self.warnings_by_line[line_num] = []
            self.warnings_by_line[line_num].append((line_num, message))

    def compile(self, custom_path: Optional[str] = None) -> bool:
        """Compile the file or directory. Returns True if compilation succeeded as expected."""
        if custom_path:
            # User specified where executable will be
            self.custom_executable_path = custom_path
            cmd = [COMPILER, str(self.test_target)]
        else:
            # Create temp file for executable
            with tempfile.NamedTemporaryFile(suffix='', delete=False) as tmp:
                self.executable = tmp.name
            cmd = [COMPILER, str(self.test_target), "-o", self.executable]
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
            self.compile_output += "\n"
            self.compile_returncode = result.returncode

            # Parse compiler messages
            self.parse_compiler_messages()

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

    def check_uncovered_messages(self) -> bool:
        """Check if there are any uncovered errors or warnings"""
        uncovered_errors = [
            (line, msg) for line, msg in self.all_errors
            if (line, msg) not in self.covered_errors
        ]
        uncovered_warnings = [
            (line, msg) for line, msg in self.all_warnings
            if (line, msg) not in self.covered_warnings
        ]

        if uncovered_errors:
            self.error_message = "Uncovered errors found:\n"
            for line, msg in uncovered_errors:
                self.error_message += f"  Line {line}: {msg}\n"
            return False

        if uncovered_warnings:
            self.error_message = "Uncovered warnings found:\n"
            for line, msg in uncovered_warnings:
                self.error_message += f"  Line {line}: {msg}\n"
            return False

        return True

    def check_uncovered_stdout(self) -> bool:
        """Check if there are any uncovered stdout lines"""
        if self.stdout_cursor < len(self.stdout_lines):
            uncovered = self.stdout_lines[self.stdout_cursor:]
            self.error_message = "Uncovered stdout found:\n"
            for i, line in enumerate(uncovered, start=self.stdout_cursor + 1):
                self.error_message += f"  Line {i}: {repr(line)}\n"
            return False

        return True

    def run(self, custom_path: Optional[str] = None) -> bool:
        """Run the compiled executable. Returns True if run succeeded as expected."""
        # Determine which executable to run
        executable_path = custom_path if custom_path else self.executable

        if not executable_path or not os.path.exists(executable_path):
            self.error_message = f"No executable to run (looking for: {executable_path})"
            return False

        try:
            cmd = [executable_path]
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

            # Split stdout into lines for sequential validation
            self.stdout_lines = self.run_output.splitlines()

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
            # Clean up temp executable (not custom paths)
            if self.executable and not custom_path and os.path.exists(self.executable):
                try:
                    os.remove(self.executable)
                except:
                    pass

    def cleanup(self):
        """Clean up any temporary files"""
        # Only delete temp files (self.executable), not custom paths
        if self.executable and os.path.exists(self.executable):
            try:
                os.remove(self.executable)
            except:
                pass


def parse_instructions_from_file(filepath: Path) -> List[Tuple[str, str, int]]:
    """Parse test instructions from a single file. Returns list of (instruction_name, args, line_number) tuples."""
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

                instructions.append((instruction_name, args, line_num))

    return instructions


def parse_instructions(test_target: Path) -> List[Tuple[str, str, int]]:
    """Parse test instructions from a file or directory. Returns list of (instruction_name, args, line_number) tuples."""
    if test_target.is_file():
        # Single file: parse it directly
        return parse_instructions_from_file(test_target)
    elif test_target.is_dir():
        # Directory: recursively find all .shiro files and collect instructions
        instructions = []
        for shiro_file in sorted(test_target.rglob("*.shiro")):
            instructions.extend(parse_instructions_from_file(shiro_file))
        return instructions
    else:
        return []


def run_test(test_target: Path) -> bool:
    """Run a single test (file or directory). Returns True if test passed."""
    instructions = parse_instructions(test_target)

    # Format display name based on type
    target_type = "project" if test_target.is_dir() else "file"
    display_name = f"{test_target} ({target_type})"

    if not instructions:
        print(f"  SKIP: {display_name} (no instructions)")
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

    for inst_name, args, line_num in instructions:
        if inst_name not in INSTRUCTION_REGISTRY:
            print(f"  ERROR: {display_name} - Unknown instruction: {inst_name}")
            return False

        inst_class = INSTRUCTION_REGISTRY[inst_name]
        inst_obj = inst_class(args, line_num)
        instruction_objects[inst_name].append(inst_obj)

    # Validate single-instance instructions
    for inst_type in ['compile', 'run', 'options']:
        if len(instruction_objects[inst_type]) > 1:
            print(f"  ERROR: {display_name} - Multiple '{inst_type}' instructions not allowed")
            return False

    # Create test context
    context = TestContext(test_target)
    context.has_error_instruction = len(instruction_objects['error']) > 0

    try:
        # Execute options first (if present)
        if instruction_objects['options']:
            if not instruction_objects['options'][0].execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                return False

        # Execute compile or run (run implies compile)
        if instruction_objects['run']:
            if not instruction_objects['run'][0].execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                print(f"    Run output:\n{context.run_output}")
                return False
        elif instruction_objects['compile']:
            if not instruction_objects['compile'][0].execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Validate error expectations
        for error_inst in instruction_objects['error']:
            if not error_inst.execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Validate warning expectations
        for warning_inst in instruction_objects['warning']:
            if not warning_inst.execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                print(f"    Compile output:\n{context.compile_output}")
                return False

        # Check for uncovered errors/warnings
        if not context.check_uncovered_messages():
            print(f"  FAIL: {display_name}")
            print(f"    {context.error_message}")
            print(f"    Compile output:\n{context.compile_output}")
            return False

        # Validate stdout expectations
        for stdout_inst in instruction_objects['stdout']:
            if not stdout_inst.execute(context):
                print(f"  FAIL: {display_name}")
                print(f"    {context.error_message}")
                print(f"    Program output:\n{context.run_output}")
                return False

        # Check for uncovered stdout
        if not context.check_uncovered_stdout():
            print(f"  FAIL: {display_name}")
            print(f"    {context.error_message}")
            print(f"    Program output:\n{context.run_output}")
            return False

        print(f"  PASS: {display_name}")
        return True

    finally:
        context.cleanup()


def find_project_directories(directory: Path, recursive: bool) -> Set[Path]:
    """Find all directories containing shiro.toml, excluding nested projects."""
    if not recursive:
        # Only check immediate subdirectories
        projects = set()
        for item in directory.iterdir():
            if item.is_dir() and (item / "shiro.toml").exists():
                projects.add(item)
        return projects

    # Find all directories with shiro.toml
    all_projects = set()
    for toml_file in directory.rglob("shiro.toml"):
        all_projects.add(toml_file.parent)

    # Filter out nested projects (keep only top-level ones)
    top_level_projects = set()
    for project in all_projects:
        # Check if any other project is a parent of this one
        is_nested = False
        for other_project in all_projects:
            if other_project != project and project.is_relative_to(other_project):
                is_nested = True
                break
        if not is_nested:
            top_level_projects.add(project)

    return top_level_projects


def find_test_targets(directory: Path, recursive: bool) -> List[Path]:
    """Find all test targets: individual .shiro files and project directories with shiro.toml."""
    # Find project directories first
    project_dirs = find_project_directories(directory, recursive)

    # Find all .shiro files
    if recursive:
        shiro_files = set(directory.rglob("*.shiro"))
    else:
        shiro_files = set(directory.glob("*.shiro"))

    # Exclude .shiro files that are inside project directories
    standalone_files = []
    for shiro_file in shiro_files:
        is_in_project = False
        for project_dir in project_dirs:
            if shiro_file.is_relative_to(project_dir):
                is_in_project = True
                break
        if not is_in_project:
            standalone_files.append(shiro_file)

    # Combine and sort: project directories first, then standalone files
    test_targets = sorted(project_dirs) + sorted(standalone_files)
    return test_targets


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
        help="Path to compiler, default assumes `shiro` findable via $PATH"
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

    # Find all test targets (files and project directories)
    test_targets = find_test_targets(args.directory, args.recursive)

    if not test_targets:
        print(f"No test targets found in {args.directory}")
        sys.exit(0)

    print(f"Running {len(test_targets)} test(s)...\n")

    # Run tests
    passed = 0
    for test_target in test_targets:
        if run_test(test_target):
            passed += 1
        else:
            # Stop on first failure
            print(f"\nStopping after first failure.")
            print(f"Results: {passed}/{len(test_targets)} passed")
            sys.exit(1)

    print(f"\nAll tests passed! ({passed}/{len(test_targets)})")
    sys.exit(0)


if __name__ == "__main__":
    main()
