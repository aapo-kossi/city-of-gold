#pragma once

#include <csignal>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "api.h"
#include "constants.h"
#include "geometry.h"

struct generate_map_failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

typedef struct {
  Requirement requirement;
  u_char n_required;
  bool is_end;
} MovementInfo;

typedef struct ConnectionInfo {
  std::vector<point> coords;
  std::vector<std::vector<int>> rotations;

} ConnectionInfo;

enum Color {
  RESET,
  BLACK,
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGENTA,
  CYAN,
  WHITE,
  DEFAULT,

  GRAY,

  BYELLOW,

  RED_BG,
  GREEN_BG,
  YELLOW_BG,
  BLUE_BG,
  MAGENTA_BG,
  CYAN_BG,
  WHITE_BG,
  MAX_COLOR
};

std::string colored(std::string to_color, std::string color);

const std::vector<std::string> COLORCODES = {
    "\x1b[0m",         "\x1b[30m",     "\x1b[31m",     "\x1b[32m",
    "\x1b[33m",        "\x1b[34m",     "\x1b[35m",     "\x1b[36m",
    "\x1b[37m",        "\x1b[39m",

    "\x1b[2m\x1b[37m",

    "\x1b[33;1m",

    "\x1b[101;30m",    "\x1b[102;30m", "\x1b[103;30m", "\x1b[104;30m",
    "\x1b[105;30m",    "\x1b[106;30m", "\x1b[107;30m"};

const std::vector<std::string> player_colors_ = {
    COLORCODES[Color::RED_BG], COLORCODES[Color::GREEN_BG],
    COLORCODES[Color::YELLOW_BG], COLORCODES[Color::BLUE_BG]};

const std::vector<std::string> REQUIREMENT_STRINGS = {
    COLORCODES[Color::GREEN] + "m" + COLORCODES[Color::RESET],
    COLORCODES[Color::BLUE] + "p" + COLORCODES[Color::RESET],
    COLORCODES[Color::YELLOW] + "c" + COLORCODES[Color::RESET],
    COLORCODES[Color::GRAY] + "u" + COLORCODES[Color::RESET],
    COLORCODES[Color::RED] + "d" + COLORCODES[Color::RESET],
};

enum PieceType { START, TRAVEL, ENDING, MAX_PIECETYPE };

enum PieceSize { LARGE, SMALL, TRIPLE_CURVED, MAX_PIECESIZE };

template <typename T>
std::vector<T> &operator+=(std::vector<T> &x, const std::vector<T> &y) {
  x.reserve(x.size() + y.size());
  x.insert(x.end(), y.begin(), y.end());
  return x;
}

class Hex {
public:
  constexpr Hex(Requirement requirement = Requirement::NULL_REQUIREMENT,
                u_char n_required = 0, bool is_end = 0,
                u_char player_start = 0); // constructor

  const Requirement requirement;
  const u_char n_required;
  const bool is_end;
  const u_char player_start;

  constexpr bool is_passable() const;
};

std::string hex_to_string(const Hex *hex, u_char occupier);

class MapPiece {
public:
  MapPiece(std::vector<const Hex *> hexes, std::vector<point> hex_coords,
           Difficulty difficulty, PieceType type,
           PieceSize size); // constructor

  const point &get_center() const;
  const std::vector<const Hex *> &get_hexes() const;
  const std::vector<std::vector<Hex *>> &get_hex_array() const;
  const int &get_rotation() const;
  const std::vector<point> &get_xy() const;
  const PieceType &get_type() const;
  const PieceSize &get_size() const;
  const Difficulty &get_difficulty() const;

  void translate(point delta);
  void rotate(int times);
  void reset();
  ConnectionInfo get_centered_connections(const MapPiece &other) const;

private:
  point center;
  std::vector<const Hex *> hexes;
  int rotation;
  std::vector<point> xy;
  PieceType type;
  PieceSize size;
  Difficulty difficulty;
  std::vector<std::vector<ConnectionInfo>> ref_connections;
  ConnectionInfo get_ref_connection_points(u_char new_size,
                                           u_char new_type) const;
};

class Map {
public:
  Map();
  void init(std::array<std::array<std::array<u_char, N_MAP_FEATURES>, GRIDSIZE>,
                       GRIDSIZE> &data_buffer);

  bool player_done(u_char id) const;
  const std::vector<MapPiece *> &get_pieces() const;
  const std::array<std::array<std::array<u_char, N_MAP_FEATURES>, GRIDSIZE>,
                   GRIDSIZE> &
  observation(u_char player_id);
  std::string draw();
  void set_movement_mask(ActionMask &mask, u_char player_id,
                         std::array<float, N_RESOURCETYPES> resources,
                         u_char n_active) const;
  MovementInfo move_to_point(u_char player_id, point point);
  void add_players(u_char player_count);
  bool add_random_piece(MapPiece *new_piece, std::default_random_engine &rng);
  void add_piece(MapPiece *new_piece, point new_point, int rotation);
  void add_players(int n_players);
  MovementInfo move_in_direction(u_char player, u_char direction_idx);
  void generate(u_char n_pieces, Difficulty difficulty, int failures,
                int max_failures, std::default_random_engine rng);
  void reset();
  const std::vector<const Hex *> &get_hexes() const;
  const std::vector<point> &get_xy() const;
  const std::vector<point> &get_player_locations() const {
    return player_locations;
  };

private:
  const Hex *get_from_array(const point &) const;
  std::array<std::array<std::array<u_char, N_MAP_FEATURES>, GRIDSIZE>, GRIDSIZE>
      *obs_array;
  std::vector<MapPiece *> pieces;
  std::vector<point> xy;
  point min_xy;
  point max_xy;
  std::vector<const Hex *> hexes;
  std::vector<std::vector<const Hex *>> hex_array;
  std::vector<std::array<size_t, 2>> hex_index;
  std::vector<point> player_locations;
  u_char n_players;

  MovementInfo _move(u_char player, point target);
  bool obs_initialized = false;
  void finalize();
};
// Declarations for functions
Hex *get_n_copies(Hex hex, u_char n);
Hex *get_overlap(Hex hex, u_char n);
