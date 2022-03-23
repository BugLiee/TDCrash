//
// Created by bugliee on 2022/1/18.
//

#include <stddef.h>
#include "tacc_libc_support.h"
#include "../tacrash/tac_errno.h"

void *tacc_libc_support_memset(void *s, int c, size_t n)
{
    char *p = (char *)s;

    while(n--)
        *p++ = (char)c;

    return s;
}

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define TACC_LIC_SUPPORT_ISLEAP(year)         ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define TACC_LIC_SUPPORT_SECS_PER_HOUR        (60 * 60)
#define TACC_LIC_SUPPORT_SECS_PER_DAY         (TACC_LIC_SUPPORT_SECS_PER_HOUR * 24)
#define TACC_LIC_SUPPORT_DIV(a, b)            ((a) / (b) - ((a) % (b) < 0))
#define TACC_LIC_SUPPORT_LEAPS_THRU_END_OF(y) (TACC_LIC_SUPPORT_DIV(y, 4) - TACC_LIC_SUPPORT_DIV(y, 100) + TACC_LIC_SUPPORT_DIV(y, 400))

static const unsigned short int tacc_libc_support_mon_yday[2][13] =
        {
                /* Normal years.  */
                { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
                /* Leap years.  */
                { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
        };

/* Compute the `struct tm' representation of *T,
   offset GMTOFF seconds east of UTC,
   and store year, yday, mon, mday, wday, hour, min, sec into *RESULT.
   Return RESULT if successful.  */
struct tm *tacc_libc_support_localtime_r(const time_t *timep, long gmtoff, struct tm *result)
{
    time_t days, rem, y;
    const unsigned short int *ip;

    if(NULL == result) return NULL;

    result->tm_gmtoff = gmtoff;

    days = ((*timep) / TACC_LIC_SUPPORT_SECS_PER_DAY);
    rem = ((*timep) % TACC_LIC_SUPPORT_SECS_PER_DAY);
    rem += gmtoff;
    while (rem < 0)
    {
        rem += TACC_LIC_SUPPORT_SECS_PER_DAY;
        --days;
    }
    while (rem >= TACC_LIC_SUPPORT_SECS_PER_DAY)
    {
        rem -= TACC_LIC_SUPPORT_SECS_PER_DAY;
        ++days;
    }
    result->tm_hour = (int)(rem / TACC_LIC_SUPPORT_SECS_PER_HOUR);
    rem %= TACC_LIC_SUPPORT_SECS_PER_HOUR;
    result->tm_min = (int)(rem / 60);
    result->tm_sec = rem % 60;
    /* January 1, 1970 was a Thursday.  */
    result->tm_wday = (4 + days) % 7;
    if (result->tm_wday < 0)
        result->tm_wday += 7;
    y = 1970;

    while (days < 0 || days >= (TACC_LIC_SUPPORT_ISLEAP(y) ? 366 : 365))
    {
        /* Guess a corrected year, assuming 365 days per year.  */
        time_t yg = y + days / 365 - (days % 365 < 0);

        /* Adjust DAYS and Y to match the guessed year.  */
        days -= ((yg - y) * 365
                 + TACC_LIC_SUPPORT_LEAPS_THRU_END_OF (yg - 1)
                 - TACC_LIC_SUPPORT_LEAPS_THRU_END_OF (y - 1));

        y = yg;
    }
    result->tm_year = (int)(y - 1900);
    if (result->tm_year != y - 1900)
    {
        /* The year cannot be represented due to overflow.  */
        errno = EOVERFLOW;
        return NULL;
    }
    result->tm_yday = (int)days;
    ip = tacc_libc_support_mon_yday[TACC_LIC_SUPPORT_ISLEAP(y)];
    for (y = 11; days < (long int) ip[y]; --y)
        continue;
    days -= ip[y];
    result->tm_mon = (int)y;
    result->tm_mday = (int)(days + 1);
    return result;
}
