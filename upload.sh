#!/bin/sh
# Usage: ./upload.sh USERID filename

scp $2 $1@iputty.kldp.net:/var/lib/gforge/chroot/home/groups/iputty/htdocs
