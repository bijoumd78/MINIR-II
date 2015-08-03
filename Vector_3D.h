//-----------------------------------------------------------------------------
//  Copyright (C) Siemens AG 2005  All Rights Reserved.
//-----------------------------------------------------------------------------
//
//    Project: IceScanNetworkFunctor
//    File: Vector_3D.h
//    Version: 1.0
//    Author: Mahamadou Diakite, PhD
//    Date: 4/16/2015
//    Language: C++
//    Description: 3D vector data structure
//
//-----------------------------------------------------------------------------

#ifndef VECTOR_3D_H
#define VECTOR_3D_H

template<typename T>
class Vector_3D
{
public:
    Vector_3D(){}
    Vector_3D(T a, T b, T c):
        x(a),
        y(b),
        z(c)
    {}

    // Getter and Setter functions
    T get_x()const {return x;}
    T get_y()const {return y;}
    T get_z()const {return z;}

    void set_x(T x1){x = x1;}
    void set_y(T y1){y = y1;}
    void set_z(T z1){z = z1;}

    // Dot and cross product
    T dotproduct ( const Vector_3D & rhs ) {
        T scalar = x * rhs.x + y * rhs.y + z * rhs.z ;
        return scalar ;
    }

    Vector_3D crossproduct ( const Vector_3D & rhs ) {
        T a = y * rhs.z - z * rhs.y ;
        T b = z * rhs.x - x * rhs.z ;
        T c = x * rhs.y - y * rhs.x ;
        Vector_3D product( a , b , c ) ;
        return product ;
    }

private:
    T x, y, z;
};

#endif // VECTOR_3D_H
