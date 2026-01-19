#!/usr/bin/env bash

wdir="$(dirname "$(readlink -f "$0")")"
#run main update script with these specific vals
script="/home/randy/__________WORKSPACE___________/_UTILS/UPDATE"
message="syncing aria repo"
$script --path "$wdir/../" --message "$message"
