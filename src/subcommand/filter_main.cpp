/** \file filter_main.cpp
 *
 * Defines the "vg filter" subcommand, which filters alignments.
 */


#include <omp.h>
#include <unistd.h>
#include <getopt.h>

#include <list>
#include <fstream>

#include "subcommand.hpp"

#include "../vg.hpp"
#include "../readfilter.hpp"

using namespace std;
using namespace vg;
using namespace vg::subcommand;

void help_filter(char** argv) {
    cerr << "usage: " << argv[0] << " filter [options] <alignment.gam> > out.gam" << endl
         << "Filter alignments by properties." << endl
         << endl
         << "options:" << endl
         << "    -n, --name-prefix NAME     keep only reads with this prefix in their names [default='']" << endl
         << "    -X, --exclude-contig REGEX drop reads with refpos annotations on contigs matching the given regex (may repeat)" << endl
         << "    -s, --min-secondary N      minimum score to keep secondary alignment [default=0]" << endl
         << "    -r, --min-primary N        minimum score to keep primary alignment [default=0]" << endl
         << "    -O, --rescore              re-score reads using default parameters and only alignment information" << endl
         << "    -f, --frac-score           normalize score based on length" << endl
         << "    -u, --substitutions        use substitution count instead of score" << endl
         << "    -o, --max-overhang N       filter reads whose alignments begin or end with an insert > N [default=99999]" << endl
         << "    -m, --min-end-matches N    filter reads that don't begin with at least N matches on each end" << endl
         << "    -S, --drop-split           remove split reads taking nonexistent edges" << endl
         << "    -x, --xg-name FILE         use this xg index (required for -R, -S, and -D)" << endl
         << "    -R, --regions-file         only output alignments that intersect regions (BED file with 0-based coordinates expected)" << endl
         << "    -B, --output-basename      output to file(s) (required for -R).  The ith file will correspond to the ith BED region" << endl
         << "    -A, --append-regions       append to alignments created with -RB" << endl
         << "    -c, --context STEPS        expand the context of the subgraph this many steps when looking up chunks" << endl
         << "    -v, --verbose              print out statistics on numbers of reads filtered by what." << endl
         << "    -q, --min-mapq N           filter alignments with mapping quality < N" << endl
         << "    -E, --repeat-ends N        filter reads with tandem repeat (motif size <= 2N, spanning >= N bases) at either end" << endl
         << "    -D, --defray-ends N        clip back the ends of reads that are ambiguously aligned, up to N bases" << endl
         << "    -C, --defray-count N       stop defraying after N nodes visited (used to keep runtime in check) [default=99999]" << endl
         << "    -d, --downsample S.P       filter out all but the given portion 0.P of the reads. S may be an integer seed as in SAMtools" << endl
         << "    -t, --threads N            number of threads [1]" << endl;
}

int main_filter(int argc, char** argv) {

    if (argc <= 2) {
        help_filter(argv);
        return 1;
    }

    // This is the better design for a subcommand: we have a class that
    // implements it and encapsulates all the default parameters, and then we
    // just feed in overrides in the option parsing code. Thsi way we don't have
    // multiple defaults all over the place.
    ReadFilter filter;

    // What XG index, if any, should we load to support the other options?
    string xg_name;

    int c;
    optind = 2; // force optind past command positional arguments
    while (true) {
        static struct option long_options[] =
            {
                {"name-prefix", required_argument, 0, 'n'},
                {"exclude-contig", required_argument, 0, 'X'},
                {"min-secondary", required_argument, 0, 's'},
                {"min-primary", required_argument, 0, 'r'},
                {"rescore", no_argument, 0, 'O'},
                {"frac-score", required_argument, 0, 'f'},
                {"substitutions", required_argument, 0, 'u'},
                {"max-overhang", required_argument, 0, 'o'},
                {"min-end-matches", required_argument, 0, 'm'},
                {"drop-split",  no_argument, 0, 'S'},
                {"xg-name", required_argument, 0, 'x'},
                {"regions-file",  required_argument, 0, 'R'},
                {"output-basename",  required_argument, 0, 'B'},
                {"append-regions", no_argument, 0, 'A'},
                {"context",  required_argument, 0, 'c'},
                {"verbose",  no_argument, 0, 'v'},
                {"min-mapq", required_argument, 0, 'q'},
                {"repeat-ends", required_argument, 0, 'E'},
                {"defray-ends", required_argument, 0, 'D'},
                {"defray-count", required_argument, 0, 'C'},
                {"downsample", required_argument, 0, 'd'},
                {"threads", required_argument, 0, 't'},
                {0, 0, 0, 0}
            };

        int option_index = 0;
        c = getopt_long (argc, argv, "n:X:s:r:Od:e:fauo:m:Sx:R:B:Ac:vq:E:D:C:d:t:",
                         long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
        case 'n':
            filter.name_prefix = optarg;
            break;
        case 'X':
            filter.excluded_refpos_contigs.push_back(parse<std::regex>(optarg));
            break;
        case 's':
            filter.min_secondary = parse<double>(optarg);
            break;
        case 'r':
            filter.min_primary = parse<double>(optarg);
            break;
        case 'O':
            filter.rescore = true;
            break;
        case 'f':
            filter.frac_score = true;
            break;
        case 'u':
            filter.sub_score = true;
            break;
        case 'o':
            filter.max_overhang = parse<int>(optarg);
            break;
        case 'm':
            filter.min_end_matches = parse<int>(optarg);
            break;            
        case 'S':
            filter.drop_split = true;
        case 'x':
            xg_name = optarg;
            break;
        case 'R':
            filter.regions_file = optarg;
            break;
        case 'B':
            filter.outbase = optarg;
            break;
        case 'A':
            filter.append_regions = true;
            break;
        case 'c':
            filter.context_size = parse<int>(optarg);
            break;
        case 'q':
            filter.min_mapq = parse<double>(optarg);
            break;
        case 'v':
            filter.verbose = true;
            break;
        case 'E':
            filter.repeat_size = parse<int>(optarg);
            break;
        case 'D':
            filter.defray_length = parse<int>(optarg);
            break;
        case 'C':
            filter.defray_count = parse<int>(optarg);
            break;
        case 'd':
            {
                // We need to split out the seed and the probability in S.P
                string opt_string(optarg);
                
                if (opt_string != "1") {
                    // We need to parse
                    auto point = opt_string.find('.');
                    
                    if (point == -1) {
                        cerr << "error: no decimal point in seed/probability " << opt_string << endl;
                        exit(1);
                    }
                    
                    // Everything including and after the decimal point is the probability
                    auto prob_string = opt_string.substr(point);
                    filter.downsample_probability = parse<double>(prob_string);
                    
                    // Everything before that is the seed
                    auto seed_string = opt_string.substr(0, point);
                    // Use the 0 seed by default even if no actual seed shows up
                    int32_t seed = 0;
                    if (seed_string != "") {
                        // If there was a seed (even 0), parse it
                        seed = parse<int32_t>(seed_string);
                    }
                    
                    if (seed != 0) {
                        // We want a nonempty mask.
                        
                        // Use the C rng like Samtools does to get a mask.
                        // See https://github.com/samtools/samtools/blob/60138c42cf04c5c473dc151f3b9ca7530286fb1b/sam_view.c#L298-L299
                        srand(seed);
                        filter.downsample_seed_mask = rand();
                    }
                }
            }
            break;
        case 't':
            filter.threads = parse<int>(optarg);
            break;

        case 'h':
        case '?':
            /* getopt_long already printed an error message. */
            help_filter(argv);
            exit(1);
            break;

        default:
            abort ();
        }
    }

    if (filter.threads < 1) {
        cerr << "error:[vg filter]: Cannot use " << filter.threads << " threads." << endl;
        exit(1);
    }

    omp_set_num_threads(filter.threads);
    
    // setup alignment stream
    if (optind >= argc) {
        help_filter(argv);
        return 1;
    }

    // What should our return code be?
    int error_code = 0;

    get_input_file(optind, argc, argv, [&](istream& in) {
        // Open up the alignment stream
        
        // If the user gave us an XG index, we probably ought to load it up.
        // TODO: make sure if we add any other error exits from this function we
        // remember to delete this!
        xg::XG* xindex = nullptr;
        if (!xg_name.empty()) {
            // read the xg index
            ifstream xg_stream(xg_name);
            if(!xg_stream) {
                cerr << "Unable to open xg index: " << xg_name << endl;
                error_code = 1;
                return;
            }
            xindex = new xg::XG(xg_stream);
        }
    
        // Read in the alignments and filter them.
        error_code = filter.filter(&in, xindex);
        
        if(xindex != nullptr) {
            delete xindex;
        }
    });

    return error_code;
}

// Register subcommand
static Subcommand vg_vectorize("filter", "filter reads", main_filter);

