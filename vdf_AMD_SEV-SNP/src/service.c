#include <stdio.h>

char *service(unsigned long requested_element) {
	return requested_element % 2 == 0? "1": "0";
}