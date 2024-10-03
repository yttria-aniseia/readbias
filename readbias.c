/*   readbias.c -- count read mapping per spot
 *   usage: readbias -r hisat2_index [-t count_threads] [-h hisat_threads] [-b bin_size] r1.fastq [r2.fastq]
 *
 *   Example usage:
 *   readbias -r ref_index/basename -h 11 -b 5000 r1.fastq r2.fastq
 *
 *   2024-09-28
 *   Author: Yttria Aniseia
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE
 *
 */

#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <htslib/sam.h>
#include <htslib/thread_pool.h>

// bam flags (intended for paired)
// for unpaired: badmapped is mapped, unmapped is unmapped (?)
static const int s_mapped(int flag) {
	return !(flag & BAM_FUNMAP);
}
static const int s_unmapped(int flag) {
	return (flag & BAM_FUNMAP);
}
static const int p_mapped(int flag) {
	return (flag & BAM_FREAD1) && (flag & BAM_FPROPER_PAIR);
}
static const int p_badmapped(int flag) {
	return (flag & BAM_FREAD1) && !(flag & BAM_FPROPER_PAIR) && !((flag & BAM_FUNMAP) || (flag & BAM_FMUNMAP));
}
static const int p_unmapped(int flag) {
	return (flag & BAM_FREAD1) && !(flag & BAM_FPROPER_PAIR) && (flag & BAM_FUNMAP) && (flag & BAM_FMUNMAP);
}
static const int p_r1_only(int flag) {
	return (flag & BAM_FREAD1) && (flag & BAM_FUNMAP) && !((flag & BAM_FPROPER_PAIR) || (flag & BAM_FMUNMAP));
}
static const int p_r2_only(int flag) {
	return (flag & BAM_FREAD1) && (flag & BAM_FMUNMAP) && !((flag & BAM_FPROPER_PAIR) || (flag & BAM_FUNMAP));
}

static void print_usage(FILE *fp)
{
	fprintf(fp, "Usage: readbias -r hisat2_index [-h hisat_threads] [-b bin_size] r1.fastq [r2.fastq]\n\
e.g. ./readbias -r ref_index/basename -h 11 -b 5000 r1.fastq r2.fastq\n\
assess mapping rate over read position\n");
}

// entry point
//
// thread usage on single file in/out maxes out at 24
// with this htslib usage
// effectively BGZF encode/decode maxes out at	12?
int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE, ret_r = 0;

	// readbias -r ref -t 12 -h 12 -b 100 r1 r2
	if (argc <= 2 || argc >= 9) {
		print_usage(stderr);
		goto end; // error goto my beloved
	}
	char *fq1 = NULL;
	char *fq2 = NULL;
	char *ref = NULL;
	int bsz = 1;
	int threads = 1;
	int hisat_threads = 4;

	int c;
	opterr = 0;

	// parse command options
	// threads -t
	// hisat2 threads -h
	// bin size -b
	// hisat2 ref -r
	// positional arguments: fastq files 1 or 2
	while (-1 != (c = getopt(argc, argv, "r:t:h:b:"))) {
		switch (c)
		{
		case 'r':
			ref = optarg;
			break;
		case 't':
			if ((threads = atoi(optarg)) == 0) {
				print_usage(stderr);
				goto end;
			}
			break;
		case 'h':
			if ((hisat_threads = atoi(optarg)) == 0) {
				print_usage(stderr);
				goto end;
			}
			break;
		case 'b':
			bsz = atoi(optarg);
			if (bsz == 0) {
				print_usage(stderr);
				goto end;
			}
			break;
		case '?':
			if (optopt == 'c')
				fprintf(stderr, "Option -%c requires an argument.\n", optopt);
			else
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			return 1;
		default:
			abort();
		}
	}

	bam1_t *bamdata = NULL;
	if (!(bamdata = bam_init1())) {
		fprintf(stderr, "Failed to allocate data memory!\n");
		goto end;
	}

	if (optind < argc)
		fq1 = argv[optind];
	else {
		print_usage(stderr);
		goto end;
	}
	if (optind+1 < argc)
		fq2 = argv[optind+1];

	char h[100];
	sprintf(h, "%d", hisat_threads);
	
	char* out = "_____temp2.sam";
	remove(out);
	if (mkfifo(out, 0600) == -1) {
		fprintf(stderr, "Failed to create fifo\n");
		goto end;
	}

	pid_t pid = fork();
	if (pid == 0) {
		if (fq2) {
			ret_r = execlp(
				"hisat2",
				"hisat2", "-p", h, "-k", "1", "-S", out, "-x", ref, "-1", fq1, "-2", fq2,
				"--reorder", "--no-temp-splicesite", "--mm", "--new-summary", NULL
				);
		} else {
			ret_r = execlp(
				"hisat2",
				"hisat2", "-p", h, "-k", "1", "-S", out, "-x", ref, "-U", fq1,
				"--reorder", "--no-temp-splicesite", "--mm", "--new-summary", NULL
				);
		}
		exit(ret_r);
	}

	// open input and output file
	samFile *infile;
	if (!(infile = sam_open(out, "r"))) {
		fprintf(stderr, "Could not open %s\n", out);
		goto end;
	}

	htsThreadPool tpool = {NULL, 0};
	if (threads > 1) { // set up thread pool for decompression/compression
		if (!(tpool.pool = hts_tpool_init(threads))) {
			printf("Failed to initialize the thread pool\n");
			goto end;
		}
		hts_set_opt(infile, HTS_OPT_THREAD_POOL, &tpool);
	}

	// SAM header
	sam_hdr_t *in_samhdr = sam_hdr_read(infile);
	if (!in_samhdr) {
		fprintf(stderr, "Failed to read header from file!\n");
		goto end;
	}

	// process entries
	unsigned long entries = 0;
	int mapped = 0;
	int badmapped = 0;
	int unmapped = 0;
	int r1_only = 0;
	int r2_only = 0;
	fprintf(stdout, "read\tmap\tbad_map\tunmap\tr1_only\tr2_only\n");
	while ((ret_r = sam_read1(infile, in_samhdr, bamdata)) >= 0) {
		uint16_t flag = bamdata->core.flag;
		if (!fq2) {
			errno = 0;

			if (s_mapped(flag)) { ++mapped; }
			else if (s_unmapped(flag)) { ++unmapped; }
			if (++entries % bsz == 0) {
				fprintf(stdout, "%lu\t%d\t0\t%d\t0\t0\n", entries, mapped, unmapped);
				mapped = 0; unmapped = 0;
			}
		} else if (fq2 && (flag & BAM_FREAD1)) {
			errno = 0;

			if (p_mapped(flag)) { ++mapped; }
			else if (p_badmapped(flag)) { ++badmapped; }
			else if (p_unmapped(flag)) { ++unmapped; }
			else if (p_r1_only(flag)) { ++r1_only; }
			else if (p_r2_only(flag)) { ++r2_only; }

			if (++entries % bsz == 0) {
				fprintf(stdout, "%lu\t%d\t%d\t%d\t%d\t%d\n", entries, mapped, badmapped, unmapped, r1_only, r2_only);
				mapped = 0; badmapped = 0; unmapped = 0; r1_only = 0; r2_only = 0;
			}
		}
	}
	if (ret_r < -1) { // read error
		fprintf(stderr, "Failed to read data\n");
		goto end;
	}

	ret = EXIT_SUCCESS;
 end:
	// cleanup
	kill(pid, 9);
	if (in_samhdr)
		sam_hdr_destroy(in_samhdr);
	if (infile)
		sam_close(infile);

	// it's not a memory leak if we exit and the host cleans up! oh well.
	if (bamdata) 
		bam_destroy1(bamdata);
	if (tpool.pool)
		hts_tpool_destroy(tpool.pool);
  
	return ret;
}