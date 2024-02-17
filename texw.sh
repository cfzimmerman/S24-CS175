#!/bin/bash

# Watches the given file and recompiles it on save. Output
# currently goes in the same directory as the watched file.

if [ "$#" -ne 1 ]; then
	echo "Usage: $0 <filename.tex>"
	exit 1
fi

FULL_PATH=$(realpath "$1")
DIR=$(dirname "$FULL_PATH")
FILE=$(basename "$FULL_PATH")

# Places compiled output next to the target file
cd "$DIR"

fswatch -o "$FILE" | while read _; do
	pdflatex -interaction=nonstopmode "$FILE"
done
