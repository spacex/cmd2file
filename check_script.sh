#!/bin/bash

SKIP=0
while true; do
  if [ -e /tmp/fifo ]; then
    if [ ! $SKIP -eq 0 ]; then
      echo
    fi
    SKIP=0
    echo -n "Checking ... "
    md5sum /tmp/fifo
    [[ "$1" != "" ]] && sleep $1
  else
    if [ $SKIP -eq 0 ]; then
      echo -n "Skipping ."
    else
      echo -n "."
    fi
    SKIP=$(($SKIP+1))
  fi
done
