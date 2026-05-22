#ifndef PCLITE_MAT_H
#define PCLITE_MAT_H

#include <array>
#include <cstddef>

// Fixed-size matrix.
// Internal storage is row-major; external callers never touch data_ directly.
// Use operator()(row, col) for element access.
// Use data() to get a flat array — column-major by default (OpenGL convention).
template <typename T, std::size_t R, std::size_t C>
struct Mat {

    // ── Element access ────────────────────────────────────────────────────────
    T&       operator()(std::size_t r, std::size_t c)       { return data_[r * C + c]; }
    const T& operator()(std::size_t r, std::size_t c) const { return data_[r * C + c]; }

    // ── Raw data ──────────────────────────────────────────────────────────────
    // isColumnBased = true  → column-major (OpenGL / GLSL convention)
    // isColumnBased = false → row-major    (C array / math convention)
    std::array<T, R * C> data(bool isColumnBased = true) const {
        if (!isColumnBased) return data_;
        std::array<T, R * C> out;
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = 0; c < C; ++c)
                out[c * R + r] = data_[r * C + c];
        return out;
    }

    // ── Componentwise ─────────────────────────────────────────────────────────
    Mat operator+(const Mat& o) const {
        Mat result;
        for (std::size_t i = 0; i < R * C; ++i) result.data_[i] = data_[i] + o.data_[i];
        return result;
    }
    Mat operator-(const Mat& o) const {
        Mat result;
        for (std::size_t i = 0; i < R * C; ++i) result.data_[i] = data_[i] - o.data_[i];
        return result;
    }
    Mat& operator+=(const Mat& o) { *this = *this + o; return *this; }
    Mat& operator-=(const Mat& o) { *this = *this - o; return *this; }

    // ── Scalar ────────────────────────────────────────────────────────────────
    Mat operator*(T s) const {
        Mat result;
        for (std::size_t i = 0; i < R * C; ++i) result.data_[i] = data_[i] * s;
        return result;
    }
    Mat operator/(T s) const {
        Mat result;
        for (std::size_t i = 0; i < R * C; ++i) result.data_[i] = data_[i] / s;
        return result;
    }
    Mat& operator*=(T s) { *this = *this * s; return *this; }
    Mat& operator/=(T s) { *this = *this / s; return *this; }

    friend Mat operator*(T s, const Mat& m) { return m * s; }

    // ── Matrix multiply ───────────────────────────────────────────────────────
    // Mat<T,R,C> * Mat<T,C,K> → Mat<T,R,K>
    template <std::size_t K>
    Mat<T, R, K> operator*(const Mat<T, C, K>& o) const {
        Mat<T, R, K> result;
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t k = 0; k < K; ++k) {
                T sum = T(0);
                for (std::size_t c = 0; c < C; ++c)
                    sum += (*this)(r, c) * o(c, k);
                result(r, k) = sum;
            }
        return result;
    }

    // ── Transpose ─────────────────────────────────────────────────────────────
    Mat<T, C, R> transposed() const {
        Mat<T, C, R> result;
        for (std::size_t r = 0; r < R; ++r)
            for (std::size_t c = 0; c < C; ++c)
                result(c, r) = (*this)(r, c);
        return result;
    }

    // ── Identity (square matrices only) ───────────────────────────────────────
    static Mat identity() requires (R == C) {
        Mat result;
        for (std::size_t i = 0; i < R; ++i) result(i, i) = T(1);
        return result;
    }

    // ── Equality ──────────────────────────────────────────────────────────────
    bool operator==(const Mat& o) const { return data_ == o.data_; }
    bool operator!=(const Mat& o) const { return data_ != o.data_; }

private:
    std::array<T, R * C> data_ = {};
};

// ── Common aliases ────────────────────────────────────────────────────────────
using Mat2i = Mat<int,    2, 2>;
using Mat3i = Mat<int,    3, 3>;
using Mat4i = Mat<int,    4, 4>;
using Mat2f = Mat<float,  2, 2>;
using Mat3f = Mat<float,  3, 3>;
using Mat4f = Mat<float,  4, 4>;
using Mat2d = Mat<double, 2, 2>;
using Mat3d = Mat<double, 3, 3>;
using Mat4d = Mat<double, 4, 4>;

#endif //PCLITE_MAT_H
