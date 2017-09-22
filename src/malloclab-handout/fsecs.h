typedef void (*fsecs_test_funct)(void *);
typedef void (*ftimer_test_exclude)(void *);

void init_fsecs(void);
double fsecs(fsecs_test_funct f, void *argp, ftimer_test_exclude g, void *argpp);
