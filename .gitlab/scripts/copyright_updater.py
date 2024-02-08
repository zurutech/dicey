#!/usr/bin/env python3

import re
import sys
import argparse
from argparse import ArgumentParser
from datetime import date
from functools import reduce
from operator import concat
from pathlib import Path

NEW_NOTICE = f"// Copyright (c) 2014-{date.today().year} Zuru Tech HK Limited, All rights reserved."
SUPPORTED_EXTENSIONS = ["h", "hh", "hpp", "c", "cc", "cpp", "cxx", "cs"]
SKIP_STRING = "// open source, not our code"

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
        for ext in SUPPORTED_EXTENSIONS:
            for f in root.rglob(f"*.{ext}"):
                fileList.append(f)

    if len(args.files) > 0:
        fileList.extend(args.files)

    return fileList


def main() -> int:
    args = get_arguments()
    fileList = get_file_list(args)
    pattern = re.compile(r"(/\*.*?\*/)|(//[^\n]*\n)", re.S)

    empty_files = []
    skipped_files = []

    for f in fileList:
        with open(f, "r+", encoding="utf-8") as fp:
            try:
                original_content = content = fp.read()
            except UnicodeDecodeError as e:
                print("UnicodeDecodeError ", e, "\nfile: ", f)
                continue

            if not content:
                empty_files.append(f)
                continue

            if content.startswith(SKIP_STRING):
                skipped_files.append(f)
                continue

            comments = pattern.findall(content)
            replaced = False
            if comments:
                comments = reduce(concat, [list(filter(None, x)) for x in comments])
                for comment in comments:
                    if "copyright" in comment.lower():
                        content = content.replace(comment, f"{NEW_NOTICE}\n", 1)
                        replaced = True
                        break
            if not comments or (not replaced and comments):
                content = f"{NEW_NOTICE}\n\n" + content

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
