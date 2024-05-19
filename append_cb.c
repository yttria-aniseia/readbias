/* The pupose of this code is to demonstrate the library apis and need proper error handling and
   optimization */

#include <getopt.h>
#include <unistd.h>
#include <htslib/sam.h>

/// print_usage - print the demo_usage
/** @param fp pointer to the file / terminal to which demo_usage to be dumped
	returns nothing
*/
static void print_usage(FILE *fp)
{
  fprintf(fp, "Usage: append_tag infile outfile val\n\
append 'val' to the 'CB' tag for all alignments\n");
}

size_t concat(const char* a, const char* b, char **out)
{
  const size_t a_len = strlen(a);
  const size_t b_len = strlen(b);
  *out = malloc(a_len + b_len + 1);
  memcpy(*out, a, a_len);
  memcpy(*out + a_len, b, b_len + 1); // copy null terminator from b
  return a_len + b_len;
}

int main(int argc, char *argv[])
{
  const char *inname = NULL, *outname = NULL, *val = NULL;
  int ret = EXIT_FAILURE, ret_r = 0, n_threads = 1;
  sam_hdr_t *in_samhdr = NULL;
  samFile *infile = NULL, *outfile = NULL;
  bam1_t *bamdata = NULL;
  uint8_t *data = NULL;
  long long missing_CB = 0;

  //mod_aux infile QNAME tag type val
  if (argc != 5) {
	print_usage(stderr);
	goto end;
  }
  inname = argv[1];
  outname = argv[2];
  val = argv[3];
  n_threads = atoi(argv[4]);
  const char *tag = "CB";
  const char type = 'Z';

  if (!(bamdata = bam_init1())) {
	fprintf(stderr, "Failed to allocate data memory!\n");
	goto end;
  }

  //open input file
  if (!(infile = sam_open(inname, "r"))) {
	fprintf(stderr, "Could not open %s\n", inname);
	goto end;
  }

  //open output file
  if (!(outfile = sam_open(outname, "wb"))) {
	fprintf(stderr, "Could not open std output\n");
	goto end;
  }
  hts_set_threads(infile, n_threads - 1);
  hts_set_threads(outfile, n_threads - 1);

  if (!(in_samhdr = sam_hdr_read(infile))) {
	fprintf(stderr, "Failed to read header from file!\n");
	goto end;
  }

  if (sam_hdr_write(outfile, in_samhdr) == -1) {
	fprintf(stderr, "Failed to write header\n");
	goto end;
  }

  long long entries = 0;
  while ((ret_r = sam_read1(infile, in_samhdr, bamdata)) >= 0) {
	fprintf(stderr, "\r% 16d", ++entries);
	errno = 0;
	// update aux
	if (!(data = bam_aux_get(bamdata, tag))) {
	  // no CB in record
	  missing_CB++;
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
  if (ret_r < -1) {
	//read error
	fprintf(stderr, "Failed to read data\n");
	goto end;
  }

  ret = EXIT_SUCCESS;
end:
  //fprintf(stderr, "entries missing CB tag: %ull\n", missing_CB);
  //cleanup
  if (in_samhdr) {
	sam_hdr_destroy(in_samhdr);
  }
  if (infile) {
	sam_close(infile);
  }
  if (outfile) {
	sam_close(outfile);
  }
  if (bamdata) {
	bam_destroy1(bamdata);
  }
  return ret;
}