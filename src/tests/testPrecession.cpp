/*
 * Stellarium 
 * Copyright (C) 2015 Georg Zotti
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include <QObject>
#include <QtDebug>
#include <QVariantList>

#include "tests/testPrecession.hpp"
#include "StelUtils.hpp"
#include "VecMath.hpp"
#include "planetsephems/sidereal_time.h"

QTEST_GUILESS_MAIN(TestPrecession)

static const double arcSec2Rad=M_PI*2.0/(360.0*3600.0);
static const double eps0=84381.406*arcSec2Rad;

void TestPrecession::initTestCase()
{
}

// example data found in Vondrak et al. 2011: New Precession Expressions, Valid for Long Time Intervals. A&A534, A22.
// Section A.5
// pecl = ( +0.00041724785764001342 -0.40495491104576162693 +0.91433656053126552350 )
//      = ( P_A                     -Q_A*C-Z*S              -Q_A*S+Z*C              )
// pequ = ( -0.29437643797369031532 -0.11719098023370257855 +0.94847708824082091796 )
//      = ( X                        Y                       sqrt(1-x^2-y^2) || 0   )
// This provides only the axes formulation. But the final Precession matrix should then be used as reference
// comparable to the matrix that can be reached via the Capitaine parametrisation that we are using.

void TestPrecession::testPrecessionAnglesVondrak()
{
	const double S=sin(eps0);
	const double C=cos(eps0);
	double Z, W;
	double JulianDay=1219339.078000; // TT
	double epsilon_A, chi_A, omega_A, psi_A;
	double P_A, Q_A, X_A, Y_A;
	double VEC2, VEC3, VEQ3;

	// get angles for Capitaine parameterisation
	getPrecessionAnglesVondrak(JulianDay, &epsilon_A, &chi_A, &omega_A, &psi_A);
	// Get reference angles. We have to call this twice with different dates to avoid returning the cached (zero) angles.
	getPrecessionAnglesVondrakPQXYe(-1234.567, &P_A, &Q_A, &X_A, &Y_A, &epsilon_A);
	getPrecessionAnglesVondrakPQXYe(JulianDay, &P_A, &Q_A, &X_A, &Y_A, &epsilon_A);
	Z=sqrt(qMax(1.0-P_A*P_A-Q_A*Q_A, 0.0));
	W=X_A*X_A+Y_A*Y_A;
	VEC2=-Q_A*C -Z*S;
	VEC3=-Q_A*S +Z*C;
	VEQ3=(W<1.0 ? sqrt(1.0-W) : 0.0);

	QVERIFY2(fabs(P_A -0.00041724785764001342)<=0.000001, QString("JD %1: Pecl,x: %2 Difference: %3").arg(JulianDay).arg(P_A ).arg(P_A -0.00041724785764001342).toUtf8());
	QVERIFY2(fabs(VEC2+0.40495491104576162693)<=0.000001, QString("JD %1: Pecl,y: %2 Difference: %3").arg(JulianDay).arg(VEC2).arg(VEC2+0.40495491104576162693).toUtf8());
	QVERIFY2(fabs(VEC3-0.91433656053126552350)<=0.000001, QString("JD %1: Pecl,z: %2 Difference: %3").arg(JulianDay).arg(VEC3).arg(VEC3-0.91433656053126552350).toUtf8());
	QVERIFY2(fabs(X_A +0.29437643797369031532)<=0.000001, QString("JD %1: Pequ,x: %2 Difference: %3").arg(JulianDay).arg(X_A ).arg(X_A +0.29437643797369031532).toUtf8());
	QVERIFY2(fabs(Y_A +0.11719098023370257855)<=0.000001, QString("JD %1: Pequ,y: %2 Difference: %3").arg(JulianDay).arg(Y_A ).arg(Y_A +0.11719098023370257855).toUtf8());
	QVERIFY2(fabs(VEQ3-0.94847708824082091796)<=0.000001, QString("JD %1: Pequ,z: %2 Difference: %3").arg(JulianDay).arg(VEQ3).arg(VEQ3-0.94847708824082091796).toUtf8());
	// the same, to be seen ...
//	qDebug() << QString("JD %1: Pecl,x: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(P_A , 15, 'f', 12).arg(P_A -0.00041724785764001342, 15, 'f', 12);
//	qDebug() << QString("JD %1: Pecl,y: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(VEC2, 15, 'f', 12).arg(VEC2+0.40495491104576162693, 15, 'f', 12);
//	qDebug() << QString("JD %1: Pecl,z: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(VEC3, 15, 'f', 12).arg(VEC3-0.91433656053126552350, 15, 'f', 12);
//	qDebug() << QString("JD %1: Pequ,x: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(X_A , 15, 'f', 12).arg(X_A +0.29437643797369031532, 15, 'f', 12);
//	qDebug() << QString("JD %1: Pequ,y: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(Y_A , 15, 'f', 12).arg(Y_A +0.11719098023370257855, 15, 'f', 12);
//	qDebug() << QString("JD %1: Pequ,z: %2 Difference: %3").arg(JulianDay, 8, 'f', 5).arg(VEQ3, 15, 'f', 12).arg(VEQ3-0.94847708824082091796, 15, 'f', 12);


	Vec3d PECL(P_A, VEC2, VEC3);
	Vec3d PEQR(X_A, Y_A, VEQ3);
	Vec3d EQX=PEQR^PECL;
	EQX.normalize();
	Vec3d V=PEQR^EQX;
	Mat3d RP(EQX[0], EQX[1], EQX[2], V[0], V[1], V[2], PEQR[0], PEQR[1], PEQR[2]); // result from paper, section A.3
	// Now we create the (hopefully) same rotation matrix from the Capitaine angles.
	// Mat4d Rrot=Mat4d::zrotation(chi_A) * Mat4d::xrotation(-omega_A) * Mat4d::zrotation(-psi_A) * Mat4d::xrotation(eps0);

	// Uh-oh - it seems the A&A paper has the matrix operations in the other direction.
	// So the matrix to be compared must be composed in reverse.
	Mat4d RRot=Mat4d::xrotation(eps0)*Mat4d::zrotation(-psi_A) * Mat4d::xrotation(-omega_A) * Mat4d::zrotation(chi_A);

	//qDebug() << "RP     : " << RP.toString(15, 'f', 12);
	//qDebug() << "RcapTr : " << RRot.upper3x3().toString(15, 'f', 12);

	Mat4d RP4(EQX[0], EQX[1], EQX[2], 0, V[0], V[1], V[2], 0, PEQR[0], PEQR[1], PEQR[2], 0, 0, 0, 0, 1);
	//QMatrix4x4 matDiff=((RRot-RP4)).convertToQMatrix();
	//qDebug() << matDiff;
	Mat3d matDiff3x3=(RRot-RP4).upper3x3();
	double max=0.0;
	for (int i=0; i<9; ++i)
	{
		if (fabs(matDiff3x3[i]) > max)
			max=fabs(matDiff3x3[i]);
	}
	//qDebug() << "largest value in difference of matrices:" << max;
	QVERIFY2(max<2e-5, QString("Some values in the precession matrices differ by too much.").toUtf8());

	double angleRef=RP.angle()*180.0/M_PI;
	double angleCap=RRot.upper3x3().angle()*180.0/M_PI;
	//qDebug() << "Rotation angle of reference matrix:" << angleRef;
	//qDebug() << "Rotation angle of Capitaine matrix:" << angleCap;
	//qDebug() << "Difference (arcseconds):" << (angleCap-angleRef)*3600.0;
	// according to Fig12 in the paper, a few arcseconds of difference between PQXY and Capitaine parametrisation are allowed.
	QVERIFY2((angleCap-angleRef)*3600.0<6.0, QString("Angle between rotation matrices too different!").toUtf8());

	//TODO: Add more dates and verify this angle difference is limited to what we can see in Fig.12
}

void TestPrecession::testUncachedEvaluatorsPreserveCacheAnchors()
{
	constexpr double baseJde = 2451545.0;
	double cachedEpsilon, cachedChi, cachedOmega, cachedPsi;
	double uncachedEpsilon, uncachedChi, uncachedOmega, uncachedPsi;

	// Force the historical wrapper's one-day cache to the base epoch.
	getPrecessionAnglesVondrak(baseJde - 10.0, &cachedEpsilon,
		&cachedChi, &cachedOmega, &cachedPsi);
	getPrecessionAnglesVondrak(baseJde, &cachedEpsilon,
		&cachedChi, &cachedOmega, &cachedPsi);
	getPrecessionAnglesVondrakUncached(baseJde, &uncachedEpsilon,
		&uncachedChi, &uncachedOmega, &uncachedPsi);
	QVERIFY(fabs(cachedEpsilon - uncachedEpsilon) < 1e-15);
	QVERIFY(fabs(cachedChi - uncachedChi) < 1e-15);
	QVERIFY(fabs(cachedOmega - uncachedOmega) < 1e-15);
	QVERIFY(fabs(cachedPsi - uncachedPsi) < 1e-15);

	// An isolated far-epoch computation must not move the shared cache anchor.
	getPrecessionAnglesVondrakUncached(baseJde + 1000.0,
		&uncachedEpsilon, &uncachedChi, &uncachedOmega, &uncachedPsi);
	double nearbyEpsilon, nearbyChi, nearbyOmega, nearbyPsi;
	getPrecessionAnglesVondrak(baseJde + 0.5, &nearbyEpsilon,
		&nearbyChi, &nearbyOmega, &nearbyPsi);
	QCOMPARE(nearbyEpsilon, cachedEpsilon);
	QCOMPARE(nearbyChi, cachedChi);
	QCOMPARE(nearbyOmega, cachedOmega);
	QCOMPARE(nearbyPsi, cachedPsi);

	double cachedDeltaPsi, cachedDeltaEpsilon;
	double uncachedDeltaPsi, uncachedDeltaEpsilon;
	getNutationAngles(baseJde - 1.0,
		&cachedDeltaPsi, &cachedDeltaEpsilon);
	getNutationAngles(baseJde,
		&cachedDeltaPsi, &cachedDeltaEpsilon);
	getNutationAnglesUncached(baseJde,
		&uncachedDeltaPsi, &uncachedDeltaEpsilon);
	QVERIFY(fabs(cachedDeltaPsi - uncachedDeltaPsi) < 1e-15);
	QVERIFY(fabs(cachedDeltaEpsilon - uncachedDeltaEpsilon) < 1e-15);

	getNutationAnglesUncached(baseJde + 1000.0,
		&uncachedDeltaPsi, &uncachedDeltaEpsilon);
	double nearbyDeltaPsi, nearbyDeltaEpsilon;
	getNutationAngles(baseJde + 1.0/48.0,
		&nearbyDeltaPsi, &nearbyDeltaEpsilon);
	QCOMPARE(nearbyDeltaPsi, cachedDeltaPsi);
	QCOMPARE(nearbyDeltaEpsilon, cachedDeltaEpsilon);

	// The uncached apparent-sidereal evaluator must use the same numerical
	// model without moving either shared cache anchor.
	getPrecessionAnglesVondrak(baseJde - 10.0, &cachedEpsilon,
		&cachedChi, &cachedOmega, &cachedPsi);
	getPrecessionAnglesVondrak(baseJde, &cachedEpsilon,
		&cachedChi, &cachedOmega, &cachedPsi);
	getNutationAngles(baseJde - 1.0,
		&cachedDeltaPsi, &cachedDeltaEpsilon);
	getNutationAngles(baseJde,
		&cachedDeltaPsi, &cachedDeltaEpsilon);
	const double cachedSidereal =
		get_apparent_sidereal_time(baseJde, baseJde);
	const double uncachedSidereal =
		get_apparent_sidereal_time_uncached(baseJde, baseJde);
	QVERIFY(fabs(cachedSidereal - uncachedSidereal) < 1e-12);

	get_apparent_sidereal_time_uncached(
		baseJde + 1000.0, baseJde + 1000.0);
	getPrecessionAnglesVondrak(baseJde + 0.5, &nearbyEpsilon,
		&nearbyChi, &nearbyOmega, &nearbyPsi);
	getNutationAngles(baseJde + 1.0/48.0,
		&nearbyDeltaPsi, &nearbyDeltaEpsilon);
	QCOMPARE(nearbyEpsilon, cachedEpsilon);
	QCOMPARE(nearbyChi, cachedChi);
	QCOMPARE(nearbyOmega, cachedOmega);
	QCOMPARE(nearbyPsi, cachedPsi);
	QCOMPARE(nearbyDeltaPsi, cachedDeltaPsi);
	QCOMPARE(nearbyDeltaEpsilon, cachedDeltaEpsilon);
}
