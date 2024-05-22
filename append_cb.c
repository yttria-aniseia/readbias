/*   append_cb.c -- append a given sequence to the end of the "CB" aux tag
 *   for every entry in a BAM file. use "-" as filename for stdin/stdout
 *   (based on htslib/samples/mod_aux.c)
 *
 *   Example usage:
 *   ./append_cb in.bam out.bam NNATG 24
 *
 *   2024-05-21
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
#include <unistd.h>
#include <htslib/sam.h>
#include <htslib/thread_pool.h>


// helper for string concatenation
//
// really we wish we could do this insertion in-place
// in an oversized bam1_t buffer, but too many support
// functions are private that it becomes much, much messier.
static size_t concat(const char* a, const char* b, char **out)
{
	const size_t a_len = strlen(a);
	const size_t b_len = strlen(b);
	*out = malloc(a_len + b_len + 1);
	memcpy(*out, a, a_len);
	memcpy(*out + a_len, b, b_len + 1); // copy null terminator from b
	return a_len + b_len;
}

static void print_usage(FILE *fp)
{
	fprintf(fp, "Usage: append_cb infile outfile val num_threads\n\
e.g. ./append_cb in.bam out.bam NNATG 24\n\
append 'val' to the 'CB' tag for all alignments\n");
}

// entry point
//
// memory usage seems to be ~ 2MB / thread
// thread usage on single file in/out maxes out at 24
// with this htslib usage
// effectively BGZF encode/decode maxes out at	12?
int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE, ret_r = 0;

	// appendcb infile outfile val n_threads
	if (argc != 5) {
		print_usage(stderr);
		goto end; // error goto my beloved
	}
	const char *inname = argv[1];
	const char *outname = argv[2];
	const char *val = argv[3];
	const int n_threads = atoi(argv[4]);

	bam1_t *bamdata = NULL;
	if (!(bamdata = bam_init1())) {
		fprintf(stderr, "Failed to allocate data memory!\n");
		goto end;
	}

	// open input and output file
	samFile *infile, *outfile;
	if (!(infile = sam_open(inname, "r"))) {
		fprintf(stderr, "Could not open %s\n", inname);
		goto end;
	}
	if (!(outfile = sam_open(outname, "wb"))) {
		fprintf(stderr, "Could not open %s\n", outname);
		goto end;
	}

	// set up thread pool for decompression/compression
	htsThreadPool tpool = {NULL, 0};
	if (n_threads > 1) {
		if (!(tpool.pool = hts_tpool_init(n_threads))) {
			printf("Failed to initialize the thread pool\n");
			goto end;
		}
		hts_set_opt(infile, HTS_OPT_THREAD_POOL, &tpool);
		hts_set_opt(outfile, HTS_OPT_THREAD_POOL, &tpool);
	}

	// SAM header
	sam_hdr_t *in_samhdr = sam_hdr_read(infile);
	if (!in_samhdr) {
		fprintf(stderr, "Failed to read header from file!\n");
		goto end;
	}
	if (sam_hdr_write(outfile, in_samhdr) == -1) {
		fprintf(stderr, "Failed to write header\n");
		goto end;
	}

	// process entries
	unsigned long entries = 0;
	unsigned long missing_CB = 0;
	const char *tag = "CB";
	const char type = 'Z';
	while ((ret_r = sam_read1(infile, in_samhdr, bamdata)) >= 0) {
		//fprintf(stderr, "\r% 16d", ++entries);
		errno = 0;

		// update CB auxdata
		uint8_t *data = NULL;
		if (!(data = bam_aux_get(bamdata, tag))) {
			missing_CB++; // no CB tag in record
		}
		else {
			char *new_val;
			const size_t new_len = concat(data + 1, val, &new_val);
			if (bam_aux_update_str(bamdata, tag, new_len, new_val)) {
				free(new_val);
				fprintf(stderr, "Failed to update string data, errno: %d\n", errno);
				goto end;
			}
			free(new_val);
		}
		if (sam_write1(outfile, in_samhdr, bamdata) < 0) {
			fprintf(stderr, "Failed to write output\n");
			goto end;
		}
	}
	if (ret_r < -1) { // read error
		fprintf(stderr, "Failed to read data\n");
		goto end;
	}

	ret = EXIT_SUCCESS;
 end:
	fprintf(stderr, "entries missing CB tag: %u\n", missing_CB);
	// cleanup
	if (in_samhdr) {
		sam_hdr_destroy(in_samhdr);
	}
	if (infile) {
		sam_close(infile);
	}
	if (outfile) {
		sam_close(outfile);
	}
	// it's not a memory leak if we exit and the host cleans up! oh well.
	if (bamdata) {
		bam_destroy1(bamdata);
	}
	if (tpool.pool) {
		hts_tpool_destroy(tpool.pool);
	}
  
	return ret;
}
