/* all signals generate coredump by default is list here */

#ifdef SIGABRT
FOREACH_SIG(SIGABRT);
#endif

#ifdef SIGBUS
FOREACH_SIG(SIGBUS);
#endif

#ifdef SIGEMT
FOREACH_SIG(SIGEMT);
#endif

#ifdef SIGFPE
FOREACH_SIG(SIGFPE);
#endif

#ifdef SIGILL
FOREACH_SIG(SIGILL);
#endif

#ifdef SIGIOT
FOREACH_SIG(SIGIOT);
#endif

#ifdef SIGQUIT
FOREACH_SIG(SIGQUIT);
#endif

#ifdef SIGSEGV
FOREACH_SIG(SIGSEGV);
#endif

#ifdef SIGSYS
FOREACH_SIG(SIGSYS);
#endif

#ifdef SIGTRAP
FOREACH_SIG(SIGTRAP);
#endif

#ifdef SIGXCPU
FOREACH_SIG(SIGXCPU);
#endif

#ifdef SIGXFSZ
FOREACH_SIG(SIGXFSZ);
#endif

