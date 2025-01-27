#ifndef SLANG_PRELUDE_CPP_TYPES_H
#define SLANG_PRELUDE_CPP_TYPES_H

#ifndef SLANG_PRELUDE_ASSERT
#   ifdef SLANG_PRELUDE_ENABLE_ASSERT
#       define SLANG_PRELUDE_ASSERT(VALUE) assert(VALUE)
#   else
#       define SLANG_PRELUDE_ASSERT(VALUE) 
#   endif
#endif

// Since we are using unsigned arithmatic care is need in this comparison.
// It is *assumed* that sizeInBytes >= elemSize. Which means (sizeInBytes >= elemSize) >= 0
// Which means only a single test is needed

// Asserts for bounds checking.
// It is assumed index/count are unsigned types.
#define SLANG_BOUND_ASSERT(index, count)  SLANG_PRELUDE_ASSERT(index < count); 
#define SLANG_BOUND_ASSERT_BYTE_ADDRESS(index, elemSize, sizeInBytes) SLANG_PRELUDE_ASSERT(index <= (sizeInBytes - elemSize) && (index & 3) == 0);

// Macros to zero index if an access is out of range
#define SLANG_BOUND_ZERO_INDEX(index, count) index = (index < count) ? index : 0; 
#define SLANG_BOUND_ZERO_INDEX_BYTE_ADDRESS(index, elemSize, sizeInBytes) index = (index <= (sizeInBytes - elemSize)) ? index : 0; 

// The 'FIX' macro define how the index is fixed. The default is to do nothing. If SLANG_ENABLE_BOUND_ZERO_INDEX
// the fix macro will zero the index, if out of range
#ifdef  SLANG_ENABLE_BOUND_ZERO_INDEX
#   define SLANG_BOUND_FIX(index, count) SLANG_BOUND_ZERO_INDEX(index, count)
#   define SLANG_BOUND_FIX_BYTE_ADDRESS(index, elemSize, sizeInBytes) SLANG_BOUND_ZERO_INDEX_BYTE_ADDRESS(index, elemSize, sizeInBytes)
#   define SLANG_BOUND_FIX_FIXED_ARRAY(index, count) SLANG_BOUND_ZERO_INDEX(index, count)
#else
#   define SLANG_BOUND_FIX(index, count) 
#   define SLANG_BOUND_FIX_BYTE_ADDRESS(index, elemSize, sizeInBytes) 
#   define SLANG_BOUND_FIX_FIXED_ARRAY(index, count) 
#endif

#ifndef SLANG_BOUND_CHECK
#   define SLANG_BOUND_CHECK(index, count) SLANG_BOUND_ASSERT(index, count) SLANG_BOUND_FIX(index, count)
#endif

#ifndef SLANG_BOUND_CHECK_BYTE_ADDRESS
#   define SLANG_BOUND_CHECK_BYTE_ADDRESS(index, elemSize, sizeInBytes) SLANG_BOUND_ASSERT_BYTE_ADDRESS(index, elemSize, sizeInBytes) SLANG_BOUND_FIX_BYTE_ADDRESS(index, elemSize, sizeInBytes)
#endif

#ifndef SLANG_BOUND_CHECK_FIXED_ARRAY
#   define SLANG_BOUND_CHECK_FIXED_ARRAY(index, count) SLANG_BOUND_ASSERT(index, count) SLANG_BOUND_FIX_FIXED_ARRAY(index, count)
#endif

#ifdef SLANG_PRELUDE_NAMESPACE
namespace SLANG_PRELUDE_NAMESPACE {
#endif

struct TypeInfo
{
    size_t typeSize;
};

template <typename T, size_t SIZE>
struct FixedArray
{
    const T& operator[](size_t index) const { SLANG_BOUND_CHECK_FIXED_ARRAY(index, SIZE); return m_data[index]; }
    T& operator[](size_t index) { SLANG_BOUND_CHECK_FIXED_ARRAY(index, SIZE); return m_data[index]; }

    T m_data[SIZE];
};

// An array that has no specified size, becomes a 'Array'. This stores the size so it can potentially 
// do bounds checking.  
template <typename T>
struct Array
{
    const T& operator[](size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    T& operator[](size_t index) { SLANG_BOUND_CHECK(index, count); return data[index]; }

    T* data;
    size_t count;
};

/* Constant buffers become a pointer to the contained type, so ConstantBuffer<T> becomes T* in C++ code.
*/

template <typename T, int COUNT>
struct Vector;

template <typename T>
struct Vector<T, 1>
{
    T x;
    const T& operator[](size_t /*index*/) const { return x; }
    T& operator[](size_t /*index*/) { return x; }
    operator T() const { return x; }
    Vector() = default;
    Vector(T scalar)
    {
        x = scalar;
    }
    template <typename U>
    Vector(Vector<U, 1> other)
    {
        x = (T)other.x;
    }
    template <typename U, int otherSize>
    Vector(Vector<U, otherSize> other)
    {
        int minSize = 1;
        if (otherSize < minSize) minSize = otherSize;
        for (int i = 0; i < minSize; i++)
            (*this)[i] = (T)other[i];
    }
};

template <typename T>
struct Vector<T, 2>
{
    T x, y;
    const T& operator[](size_t index) const { return index == 0 ? x : y; }
    T& operator[](size_t index) { return index == 0 ? x : y; }
    Vector() = default;
    Vector(T scalar)
    {
        x = y = scalar;
    }
    Vector(T _x, T _y)
    {
        x = _x;
        y = _y;
    }
    template <typename U>
    Vector(Vector<U, 2> other)
    {
        x = (T)other.x;
        y = (T)other.y;
    }
    template <typename U, int otherSize>
    Vector(Vector<U, otherSize> other)
    {
        int minSize = 2;
        if (otherSize < minSize) minSize = otherSize;
        for (int i = 0; i < minSize; i++)
            (*this)[i] = (T)other[i];
    }
};

template <typename T>
struct Vector<T, 3>
{
    T x, y, z;
    const T& operator[](size_t index) const { return *((T*)(this) + index); }
    T& operator[](size_t index) { return *((T*)(this) + index); }

    Vector() = default;
    Vector(T scalar)
    {
        x = y = z = scalar;
    }
    Vector(T _x, T _y, T _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    template <typename U>
    Vector(Vector<U, 3> other)
    {
        x = (T)other.x;
        y = (T)other.y;
        z = (T)other.z;
    }
    template <typename U, int otherSize>
    Vector(Vector<U, otherSize> other)
    {
        int minSize = 3;
        if (otherSize < minSize) minSize = otherSize;
        for (int i = 0; i < minSize; i++)
            (*this)[i] = (T)other[i];
    }
};

template <typename T>
struct Vector<T, 4>
{
    T x, y, z, w;

    const T& operator[](size_t index) const { return *((T*)(this) + index); }
    T& operator[](size_t index) { return *((T*)(this) + index); }
    Vector() = default;
    Vector(T scalar)
    {
        x = y = z = w = scalar;
    }
    Vector(T _x, T _y, T _z, T _w)
    {
        x = _x;
        y = _y;
        z = _z;
        w = _w;
    }
    template <typename U, int otherSize>
    Vector(Vector<U, otherSize> other)
    {
        int minSize = 4;
        if (otherSize < minSize) minSize = otherSize;
        for (int i = 0; i < minSize; i++)
            (*this)[i] = (T)other[i];
    }
 
};

template<typename T, int N>
SLANG_FORCE_INLINE T _slang_vector_get_element(Vector<T, N> x, int index)
{
    return x[index];
}

template<typename T, int N>
SLANG_FORCE_INLINE const T* _slang_vector_get_element_ptr(const Vector<T, N>* x, int index)
{
    return &((*const_cast<Vector<T,N>*>(x))[index]);
}

template<typename T, int N>
SLANG_FORCE_INLINE T* _slang_vector_get_element_ptr(Vector<T, N>* x, int index)
{
    return &((*x)[index]);
}

template<typename T, int n, typename OtherT, int m>
SLANG_FORCE_INLINE Vector<T, n> _slang_vector_reshape(const Vector<OtherT, m> other)
{
    Vector<T, n> result;
    for (int i = 0; i < n; i++)
    {
        OtherT otherElement = T(0);
        if (i < m)
            otherElement = _slang_vector_get_element(other, i);
        *_slang_vector_get_element_ptr(&result, i) = (T)otherElement;
    }
    return result;
}

typedef uint32_t uint;

typedef Vector<float, 2> float2;
typedef Vector<float, 3> float3;
typedef Vector<float, 4> float4;

typedef Vector<int32_t, 2> int2;
typedef Vector<int32_t, 3> int3;
typedef Vector<int32_t, 4> int4;

typedef Vector<uint32_t, 2> uint2;
typedef Vector<uint32_t, 3> uint3;
typedef Vector<uint32_t, 4> uint4;

#define SLANG_VECTOR_BINARY_OP(T, op) \
    template<int n> \
    SLANG_FORCE_INLINE Vector<T, n> operator op(const Vector<T, n>& thisVal, const Vector<T, n>& other) \
    { \
        Vector<T, n> result;\
        for (int i = 0; i < n; i++) \
            result[i] = thisVal[i] op other[i]; \
        return result;\
    }
#define SLANG_VECTOR_BINARY_COMPARE_OP(T, op) \
    template<int n> \
    SLANG_FORCE_INLINE Vector<bool, n> operator op(const Vector<T, n>& thisVal, const Vector<T, n>& other) \
    { \
        Vector<bool, n> result;\
        for (int i = 0; i < n; i++) \
            result[i] = thisVal[i] op other[i]; \
        return result;\
    }

#define SLANG_VECTOR_UNARY_OP(T, op) \
    template<int n> \
    SLANG_FORCE_INLINE Vector<T, n> operator op(const Vector<T, n>& thisVal) \
    { \
        Vector<T, n> result;\
        for (int i = 0; i < n; i++) \
            result[i] = op thisVal[i]; \
        return result;\
    }
#define SLANG_INT_VECTOR_OPS(T) \
    SLANG_VECTOR_BINARY_OP(T, +)\
    SLANG_VECTOR_BINARY_OP(T, -)\
    SLANG_VECTOR_BINARY_OP(T, *)\
    SLANG_VECTOR_BINARY_OP(T, / )\
    SLANG_VECTOR_BINARY_OP(T, &)\
    SLANG_VECTOR_BINARY_OP(T, |)\
    SLANG_VECTOR_BINARY_OP(T, &&)\
    SLANG_VECTOR_BINARY_OP(T, ||)\
    SLANG_VECTOR_BINARY_OP(T, ^)\
    SLANG_VECTOR_BINARY_OP(T, %)\
    SLANG_VECTOR_BINARY_OP(T, >>)\
    SLANG_VECTOR_BINARY_OP(T, <<)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, >)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, <)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, >=)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, <=)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, ==)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, !=)\
    SLANG_VECTOR_UNARY_OP(T, !)\
    SLANG_VECTOR_UNARY_OP(T, ~)
#define SLANG_FLOAT_VECTOR_OPS(T) \
    SLANG_VECTOR_BINARY_OP(T, +)\
    SLANG_VECTOR_BINARY_OP(T, -)\
    SLANG_VECTOR_BINARY_OP(T, *)\
    SLANG_VECTOR_BINARY_OP(T, /)\
    SLANG_VECTOR_UNARY_OP(T, -)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, >)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, <)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, >=)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, <=)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, ==)\
    SLANG_VECTOR_BINARY_COMPARE_OP(T, !=)

SLANG_INT_VECTOR_OPS(bool)
SLANG_INT_VECTOR_OPS(int)
SLANG_INT_VECTOR_OPS(int8_t)
SLANG_INT_VECTOR_OPS(int16_t)
SLANG_INT_VECTOR_OPS(int64_t)
SLANG_INT_VECTOR_OPS(uint)
SLANG_INT_VECTOR_OPS(uint8_t)
SLANG_INT_VECTOR_OPS(uint16_t)
SLANG_INT_VECTOR_OPS(uint64_t)

SLANG_FLOAT_VECTOR_OPS(float)
SLANG_FLOAT_VECTOR_OPS(double)

#define SLANG_VECTOR_INT_NEG_OP(T) \
    template<int N>\
    Vector<T, N> operator-(const Vector<T, N>& thisVal) \
    { \
        Vector<T, N> result;\
        for (int i = 0; i < N; i++) \
            result[i] = 0 - thisVal[i]; \
        return result;\
    }
SLANG_VECTOR_INT_NEG_OP(int)
SLANG_VECTOR_INT_NEG_OP(int8_t)
SLANG_VECTOR_INT_NEG_OP(int16_t)
SLANG_VECTOR_INT_NEG_OP(int64_t)
SLANG_VECTOR_INT_NEG_OP(uint)
SLANG_VECTOR_INT_NEG_OP(uint8_t)
SLANG_VECTOR_INT_NEG_OP(uint16_t)
SLANG_VECTOR_INT_NEG_OP(uint64_t)

#define SLANG_FLOAT_VECTOR_MOD(T)\
    template<int N> \
    Vector<T, N> operator%(const Vector<T, N>& left, const Vector<T, N>& right) \
    {\
        Vector<T, N> result;\
        for (int i = 0; i < N; i++) \
            result[i] = _slang_fmod(left[i], right[i]); \
        return result;\
    }

SLANG_FLOAT_VECTOR_MOD(float)
SLANG_FLOAT_VECTOR_MOD(double)
#undef SLANG_FLOAT_VECTOR_MOD
#undef SLANG_VECTOR_BINARY_OP
#undef SLANG_VECTOR_UNARY_OP
#undef SLANG_INT_VECTOR_OPS
#undef SLANG_FLOAT_VECTOR_OPS
#undef SLANG_VECTOR_INT_NEG_OP
#undef SLANG_FLOAT_VECTOR_MOD

template <typename T, int ROWS, int COLS>
struct Matrix
{
    Vector<T, COLS> rows[ROWS];
    Vector<T, COLS>& operator[](size_t index) { return rows[index]; }
    Matrix() = default;
    Matrix(T scalar)
    {
        for (int i = 0; i < ROWS; i++)
            rows[i] = Vector<T, COLS>(scalar);
    }
    Matrix(const Vector<T, COLS>& row0)
    {
        rows[0] = row0;
    }
    Matrix(const Vector<T, COLS>& row0, const Vector<T, COLS>& row1)
    {
        rows[0] = row0;
        rows[1] = row1;
    }
    Matrix(const Vector<T, COLS>& row0, const Vector<T, COLS>& row1, const Vector<T, COLS>& row2)
    {
        rows[0] = row0;
        rows[1] = row1;
        rows[2] = row2;
    }
    Matrix(const Vector<T, COLS>& row0, const Vector<T, COLS>& row1, const Vector<T, COLS>& row2, const Vector<T, COLS>& row3)
    {
        rows[0] = row0;
        rows[1] = row1;
        rows[2] = row2;
        rows[3] = row3;
    }
    template<typename U, int otherRow, int otherCol>
    Matrix(const Matrix<U, otherRow, otherCol>& other)
    {
        int minRow = ROWS;
        int minCol = COLS;
        if (minRow > otherRow) minRow = otherRow;
        if (minCol > otherCol) minCol = otherCol;
        for (int i = 0; i < minRow; i++)
            for (int j = 0; j < minCol; j++)
                rows[i][j] = (T)other.rows[i][j];
    }
    Matrix(T v0, T v1, T v2, T v3)
    {
        rows[0][0] = v0;  rows[0][1] = v1;
        rows[1][0] = v2;  rows[1][1] = v3;
    }
    Matrix(T v0, T v1, T v2, T v3, T v4, T v5)
    {
        if (COLS == 3)
        {
            rows[0][0] = v0;  rows[0][1] = v1; rows[0][2] = v2;
            rows[1][0] = v3;  rows[1][1] = v4; rows[1][2] = v5;
        }
        else
        {
            rows[0][0] = v0;  rows[0][1] = v1;
            rows[1][0] = v2;  rows[1][1] = v3;
            rows[2][0] = v4;  rows[2][1] = v5;
        }
    }
    Matrix(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
    {
        if (COLS == 4)
        {
            rows[0][0] = v0;  rows[0][1] = v1; rows[0][2] = v2; rows[0][3] = v3;
            rows[1][0] = v4;  rows[1][1] = v5; rows[1][2] = v6; rows[1][3] = v7;
        }
        else
        {
            rows[0][0] = v0;  rows[0][1] = v1;
            rows[1][0] = v2;  rows[1][1] = v3;
            rows[2][0] = v4;  rows[2][1] = v5;
            rows[3][0] = v6;  rows[3][1] = v7;
        }
    }
    Matrix(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8)
    {
        rows[0][0] = v0;  rows[0][1] = v1;  rows[0][2] = v2;
        rows[1][0] = v3;  rows[1][1] = v4;  rows[1][2] = v5;
        rows[2][0] = v6;  rows[2][1] = v7;  rows[2][2] = v8;
    }
    Matrix(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9, T v10, T v11)
    {
        if (COLS == 4)
        {
            rows[0][0] = v0;  rows[0][1] = v1;  rows[0][2] = v2;  rows[0][3] = v3;
            rows[1][0] = v4;  rows[1][1] = v5;  rows[1][2] = v6;  rows[1][3] = v7;
            rows[2][0] = v8;  rows[2][1] = v9;  rows[2][2] = v10; rows[2][3] = v11;
        }
        else
        {
            rows[0][0] = v0;  rows[0][1] = v1;  rows[0][2] = v2;
            rows[1][0] = v3;  rows[1][1] = v4;  rows[1][2] = v5;
            rows[2][0] = v6;  rows[2][1] = v7;  rows[2][2] = v8;
            rows[3][0] = v9;  rows[3][1] = v10; rows[3][2] = v11;
        }
    }
    Matrix(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9, T v10, T v11, T v12, T v13, T v14, T v15)
    {
        rows[0][0] = v0;  rows[0][1] = v1;  rows[0][2] = v2;  rows[0][3] = v3;
        rows[1][0] = v4;  rows[1][1] = v5;  rows[1][2] = v6;  rows[1][3] = v7;
        rows[2][0] = v8;  rows[2][1] = v9;  rows[2][2] = v10; rows[2][3] = v11;
        rows[3][0] = v12; rows[3][1] = v13; rows[3][2] = v14; rows[3][3] = v15;
    }
};

#define SLANG_MATRIX_BINARY_OP(T, op) \
    template<int R, int C> \
    Matrix<T, R, C> operator op(const Matrix<T, R, C>& thisVal, const Matrix<T, R, C>& other) \
    { \
        Matrix<T, R, C> result;\
        for (int i = 0; i < R; i++) \
            for (int j = 0; j < C; j++) \
                result.rows[i][j] = thisVal.rows[i][j] op other.rows[i][j]; \
        return result;\
    }

#define SLANG_MATRIX_UNARY_OP(T, op) \
    template<int R, int C> \
    Matrix<T, R, C> operator op(const Matrix<T, R, C>& thisVal) \
    { \
        Matrix<T, R, C> result;\
        for (int i = 0; i < R; i++) \
            for (int j = 0; j < C; j++) \
                result[i].rows[i][j] = op thisVal.rows[i][j]; \
        return result;\
    }
#define SLANG_INT_MATRIX_OPS(T) \
    SLANG_MATRIX_BINARY_OP(T, +)\
    SLANG_MATRIX_BINARY_OP(T, -)\
    SLANG_MATRIX_BINARY_OP(T, *)\
    SLANG_MATRIX_BINARY_OP(T, / )\
    SLANG_MATRIX_BINARY_OP(T, &)\
    SLANG_MATRIX_BINARY_OP(T, |)\
    SLANG_MATRIX_BINARY_OP(T, &&)\
    SLANG_MATRIX_BINARY_OP(T, ||)\
    SLANG_MATRIX_BINARY_OP(T, ^)\
    SLANG_MATRIX_BINARY_OP(T, %)\
    SLANG_MATRIX_UNARY_OP(T, !)\
    SLANG_MATRIX_UNARY_OP(T, ~)
#define SLANG_FLOAT_MATRIX_OPS(T) \
    SLANG_MATRIX_BINARY_OP(T, +)\
    SLANG_MATRIX_BINARY_OP(T, -)\
    SLANG_MATRIX_BINARY_OP(T, *)\
    SLANG_MATRIX_BINARY_OP(T, /)\
    SLANG_MATRIX_UNARY_OP(T, -)
SLANG_INT_MATRIX_OPS(int)
SLANG_INT_MATRIX_OPS(int8_t)
SLANG_INT_MATRIX_OPS(int16_t)
SLANG_INT_MATRIX_OPS(int64_t)
SLANG_INT_MATRIX_OPS(uint)
SLANG_INT_MATRIX_OPS(uint8_t)
SLANG_INT_MATRIX_OPS(uint16_t)
SLANG_INT_MATRIX_OPS(uint64_t)

SLANG_FLOAT_MATRIX_OPS(float)
SLANG_FLOAT_MATRIX_OPS(double)

#define SLANG_MATRIX_INT_NEG_OP(T) \
    template<int R, int C>\
    SLANG_FORCE_INLINE Matrix<T, R, C> operator-(Matrix<T, R, C> thisVal) \
    { \
        Matrix<T, R, C> result;\
        for (int i = 0; i < R; i++) \
            for (int j = 0; j < C; j++) \
            result.rows[i][j] = 0 - thisVal.rows[i][j]; \
        return result;\
    }
    SLANG_MATRIX_INT_NEG_OP(int)
    SLANG_MATRIX_INT_NEG_OP(int8_t)
    SLANG_MATRIX_INT_NEG_OP(int16_t)
    SLANG_MATRIX_INT_NEG_OP(int64_t)
    SLANG_MATRIX_INT_NEG_OP(uint)
    SLANG_MATRIX_INT_NEG_OP(uint8_t)
    SLANG_MATRIX_INT_NEG_OP(uint16_t)
    SLANG_MATRIX_INT_NEG_OP(uint64_t)

#define SLANG_FLOAT_MATRIX_MOD(T)\
    template<int R, int C> \
    SLANG_FORCE_INLINE Matrix<T, R, C> operator%(Matrix<T, R, C> left, Matrix<T, R, C> right) \
    {\
        Matrix<T, R, C> result;\
        for (int i = 0; i < R; i++) \
            for (int j = 0; j < C; j++) \
                result.rows[i][j] = _slang_fmod(left.rows[i][j], right.rows[i][j]); \
        return result;\
    }

    SLANG_FLOAT_MATRIX_MOD(float)
    SLANG_FLOAT_MATRIX_MOD(double)
#undef SLANG_FLOAT_MATRIX_MOD
#undef SLANG_MATRIX_BINARY_OP
#undef SLANG_MATRIX_UNARY_OP
#undef SLANG_INT_MATRIX_OPS
#undef SLANG_FLOAT_MATRIX_OPS
#undef SLANG_MATRIX_INT_NEG_OP
#undef SLANG_FLOAT_MATRIX_MOD

// We can just map `NonUniformResourceIndex` type directly to the index type on CPU, as CPU does not require
// any special handling around such accesses.
typedef size_t NonUniformResourceIndex;

// ----------------------------- ResourceType -----------------------------------------

// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-object-structuredbuffer-getdimensions
// Missing  Load(_In_  int  Location, _Out_ uint Status);

template <typename T>
struct RWStructuredBuffer
{
    SLANG_FORCE_INLINE T& operator[](size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    const T& Load(size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }  
    void GetDimensions(uint32_t* outNumStructs, uint32_t* outStride) { *outNumStructs = uint32_t(count); *outStride = uint32_t(sizeof(T)); }
  
    T* data;
    size_t count;
};

template <typename T>
struct StructuredBuffer
{
    SLANG_FORCE_INLINE const T& operator[](size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    const T& Load(size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    void GetDimensions(uint32_t* outNumStructs, uint32_t* outStride) { *outNumStructs = uint32_t(count); *outStride = uint32_t(sizeof(T)); }
    
    T* data;
    size_t count;
};


template <typename T>
struct RWBuffer
{
    SLANG_FORCE_INLINE T& operator[](size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    const T& Load(size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    void GetDimensions(uint32_t* outCount) { *outCount = uint32_t(count); }
    
    T* data;
    size_t count;
};

template <typename T>
struct Buffer
{
    SLANG_FORCE_INLINE const T& operator[](size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    const T& Load(size_t index) const { SLANG_BOUND_CHECK(index, count); return data[index]; }
    void GetDimensions(uint32_t* outCount) { *outCount = uint32_t(count); }
    
    T* data;
    size_t count;
};

// Missing  Load(_In_  int  Location, _Out_ uint Status);
struct ByteAddressBuffer
{
    void GetDimensions(uint32_t* outDim) const { *outDim = uint32_t(sizeInBytes); }
    uint32_t Load(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 4, sizeInBytes);
        return data[index >> 2]; 
    }
    uint2 Load2(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 8, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint2{data[dataIdx], data[dataIdx + 1]}; 
    }
    uint3 Load3(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 12, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint3{data[dataIdx], data[dataIdx + 1], data[dataIdx + 2]}; 
    }
    uint4 Load4(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 16, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint4{data[dataIdx], data[dataIdx + 1], data[dataIdx + 2], data[dataIdx + 3]}; 
    }
    template<typename T>
    T Load(size_t index) const
    {
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, sizeof(T), sizeInBytes);
        return *(const T*)(((const char*)data) + index);
    }
    
    const uint32_t* data;
    size_t sizeInBytes;  //< Must be multiple of 4
};

// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/sm5-object-rwbyteaddressbuffer
// Missing support for Atomic operations 
// Missing support for Load with status
struct RWByteAddressBuffer
{
    void GetDimensions(uint32_t* outDim) const { *outDim = uint32_t(sizeInBytes); }
    
    uint32_t Load(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 4, sizeInBytes);
        return data[index >> 2]; 
    }
    uint2 Load2(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 8, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint2{data[dataIdx], data[dataIdx + 1]}; 
    }
    uint3 Load3(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 12, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint3{data[dataIdx], data[dataIdx + 1], data[dataIdx + 2]}; 
    }
    uint4 Load4(size_t index) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 16, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        return uint4{data[dataIdx], data[dataIdx + 1], data[dataIdx + 2], data[dataIdx + 3]}; 
    }
    template<typename T>
    T Load(size_t index) const
    {
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, sizeof(T), sizeInBytes);
        return *(const T*)(((const char*)data) + index);
    }

    void Store(size_t index, uint32_t v) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 4, sizeInBytes);
        data[index >> 2] = v; 
    }
    void Store2(size_t index, uint2 v) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 8, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        data[dataIdx + 0] = v.x;
        data[dataIdx + 1] = v.y;
    }
    void Store3(size_t index, uint3 v) const 
    {  
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 12, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        data[dataIdx + 0] = v.x;
        data[dataIdx + 1] = v.y;
        data[dataIdx + 2] = v.z;
    }
    void Store4(size_t index, uint4 v) const 
    { 
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, 16, sizeInBytes);
        const size_t dataIdx = index >> 2; 
        data[dataIdx + 0] = v.x;
        data[dataIdx + 1] = v.y;
        data[dataIdx + 2] = v.z;
        data[dataIdx + 3] = v.w;
    }
    template<typename T>
    void Store(size_t index, T const& value) const
    {
        SLANG_BOUND_CHECK_BYTE_ADDRESS(index, sizeof(T), sizeInBytes);
        *(T*)(((char*)data) + index) = value;
    }

    uint32_t* data;
    size_t sizeInBytes; //< Must be multiple of 4 
};

struct ISamplerState;
struct ISamplerComparisonState;

struct SamplerState
{
    ISamplerState* state;
};

struct SamplerComparisonState
{
    ISamplerComparisonState* state;
};

#ifndef SLANG_RESOURCE_SHAPE
#    define SLANG_RESOURCE_SHAPE
typedef unsigned int SlangResourceShape;
enum
{
    SLANG_RESOURCE_BASE_SHAPE_MASK = 0x0F,

    SLANG_RESOURCE_NONE = 0x00,

    SLANG_TEXTURE_1D = 0x01,
    SLANG_TEXTURE_2D = 0x02,
    SLANG_TEXTURE_3D = 0x03,
    SLANG_TEXTURE_CUBE = 0x04,
    SLANG_TEXTURE_BUFFER = 0x05,

    SLANG_STRUCTURED_BUFFER = 0x06,
    SLANG_BYTE_ADDRESS_BUFFER = 0x07,
    SLANG_RESOURCE_UNKNOWN = 0x08,
    SLANG_ACCELERATION_STRUCTURE = 0x09,

    SLANG_RESOURCE_EXT_SHAPE_MASK = 0xF0,

    SLANG_TEXTURE_FEEDBACK_FLAG = 0x10,
    SLANG_TEXTURE_ARRAY_FLAG = 0x40,
    SLANG_TEXTURE_MULTISAMPLE_FLAG = 0x80,

    SLANG_TEXTURE_1D_ARRAY = SLANG_TEXTURE_1D | SLANG_TEXTURE_ARRAY_FLAG,
    SLANG_TEXTURE_2D_ARRAY = SLANG_TEXTURE_2D | SLANG_TEXTURE_ARRAY_FLAG,
    SLANG_TEXTURE_CUBE_ARRAY = SLANG_TEXTURE_CUBE | SLANG_TEXTURE_ARRAY_FLAG,

    SLANG_TEXTURE_2D_MULTISAMPLE = SLANG_TEXTURE_2D | SLANG_TEXTURE_MULTISAMPLE_FLAG,
    SLANG_TEXTURE_2D_MULTISAMPLE_ARRAY =
        SLANG_TEXTURE_2D | SLANG_TEXTURE_MULTISAMPLE_FLAG | SLANG_TEXTURE_ARRAY_FLAG,
};
#endif

// 
struct TextureDimensions
{
    void reset()
    {
        shape = 0;
        width = height = depth = 0;
        numberOfLevels = 0;
        arrayElementCount = 0;
    }
    int getDimSizes(uint32_t outDims[4]) const
    {
        const auto baseShape = (shape & SLANG_RESOURCE_BASE_SHAPE_MASK);
        int count = 0;
        switch (baseShape)
        {
            case SLANG_TEXTURE_1D:
            {
                outDims[count++] = width;
                break;
            }
            case SLANG_TEXTURE_2D:
            {
                outDims[count++] = width;
                outDims[count++] = height;
                break;
            }
            case SLANG_TEXTURE_3D:
            {
                outDims[count++] = width;
                outDims[count++] = height;
                outDims[count++] = depth;
                break;
            }
            case SLANG_TEXTURE_CUBE:
            {
                outDims[count++] = width;
                outDims[count++] = height;
                outDims[count++] = 6;
                break;
            }
        }

        if (shape & SLANG_TEXTURE_ARRAY_FLAG)
        {
            outDims[count++] = arrayElementCount;
        }
        return count;
    }
    int getMIPDims(int outDims[3]) const
    {
        const auto baseShape = (shape & SLANG_RESOURCE_BASE_SHAPE_MASK);
        int count = 0;
        switch (baseShape)
        {
            case SLANG_TEXTURE_1D:
            {
                outDims[count++] = width;
                break;
            }
            case SLANG_TEXTURE_CUBE:
            case SLANG_TEXTURE_2D:
            {
                outDims[count++] = width;
                outDims[count++] = height;
                break;
            }
            case SLANG_TEXTURE_3D:
            {
                outDims[count++] = width;
                outDims[count++] = height;
                outDims[count++] = depth;
                break;
            }
        }
        return count;
    }
    int calcMaxMIPLevels() const
    {
        int dims[3];
        const int dimCount = getMIPDims(dims);
        for (int count = 1; true; count++)
        {
            bool allOne = true;
            for (int i = 0; i < dimCount; ++i)
            {
                if (dims[i] > 1)
                {
                    allOne = false;
                    dims[i] >>= 1;
                }
            }
            if (allOne)
            {
                return count;
            }
        }
    }

    uint32_t shape;
    uint32_t width, height, depth;
    uint32_t numberOfLevels;
    uint32_t arrayElementCount;                  ///< For array types, 0 otherwise
};





// Texture

struct ITexture
{
    virtual TextureDimensions GetDimensions(int mipLevel = -1) = 0;
    virtual void Load(const int32_t* v, void* outData, size_t dataSize) = 0;
    virtual void Sample(SamplerState samplerState, const float* loc, void* outData, size_t dataSize) = 0;
    virtual void SampleLevel(SamplerState samplerState, const float* loc, float level, void* outData, size_t dataSize) = 0;
};

template <typename T>
struct Texture1D
{
    void GetDimensions(uint32_t* outWidth) { *outWidth = texture->GetDimensions().width; }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outNumberOfLevels) 
    { 
        auto dims = texture->GetDimensions(mipLevel); 
        *outWidth = dims.width; 
        *outNumberOfLevels = dims.numberOfLevels; 
    }
    
    void GetDimensions(float* outWidth) { *outWidth = texture->GetDimensions().width; }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outNumberOfLevels) 
    { 
        auto dims = texture->GetDimensions(mipLevel); 
        *outWidth = dims.width; 
        *outNumberOfLevels = dims.numberOfLevels; 
    }
    
    T Load(const int2& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T Sample(SamplerState samplerState, float loc) const { T out; texture->Sample(samplerState, &loc, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, float loc, float level) { T out; texture->SampleLevel(samplerState, &loc, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct Texture2D
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int3& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T Sample(SamplerState samplerState, const float2& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float2& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct Texture3D
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outDepth)
    {
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
        *outDepth = dims.depth;
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outDepth, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outDepth = dims.depth;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight, float* outDepth)
    {
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
        *outDepth = dims.depth;
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outDepth, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outDepth = dims.depth;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int4& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T Sample(SamplerState samplerState, const float3& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float3& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct TextureCube
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Sample(SamplerState samplerState, const float3& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float3& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct Texture1DArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outElements) { auto dims = texture->GetDimensions(); *outWidth = dims.width; *outElements = dims.arrayElementCount; }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outElements, uint32_t* outNumberOfLevels) 
    {
        auto dims = texture->GetDimensions(mipLevel); 
        *outWidth = dims.width; 
        *outNumberOfLevels = dims.numberOfLevels;
        *outElements = dims.arrayElementCount; 
    }        
    void GetDimensions(float* outWidth, float* outElements) { auto dims = texture->GetDimensions(); *outWidth = dims.width; *outElements = dims.arrayElementCount; }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outElements, float* outNumberOfLevels) 
    {
        auto dims = texture->GetDimensions(mipLevel); 
        *outWidth = dims.width; 
        *outNumberOfLevels = dims.numberOfLevels;
        *outElements = dims.arrayElementCount; 
    }
    
    T Load(const int3& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T Sample(SamplerState samplerState, const float2& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float2& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct Texture2DArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements)
    {
        auto dims = texture->GetDimensions();
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements, uint32_t* outNumberOfLevels)
    {
        auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    void GetDimensions(uint32_t* outWidth, float* outHeight, float* outElements)
    {
        auto dims = texture->GetDimensions();
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outElements, float* outNumberOfLevels)
    {
        auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int4& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T Sample(SamplerState samplerState, const float3& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float3& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

template <typename T>
struct TextureCubeArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements)
    {
        auto dims = texture->GetDimensions();
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements, uint32_t* outNumberOfLevels)
    {
        auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    void GetDimensions(uint32_t* outWidth, float* outHeight, float* outElements)
    {
        auto dims = texture->GetDimensions();
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outElements, float* outNumberOfLevels)
    {
        auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Sample(SamplerState samplerState, const float4& loc) const { T out; texture->Sample(samplerState, &loc.x, &out, sizeof(out)); return out; }
    T SampleLevel(SamplerState samplerState, const float4& loc, float level) { T out; texture->SampleLevel(samplerState, &loc.x, level, &out, sizeof(out)); return out; }
    
    ITexture* texture;              
};

/* !!!!!!!!!!!!!!!!!!!!!!!!!!! RWTexture !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

struct IRWTexture : ITexture
{
        /// Get the reference to the element at loc. 
    virtual void* refAt(const uint32_t* loc) = 0;
};

template <typename T>
struct RWTexture1D
{
    void GetDimensions(uint32_t* outWidth) { *outWidth = texture->GetDimensions().width; }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outNumberOfLevels) { auto dims = texture->GetDimensions(mipLevel); *outWidth = dims.width; *outNumberOfLevels = dims.numberOfLevels; }
    
    void GetDimensions(float* outWidth) { *outWidth = texture->GetDimensions().width; }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outNumberOfLevels) { auto dims = texture->GetDimensions(mipLevel); *outWidth = dims.width; *outNumberOfLevels = dims.numberOfLevels; }
    
    T Load(int32_t loc) const { T out; texture->Load(&loc, &out, sizeof(out)); return out; }
    T& operator[](uint32_t loc) { return *(T*)texture->refAt(&loc); }
    IRWTexture* texture;              
};

template <typename T>
struct RWTexture2D
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int2& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T& operator[](const uint2& loc) { return *(T*)texture->refAt(&loc.x); }
    IRWTexture* texture;
};

template <typename T>
struct RWTexture3D
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outDepth)
    {
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
        *outDepth = dims.depth;
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outDepth, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outDepth = dims.depth;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight, float* outDepth)
    {
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
        *outDepth = dims.depth;
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outDepth, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outDepth = dims.depth;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int3& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T& operator[](const uint3& loc) { return *(T*)texture->refAt(&loc.x); }
    IRWTexture* texture;
};


template <typename T>
struct RWTexture1DArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outElements) 
    { 
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outElements, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outElements) 
    { 
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outElements, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(int2 loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T& operator[](uint2 loc) { return *(T*)texture->refAt(&loc.x); }

    IRWTexture* texture;
};

template <typename T>
struct RWTexture2DArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements)
    {
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight, float* outElements)
    {
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outElements, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    T Load(const int3& loc) const { T out; texture->Load(&loc.x, &out, sizeof(out)); return out; }
    T& operator[](const uint3& loc) { return *(T*)texture->refAt(&loc.x); }

    IRWTexture* texture;
};

// FeedbackTexture

struct FeedbackType {};
struct SAMPLER_FEEDBACK_MIN_MIP : FeedbackType {};
struct SAMPLER_FEEDBACK_MIP_REGION_USED : FeedbackType {};

struct IFeedbackTexture
{
    virtual TextureDimensions GetDimensions(int mipLevel = -1) = 0;

    // Note here we pass the optional clamp parameter as a pointer. Passing nullptr means no clamp. 
    // This was preferred over having two function definitions, and having to differentiate their names
    virtual void WriteSamplerFeedback(ITexture* tex, SamplerState samp, const float* location, const float* clamp = nullptr) = 0;
    virtual void WriteSamplerFeedbackBias(ITexture* tex, SamplerState samp, const float* location, float bias, const float* clamp = nullptr) = 0;
    virtual void WriteSamplerFeedbackGrad(ITexture* tex, SamplerState samp, const float* location, const float* ddx, const float* ddy, const float* clamp = nullptr) = 0;
    
    virtual void WriteSamplerFeedbackLevel(ITexture* tex, SamplerState samp, const float* location, float lod) = 0;
};

template <typename T>
struct FeedbackTexture2D
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight) 
    { 
        const auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    template <typename S>
    void WriteSamplerFeedback(Texture2D<S> tex, SamplerState samp, float2 location, float clamp) { texture->WriteSamplerFeedback(tex.texture, samp, &location.x, &clamp); } 

    template <typename S>
    void WriteSamplerFeedbackBias(Texture2D<S> tex, SamplerState samp, float2 location, float bias, float clamp) { texture->WriteSamplerFeedbackBias(tex.texture, samp, &location.x, bias, &clamp); }

    template <typename S>
    void WriteSamplerFeedbackGrad(Texture2D<S> tex, SamplerState samp, float2 location, float2 ddx, float2 ddy, float clamp) { texture->WriteSamplerFeedbackGrad(tex.texture, samp, &location.x, &ddx.x, &ddy.x, &clamp); }

    // Level

    template <typename S> 
    void WriteSamplerFeedbackLevel(Texture2D<S> tex, SamplerState samp, float2 location, float lod) { texture->WriteSamplerFeedbackLevel(tex.texture, samp, &location.x, lod); }
    
    // Without Clamp
    template <typename S> 
    void WriteSamplerFeedback(Texture2D<S> tex, SamplerState samp, float2 location) { texture->WriteSamplerFeedback(tex.texture, samp, &location.x); }

    template <typename S> 
    void WriteSamplerFeedbackBias(Texture2D<S> tex, SamplerState samp, float2 location, float bias) { texture->WriteSamplerFeedbackBias(tex.texture, samp, &location.x, bias); }

    template <typename S> 
    void WriteSamplerFeedbackGrad(Texture2D<S> tex, SamplerState samp, float2 location, float2 ddx, float2 ddy) { texture->WriteSamplerFeedbackGrad(tex.texture, samp, &location.x, &ddx.x, &ddy.x); }
    
    IFeedbackTexture* texture;
};

template <typename T>
struct FeedbackTexture2DArray
{
    void GetDimensions(uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements)
    {
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, uint32_t* outWidth, uint32_t* outHeight, uint32_t* outElements, uint32_t* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    void GetDimensions(float* outWidth, float* outHeight, float* outElements)
    {
        auto dims = texture->GetDimensions(); 
        *outWidth = dims.width; 
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount; 
    }
    void GetDimensions(uint32_t mipLevel, float* outWidth, float* outHeight, float* outElements, float* outNumberOfLevels)
    {
        const auto dims = texture->GetDimensions(mipLevel);
        *outWidth = dims.width;
        *outHeight = dims.height;
        *outElements = dims.arrayElementCount;
        *outNumberOfLevels = dims.numberOfLevels;
    }
    
    template <typename S>
    void WriteSamplerFeedback(Texture2DArray<S> texArray, SamplerState samp, float3 location, float clamp) { texture->WriteSamplerFeedback(texArray.texture, samp, &location.x, &clamp); }

    template <typename S>
    void WriteSamplerFeedbackBias(Texture2DArray<S> texArray, SamplerState samp, float3 location, float bias, float clamp) { texture->WriteSamplerFeedbackBias(texArray.texture, samp, &location.x, bias, &clamp); }

    template <typename S>
    void WriteSamplerFeedbackGrad(Texture2DArray<S> texArray, SamplerState samp, float3 location, float3 ddx, float3 ddy, float clamp) { texture->WriteSamplerFeedbackGrad(texArray.texture, samp, &location.x, &ddx.x, &ddy.x, &clamp); }

    // Level
    template <typename S>
    void WriteSamplerFeedbackLevel(Texture2DArray<S> texArray, SamplerState samp, float3 location, float lod) { texture->WriteSamplerFeedbackLevel(texArray.texture, samp, &location.x, lod); }

    // Without Clamp

    template <typename S>
    void WriteSamplerFeedback(Texture2DArray<S> texArray, SamplerState samp, float3 location) { texture->WriteSamplerFeedback(texArray.texture, samp, &location.x); }

    template <typename S>
    void WriteSamplerFeedbackBias(Texture2DArray<S> texArray, SamplerState samp, float3 location, float bias) { texture->WriteSamplerFeedbackBias(texArray.texture, samp, &location.x, bias); }

    template <typename S>
    void WriteSamplerFeedbackGrad(Texture2DArray<S> texArray, SamplerState samp, float3 location, float3 ddx, float3 ddy) { texture->WriteSamplerFeedbackGrad(texArray.texture, samp, &location.x, &ddx.x, &ddy.x); }
    
    IFeedbackTexture* texture;
};

/* Varying input for Compute */

/* Used when running a single thread */
struct ComputeThreadVaryingInput
{
    uint3 groupID;
    uint3 groupThreadID;
};

struct ComputeVaryingInput
{
    uint3 startGroupID;     ///< start groupID
    uint3 endGroupID;       ///< Non inclusive end groupID
};

// The uniformEntryPointParams and uniformState must be set to structures that match layout that the kernel expects.
// This can be determined via reflection for example.

typedef void(*ComputeThreadFunc)(ComputeThreadVaryingInput* varyingInput, void* uniformEntryPointParams, void* uniformState);
typedef void(*ComputeFunc)(ComputeVaryingInput* varyingInput, void* uniformEntryPointParams, void* uniformState);

template<typename TResult, typename TInput>
TResult slang_bit_cast(TInput val)
{
    return *(TResult*)(&val);
}

#ifdef SLANG_PRELUDE_NAMESPACE
}
#endif

#endif


