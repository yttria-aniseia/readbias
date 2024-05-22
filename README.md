# append_cb (Y's C implementation)

## Summary
appendcb appends a specified sequence to the end of all "CB" auxdata records in a BAM file.
it also reports the number of entries missing a "CB" tag, and does not modify them.

this is not a general purpose BAM manipulation program!

## Setup
```
git clone git@github.com:yttria-aniseia/append_cb.git
git submodule update --init --recursive --depth=1
make
```

## Usage
```
./appendcb in.bam out.bam value threads
```

Typical output:
```
# ./appendcb in.bam out.bam NNATG 24
entries missing CB tag: 0
```
the order of aux tags in out.bam is not guaranteed,
but they should still be present and identical,
with the exception of "CB" which will have the value
specified appended to its contents.

all messages are reported on stderr, and stdin/stdout
may be used by specifying the filename `-`

there are currently no settings other than the number of threads to use.
"24" is the suggested maximum -- single-file decoding in htslib doesn't
scale past this number.

an approximate memory usage for 24 threads might be 36MB.
memory requirements has not been exhaustively tested but is assumed to be
on the order of 2MB per thread.

processed 27,432,660 BAM entries in 33.37s @ 24 threads on an AMD EPYC 7H12
(822,075 it/s)