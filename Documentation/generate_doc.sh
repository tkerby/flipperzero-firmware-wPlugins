#!/bin/bash

SCRIPT_DIR=$(realpath $(dirname $0))
SOURCE_DIR=$(dirname $SCRIPT_DIR)
DOC_DIR=$SCRIPT_DIR/html
IMAGE_DIR=$SCRIPT_DIR/images
DOXYFILE=$SCRIPT_DIR/Doxyfile

OPEN=true

usage()
{
    echo "script used to generate flipper zero usb can brigge SW documentation."
    echo ""
    echo "Usage: $0  [--open]"
    echo ""
    echo "Options:"
    echo "  --open: open generated documentation (index.html)"
}

case $# in
    0) 
        OPEN=false;; 
    1) 
        if [ "$1" != "--open" ];then
            usage
            exit 1
        fi;;
    ?)
        usage
        exit 1;;
esac

sed -i "s|^INPUT *=.*|INPUT = $SOURCE_DIR,$SCRIPT_DIR/mainpage.md|" "$DOXYFILE"
sed -i "s|^HTML_OUTPUT *=.*|HTML_OUTPUT = $DOC_DIR|" "$DOXYFILE"
sed -i "s|^IMAGE_PATH *=.*|IMAGE_PATH = $IMAGE_DIR|" "$DOXYFILE"
sed -i "s|^PROJECT_LOGO *=.*|PROJECT_LOGO = $IMAGE_DIR/logo.png|" "$DOXYFILE"
doxygen $DOXYFILE


if $OPEN; then
open $DOC_DIR/index.html
fi
