#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <iostream>
#include <sstream>
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

    int parseRange(string range) {
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
            chRanges.push_back(cr);
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
            showHelp();
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
                showHelp();
                return;
            }
        }
    }

    void showHelp() {
        cout << "help message" << endl;
    }

    bool valid() {
        if (!isValid) {
            /* Already reported about the error */
            return false;
        } else  if (hash.empty()) {
            cerr << "'--hash' parameter is mandatory";
            return false;
        } else if (!chRanges.size()) {
            cerr << "'--range' parametr is mandatory";
            return false;
        }

        return true;
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
