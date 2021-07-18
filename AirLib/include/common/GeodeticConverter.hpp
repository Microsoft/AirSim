// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef air_GeodeticConverter_hpp
#define air_GeodeticConverter_hpp

#include <cmath>
#include "VectorMath.hpp"

namespace msr
{
namespace airlib
{

    class GeodeticConverter
    {
    public:
        GeodeticConverter(double home_latitude = 0, double home_longitude = 0, float home_altitude = 0)
        {
            setHome(home_latitude, home_longitude, home_altitude);
        }

        GeodeticConverter(const GeoPoint& home_geopoint)
        {
            setHome(home_geopoint);
        }

        void setHome(double home_latitude, double home_longitude, float home_altitude)
        {
            home_latitude_ = home_latitude;
            home_longitude_ = home_longitude;
            home_altitude_ = home_altitude;

            // Save NED origin
            home_latitude_rad_ = deg2Rad(home_latitude);
            home_longitude_rad_ = deg2Rad(home_longitude);

            // Compute ECEF of NED origin
            geodetic2Ecef(home_latitude_, home_longitude_, home_altitude_, &home_ecef_x_, &home_ecef_y_, &home_ecef_z_);

            // Compute ECEF to NED and NED to ECEF matrices
            ecef_to_ned_matrix_ = nRe(home_latitude_rad_, home_longitude_rad_);
            ned_to_ecef_matrix_ = ecef_to_ned_matrix_.inverse();
        }

        void setHome(const GeoPoint& home_geopoint)
        {
            setHome(home_geopoint.latitude, home_geopoint.longitude, home_geopoint.altitude);
        }

        void getHome(double* latitude, double* longitude, float* altitude)
        {
            *latitude = home_latitude_;
            *longitude = home_longitude_;
            *altitude = home_altitude_;
        }

        void geodetic2Ecef(const double latitude, const double longitude, const double altitude, double* x,
                           double* y, double* z)
        {
            // Convert geodetic coordinates to ECEF.
            // http://code.google.com/p/pysatel/source/browse/trunk/coord.py?r=22
            double lat_rad = deg2Rad(latitude);
            double lon_rad = deg2Rad(longitude);
            double xi = sqrt(1 - kFirstEccentricitySquared * sin(lat_rad) * sin(lat_rad));
            *x = (kSemimajorAxis / xi + altitude) * cos(lat_rad) * cos(lon_rad);
            *y = (kSemimajorAxis / xi + altitude) * cos(lat_rad) * sin(lon_rad);
            *z = (kSemimajorAxis / xi * (1 - kFirstEccentricitySquared) + altitude) * sin(lat_rad);
        }

        void ecef2Geodetic(const double x, const double y, const double z, double* latitude,
                           double* longitude, float* altitude)
        {
            // Convert ECEF coordinates to geodetic coordinates.
            // J. Zhu, "Conversion of Earth-centered Earth-fixed coordinates
            // to geodetic coordinates," IEEE Transactions on Aerospace and
            // Electronic Systems, vol. 30, pp. 957-961, 1994.

            double r = sqrt(x * x + y * y);
            double Esq = kSemimajorAxis * kSemimajorAxis - kSemiminorAxis * kSemiminorAxis;
            double F = 54 * kSemiminorAxis * kSemiminorAxis * z * z;
            double G = r * r + (1 - kFirstEccentricitySquared) * z * z - kFirstEccentricitySquared * Esq;
            double C = (kFirstEccentricitySquared * kFirstEccentricitySquared * F * r * r) / pow(G, 3);
            double S = cbrt(1 + C + sqrt(C * C + 2 * C));
            double P = F / (3 * pow((S + 1 / S + 1), 2) * G * G);
            double Q = sqrt(1 + 2 * kFirstEccentricitySquared * kFirstEccentricitySquared * P);
            double r_0 = -(P * kFirstEccentricitySquared * r) / (1 + Q) + sqrt(
                                                                              0.5 * kSemimajorAxis * kSemimajorAxis * (1 + 1.0 / Q) - P * (1 - kFirstEccentricitySquared) * z * z / (Q * (1 + Q)) - 0.5 * P * r * r);
            double U = sqrt(pow((r - kFirstEccentricitySquared * r_0), 2) + z * z);
            double V = sqrt(
                pow((r - kFirstEccentricitySquared * r_0), 2) + (1 - kFirstEccentricitySquared) * z * z);
            double Z_0 = kSemiminorAxis * kSemiminorAxis * z / (kSemimajorAxis * V);
            *altitude = static_cast<float>(U * (1 - kSemiminorAxis * kSemiminorAxis / (kSemimajorAxis * V)));
            *latitude = rad2Deg(atan((z + kSecondEccentricitySquared * Z_0) / r));
            *longitude = rad2Deg(atan2(y, x));
        }

        void ecef2Ned(const double x, const double y, const double z, double* north, double* east,
                      double* down)
        {
            // Converts ECEF coordinate position into local-tangent-plane NED.
            // Coordinates relative to given ECEF coordinate frame.

            Vector3d vect, ret;
            vect(0) = x - home_ecef_x_;
            vect(1) = y - home_ecef_y_;
            vect(2) = z - home_ecef_z_;
            ret = ecef_to_ned_matrix_ * vect;
            *north = ret(0);
            *east = ret(1);
            *down = -ret(2);
        }

        void ned2Ecef(const double north, const double east, const float down, double* x, double* y,
                      double* z)
        {
            // NED (north/east/down) to ECEF coordinates
            Vector3d ned, ret;
            ned(0) = north;
            ned(1) = east;
            ned(2) = -down;
            ret = ned_to_ecef_matrix_ * ned;
            *x = ret(0) + home_ecef_x_;
            *y = ret(1) + home_ecef_y_;
            *z = ret(2) + home_ecef_z_;
        }

        void geodetic2Ned(const double latitude, const double longitude, const float altitude,
                          double* north, double* east, double* down)
        {
            // Geodetic position to local NED frame
            double x, y, z;
            geodetic2Ecef(latitude, longitude, altitude, &x, &y, &z);
            ecef2Ned(x, y, z, north, east, down);
        }

        void ned2Geodetic(const double north, const double east, const float down, double* latitude,
                          double* longitude, float* altitude)
        {
            // Local NED position to geodetic coordinates
            double x, y, z;
            ned2Ecef(north, east, down, &x, &y, &z);
            ecef2Geodetic(x, y, z, latitude, longitude, altitude);
        }

        void ned2Geodetic(const Vector3r& ned_pos, GeoPoint& geopoint)
        {
            ned2Geodetic(ned_pos[0], ned_pos[1], ned_pos[2], &geopoint.latitude, &geopoint.longitude, &geopoint.altitude);
        }

        void geodetic2Enu(const double latitude, const double longitude, const double altitude,
                          double* east, double* north, double* up)
        {
            // Geodetic position to local ENU frame
            double x, y, z;
            geodetic2Ecef(latitude, longitude, altitude, &x, &y, &z);

            double aux_north, aux_east, aux_down;
            ecef2Ned(x, y, z, &aux_north, &aux_east, &aux_down);

            *east = aux_east;
            *north = aux_north;
            *up = -aux_down;
        }

        void enu2Geodetic(const double east, const double north, const float up, double* latitude,
                          double* longitude, float* altitude)
        {
            // Local ENU position to geodetic coordinates

            const double aux_north = north;
            const double aux_east = east;
            const float aux_down = -up;
            double x, y, z;
            ned2Ecef(aux_north, aux_east, aux_down, &x, &y, &z);
            ecef2Geodetic(x, y, z, latitude, longitude, altitude);
        }

    private:
        typedef msr::airlib::VectorMathf VectorMath;
        typedef VectorMath::Vector3d Vector3d;
        typedef VectorMath::Matrix3x3d Matrix3x3d;

        // Geodetic system parameters
        static constexpr double kSemimajorAxis = 6378137;
        static constexpr double kSemiminorAxis = 6356752.3142;
        static constexpr double kFirstEccentricitySquared = 6.69437999014 * 0.001;
        static constexpr double kSecondEccentricitySquared = 6.73949674228 * 0.001;
        static constexpr double kFlattening = 1 / 298.257223563;

        inline Matrix3x3d nRe(const double lat_radians, const double lon_radians)
        {
            const double sLat = sin(lat_radians);
            const double sLon = sin(lon_radians);
            const double cLat = cos(lat_radians);
            const double cLon = cos(lon_radians);

            Matrix3x3d ret;
            ret(0, 0) = -sLat * cLon;
            ret(0, 1) = -sLat * sLon;
            ret(0, 2) = cLat;
            ret(1, 0) = -sLon;
            ret(1, 1) = cLon;
            ret(1, 2) = 0.0;
            ret(2, 0) = cLat * cLon;
            ret(2, 1) = cLat * sLon;
            ret(2, 2) = sLat;

            return ret;
        }

        inline double rad2Deg(const double radians)
        {
            return (radians / M_PI) * 180.0;
        }

        inline double deg2Rad(const double degrees)
        {
            return (degrees / 180.0) * M_PI;
        }

        double home_latitude_rad_, home_latitude_;
        double home_longitude_rad_, home_longitude_;
        float home_altitude_;

        double home_ecef_x_;
        double home_ecef_y_;
        double home_ecef_z_;

        Matrix3x3d ecef_to_ned_matrix_;
        Matrix3x3d ned_to_ecef_matrix_;

    }; // class GeodeticConverter
}
}
#endif // GEODETIC_CONVERTER_H_
