# RELEASE NOTE

---
2017-9-28

Segregate RC1, with O(N) coalesce strategy.

```
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   97%    5694  0.000805  7072
 1       yes   95%    5848  0.000946  6179
 2       yes   97%    6648  0.001111  5981
 3       yes   98%    5380  0.000793  6782
 4       yes   99%   14400  0.000697 20648
 5       yes   86%    4800  0.006196   775
 6       yes   83%    4800  0.005848   821
 7       yes   55%    6000  0.016051   374
 8       yes   51%    7200  0.019417   371
 9       yes   38%   14401  0.046018   313
10       yes   20%   14401  0.001737  8292
Total          74%   89572  0.099621   899

Perf index = 45 (util) + 40 (thru) = 85/100
```


2017-9-25

Implicit allocator with best-fit placement policy finished:

```
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.011192   509
 1       yes  100%    5848  0.010927   535
 2       yes   99%    6648  0.016786   396
 3       yes  100%    5380  0.012604   427
 4       yes  100%   14400  0.000314 45904
 5       yes   96%    4800  0.017970   267
 6       yes   95%    4800  0.018027   266
 7       yes   55%    6000  0.034815   172
 8       yes   51%    7200  0.032239   223
 9       yes  100%   14401  0.000384 37473
10       yes   87%   14401  0.000378 38048
Total          89%   89572  0.155639   576

Perf index = 54 (util) + 38 (thru) = 92/100
```


Implicit allocator with next-fit placement policy finished:

```
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   91%    5694  0.002811  2026
 1       yes   91%    5848  0.002011  2908
 2       yes   95%    6648  0.005309  1252
 3       yes   97%    5380  0.005002  1076
 4       yes  100%   14400  0.000291 49417
 5       yes   92%    4800  0.005288   908
 6       yes   90%    4800  0.005478   876
 7       yes   55%    6000  0.033569   179
 8       yes   51%    7200  0.031423   229
 9       yes  100%   14401  0.000344 41912
10       yes   87%   14401  0.000339 42493
Total          86%   89572  0.091865   975

Perf index = 52 (util) + 40 (thru) = 92/100
```


Implicit allocator with first-fit placement policy finished:

```
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.010045   567
 1       yes  100%    5848  0.009374   624
 2       yes   99%    6648  0.015062   441
 3       yes  100%    5380  0.011336   475
 4       yes  100%   14400  0.000300 48016
 5       yes   93%    4800  0.009154   524
 6       yes   92%    4800  0.009377   512
 7       yes   55%    6000  0.032687   184
 8       yes   51%    7200  0.031291   230
 9       yes  100%   14401  0.000340 42356
10       yes   87%   14401  0.000311 46335
Total          89%   89572  0.129277   693

Perf index = 53 (util) + 40 (thru) = 93/100
```
