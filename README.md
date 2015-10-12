# cron-output-hider
Use in crontabs in front of the real program to hide program output unless there is an error.

# Example Usage
A typical crontab entry might look like this:

    4 * * * * some-random-job.sh

If `some-random-job.sh` generates output, you will see it every time the script runs.  But if you only want to see the output when the script fails, you can run it through cron-output-hider like this:

    4 * * * * cron-output-hider -- some-random-job.sh

Now you'll only get the cron job email when it fails.
