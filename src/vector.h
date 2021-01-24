#ifndef VECTOR_H
#define VECTOR_H

#include <cstdint>
#include <stddef.h>
#include <math.h>
#include <vector>


template <class T> class Vector3 {
public:
	T v[3];

	Vector3() {}
	Vector3(T x, T y, T z) {
		v[0] = x; v[1] = y; v[2] = z;
	}

	T &operator[](int i) { return v[i]; }
	const T &operator[](int i) const { return v[i]; }

	Vector3 operator+(const Vector3 &b) const { return Vector3(v[0]+b[0], v[1]+b[1], v[2]+b[2]);	}
	Vector3 operator-(const Vector3 &b) const { return Vector3(v[0]-b[0], v[1]-b[1], v[2]-b[2]);	}
	Vector3 operator*(float d) { return Vector3(v[0]*d, v[1]*d, v[2]*d); }
	T operator*(const Vector3 &b) { return v[0]*b[0] + v[1]*b[1] + v[2]*b[2]; }
	Vector3 operator/(float d) { return Vector3(v[0]/d, v[1]/d, v[2]/d); }
	double squaredNorm() { return v[0]*(double)v[0] + v[1]*(double)v[1] + v[2]*(double)v[2]; }
	double norm() { return sqrt(squaredNorm()); }
};

typedef Vector3<uint16_t> Vector3us;
typedef Vector3<float> Vector3f;
typedef Vector3<double> Vector3d;


template <class T> struct Color3 {
	T r, g, b;
	Color3() {}
	Color3(T _r, T _g, T _b): r(_r), g(_g), b(_b) {}
	T &operator[](int n) { return ((T *)this)[n]; }

	//This is actually worse than the approximate ycc below.
	Color3 RgbToYCbCr() {
		Color3 c;
		c.r =       0.299   * r + 0.587   * g + 0.114   * b;
		c.g = 0.5 - 0.16874 * r - 0.33126 * g + 0.5     * b;
		c.b = 0.5 + 0.50000 * r - 0.41869 * g - 0.08131 * b;
		return c;
	}

	Color3 YCbCrToRgb() {
		T G = g - 0.5f;
		T B = b - 0.5f;

		Color3 c;
		c.r = r +                 1.402    * B;
		c.g = r + -0.344136 * G - 0.714136 * B;
		c.b = r +  1.772    * G;
		return c;
	}

	Color3 toYcc() {
		Color3 yog;
		yog.g = r - b;
		T tmp = b + yog.g/2;
		yog.b = g - tmp;
		yog.r = tmp + yog.b/2;
		return yog;
	}
	Color3 toRgb() {
		Color3 rgb;
		T tmp = r - b/2;
		rgb.g = b + tmp;
		rgb.b = tmp - g/2;
		rgb.r = rgb.b + g;
		return rgb;
	}
	void operator*=(float v) {
		r *= v;
		g *= v;
		b *= v;
	}
};
typedef Color3<uint8_t>  Color3b;
typedef Color3<uint16_t>  Color3us;
typedef Color3<float>  Color3f;
typedef Color3<double>  Color3d;


//a Pixel is the collection of N lights intensity.
//pixel array is organized by pixel:
//pixel0: light1, light2 ... light n;
//then pixel1: etc etc.

class PixelArray: public std::vector<Color3f> {
public:
	uint32_t nlights;

	PixelArray(size_t n = 0, size_t k = 0): nlights(k) {
		std::vector<Color3f>::resize(n*nlights);
	}
	void resize(size_t n, size_t k) {
		nlights = k;
		std::vector<Color3f>::resize(n*nlights);
	}
	uint32_t components() { return nlights; }
	uint32_t npixels() { return size()/nlights; }
	Color3f *pixel(size_t i) {
		return data() + i*nlights;
	}
	Color3f *operator()(int i) {
		return (data() + i*nlights);
	}
	Color3f &operator()(size_t i, size_t k) { return *(data() + i*nlights + k); }
	int npixels() const { return (int)size()/nlights; }
	float *rawdata() { return (float *)data(); }

};

#endif // VECTOR_H
