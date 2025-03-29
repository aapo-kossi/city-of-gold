#pragma once

#include "constants.h"
#include <array>
#include <cmath>

typedef struct {
  float u;
  float v;
  float w;
} cubepoint;

typedef struct point {
  float x;
  float y;
  point() = default;
  constexpr point(float x_, float y_) {
    x = x_;
    y = y_;
  };
  template <typename T> constexpr point(std::array<T, 2> arr) {
    x = arr[0];
    y = arr[1];
  };
  inline constexpr bool operator==(const point &) const = default;
  inline constexpr bool operator!=(const point &) const = default;
  inline constexpr bool operator<(const point &other) const {
    return (x < other.x) || (x == other.x && y < other.y);
  };
  inline constexpr point operator+(const point &other) const {
    return {x + other.x, y + other.y};
  };
  inline constexpr point operator-(const point &other) const {
    return {x - other.x, y - other.y};
  };
  inline constexpr point operator-() const { return {-x, -y}; };
} point;

enum Direction {
  NONE = 0,
  EAST,
  NORTHEAST,
  NORTHWEST,
  WEST,
  SOUTHWEST,
  SOUTHEAST
};

constexpr std::array<point, 7> DIRECTIONS = {{
    {0, 0},
    {1, 0},
    {0, 1},
    {-1, 1},
    {-1, 0},
    {0, -1},
    {1, -1},
}};
constexpr u_char N_DIRECTIONS = DIRECTIONS.size();

inline constexpr point cube_to_xy(const cubepoint &uvw) {
  point ret{};
  ret.x = -4.0f / 3.0f * (uvw.v + 0.5f * uvw.u);
  ret.y = 4.0f / 3.0f * (uvw.u + 0.5f * uvw.v);
  return ret;
}

inline constexpr cubepoint xy_to_cube(const point &xy) {
  float halfx = xy.x / 2;
  float halfy = xy.y / 2;
  float u = halfx + xy.y;
  float v = -xy.x - halfy;
  float w = halfx - halfy;
  return {u, v, w};
}

inline constexpr cubepoint cube_rotate(cubepoint uvw, int times) {
  float u = -uvw.u;
  float v = -uvw.v;
  float w = -uvw.w;
  if (times == 1) {
    return cubepoint{v, w, u};
  } else if (times == -1) {
    return cubepoint{w, u, v};
  } else {
    bool clockwise = std::signbit(times);
    int single = 1 - 2 * clockwise;
    return cube_rotate(cube_rotate(uvw, single), times - single);
  }
}

inline constexpr point point_rotate(point xy, int times) {
  times = times % 6;
  cubepoint uvw = xy_to_cube(xy);
  uvw = cube_rotate(uvw, times);
  return cube_to_xy(uvw);
}

