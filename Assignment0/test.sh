#!/usr/bin/env bash

# Exit immediately if any command below fails.
set -e

make

echo "Generating a test_files directory.."
mkdir -p test_files
rm -f test_files/*

echo "Generating test files.."

### ASCII ###
printf "Hello, World!\n" > test_files/ascii1.input
printf "Hello, World!" > test_files/ascii2.input
printf "1234567890\n" > test_files/ascii_numbers.input
printf "!@#$%%&*()\n" > test_files/ascii_symbols.input
printf "NoNewline" > test_files/ascii_nonewline.input
printf "abc123XYZ\n" > test_files/ascii_mix.input
printf "multi\nline\nascii\n" > test_files/ascii_multiline.input
printf " " > test_files/ascii_space.input
printf "Tab\tDelimited\n" > test_files/ascii_tab.input
printf "UpperCASELOWERcase\n" > test_files/ascii_case.input
printf "END.\n" > test_files/ascii_end.input
printf "ASCII only text file\n" > test_files/ascii12.input

### ISO-8859-1 ###
printf "Hællø\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso1.input
printf "Smørrebrød\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso2.input
printf "ÆØÅæøå\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_aeoeaa.input
printf "café naïve\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_accent.input
printf "Fußgänger üben\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_german.input
printf "français été\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_french.input
printf "Español niño\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_spanish.input
printf "München\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_munchen.input
printf "Zürich\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_swiss.input
printf "olá, ação\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_portuguese.input
printf "façade rôle\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_extra.input
printf "Göteborg\n" | iconv -f UTF-8 -t ISO-8859-1 > test_files/iso_swedish.input

### UTF-8 ###
printf "Hello 🌍\n" > test_files/utf81.input
printf "中文测试\n" > test_files/utf82.input
printf "Привет\n" > test_files/utf8_russian.input
printf "😀🔥🎉\n" > test_files/utf8_emojis.input
printf "مرحبا\n" > test_files/utf8_arabic.input
printf "हिंदी\n" > test_files/utf8_hindi.input
printf "한국어\n" > test_files/utf8_korean.input
printf "日本語\n" > test_files/utf8_japanese.input
printf "עִברִית\n" > test_files/utf8_hebrew.input
printf "Tiếng Việt\n" > test_files/utf8_vietnamese.input
printf "emoji 🚀✅❤️\n" > test_files/utf8_emoji2.input
printf "mix αβγ\n" > test_files/utf8_greek.input

### DATA ###
printf "Hello,\x00World!\n" > test_files/data1.input
dd if=/dev/zero of=test_files/data2.input bs=1 count=10 2>/dev/null
printf "\xFF\xFE\xFD\xFC" > test_files/data3.input
head -c 20 /dev/urandom > test_files/data4.input
printf "abc\x00def\n" > test_files/data5.input
printf "\x01\x02\x03\x04\n" > test_files/data6.input
printf "\x7F\x80\x81\x82\n" > test_files/data7.input
printf "\xC0\xAF\n" > test_files/data8.input
dd if=/dev/urandom of=test_files/data9.input bs=1 count=50 2>/dev/null
printf "\x00\xFF\x00\xFF\n" > test_files/data10.input

### EMPTY ###
printf "" > test_files/empty.input

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
                  -e 's/OpenPGP Public Key/ISO-8859 text/' \
                  -e 's/Non-ISO extended-ASCII text/data/' \
                  -e 's/very short file (no magic)/ASCII text/' \
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
