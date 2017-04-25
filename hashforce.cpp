#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

class ArgOptions {
public:
    uint32_t blockLength = 1000000;
    uint32_t cores       = thread::hardware_concurrency();

    typedef struct chRange {
        uint8_t from;
        uint8_t to;
        uint8_t count;
    } chRange;
    vector<chRange> chRanges;
    uint16_t        chRangesSum;

    uint16_t        rangeFrom;
    uint16_t        rangeTo;
    uint64_t        rangeCapacity;

    string          hash;

private:
    bool            isValid = true;

    int
    parseRange(string range) {
        istringstream iss(range);
        uint16_t      from, to;

        /* parse word range */
        if (!(iss >> from >> to)) {
            cerr << "Unsupported format for '--range' option" << endl;
            return -1;
        }
        rangeFrom = (from < to) ? from : to;
        rangeTo   = (from < to) ? to : from;

        /* parse character ranges */
        std::string token;
        while (getline(iss, token, ':')) {
            replace(token.begin(), token.end(), '-', ' ');
            istringstream tss(token);
            if (!(tss >> from >> to)) {
                cerr << "Unsupported format for '--range' option" << endl;
                return -1;
            }

            chRange cr;
            cr.from = (from < to) ? from : to;
            cr.to   = (from < to) ? to : from;
            if (cr.to > 128) {
                cerr << "Unsupported format for '--range' option" << endl;
                cerr << "   characters range must be in [0, 128]" << endl;
                return -1;
            }
            chRanges.push_back(move(cr));
        }

        /*  sort & merge for character ranges */
        sort(chRanges.begin(), chRanges.end(),
             []( const chRange &a, const chRange &b) -> bool {return a.from < b.from;});

        int mrange = 0;
        for (int irange = 1;
                 irange < chRanges.size(); irange++) {
            if (chRanges[mrange].to >= chRanges[irange].from) {
                if (chRanges[mrange].to < chRanges[irange].to) {
                    chRanges[mrange].to = chRanges[irange].to;
                }
            } else {
                mrange++;
            }
        }
        chRanges.resize(mrange + 1);

        /* calculate character ranges sum */
        for (chRange &cr: chRanges) {
            cr.count = cr.to - cr.from + 1;
            chRangesSum += cr.count;
        }

        /* calculate word range capacity */
        uint64_t rank = 1;
        for (int i = 1; i <= rangeTo; i++) {
            if ((rank * chRangesSum) < rank) {
                cerr << "range capacity overflow (> UINT64_MAX)" << endl;
                return -1;
            }
            rank *= chRangesSum;
            if (i < rangeFrom) {
                 continue;
            }
            if ((rangeCapacity + rank) < rangeCapacity) {
                cerr << "range capacity overflow (> UINT64_MAX)" << endl;
                return -1;
            }
            rangeCapacity += rank;
        }

        return 0;
    }

public:
    ArgOptions(int argc, char *argv[]) {
        if (argc <= 2) {
            isValid = false;
            return;
        }

        for (int i = 1 ; i < argc - 1; i++) {
            if (string(argv[i]) == "--block-length") {
                blockLength = stoi(argv[i+1]);
                i++;
            } else if (string(argv[i]) == "--cores") {
                cores = stoi(argv[i+1]);
                i++;
            } else if (string(argv[i]) == "--range") {
                isValid = (parseRange(argv[i+1]) == 0);
                i++;
            } else if (string(argv[i]) == "--hash") {
                hash = argv[i+1];
                i++;
            } else {
                isValid = false;
                return;
            }
        }
    }

    void
    showHelp() {
        cout << "help message" << endl;
    }

    bool
    valid() {
        if (hash.empty()) {
            cerr << "'--hash' parameter is mandatory" << endl;
            return false;
        } else if (!chRanges.size()) {
            cerr << "'--range' parametr is mandatory" << endl;
            return false;
        } else if (!isValid) {
            return false;
        }

        return true;
    }
};

class Word {
private:
    ArgOptions      *options;
    uint16_t         size;
    uint16_t         len;
    vector<uint8_t>  data;
    vector<uint8_t>  iranges;
    vector<uint8_t>  ioffsets;

public:
    Word(ArgOptions *argOptions):
        options(argOptions),
        size(argOptions->rangeTo),
        len(argOptions->rangeFrom) {

        data.resize(size, options->chRanges[0].from);
        iranges.resize(size, 0);
        ioffsets.resize(size, 0);
    }

    bool
    Increment(int rankIdx = 0) {
        int rank = size - 1 -  rankIdx;

        if ((rank > size - 1) || (rank < 0)) {
            return false;
        }
        if (rank < size - len) {
            len++;
        }

        int rcode;
        if (ioffsets[rank] < (options->chRanges[iranges[rank]].count - 1)) {
            ioffsets[rank]++;
            rcode = true;
        } else if (iranges[rank] < (options->chRanges.size() - 1)) {
            iranges[rank]++;
            ioffsets[rank] = 0;
            rcode = true;
        } else {
            iranges[rank] = 0;
            ioffsets[rank] = 0;
            rcode = Increment(rankIdx + 1);
        }
        data[rank] = options->chRanges[iranges[rank]].from + ioffsets[rank];
        return rcode;
    }

    bool
    Set(uint64_t offset) {
        for (int widx = size - 1; offset > 0; widx--) {
            if (widx < 0) {
                return false;
            }
            if (widx < size - len) {
                len++;
            }

            uint16_t choff = offset % options->chRangesSum;  /* char value offset */
            int      chidx = 0;                              /* char range index  */
            while ((chidx < options->chRanges.size()) &&
                   (choff >= options->chRanges[chidx].count)) {
                choff -= options->chRanges[chidx].count;
                chidx++;
            }
            if (chidx == options->chRanges.size()) {
                return false;
            }
            data[widx]     = options->chRanges[chidx].from + choff;
            iranges[widx]  = chidx;
            ioffsets[widx] = choff;
            offset /= options->chRangesSum;
        }
        return true;
    }

    void
    Print(void) {

    }
};

class HashForce {
private:
    mutex              mtx;
    condition_variable condBegin;
    condition_variable condEnd;

    vector<thread *>   threads;
    vector<Word>       words;
    vector<uint64_t>   cycles;

    ArgOptions        *options;
    uint64_t           blockNum      = 0;
    uint64_t           offset        = 0;
    uint32_t           workersWait   = 0;
    bool               answerIsFound = false;

    void
    workerThread(int nworker) {
        while (1) {
            uint64_t cyclesCount = cycles[nworker];
            for (int i = 0; i < cyclesCount; i++) {
                words[nworker].Increment();
                if (0) {
                    /* catched !! */
                    answerIsFound = true;
                    break;
                }
            }
            unique_lock<mutex> lock(mtx);
            workersWait++;
            condEnd.notify_one();
            condBegin.wait(lock);
        }
    }

    bool
    prepareBlock(void) {
        uint64_t length = options->blockLength;
        uint32_t remain = 0;

        if ((options->rangeCapacity - offset) < (options->cores * options->blockLength)) {
            length = (options->rangeCapacity - offset) / options->cores;
            remain = (options->rangeCapacity - offset) % options->cores;
        } else {
            return false;
        }
        for (Word &word: words) {
            word.Set(offset);
        }
        for (int iworker = 0; iworker < options->cores; iworker++) {
            words[iworker].Set(offset);
            cycles[iworker] = length + ((iworker < remain) ? 1 : 0);
            offset += cycles[iworker];
        }
        return true;
    }

public:
    HashForce(ArgOptions *argOptions):
        options(argOptions) {

        cycles.resize(options->cores, 0);
        for (int iworker = 0; iworker < options->cores; iworker++) {
            words.emplace_back(options);
        }
    }

    int
    Manage(void) {
        prepareBlock();
        for (int iworker = 0; iworker < options->cores; iworker++) {
            threads.push_back(new thread(&HashForce::workerThread, this, iworker));
        }
        while (1) {
            unique_lock<mutex> lock(mtx);
            while ((workersWait < options->cores) && !answerIsFound) {
                condEnd.wait(lock);
            }
            if (answerIsFound || (offset == options->rangeCapacity)) {
                if (answerIsFound) {
                    /* catched! */
                } else {
                    /* checkout all range - no luck :( */
                }
                /* stop all threads & break */
                for (thread *thr: threads) {
                    delete thr;
                }
                break;
            }
            workersWait = 0;
            blockNum++;
            prepareBlock();
            condBegin.notify_all();
        }
        return 0;
    }
};

int main(int argc, char *argv[])
{
    ArgOptions args(argc, argv);

    if (!args.valid()) {
        args.showHelp();
        return -1;
    }

    return 0;
}
