# readbias (count reads mapping by position in file)

## Summary
`readbias` calls `hisat2` to map the specified `fastq` files to a reference genome,
and counts (with a specified bin size) occurrences of each mapping 'type.'
this can be visualized (e.g. with a stacked area chart) to get an overview of position-dependent
mapping bias in a dataset.

## Setup
```
git clone git@github.com:yttria-aniseia/readbias.git
cd readbias
git submodule update --init --recursive --depth=1
make
```

This repository links against static versions of
 - https://github.com/samtools/htslib (https://doi.org/10.1093/gigascience/giab007)
 - https://github.com/ebiggers/libdeflate
, dependencies which are managed as git submodules.


## Usage
[`hisat2`](https://daehwankimlab.github.io/hisat2/manual/) must be in path and `hisat2_index` must be the basename of a [reference index created with `hisat2-build`](https://daehwankimlab.github.io/hisat2/manual/#main-arguments).
```
 readbias -r hisat2_index [-t count_threads] [-h hisat_threads] [-b bin_size] r1.fastq [r2.fastq]
 ```

Typical output:
```
# ./readbias -t 1 -h 13 -b5000 -r hisat2_Danio_rerio.GRCz11.dna_sm.primary_assembly_indexes/Danio_rerio SRR7240617_10M_1.fastq SRR7240617_10M_2.fastq
read	map	bad_map	unmap	r1_only	r2_only
5000	5	438	1986	473	2098
10000	10	498	1921	445	2126
...
```

output is TSV to stdout.
read: read number
map: read aligns, or (if paired) reads align in proper pair
bad_map: (if paired) both reads align, but discordantly (violating paired read assumptions)
unmap: read does not align, or (if paired) neither read aligns
r1_only: (if paired) R1 aligns, but not its mate
r2_only: (if paired) R2 aligns, but not its mate


## Notes
currently readbias always calls `hisat2` in order-preserving way for correctness, but at large bin sizes this doesn't really matter and is a performance hit.  this can be changed by removing the `"--reorder"` parameter on line 167 and 173
