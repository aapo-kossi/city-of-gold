#pragma once

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#ifdef _WIN32
    #ifdef BINDINGS_LIBRARY_EXPORTS
        #define BINDINGS_API __declspec(dllexport)
    #else
        #define BINDINGS_API __declspec(dllimport)
    #endif
#else
    #define BINDINGS_API __attribute__((visibility("default")))
#endif

namespace py = pybind11;

void bind_single_env(py::module_ &m);

template <typename Class, typename T, size_t N>
void bind_array(py::class_<Class> &pyclass, const char *name,
                std::array<T, N> Class::*member) {
  pyclass.def_property(
      name,
      [member](Class &self) {
        return py::array_t<T>({N}, (self.*member).data(), py::cast(&self));
      },
      [member](Class &self, py::array_t<T> array) {
        if (array.size() != N) {
          throw std::runtime_error("Invalid array size");
        }
        std::copy(array.data(), array.data() + N, (self.*member).begin());
      });
}

template <typename Class, typename T, size_t Rows, size_t Cols>
void bind_array2(py::class_<Class> &pyclass, const char *name,
                 std::array<std::array<T, Cols>, Rows> Class::*member) {
  pyclass.def_property(
      name,
      [member](Class &self) {
        // Create a 2D NumPy array with a shared pointer to the C++ array
        return py::array_t<T>(
            {Rows, Cols},                  // Shape of the array
            {Cols * sizeof(T), sizeof(T)}, // Strides (row-major layout)
            (self.*member).front().data(), // Pointer to the first element
            py::cast(&self)                // Owner reference
        );
      },
      [member](Class &self, py::array_t<T> array) {
        if (array.ndim() != 2 || array.shape(0) != Rows ||
            array.shape(1) != Cols) {
          throw std::runtime_error("Invalid array shape");
        }
        // Copy data from the NumPy array to the C++ array
        for (size_t i = 0; i < Rows; ++i) {
          std::copy(array.data(i), array.data(i) + Cols,
                    (self.*member)[i].begin());
        }
      });
}

template <typename Class, typename T, size_t Rows, size_t Cols, size_t Depth>
void bind_array3(
    py::class_<Class> &pyclass, const char *name,
    std::array<std::array<std::array<T, Depth>, Cols>, Rows> Class::*member) {
  pyclass.def_property(
      name,
      [member](Class &self) {
        // Create a 3D NumPy array with a shared pointer to the C++ array
        return py::array_t<T>({Rows, Cols, Depth},       // Shape of the array
                              {Cols * Depth * sizeof(T), // Stride for rows
                               Depth * sizeof(T),        // Stride for columns
                               sizeof(T)},               // Stride for depth
                              (self.*member)
                                  .front()
                                  .front()
                                  .data(),    // Pointer to the first element
                              py::cast(&self) // Owner reference
        );
      },
      [member](Class &self, py::array_t<T> array) {
        if (array.ndim() != 3 || array.shape(0) != Rows ||
            array.shape(1) != Cols || array.shape(2) != Depth) {
          throw std::runtime_error("Invalid array shape");
        }
        // Copy data from the NumPy array to the C++ array
        for (size_t i = 0; i < Rows; ++i) {
          for (size_t j = 0; j < Cols; ++j) {
            std::copy(array.data(i, j), array.data(i, j) + Depth,
                      (self.*member)[i][j].begin());
          }
        }
      });
}

// Generic template function to create NumPy views
template <typename T, size_t N>
py::array_t<T> create_numpy_view(T *data,
                                 const std::array<ptrdiff_t, N> &strides) {
  return py::array_t<T>(strides, data, py::capsule(data, [](void *p) {}));
}

template <typename T, size_t N>
py::array_t<T> create_numpy_view(const T *data,
                                 const std::array<ptrdiff_t, N> &strides) {
  return py::array_t<T>(strides, data, py::capsule(data, [](void *p) {}));
}

inline auto numpy_to_std_array(const py::array &arr, auto &std_array) {
  if (arr.size() != std_array.size()) {
    throw std::runtime_error("Array size mismatch.");
  }
  std::memcpy(std_array.data(), arr.data(), arr.size() * sizeof(std_array[0]));
};

template <typename T>
py::array_t<T> vec_to_numpy(const std::vector<T> &data, size_t N) {
  // Ensure the vector size matches the provided dimensions
  if (data.size() != N) {
    throw std::runtime_error("Data size does not match provided dimensions.");
  }

  // Create a NumPy array with shared memory
  return py::array_t<T>(
      {N},         // Shape
      {sizeof(T)}, // Stride (number of bytes per column)
      data.data(), // Pointer to the underlying data
      py::capsule(data.data(),
                  [](void *) {}) // Capsule to manage lifetime (no copy)
  );
}

template <typename T>
py::array_t<T> vec_to_numpy2(const std::vector<T> &data, size_t N, size_t M) {
  // Ensure the vector size matches the provided dimensions
  if (data.size() != N * M) {
    throw std::runtime_error("Data size does not match provided dimensions.");
  }

  // Create a NumPy array with shared memory
  return py::array_t<T>(
      {N, M},         // Shape
      {M * sizeof(T), // Row stride (number of bytes per row)
       sizeof(T)},    // Column stride (number of bytes per column)
      data.data(),    // Pointer to the underlying data
      py::capsule(data.data(),
                  [](void *) {}) // Capsule to manage lifetime (no copy)
  );
}
