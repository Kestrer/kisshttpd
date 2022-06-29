#include "serverLog.h"

#include <stdio.h>

#define MAXTIME sizeof("2038-01-19T03:14:07Z")

void
startServerLog()
{
	char isoTime[MAXTIME];
	time_t const now = time(NULL);
	strftime(isoTime, MAXTIME, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

	printf("[%s]");
}
