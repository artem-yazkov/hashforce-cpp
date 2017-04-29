hashforce - a shell interface application to perform distributed brute-force attack against a provided hash

You can compile app with provided makefie
```
$ make
g++ -Wall -std=c++11 -lpthread -o hashforce hashforce.cpp md5.cpp
```
Run `hashforce` to see a help message with all available opts description & default values

Some example: 
```
$ echo -n luck | md5sum
867c4235c7d5abbefd2b8abd92b57f8a  -
$ ./hashforce --block-length 100000 --range "1 4 65-90:97-122" --hash 867c4235c7d5abbefd2b8abd92b57f8a
block № 0: start from 0 ... + 12 * 100000 hashes was checked
block № 1: start from 1200000 ... + 12 * 100000 hashes was checked
block № 2: start from 2400000 ... + 12 * 100000 hashes was checked
block № 3: start from 3600000 ... + 12 * 100000 hashes was checked
block № 4: start from 4800000 ... 
catched!
   raw form:  luck
   hex form:  6c75636b
```
Try with wrong hash:
```
$ ./hashforce --block-length 100000 --range "1 4 65-90:97-122" --hash 0123456789abcdef0123456789abcdef
block № 0: start from 0 ... + 12 * 100000 hashes was checked
block № 1: start from 1200000 ... + 12 * 100000 hashes was checked
block № 2: start from 2400000 ... + 12 * 100000 hashes was checked
block № 3: start from 3600000 ... + 12 * 100000 hashes was checked
block № 4: start from 4800000 ... + 12 * 100000 hashes was checked
block № 5: start from 6000000 ... + 12 * 100000 hashes was checked
block № 6: start from 7200000 ... 
checkout all range - no luck :(
```

You can interrupt computation process at any point and continue it later
