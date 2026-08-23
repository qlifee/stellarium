/*
 * Stellarium
 * Copyright (C) 2026 Stellarium Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef STELPLUGINAPI_HPP
#define STELPLUGINAPI_HPP

#include "StelMainExport.hpp"

#include <QString>
#include <QVariantMap>

class StelCore;

//! Narrow, read-only API for external dynamic plug-ins.
namespace StelPluginAPI
{
//! Copy current time and observer state from a running Stellarium instance.
//!
//! The returned map owns its values and does not expose StelCore or
//! StelLocation object layouts. A successful snapshot contains these keys:
//! - `schemaVersion` (`int`, currently 1)
//! - `julianDayUt`, `julianDayTt`, `deltaTSeconds`, `utcOffsetHours`,
//!   `timeRateJdPerSecond`, `longitudeDegrees`, and `latitudeDegrees`
//!   (`double`)
//! - `currentTimeZone`, `locationTimeZone`, `locationId`, and `planetName`
//!   (`QString`)
//! - `locationValid` (`bool`)
//! - `altitudeMeters` (`int`)
//!
//! Longitude is east-positive and latitude is north-positive. The coordinates
//! use StelLocation's view-effective values, including its Observer
//! pseudo-planet behavior. Call this on Stellarium's main thread. Pass only the
//! opaque borrowed pointer returned by StelApp::getCore() in the matching
//! running Stellarium build; external code must not dereference it or retain
//! and use it after host shutdown.
//!
//! @return an empty map if @p core is null, otherwise the complete snapshot.
STELMAIN_EXPORT QVariantMap getCoreStateSnapshot(const StelCore* core);

//! Copy current observer-centred state for a Solar System body.
//!
//! @p bodyId currently accepts `Sun` or `Moon`, matched case-insensitively. A
//! successful snapshot always contains:
//! - `schemaVersion` (`int`, currently 1)
//! - `englishName` (`QString`)
//! - `julianDayUt`, `julianDayTt`, `rightAscensionJ2000Degrees`,
//!   `declinationJ2000Degrees`, `azimuthGeometricDegrees`,
//!   `altitudeGeometricDegrees`, `distanceAu`, and
//!   `angularDiameterUnscaledDegrees` (`double`)
//! - `aberrationEnabled`, `topocentricCoordinatesEnabled`, and
//!   `phaseDataAvailable` (`bool`). `aberrationEnabled` reports the host
//!   setting; individual body algorithms may suppress a particular correction.
//!
//! If `phaseDataAvailable` is true, the snapshot additionally contains
//! `illuminatedFraction`, `phaseAngleDegrees`, and `elongationDegrees`
//! (`double`). These keys are absent for the Sun or degenerate geometry.
//!
//! Right ascension is normalized to [0, 360) degrees. Geometric altitude is
//! airless (refraction off). Azimuth is normalized to [0, 360) degrees with
//! north at 0 and east at 90, independently of the user's display convention.
//! Distance is observer-to-body distance. Angular diameter ignores artificial
//! display scaling.
//!
//! The returned map owns its values and exposes no SolarSystem, Planet, or
//! StelObject layout or pointer. Call this on Stellarium's main thread after
//! the host has completed a core update. The @p core pointer follows the same
//! borrowed-pointer contract as getCoreStateSnapshot().
//!
//! @return an empty map for a null core, unavailable SolarSystem, an empty or
//! unknown body ID, or unavailable/non-finite base position data.
STELMAIN_EXPORT QVariantMap getSolarSystemBodyStateSnapshot(
	const StelCore* core, const QString& bodyId);

//! Sample the signed geocentric Moon-Sun longitude difference at a TT date.
//!
//! This deliberately small query supports arbitrary-date conjunction searches
//! without exposing Planet objects or changing the running simulation. @p
//! julianDayTt is a Julian Day in TT (Stellarium's JDE time scale), not UT or a
//! local civil date, and must be within 200,000 Julian years of J2000. A
//! successful result contains exactly:
//! - `schemaVersion` (`int`, currently 1)
//! - `julianDayTt` (`double`)
//! - `moonSunGeocentricEclipticLongitudeDifferenceDegrees` (`double`)
//!
//! The longitude difference is Moon minus Sun in Stellarium's geometric
//! VSOP87/J2000 ecliptic frame, normalized to (-180, 180] degrees. It is
//! Earth-centred and does not include topocentric parallax, refraction,
//! aberration, or light-time correction. Call this only on Stellarium's main
//! thread after Solar System initialization. The call does not change the
//! host clock, location, live body positions, or Planet orbit cache.
//! The accepted date span is a numerical safety boundary, not a uniform
//! accuracy guarantee; accuracy depends on the host ephemeris models.
//!
//! @return an empty map for a null/foreign core, non-finite or out-of-range
//! date, unavailable bodies, a non-main-thread call, or invalid output.
STELMAIN_EXPORT QVariantMap getMoonSunConjunctionSampleAtJulianDayTt(
	const StelCore* core, double julianDayTt);

//! Sample geometric Sun-Moon visibility data at an arbitrary UT date.
//!
//! @p julianDayUt is Stellarium's UT-like Julian Day, not local civil time, and
//! must be within 200,000 Julian years of J2000; the derived TT date must be in
//! the same range. It is used as UT1 for sidereal rotation, without a separate
//! DUT1 correction. Stellarium's configured Delta-T model supplies TT. The
//! observer is the current stationary Earth location. Topocentric parallax and
//! Stellarium's physical Earth nutation model are applied independently of the
//! host's display toggles. The nutation result fades to zero outside that
//! model's configured date range. Atmosphere, refraction, aberration, and
//! light-time correction are not applied. Azimuth is normalized to [0, 360)
//! degrees with north at 0 and east at 90.
//!
//! A successful result contains exactly:
//! - `schemaVersion` (`int`, currently 1)
//! - `julianDayUt`, `julianDayTt`, and `deltaTSeconds` (`double`)
//! - `sunAzimuthTopocentricGeometricDegrees` and
//!   `sunAltitudeTopocentricGeometricDegrees` (`double`)
//! - `moonAzimuthTopocentricGeometricDegrees` and
//!   `moonAltitudeTopocentricGeometricDegrees` (`double`)
//! - `moonIlluminatedFraction` (`double`, in [0, 1])
//! - `moonAngularDiameterTopocentricUnscaledDegrees` (`double`)
//! - `moonHorizontalParallaxGeocentricDegrees` (`double`)
//!
//! This function computes into local values and does not change the host
//! clock, time rate, location, live Solar System state, or Planet orbit cache.
//! Call it only on Stellarium's main thread after Solar System initialization.
//! The accepted date span is a numerical safety boundary, not a uniform
//! accuracy guarantee for the host Delta-T, nutation, or ephemeris models.
//!
//! @return an empty map for a null/foreign core, non-finite or out-of-range
//! date, unsupported or moving observer, unavailable bodies, a non-main-thread
//! call, or invalid output.
STELMAIN_EXPORT QVariantMap getSunMoonVisibilitySampleAtJulianDayUt(
	const StelCore* core, double julianDayUt);
}

#endif // STELPLUGINAPI_HPP
