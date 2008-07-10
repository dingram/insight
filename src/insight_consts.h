#ifndef __INSIGHT_CONSTS_H
#define __INSIGHT_CONSTS_H
/*
 * Copyright (C) 2008 David Ingram
 *
 * This program is released under a Creative Commons
 * Attribution-NonCommerical-ShareAlike2.5 License.
 *
 * For more information, please see
 *   http://creativecommons.org/licenses/by-nc-sa/2.5/
 *
 * You are free:
 *
 *   * to copy, distribute, display, and perform the work
 *   * to make derivative works
 *
 * Under the following conditions:
 *   Attribution:   You must attribute the work in the manner specified by the
 *                  author or licensor.
 *   Noncommercial: You may not use this work for commercial purposes.
 *   Share Alike:   If you alter, transform, or build upon this work, you may
 *                  distribute the resulting work only under a license identical
 *                  to this one.
 *
 *   * For any reuse or distribution, you must make clear to others the
 *     license terms of this work.
 *   * Any of these conditions can be waived if you get permission from the
 *     copyright holder.
 *
 * Your fair use and other rights are in no way affected by the above.
 */

/*
 * NOTE:
 * -----
 * Subkey separator and indicator characters should NOT be from the set:
 *    < > : " / \ | * ?
 * for compatability with Windows systems.
 */

/** String used to separate subkey parts (e.g. the "." in "type.music") */
#define INSIGHT_SUBKEY_SEP "`"

/** String used to indicate subkey to follow (e.g. the ":" in "/type:") */
#define INSIGHT_SUBKEY_IND ":"

/** Character used to separate subkey parts (e.g. the "." in "type.music") */
#define INSIGHT_SUBKEY_SEP_C '`'

/** Character used to indicate subkey to follow (e.g. the ":" in "/type:") */
#define INSIGHT_SUBKEY_IND_C ':'

#endif
