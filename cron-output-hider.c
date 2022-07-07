
/*
 * cron-output-hider
 *
 * Author: Duane Wessels
 *
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * For more information, please refer to <http://unlicense.org>
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <memory.h>
#include <err.h>
#include <sys/types.h>
#include <signal.h>
#include <err.h>
#include <sys/file.h>
#include <time.h>
#include <sys/time.h>

#define MAXLINES 1024
char *lines[MAXLINES];
unsigned int nlines = 0;
const char *progname = 0;

void
usage(void)
{
    fprintf(stderr, "usage: %s [-e] -- cmd ...\n", progname);
    fprintf(stderr, "\t-e        capture stderr\n");
    fprintf(stderr, "\t-o file   save output to file\n");
    fprintf(stderr, "\t-l        lock output file\n");
    fprintf(stderr, "\t-t file   record command run time\n");
    exit(1);
}

void
log_time(const char *fn, const struct timeval *t1, const struct timeval *t2, int status, char *argv[])
{
    FILE *fp = 0;
    char start_strftime[128];
    unsigned int hours = 0;
    unsigned int mins = 0;
    unsigned int secs = 0;
    if (fn == 0)
	return;
    fp = fopen(fn, "a");
    if (fp == 0) {
	warnx("%s", fn);
	return;
    }
    if (0 != flock(fileno(fp), LOCK_EX)) {
	warnx("Could not lock timelog file");
	return;
    }
    strftime(start_strftime, sizeof(start_strftime), "%Y-%m-%dT%H:%M:%S", gmtime(&t1->tv_sec));
    hours = (t2->tv_sec - t1->tv_sec) / 3600;
    mins = (t2->tv_sec - t1->tv_sec - 3600 * hours) / 60;
    secs = (t2->tv_sec - t1->tv_sec - 3600 * hours - 60 * mins);
    fseek(fp, 0L, SEEK_END);
    fprintf(fp, "%s %02d:%02d:%02d", start_strftime, hours, mins, secs);
    fprintf(fp, " %d %d", WEXITSTATUS(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    while (*argv)
	fprintf(fp, " %s", *(argv++));
    fprintf(fp, "\n");
    fclose(fp);
    flock(fileno(fp), LOCK_UN);
}

int
main(int argc, char *argv[])
{
    pid_t kid;
    int opt;
    int opt_stderr = 0;
    FILE *opt_output = 0;
    memset(lines, 0, sizeof(lines));
    int stdoutpipe[2];
    char *opt_timelog = 0;

    progname = argv[0];
    while ((opt = getopt(argc, argv, "eo:lt:")) != -1) {
	switch (opt) {
	case 'e':
	    opt_stderr = 1;
	    break;
	case 'o':
	    opt_output = fopen(optarg, "w");
	    if (opt_output == 0)
		err(1, "%s", optarg);
	    break;
	case 'l':
	    if (opt_output == 0)
		errx(1, "must use -o file before -l");
	    if (0 != flock(fileno(opt_output), LOCK_EX|LOCK_NB))
		errx(1, "Could not lock output file");
	    break;
	case 't':
	    opt_timelog = strdup(optarg);
	    if (opt_timelog == 0)
		err(1, "strdup");
	    break;
	default:
	    usage();
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    if (0 == argc || 0 == argv[0])
	usage();

    if (pipe(stdoutpipe) < 0)
	err(1, "pipe");
    kid = fork();
    if (kid < 0)
	err(1, "fork");
    if (0 == kid) {
	/* child */
	close(stdoutpipe[0]);
	dup2(stdoutpipe[1], 1);
	if (opt_stderr)
	    dup2(stdoutpipe[1], 2);
	close(stdoutpipe[1]);
	execvp(argv[0], argv);
	err(1, "%s", argv[0]);
    } else {
	/* parent */
	char buf[512];
	FILE *fp = fdopen(stdoutpipe[0], "r");
	int status;
	int i;
	struct timeval start;
	struct timeval stop;
	if (0 == fp)
	    err(1, "fdopen");
	close(stdoutpipe[1]);
	gettimeofday(&start, 0);
	while (fgets(buf, sizeof(buf), fp)) {
	    if (opt_output)
		fputs(buf, opt_output);
	    if (MAXLINES == nlines) {
		free(lines[0]);
		memmove(&lines[0], &lines[1], --nlines * sizeof(lines[0]));
	    }
	    buf[sizeof(buf) - 1] = 0;
	    lines[nlines++] = strdup(buf);
	}
	gettimeofday(&stop, 0);
        if (opt_output)
	    fclose(opt_output);
	waitpid(kid, &status, 0);
	log_time(opt_timelog, &start, &stop, status, argv);
	if (WIFSIGNALED(status)) {
	    for (i = 0; i < nlines; i++)
		write(1, lines[i], strlen(lines[i]));
	    kill(getpid(), WTERMSIG(status));
	    exit(255);
	} else if (WEXITSTATUS(status)) {
	    for (i = 0; i < nlines; i++)
		write(1, lines[i], strlen(lines[i]));
	    exit(WEXITSTATUS(status));
	}
    }
    return 0;
}
