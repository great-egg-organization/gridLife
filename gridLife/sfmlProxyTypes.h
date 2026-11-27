#pragma once

#ifndef INTERACTIVE_SIM
namespace sf{
    template <typename T>
    struct Vector2{
    	T x;
    	T y;
    	
    	constexpr Vector2() = default;
    	constexpr Vector2(T _x, T _y) : x(_x), y(_y) {}
    	constexpr Vector2(const Vector2<T>&) = default;
    	constexpr Vector2<T>& operator=(const Vector2<T>&) = default;
    	
    	Vector2<T> operator+(const Vector2<T>& other) const{
    		return {x + other.x, y + other.y};
    	}
    	
    	Vector2<T> operator-(const Vector2<T>& other) const{
    		return {x - other.x, y - other.y};
    	}
    	
    	Vector2<T> operator-() const{
    		return {-x, -y};
    	}
    	
    	Vector2<T> operator*(T s) const{
    		return {x * s, y * s};
    	}
    	
    	Vector2<T> perpendicular() const{
    		return {-y, x};
    	}
    };
    
    typedef Vector2<int> Vector2i;
    typedef Vector2<float> Vector2f;
}
#else
#include <SFML/System.hpp>
#endif
