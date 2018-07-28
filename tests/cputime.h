/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

double getCPUTime(void);

/* Example usage:
 * ==============
 *
 * double startTime, endTime;
 *
 * startTime = getCPUTime( );
 * ...
 * endTime = getCPUTime( );
 *
 * fprintf( stderr, "CPU time used = %lf\n", (endTime - startTime) );
 */