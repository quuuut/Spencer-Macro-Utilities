#!/bin/sh

password=$(osascript -e 'Tell application "System Events" to display dialog "Enter your password to run the Input Helper:" default answer "" with hidden answer with title "Authentication Required"' \
    | grep -o 'text returned:[^,]*' \
    | sed 's/text returned://;s/^ *//;s/ *$//')

echo "Password entered: $password"