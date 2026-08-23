/*
 * Stellarium
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef STELMAINEXPORT_HPP
#define STELMAINEXPORT_HPP

#include <QtGlobal>

#if defined(Q_OS_WIN)
#  if defined(STELMAIN_BUILD)
#    define STELMAIN_EXPORT Q_DECL_EXPORT
#  elif defined(STELMAIN_USE_DLL)
#    define STELMAIN_EXPORT Q_DECL_IMPORT
#  else
#    define STELMAIN_EXPORT
#  endif
#else
#  define STELMAIN_EXPORT
#endif

#endif // STELMAINEXPORT_HPP
