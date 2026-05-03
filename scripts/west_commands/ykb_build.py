#!/usr/bin/env python3

import argparse
import subprocess
from pathlib import Path

from west.commands import WestCommand


class YkbBuild(WestCommand):

    def __init__(self):
        super().__init__(
            "ykb-build",
            "smart YKB board build",
            "Build a YKB board, resolving split boards to the left-half sysbuild entrypoint.",
            accepts_unknown_args=True,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description,
            formatter_class=argparse.RawDescriptionHelpFormatter,
        )
        parser.add_argument(
            "board",
            help="Board name or fully qualified board target",
        )
        parser.add_argument(
            "-s",
            "--source-dir",
            default="app",
            help=
            "Application source directory relative to the repository root (default: %(default)s)",
        )
        parser.add_argument(
            "-d",
            "--build-dir",
            help="Optional build directory passed through to `west build -d`",
        )
        parser.add_argument(
            "-p",
            "--pristine",
            action="store_true",
            help="Build with `--pristine`",
        )
        parser.add_argument(
            "--debug",
            action="store_true",
            help="Apply the debug config fragment at app/conf/debug.conf",
        )
        parser.add_argument(
            "-n",
            "--dry-run",
            action="store_true",
            help=
            "Print the resolved board and build command without executing it",
        )
        return parser

    def do_run(self, args, unknown_args):
        repo_root = Path(__file__).resolve().parents[2]
        source_dir = Path(args.source_dir)
        if not source_dir.is_absolute():
            source_dir = (repo_root / source_dir).resolve()

        if not source_dir.exists():
            self.die(f"source directory does not exist: {source_dir}")

        resolved_board = self._resolve_board(repo_root, args.board)

        cmd = ["west", "build", str(source_dir), "-b", resolved_board]
        if args.build_dir:
            cmd.extend(["-d", args.build_dir])
        if args.pristine:
            cmd.append("--pristine")

        cmake_args = []
        if args.debug:
            debug_conf = source_dir / "conf" / "debug.conf"
            if not debug_conf.exists():
                self.die(f"debug config does not exist: {debug_conf}")
            cmake_args.append(f"-DEXTRA_CONF_FILE={debug_conf}")

        passthrough_args = list(unknown_args)
        if passthrough_args and passthrough_args[0] == "--":
            passthrough_args = passthrough_args[1:]

        if cmake_args or passthrough_args:
            cmd.append("--")
            cmd.extend(cmake_args)
            cmd.extend(passthrough_args)

        self.inf(f"resolved board: {resolved_board}")
        self.inf("command: " + " ".join(cmd))

        if args.dry_run:
            return

        result = subprocess.run(cmd, cwd=repo_root)
        if result.returncode != 0:
            self.die(f"`west build` failed with exit code {result.returncode}")

    def _resolve_board(self, repo_root: Path, board: str) -> str:
        if "/" in board:
            return board

        board_dirs = sorted((repo_root / "boards").glob(f"*/{board}"))
        if not board_dirs:
            return board
        if len(board_dirs) > 1:
            self.die(f"board '{board}' is ambiguous; matching directories: " +
                     ", ".join(str(path) for path in board_dirs))

        board_dir = board_dirs[0]
        left_variants = []
        right_variants = set()

        for yaml_file in sorted(board_dir.glob(f"{board}_*.yaml")):
            qualifier = yaml_file.stem[len(board) + 1:].replace("_", "/")
            if qualifier.endswith("/left"):
                left_variants.append(qualifier)
            elif qualifier.endswith("/right"):
                right_variants.add(qualifier)

        split_pairs = [
            qualifier for qualifier in left_variants
            if qualifier[:-len("/left")] + "/right" in right_variants
        ]

        if len(split_pairs) == 1:
            return f"{board}/{split_pairs[0]}"
        if len(split_pairs) > 1:
            self.die(
                f"board '{board}' has multiple split left variants; use a fully qualified target. "
                f"Candidates: {', '.join(f'{board}/{qualifier}' for qualifier in split_pairs)}"
            )

        return board
