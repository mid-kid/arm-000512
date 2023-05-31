/* 
 * dos.h
 *
 * DOS-specific functions and structures.
 *
 * This file is part of the Mingw32 package.
 *
 * Contributors:
 *  Created by J.J. van der Heijden <J.J.vanderHeijden@student.utwente.nl>
 *
 *  THIS SOFTWARE IS NOT COPYRIGHTED
 *
 *  This source code is offered for use in the public domain. You may
 *  use, modify or distribute it freely.
 *
 *  This code is distributed in the hope that it will be useful but
 *  WITHOUT ANY WARRANTY. ALL WARRANTIES, EXPRESS OR IMPLIED ARE HEREBY
 *  DISCLAMED. This includes but is not limited to warranties of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Revision: 1.2 $
 * $Author: noer $
 * $Date: 1998/10/20 01:24:49 $
 *
 */

#ifndef	__STRICT_ANSI__

#ifndef	_DOS_H_
#define	_DOS_H_

#define __need_wchar_t
#ifndef RC_INVOKED
#include <stddef.h>
#endif	/* Not RC_INVOKED */

/* For DOS file attributes */
#include <dir.h>

#ifndef RC_INVOKED

#ifdef __cplusplus
extern "C" {
#endif

extern char** __imp__pgmptr_dll;
#define _pgmptr (*__imp__pgmptr_dll)

/* Wide character equivalent */
extern wchar_t** __imp_wpgmptr_dll;
#define _wpgmptr (*__imp__wpgmptr_dll)

extern unsigned int *__imp__basemajor__dll;
extern unsigned int *__imp__baseminor__dll;
extern unsigned int *__imp__baseversion__dll;
extern unsigned int *__imp__osmajor_dll;
extern unsigned int *__imp__osminor_dll;
extern unsigned int *__imp__osmode_dll;

#define _basemajor (*__imp__basemajor_dll)
#define _baseminor (*__imp__baseminor_dll)
#define _baseversion (*__imp__baseversion_dll)
#define _osmajor (*__imp__osmajor_dll)
#define _osminor (*__imp__osminor_dll)
#define _osmode (*__imp__osmode_dll)

#ifdef __cplusplus
}
#endif

#endif	/* Not RC_INVOKED */

#endif	/* Not _DOS_H_ */

#endif	/* Not __STRICT_ANSI__ */

