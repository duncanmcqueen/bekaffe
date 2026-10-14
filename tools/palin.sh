#!/bin/bash
# Show BigPalindrome.java at the bottom of the terminal with a
# more(1)-style end prompt, as in the 1999 BeKaffe screenshot.
cd /boot/home/Projects/bekaffe/haiku-peer/demos/BigPalindrome
clear
for i in $(seq 1 30); do echo; done
cat BigPalindrome.java
printf "\n\033[7mBigPalindrome.java (END)\033[0m"
read -r
