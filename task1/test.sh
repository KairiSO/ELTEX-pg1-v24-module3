#!/bin/bash
set -e

echo "=== Build ==="
make clean && make

echo "=== Test files ==="
echo "Hello" > f1.txt
echo "Longer content for block testing" > f2.txt
dd if=/dev/urandom of=f3.bin bs=1024 count=10 2>/dev/null

echo "=== Test 1: unnamed pipes ==="
./file_copier f1.txt f2.txt f3.bin missing.txt || true
for f in f1.txt f2.txt f3.bin; do
    cmp -s "$f" "$f.copy" && echo "$f OK" || echo "$f FAIL"
done

echo "=== Test 2: named pipes ==="
rm -f *.copy
./file_copier -p mypipe f1.txt f2.txt
for f in f1.txt f2.txt; do
    cmp -s "$f" "$f.copy" && echo "$f OK" || echo "$f FAIL"
done

echo "=== Cleanup ==="
make clean
rm -f f1.txt f2.txt f3.bin *.copy mypipe.in mypipe.out