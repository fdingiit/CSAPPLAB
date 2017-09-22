/* 
 * Function timers 
 */
typedef void (*ftimer_test_funct)(void *);
typedef void (*ftimer_test_exclude)(void *);

/* Estimate the running time of f(argp) using the Unix interval timer.
   Return the average of n runs */
double ftimer_itimer(ftimer_test_funct f, void *argp, int n);


/* Estimate the running time of f(argp) using gettimeofday 
   Return the average of n runs */
double ftimer_gettod(ftimer_test_funct f, void *argp, ftimer_test_exclude g, void *argpp, int n);

