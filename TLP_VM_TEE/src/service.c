#include <stdio.h>

/*
 * This is the service behind the rate-limiting.
 * In this prototype, it is a dummy implementation of a private set, where the 
 * private set is simply all even elements.
 *
 * Params:
 *	* requested_element:	elem for which membership in privat set is asked
 *
 * Returns:
 *	* "1"			if requested_element is even
 *	* "0"			if requested_element is odd
 */
char *service(unsigned long requested_element) {
	return requested_element % 2 == 0? "1": "0";
}