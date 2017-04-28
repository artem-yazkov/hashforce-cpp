#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <thread>
#include <vector>

#include "md5.h"

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
    vector<chRange>     chRanges;
    uint16_t            chRangesSum   = 0;

    uint16_t            rangeFrom     = 0;
    uint16_t            rangeTo       = 0;
    uint64_t            rangeCapacity = 0;
    array<uint8_t,16>   hash;

private:
    bool                isValid = true;

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

        uint mrange = 0;
        for (uint irange = 1;
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

    int
    parseHash(string shash) {
        if (shash.length() != (hash.size() << 1)) {
            cerr << "incorrect hash length" << endl;
            return -1;
        }
        for (uint i = 0; i < hash.size(); i++) {
            sscanf(&shash[i << 1], "%02hhx", &hash[i]);
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
                isValid &= (parseRange(argv[i+1]) == 0);
                i++;
            } else if (string(argv[i]) == "--hash") {
                isValid &= (parseHash(argv[i+1]) == 0);
                i++;
            } else {
                cerr << "unexpected option: " << argv[i] << endl;
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
    uint16_t         len;
    uint16_t         size;
    vector<uint8_t>  data;
    vector<uint8_t>  iranges;
    vector<uint8_t>  ioffsets;

public:
    Word(ArgOptions *argOptions):
        options(argOptions),
        len(argOptions->rangeFrom),
        size(argOptions->rangeTo)
    {
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
        len = options->rangeFrom;
        for (int widx = size - 1; offset > 0; widx--) {
            if (widx < 0) {
                return false;
            }
            if (widx < size - len) {
                len++;
            }

            uint16_t choff = offset % options->chRangesSum;  /* char value offset */
            uint     chidx = 0;                              /* char range index  */
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

    uint8_t *
    getData(void) {
        return &data[size - len];
    }

    uint16_t
    getLen(void) {
        return len;
    }

    void
    Print(void) {
        for (int i = 0; i < size; i++) {
            if (i < size - len)
                printf(" ");
            else
                printf("%c", data[i]);
        }
        printf("\n");
    }
};

class HashForce {
private:
    mutex              mut;
    condition_variable condBegin;
    condition_variable condEnd;

    vector<thread>     threads;
    vector<Word>       words;
    vector<uint64_t>   cycles;

    ArgOptions        *options;
    uint64_t           blockNum      = 0;
    uint64_t           offset        = 0;
    uint32_t           workersWait   = 0;

    atomic<int>        answerWorkIdx;
    enum eStatus {
        stSearch, stFound, stNotFound
    } status = stSearch;

    void
    workerThread(int nworker) {

        while (1) {
            for (uint i = 0; (i < cycles[nworker]) && (answerWorkIdx < 0); i++) {
                MD5 md5;
                md5.update(words[nworker].getData(), words[nworker].getLen());
                md5.finalize();
                int dc = 0;
                for (; dc < 16; dc++) { if (md5.digest[dc] != options->hash[dc]) break; }
                if (dc == 16) {
                    /* catched !! */
                    unique_lock<mutex> lock(mut);
                    status = stFound;
                    answerWorkIdx = nworker;
                    break;
                }
                words[nworker].Increment();
            }
            unique_lock<mutex> lock(mut);
            workersWait++;
            condEnd.notify_one();
            condBegin.wait(lock);
            if (status != stSearch) {
                return;
            }
        }
    }

    bool
    prepareBlock(void) {
        cout <<  "block № " << blockNum << ": start from " << offset << " ... ";

        uint64_t length = options->blockLength;
        uint32_t remain = 0;

        if ((options->rangeCapacity - offset) < (options->cores * options->blockLength)) {
            length = (options->rangeCapacity - offset) / options->cores;
            remain = (options->rangeCapacity - offset) % options->cores;
        }
        for (Word &word: words) {
            word.Set(offset);
        }
        for (uint iworker = 0; iworker < options->cores; iworker++) {
            words[iworker].Set(offset);
            cycles[iworker] = length + ((iworker < remain) ? 1 : 0);
            offset += cycles[iworker];
        }
        return true;
    }

public:
    HashForce(ArgOptions *argOptions):
        options(argOptions), answerWorkIdx(-1) {

        cycles.resize(options->cores, 0);
        for (uint iworker = 0; iworker < options->cores; iworker++) {
            words.emplace_back(options);
        }
    }

    int
    Manage(void) {
        blockNum = 1;
        prepareBlock();
        for (uint iworker = 0; iworker < options->cores; iworker++) {
            threads.emplace_back(&HashForce::workerThread, this, iworker);
        }
        while (1) {
            {
                unique_lock<mutex> lock(mut);
                while (workersWait < options->cores) {
                    condEnd.wait(lock);
                }
            }
            if ((status != stSearch) || (offset == options->rangeCapacity)) {
                if (status == stFound) {
                    cout << endl << "catched!" << endl;
                    words[answerWorkIdx].Print();
                } else {
                    cout << endl << "checkout all range - no luck :(" << endl;
                    status = stNotFound;
                }
                condBegin.notify_all();
                for (thread &thr: threads) {
                    thr.join();
                }
                return 0;
            }
            cout << "+ " << options->cores << " * " << options->blockLength <<
                    " hashes was checked" << endl;
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

    HashForce hashForce(&args);
    hashForce.Manage();

#if 0
    printf("blockLength     : %u\n", args.blockLength);
    printf("cores           : %u\n", args.cores);
    printf("rangeFrom       : %u\n", args.rangeFrom);
    printf("rangeTo         : %u\n", args.rangeTo);
    printf("rangeCapacity   : %llu\n", args.rangeCapacity);
    for (ArgOptions::chRange &ch: args.chRanges) {
        printf("from: %u, to: %u, count: %u\n", ch.from, ch.to, ch.count);
    }
#endif

    return 0;
}
