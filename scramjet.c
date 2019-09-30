#include <stdio.h>
#include <stdlib.h>

#include <cio_version.h>

int main(void)
{
	printf("cio_version: %s\n", cio_get_version_string());
	return EXIT_SUCCESS;
}
