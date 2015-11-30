#include "config.h"

#include "BloomDBG/bloom-dbg.h"
#include "Common/Kmer.h"
#include "Common/StringUtil.h"
#include "Common/Options.h"
#include "lib/bloomfilter-9061f087d8714109b865415f2ac05e4796d0cd74/BloomFilter.hpp"

#include <getopt.h>
#include <iostream>
#include <sstream>
#include <cstdlib>

#if _OPENMP
# include <omp.h>
#endif

using namespace std;

#define PROGRAM "abyss-bloom-dbg"

static const char VERSION_MESSAGE[] =
	PROGRAM " (" PACKAGE_NAME ") " VERSION "\n"
	"Written by Ben Vandervalk, Shaun Jackman, Hamid Mohamadi,\n"
	"Justin Chu, and Anthony Raymond.\n"
	"\n"
	"Copyright 2015 Canada's Michael Smith Genome Science Centre\n";

static const char USAGE_MESSAGE[] =
"Usage: " PROGRAM " -C <read_coverage> -G <genome_size> -k <kmer_size> [options] \\\n"
"    <FASTQ> [FASTQ]... > assembly.fasta\n"
"\n"
"Perform a de Bruijn graph assembly of the given FASTQ files.\n"
"\n"
"Options:\n"
"\n"
"  -C, --read-coverage=N      approx read coverage [required]\n"
"  -e, --seq-error-rate=N     approx sequencing error rate [0.001]\n"
"  -f, --fpr=N                target false positive rate for Bloom\n"
"                             filter [0.05]\n"
"  -G, --genome-size=N        approx genome size with suffix\n"
"                             'k', 'M', or 'G' [required]\n"
"      --help                 display this help and exit\n"
"  -j, --threads=N            use N parallel threads [1]\n"
"  -k, --kmer=N               the size of a k-mer [required]\n"
"  -v, --verbose              display verbose output\n"
"      --version              output version information and exit\n"
"\n"
"Example:\n"
"\n"
"  Assemble a 100 Mbp genome with 50X coverage and a k-mer size of 50bp:\n"
"\n"
"  $ " PROGRAM " -C50 -G100M -k50 reads1.fq.gz reads2.fq.gz > assembly.fa\n"
"\n"
"Report bugs to <" PACKAGE_BUGREPORT ">.\n";

namespace opt {

	/** Approx. read coverage. */
	static double readCoverage = 0.0;

	/** Approx. sequencing error rate (fraction between 0 and 1) */
	static double seqErrorRate = 0.001;

	/** Target false positive rate (FPR) for Bloom filter */
	static double fpr = 0.05;

	/** Approx. genome size */
	static size_t genomeSize = 0;

	/** The number of parallel threads. */
	static unsigned threads = 1;

	/** The size of a k-mer. */
	static unsigned k;

}

static const char shortopts[] = "C:e:f:G:j:k:v";

enum { OPT_HELP = 1, OPT_VERSION };

static const struct option longopts[] = {
	{ "read-coverage",    required_argument, NULL, 'C' },
	{ "seq-error-rate",   required_argument, NULL, 'e' },
	{ "fpr",              required_argument, NULL, 'f' },
	{ "genome-size",      required_argument, NULL, 'G' },
	{ "help",             no_argument, NULL, OPT_HELP },
	{ "threads",          required_argument, NULL, 'j' },
	{ "kmer",             required_argument, NULL, 'k' },
	{ "verbose",          no_argument, NULL, 'v' },
	{ "version",          no_argument, NULL, OPT_VERSION },
	{ NULL, 0, NULL, 0 }
};

/**
 * Create a de novo genome assembly using a Bloom filter de
 * Bruijn graph.
 */
int main(int argc, char** argv)
{
	bool die = false;

	for (int c; (c = getopt_long(argc, argv,
					shortopts, longopts, NULL)) != -1;) {
		istringstream arg(optarg != NULL ? optarg : "");
		switch (c) {
		  case '?':
			die = true; break;
		  case 'C':
			arg >> opt::readCoverage; break;
		  case 'e':
			arg >> opt::seqErrorRate; break;
		  case 'f':
			arg >> opt::fpr; break;
		  case 'G':
			  opt::genomeSize = (size_t)fromSI(arg); break;
		  case 'j':
			arg >> opt::threads; break;
		  case 'k':
			arg >> opt::k; break;
		  case 'v':
			++opt::verbose; break;
		  case OPT_HELP:
			cout << USAGE_MESSAGE;
			exit(EXIT_SUCCESS);
		  case OPT_VERSION:
			cout << VERSION_MESSAGE;
			exit(EXIT_SUCCESS);
		}
		if (optarg != NULL && (!arg.eof() || arg.fail())) {
			cerr << PROGRAM ": invalid option: `-"
				<< (char)c << optarg << "'\n";
			exit(EXIT_FAILURE);
		}
	}

	if (opt::readCoverage == 0.0) {
		cerr << PROGRAM ": missing mandatory option `-C'\n";
		die = true;
	}

	if (opt::genomeSize == 0) {
		cerr << PROGRAM ": missing mandatory option `-G'\n";
		die = true;
	}

	if (opt::k == 0) {
		cerr << PROGRAM ": missing mandatory option `-k'\n";
		die = true;
	}

	if (argc - optind < 1) {
		cerr << PROGRAM ": missing input file arguments\n";
		die = true;
	}

	if (die) {
		cerr << "Try `" << PROGRAM
			<< " --help' for more information.\n";
		exit(EXIT_FAILURE);
	}

#if _OPENMP
	if (opt::threads > 0)
		omp_set_num_threads(opt::threads);
#endif

	Kmer::setLength(opt::k);

	/* calculate optimal size and number of hash functions for Bloom filter */

	BloomParams bloomParams = calcBloomParams(opt::genomeSize,
			opt::readCoverage, opt::seqErrorRate, opt::k, opt::fpr);

	if (opt::verbose) {
		cerr << "Building Bloom filter with size "
			<< bytesToSI((bloomParams.size+7)/8)
			<< " and " << bloomParams.hashes
			<< " hash functions" << endl;
	}

	return EXIT_SUCCESS;
}