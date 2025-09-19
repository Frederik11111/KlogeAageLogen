#!/usr/bin/env bash

# Exit immediately if any command below fails.
set -e

make


echo "Generating a test_files directory.."
mkdir -p test_files
rm -f test_files/*


echo "Generating test files.."
printf "Hello, World!\n" > test_files/ascii.input
printf "Hello, World!" > test_files/ascii2.input
printf "Hello,\x00World!\n" > test_files/data.input
printf "" > test_files/empty.input

# ISO-8859-1 test (latin1 æøå)
printf "Hællø\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso1.input
printf "Smørrebrød\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso2.input



# UTF-8 test med emojis og kinesiske tegn
printf "Hello 🌍\n" > test_files/utf81.input
printf "中文测试\n" > test_files/utf82.input

# Flere datafiler
dd if=/dev/zero of=test_files/data2.input bs=1 count=10 2>/dev/null
printf "\xFF\xFE\xFD\xFC" > test_files/data3.input



echo "Running the tests.."
exitcode=0
for f in test_files/*.input
do
  echo ">>> Testing ${f}.."
  file ${f} | sed -e 's/ASCII text.*/ASCII text/' \
                  -e 's/UTF-8 Unicode text.*/UTF-8 Unicode text/' \
                  -e 's/Unicode text, UTF-8 text.*/UTF-8 Unicode text/' \
                  -e 's/ISO-8859 text.*/ISO-8859 text/' \
                  -e 's/Unicode text, UTF-16.*/data/' \
                  -e 's/writable, regular file, no read permission/cannot determine (Permission denied)/' \
                  > "${f}.expected"

  ./file  "${f}" > "${f}.actual"

  if ! diff -u "${f}.expected" "${f}.actual"
  then
    echo ">>> Failed :-("
    exitcode=1
  else
    echo ">>> Success :-)"
  fi
done
exit $exitcode
