#!/usr/bin/env python3

import re
import sys
import argparse

from argparse import ArgumentParser
from dataclasses import astuple, dataclass
from datetime import date
from functools import reduce
from operator import concat
from pathlib import Path
from typing import Optional

NOTICE_STR = f"Copyright (c) 2014-{date.today().year} Zuru Tech HK Limited, All rights reserved."

NOTICES = {'c' : f'// {NOTICE_STR}', 'sh' : f'# {NOTICE_STR}'}

CLIKE_EXTENSIONS = {".h", ".hh", ".hpp", ".c", ".cc", ".cpp", ".cxx", ".cs", ".rs", ".java", ".kt", ".go"}
SHLIKE_EXTENSIONS = {".sh", ".pl", ".py", ".pyx", ".pxd", ".rb"}
SKIP_STRINGS = {"// open source, not our code", "# open source, not our code"}

PATTERNS = {
    "c" : re.compile(r"(/\*.*?\*/)|(//[^\n]*\n)", re.S),
    "sh" : re.compile(r"(#[^\n]*\n)", re.S),
}


@dataclass
class ProbedFile:
    tag: str
    file: Path

    def __iter__(self):
        return iter(astuple(self))


def get_arguments() -> argparse.Namespace:
    """CLI parser.
    Returns:
        The folder to use as root.
    """
    parser = ArgumentParser(
        description="Update the copyright notice of all the C, C++ and C# files in the specified folder"
    )
    parser.add_argument(
        "--folder", default="-", help="Root folder that will be traversed"
    )

    parser.add_argument(
        "--files", default=[], type=str, nargs="+", help="The list of files to update"
    )

    args = parser.parse_args()
    return args


def get_file_list(args: list[str]) -> list[str]:
    fileList = []
    if args.folder != "-":
        root = Path(args.folder)
        for ext in CLIKE_EXTENSIONS:
            for f in root.rglob(f"*.{ext}"):
                fileList.append(ProbedFile('c', f))

        for ext in SHLIKE_EXTENSIONS:
            for f in root.rglob(f"*.{ext}"):
                fileList.sh.append(ProbedFile('sh', f))

    fileList.extend(filter(None, map(lambda f: tag_file(Path(f)), args.files)))

    return fileList


def match_comments(tag: str, content: str) -> list[str]:
    comments = PATTERNS[tag].findall(content)

    if comments and tag == "c":
        comments = reduce(concat, [list(filter(None, x)) for x in comments])

    return comments


def should_skip(content: str) -> bool:
    return any(skip_string in content for skip_string in SKIP_STRINGS)


def split_shebang(content: str) -> tuple[str, str]:
    if content.startswith("#!"):
        shebang, content = content.split("\n", 1)

        return shebang + '\n\n', content

    return "", content


def tag_file(file: Path) -> Optional[ProbedFile]:
    if file.suffix in CLIKE_EXTENSIONS:
        return ProbedFile("c", file)
    elif file.suffix in SHLIKE_EXTENSIONS:
        return ProbedFile("sh", file)


def main() -> int:
    args = get_arguments()
    fileList = get_file_list(args)

    empty_files = []
    skipped_files = []

    for (tag, f) in fileList:
        with open(f, "r+", encoding="utf-8") as fp:
            try:
                original_content = content = fp.read()
            except UnicodeDecodeError as e:
                print("UnicodeDecodeError ", e, "\nfile: ", f)
                continue

            if not content:
                empty_files.append(f)
                continue

            if should_skip(content):
                skipped_files.append(f)
                continue

            comments = match_comments(tag, content)
            replaced = False
            if comments:
                for comment in comments:
                    if "copyright" in comment.lower():
                        content = content.replace(comment, f"{NOTICES[tag]}\n", 1)
                        replaced = True
                        break
            if not comments or (not replaced and comments):
                shebang, content = split_shebang(content)

                content = f"{shebang}{NOTICES[tag]}\n\n" + content

        # Write the new content with unix style EOL (otherwise, on windows it would be "\r\n")
        if original_content != content:
            print("Updating ", f)
            with open(f, "w", encoding="utf-8", newline='\n') as fp:
                fp.write(content)

    if empty_files:
        print("Find the following empty files while working...")
        for idx, f in enumerate(empty_files, start=1):
            print(idx, "> ", f)

    if skipped_files:
        print("The following files self-identified as open source and were thus skipped:")
        for idx, f in enumerate(skipped_files, start=1):
            print(idx, "> ", f)

    return 0


if __name__ == "__main__":
    sys.exit(main())
