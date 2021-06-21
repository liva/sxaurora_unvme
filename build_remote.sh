#!/bin/bash
cd $(cd $(dirname $0);pwd)
../sync_remote.sh
ssh -t vh403 "cd release/unvme; ./build.sh"
