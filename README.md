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
block # 0: start from 0 ... + 12 * 100000 hashes was checked (16.0966 % of total)
block # 1: start from 1200000 ... + 12 * 100000 hashes was checked (32.1932 % of total)
block # 2: start from 2400000 ... + 12 * 100000 hashes was checked (48.2899 % of total)
block # 3: start from 3600000 ... + 12 * 100000 hashes was checked (64.3865 % of total)
block # 4: start from 4800000 ... 
catched!
   raw form:  luck
   hex form:  6c75636b

```
Try with wrong hash:
```
$ ./hashforce --block-length 100000 --range "1 4 65-90:97-122" --hash 0123456789abcdef0123456789abcdef
block # 0: start from 0 ... + 12 * 100000 hashes was checked (16.0966 % of total)
block # 1: start from 1200000 ... + 12 * 100000 hashes was checked (32.1932 % of total)
block # 2: start from 2400000 ... + 12 * 100000 hashes was checked (48.2899 % of total)
block # 3: start from 3600000 ... + 12 * 100000 hashes was checked (64.3865 % of total)
block # 4: start from 4800000 ... + 12 * 100000 hashes was checked (80.4831 % of total)
block # 5: start from 6000000 ... + 12 * 100000 hashes was checked (96.5797 % of total)
block # 6: start from 7200000 ... 
checkout all range - no luck :(
```

You can interrupt computation process at any point and continue it later with decent block
```
$ ./hashforce  --range "4 8 48-57:65-70" --hash "b596701870a862bcfac3384c62f94643"
block # 0: start from 0 ... + 12 * 1000000 hashes was checked (0.261935 % of total)
block # 1: start from 12000000 ... + 12 * 1000000 hashes was checked (0.523869 % of total)
block # 2: start from 24000000 ... + 12 * 1000000 hashes was checked (0.785804 % of total)
....
block # 303: start from 3636000000 ... + 12 * 1000000 hashes was checked (79.6282 % of total)
block # 304: start from 3648000000 ... ^C
```
```
$ ./hashforce --block-offset 304 --range "4 8 48-57:65-70" --hash "b596701870a862bcfac3384c62f94643"
block # 304: start from 3648000000 ... + 12 * 1000000 hashes was checked (79.8901 % of total)
block # 305: start from 3660000000 ... + 12 * 1000000 hashes was checked (80.152 % of total)
block # 306: start from 3672000000 ... + 12 * 1000000 hashes was checked (80.414 % of total)
block # 307: start from 3684000000 ... 
catched!
   raw form:  CAFEBABE
   hex form:  4341464542414245
```
