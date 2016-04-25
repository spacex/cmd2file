#!/bin/bash

while true; do if [ -e /tmp/fifo ]; then echo -n "Checking ... "; md5sum /tmp/fifo; fi; sleep .01; done
