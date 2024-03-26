#!/bin/bash

# searchs recursively scripts within the current directory
# assumes that all scripts end in  '.sh'
# add elements to the pattern to expand the scope of the search

find . -type f \( -name "*.sh" \) -exec chmod u+rwx {} \; -exec echo "{} - Added permissions: u+rwx" \;
