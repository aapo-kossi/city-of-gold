#include "api.h"
#include "constants.h"
#include "geometry.h"
#include "map.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <string>

std::vector<const Hex *> get_n_copies(const Hex *hex, size_t n) {
  std::vector<const Hex *> ret(n);
  fill(ret.begin(), ret.end(), hex);
  return ret;
};

inline std::vector<point> rotate(std::vector<point> xy, int times) {
  times = times % 6;
  std::transform(xy.begin(), xy.end(), xy.begin(),
                 [times](point x) { return point_rotate(x, times); });
  return xy;
}

inline std::vector<point> translate(std::vector<point> xy, point delta) {
  std::transform(xy.begin(), xy.end(), xy.begin(), [delta](point a) {
    return point{a.x + delta.x, a.y + delta.y};
  });
  return xy;
}

bool overlap(std::vector<point> p1, std::vector<point> p2) {
  std::sort(p1.begin(), p1.end());
  std::sort(p2.begin(), p2.end());
  bool found = false;
  bool done = p1.empty() || p2.empty();
  size_t i = 0;
  size_t j = 0;
  while (!done) {
    point a = p1[i];
    point b = p2[j];
    found = a == b;
    done = found;
    if (a < b) {
      i++;
      done |= i >= p1.size();
    } else {
      j++;
      done |= j >= p2.size();
    }
  }
  return found;
}

std::string colored(std::string to_color, std::string color) {
  return color + to_color + COLORCODES[Color::RESET];
}

constexpr Hex::Hex(Requirement requirement_, u_char n_required_, bool is_end_,
                   u_char player_start_)
    : requirement{requirement_}, n_required{n_required_}, is_end{is_end_},
      player_start{player_start_} {}

constexpr bool Hex::is_passable() const {
  return requirement != Requirement::NULL_REQUIREMENT;
}

std::string hex_to_string(const Hex *hex, u_char occupier) {
  char player_char = ' ';
  if (occupier) {
    player_char = static_cast<char>(occupier) + '0';
  }
  std::string player_string =
      colored(std::string() + player_char, player_colors_[occupier]);
  std::string s;
  if (hex->player_start) {
    s = std::string() + 'S' + static_cast<char>(hex->player_start + '0');
  } else {
    s = "";
    Color info_color = Color::RESET;
    if (hex->is_end) {
      info_color = Color::YELLOW_BG;
    }
    s += colored(std::to_string(hex->n_required) +
                     REQUIREMENT_STRINGS[static_cast<u_char>(hex->requirement)],
                 COLORCODES[info_color]);
  }
  s += player_string;
  return s;
}

constexpr Hex mountain = Hex();

constexpr std::array<Hex, 4> start_hexes = {
    Hex(Requirement::NULL_REQUIREMENT, 0, 0, 1),
    Hex(Requirement::NULL_REQUIREMENT, 0, 0, 2),
    Hex(Requirement::NULL_REQUIREMENT, 0, 0, 3),
    Hex(Requirement::NULL_REQUIREMENT, 0, 0, 4)};

constexpr std::array<Hex, 2> end_hexes = {Hex(Requirement::PADDLE, 1, 1),
                                          Hex(Requirement::MACHETE, 1, 1)};

constexpr std::array<Hex, 5> jungle = {
    Hex(Requirement::MACHETE, 1), Hex(Requirement::MACHETE, 2),
    Hex(Requirement::MACHETE, 3), Hex(Requirement::MACHETE, 4),
    Hex(Requirement::MACHETE, 5),
};

constexpr std::array<Hex, 5> water = {
    Hex(Requirement::PADDLE, 1), Hex(Requirement::PADDLE, 2),
    Hex(Requirement::PADDLE, 3), Hex(Requirement::PADDLE, 4),
    Hex(Requirement::PADDLE, 5),
};

constexpr std::array<Hex, 5> desert = {
    Hex(Requirement::COIN, 1), Hex(Requirement::COIN, 2),
    Hex(Requirement::COIN, 3), Hex(Requirement::COIN, 4),
    Hex(Requirement::COIN, 5),
};

constexpr std::array<Hex, 5> rubble = {
    Hex(Requirement::DISCARD, 1), Hex(Requirement::DISCARD, 2),
    Hex(Requirement::DISCARD, 3), Hex(Requirement::DISCARD, 4),
    Hex(Requirement::DISCARD, 5),
};

constexpr std::array<Hex, 5> basecamp = {
    Hex(Requirement::REMOVE, 1), Hex(Requirement::REMOVE, 2),
    Hex(Requirement::REMOVE, 3), Hex(Requirement::REMOVE, 4),
    Hex(Requirement::REMOVE, 5),
};

MapPiece::MapPiece(std::vector<const Hex *> hexes_,
                   std::vector<point> hex_coords, Difficulty difficulty_,
                   PieceType type_, PieceSize size_)
    : center{0, 0}, hexes{hexes_}, rotation{0}, xy{hex_coords}, type{type_},
      size{size_}, difficulty{difficulty_} {
  ref_connections.resize(PieceType::MAX_PIECETYPE);
  for (u_char i = 0; i < PieceType::MAX_PIECETYPE; i++) {
    ref_connections[i].resize(PieceSize::MAX_PIECESIZE);
    for (u_char j = 0; j < PieceSize::MAX_PIECESIZE; j++) {
      ref_connections[i][j] = get_ref_connection_points(j, i);
    }
  }
}

const std::vector<const Hex *> &MapPiece::get_hexes() const { return hexes; };
const std::vector<point> &MapPiece::get_xy() const { return xy; };
const Difficulty &MapPiece::get_difficulty() const { return difficulty; };
const PieceType &MapPiece::get_type() const { return type; };
const PieceSize &MapPiece::get_size() const { return size; };

void MapPiece::reset() {
  translate(-center);
  rotate(-rotation);
};

void MapPiece::rotate(int times) {
  times = times % 6;
  std::transform(xy.begin(), xy.end(), xy.begin(),
                 [times](point a) { return point_rotate(a, times); });
  rotation = rotation + times;
}

void MapPiece::translate(point delta) {
  std::transform(xy.begin(), xy.end(), xy.begin(), [delta](point a) {
    return point{a.x + delta.x, a.y + delta.y};
  });
  center = center + delta;
}
ConnectionInfo MapPiece::get_centered_connections(const MapPiece &other) const {
  ConnectionInfo connections = ref_connections[other.type][other.size];
  for (auto &rotation_options : connections.rotations) {
    for (auto &rot : rotation_options) {
      rot += rotation;
    }
  }
  return {::translate(::rotate(connections.coords, rotation), center),
          connections.rotations};
};

ConnectionInfo MapPiece::get_ref_connection_points(u_char new_size,
                                                   u_char /*new_type*/) const {

  std::vector<std::vector<int>> rotations;
  std::vector<point> coords;
  bool can_rotate = false;
  if (size == PieceSize::LARGE) {
    if (new_size == PieceSize::LARGE) {
      can_rotate = true;
      rotations.push_back({-2, -1, 0, 1, 2, 3});
      rotations.push_back({-2, -1, 0, 1, 2, 3});
      coords = {{4, 3}, {3, 4}};
    } else if (new_size == PieceSize::SMALL) {
      can_rotate = true;
      rotations.push_back({-1, 2});
      rotations.push_back({-1, 2});
      rotations.push_back({-1, 2});
      coords = {
          {1.5, 3.5},
          {2.5, 2.5},
          {3.5, 1.5},
      };
    } else if ((new_size == PieceSize::TRIPLE_CURVED) &&
               (type != PieceType::START)) {
      can_rotate = true;
      rotations.push_back({-3});
      coords = {{0, 4}};
    } else {
      // invalid combination of pieces
      rotations = {};
      coords = {};
    }
    // Start piece only has the single connection direction
    if (type == PieceType::START) {
      can_rotate = false;
    }
  } else if ((size == PieceSize::SMALL) && (new_size == PieceSize::LARGE)) {
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    rotations.push_back({-2, -1, 0, 1, 2, 3});
    coords = {{-3.5, 5}, {-2.5, 5}, {-1.5, 5}, {3.5, -5}, {2.5, -5}, {1.5, -5}};
  } else {
    coords = {};
    rotations = {};
  }
  size_t n_coord_alts = coords.size();
  if (can_rotate) {
    for (size_t i = 0; i < 6; i++) {
      for (size_t j = 0; j < n_coord_alts; j++) {
        std::vector<int> this_rotations(rotations[i * n_coord_alts + j].size());
        std::transform(rotations[i * n_coord_alts + j].begin(),
                       rotations[i * n_coord_alts + j].end(),
                       this_rotations.begin(), [](int d) { return d + 1; });
        coords.push_back(point_rotate(coords[i * n_coord_alts + j], 1));
        rotations.push_back(this_rotations);
      }
    }
  }

  return {coords, rotations};
};

Map::Map() : obs_array(nullptr), pieces{}, min_xy{0, 0}, max_xy{0, 0} {};

void Map::init(
    std::array<std::array<std::array<u_char, N_MAP_FEATURES>, GRIDSIZE>,
               GRIDSIZE> &data_buffer) {
  obs_array = &data_buffer;
}

const Hex *Map::get_from_array(const point &p) const {
  return hex_array[static_cast<size_t>(p.x) + 1][static_cast<size_t>(p.y) + 1];
};

bool Map::add_random_piece(MapPiece *new_piece,
                           std::default_random_engine &rng) {
  new_piece->reset();
  ConnectionInfo candidates;
  ConnectionInfo valid_connections;
  for (auto piece : pieces) {
    ConnectionInfo candidate_locs = piece->get_centered_connections(*new_piece);
    candidates.coords += candidate_locs.coords;
    candidates.rotations += candidate_locs.rotations;
  }
  for (size_t i = 0; i < candidates.coords.size(); i++) {
    std::vector<point> footprint =
        rotate(new_piece->get_xy(), candidates.rotations[i][0]);
    footprint = translate(footprint, candidates.coords[i]);
    if (!overlap(footprint, xy)) {
      valid_connections.coords.push_back(candidates.coords[i]);
      valid_connections.rotations.push_back(candidates.rotations[i]);
    }
  }
  size_t loc_opts = valid_connections.coords.size();
  if (loc_opts) {
    size_t idx = std::uniform_int_distribution<size_t>(0, loc_opts - 1)(rng);
    point c = valid_connections.coords[idx];
    size_t rot_idx = std::uniform_int_distribution<size_t>(
        0, valid_connections.rotations[idx].size() - 1)(rng);
    int rot = valid_connections.rotations[idx][rot_idx];
    add_piece(new_piece, c, rot);
    return true;
  }
  return false;
};

void Map::add_piece(MapPiece *new_piece, point new_point, int rotation) {
  new_piece->rotate(rotation);
  new_piece->translate(new_point);
  pieces.push_back(new_piece);

  hexes += new_piece->get_hexes();
  xy += new_piece->get_xy();

  std::array<float, 4> xy_bounds = std::accumulate(
      new_piece->get_xy().cbegin(), new_piece->get_xy().cend(),
      std::array<float, 4>{max_xy.x, max_xy.y, min_xy.x, min_xy.y},
      [](std::array<float, 4> b, point a) -> std::array<float, 4> {
        return {a.x > b[0] ? a.x : b[0], a.y > b[1] ? a.y : b[1],
                a.x < b[2] ? a.x : b[2], a.y < b[3] ? a.y : b[3]};
      });
  // dimx and dimy include a buffer ring of empty hexes around the map
  // To guard from invalid addresses during movement masking
  size_t dimx = static_cast<size_t>(3 + xy_bounds[0] - xy_bounds[2]);
  size_t dimy = static_cast<size_t>(3 + xy_bounds[1] - xy_bounds[3]);
  max_xy = {xy_bounds[0], xy_bounds[1]};
  min_xy = {xy_bounds[2], xy_bounds[3]};

  hex_array.clear();
  hex_array.resize(dimx, std::vector<const Hex *>(dimy, &mountain));
  hex_index.clear();
  hex_index.resize(xy.size());
  for (size_t i = 0; i < xy.size(); i++) {
    size_t idx_x = static_cast<size_t>(xy[i].x - min_xy.x + 1);
    size_t idx_y = static_cast<size_t>(xy[i].y - min_xy.y + 1);
    hex_array[idx_x][idx_y] = hexes[i];
    hex_index[i] = {idx_x, idx_y};
  }
};

void Map::add_players(u_char n_players_) {
  n_players = n_players_;
  MapPiece &start_piece = *pieces[0];
  player_locations.resize(n_players);
  for (u_char i = 0; i < start_piece.get_hexes().size(); i++) {
    const Hex *hex = start_piece.get_hexes()[i];
    u_char start = hex->player_start;
    if ((start > 0) && (start < n_players + 1)) {
      player_locations[i] = xy[i];
    }
  }
};

MovementInfo Map::move_in_direction(u_char player, u_char direction_idx) {
  point current_location = player_locations[player];
  current_location = current_location + DIRECTIONS[direction_idx];
  return _move(player, current_location);
}
MovementInfo Map::_move(u_char player, point target) {
  point idx = target - min_xy;
  const Hex *hex = get_from_array(idx);

  player_locations[player] = target;
  return {hex->requirement, hex->n_required, hex->is_end};
}

void Map::set_movement_mask(ActionMask &mask, u_char player,
                            std::array<float, N_RESOURCETYPES> resources,
                            u_char n_active) const {
  point original_loc = player_locations[player];

  for (u_char i = 1; i < N_DIRECTIONS; i++) {
    point target_loc = original_loc + DIRECTIONS[i];
    point idx = target_loc - min_xy;
    const Hex *hex = get_from_array(idx);
    bool req_filled;
    u_char req_idx = static_cast<u_char>(hex->requirement);
    if (req_idx >= static_cast<u_char>(Requirement::DISCARD)) {
      // Check if able to discard or remove enough cards
      req_filled = n_active > hex->n_required;
    } else {
      // Check if resources sufficient
      req_filled = resources[req_idx] >= hex->n_required;
    }

    // disallow points already occupied by a player
    for (point other_loc : player_locations) {
      req_filled &= (other_loc != target_loc);
    }
    mask.move[i] =
        (hex->requirement != Requirement::NULL_REQUIREMENT) && req_filled;
  };
};

void Map::finalize() {
  for (auto &column : *obs_array) {
    for (auto &cell : column) {
      cell.fill(0);
    }
  }
  for (size_t i = 0; i < hexes.size(); i++) {
    auto hex = hexes[i];
    auto idx = hex_index[i];
    std::array<u_char, N_MAP_FEATURES> features = {};
    if (hex->requirement != Requirement::NULL_REQUIREMENT) {
      features[static_cast<u_char>(hex->requirement) + 1] = hex->n_required;
    }
    features[N_MAP_FEATURES - 1] = hex->is_end;
    (*obs_array)[idx[0]][idx[1]] = features;
  }
};

bool Map::player_done(u_char id) const {
  const point &loc = player_locations[id];
  return get_from_array(loc - min_xy)->is_end;
};
const std::vector<MapPiece *> &Map::get_pieces() const { return pieces; };

std::string Map::draw() { return "Drawing the map is a work in progress"; };

/*constexpr std::array<float, 37> largepiece_x = {*/
/*              0, 1, 2, 3*/
/*          -1, 0, 1, 2, 3*/
/*       -2,-1, 0, 1, 2, 3*/
/*       -2,-1, 0, 1, 2, 3*/
/*    -3,-2,-1, 0, 1, 2,*/
/*    -3,-2,-1, 0, 1,*/
/*    -3,-2,-1, 0,*/
/*};*/
/*constexpr std::array<float, 37> largepiece_y = {*/
/*             -3,-3,-3,-3,*/
/*          -2,-2,-2,-2,-2,*/
/*       -1,-1,-1,-1,-1,-1,*/
/*     0, 0, 0, 0, 0, 0, 0,*/
/*     1, 1, 1, 1, 1, 1,*/
/*     2, 2, 2, 2, 2,*/
/*     3, 3, 3, 3*/
/*};*/
/**/
/*constexpr std::array<float, 16> smallpiece_x = {*/
/*         -1.5,-0.5, 0.5, 1.5, 2.5,*/
/*    -2.5,-1.5,-0.5, 0.5, 1.5, 2.5,*/
/*    -2.5,-1.5,-0.5, 0.5, 1.5*/
/*};*/
/**/
/*constexpr std::array<float, 16> smallpiece_y = {*/
/*      -1,-1,-1,-1,-1,*/
/*    0, 0, 0, 0, 0, 0,*/
/*    1, 1, 1, 1, 1*/
/*};*/

std::vector<point> largepiece_coords = {
    point{0, -3},  point{1, -3}, point{2, -3}, point{3, -3}, point{-1, -2},
    point{0, -2},  point{1, -2}, point{2, -2}, point{3, -2}, point{-2, -1},
    point{-1, -1}, point{0, -1}, point{1, -1}, point{2, -1}, point{3, -1},
    point{-3, 0},  point{-2, 0}, point{-1, 0}, point{0, 0},  point{1, 0},
    point{2, 0},   point{3, 0},  point{-3, 1}, point{-2, 1}, point{-1, 1},
    point{0, 1},   point{1, 1},  point{2, 1},  point{-3, 2}, point{-2, 2},
    point{-1, 2},  point{0, 2},  point{1, 2},  point{-3, 3}, point{-2, 3},
    point{-1, 3},  point{0, 3}};

std::vector<point> smallpiece_coords = {
    point{-1.5, -1}, point{-0.5, -1}, point{0.5, -1}, point{1.5, -1},
    point{2.5, -1},  point{-2.5, 0},  point{-1.5, 0}, point{-0.5, 0},
    point{0.5, 0},   point{1.5, 0},   point{2.5, 0},  point{-2.5, 1},
    point{-1.5, 1},  point{-0.5, 1},  point{0.5, 1},  point{1.5, 1}};

std::vector<point> endpiece_coords = {point{0, 0}, point{1, 0}, point{-1, 1}};

std::array<MapPiece, 2> start_pieces = {
    // Apiece (StartPiece)
    MapPiece(
        {
            &start_hexes[0], &start_hexes[1], &start_hexes[2], &start_hexes[3],
            &jungle[0],      &jungle[0],      &jungle[0],      &jungle[0],
            &jungle[0],      &jungle[0],      &jungle[0],      &desert[0],
            &jungle[0],      &water[0],       &jungle[0],      &jungle[0],
            &desert[0],      &jungle[0],      &water[0],       &jungle[0],
            &desert[0],      &jungle[0],      &jungle[0],      &mountain,
            &desert[0],      &jungle[0],      &jungle[0],      &jungle[0],
            &water[0],       &mountain,       &jungle[0],      &jungle[0],
            &desert[0],      &jungle[0],      &basecamp[0],    &jungle[0],
            &jungle[0],
        },
        largepiece_coords, // Coordinates
        Difficulty::EASY,  // Difficulty level
        PieceType::START,  // Type of the piece
        PieceSize::LARGE),

    // Bpiece (StartPiece)
    MapPiece(
        {&start_hexes[3], &start_hexes[2], &start_hexes[1], &start_hexes[0],
         &jungle[0],      &jungle[0],      &jungle[0],      &jungle[0],
         &jungle[0],      &jungle[0],      &jungle[0],      &water[0],
         &jungle[0],      &jungle[0],      &jungle[0],      &water[0],
         &jungle[0],      &desert[0],      &jungle[0],      &desert[0],
         &jungle[0],      &jungle[0],      &jungle[0],      &desert[0],
         &jungle[0],      &jungle[0],      &jungle[0],      &jungle[0],
         &jungle[0],      &jungle[0],      &desert[0],      &mountain,
         &jungle[0],      &jungle[0],      &water[0],       &basecamp[0],
         &water[0]},
        largepiece_coords, Difficulty::EASY, PieceType::START,
        PieceSize::LARGE)};

std::array<MapPiece, 16> travel_pieces = {
    // Cpiece (LargePiece)
    MapPiece({&jungle[0], &jungle[0], &water[0],  &water[0],  &desert[0],
              &rubble[0], &jungle[0], &desert[0], &water[0],  &desert[0],
              &rubble[0], &water[0],  &water[0],  &desert[0], &desert[0],
              &water[0],  &desert[0], &rubble[0], &mountain,  &water[0],
              &rubble[0], &rubble[0], &water[0],  &water[0],  &desert[0],
              &desert[0], &rubble[0], &water[0],  &jungle[0], &desert[0],
              &rubble[0], &water[0],  &water[0],  &jungle[0], &jungle[0],
              &rubble[0], &rubble[0]},
             largepiece_coords, Difficulty::EASY, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Dpiece (LargePiece)
    MapPiece({&jungle[1], &jungle[0], &jungle[0], &jungle[0], &jungle[0],
              &water[0],  &water[0],  &water[0],  &jungle[0], &jungle[0],
              &water[0],  &water[1],  &water[0],  &water[0],  &jungle[0],
              &jungle[1], &jungle[0], &jungle[0], &mountain,  &water[1],
              &water[0],  &jungle[1], &jungle[0], &desert[2], &mountain,
              &jungle[0], &water[0],  &jungle[0], &mountain,  &desert[0],
              &desert[2], &jungle[0], &jungle[0], &water[2],  &mountain,
              &jungle[0], &jungle[1]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Epiece
    MapPiece({&jungle[0], &jungle[0],  &jungle[0], &rubble[0], &rubble[0],
              &water[1],  &mountain,   &jungle[1], &jungle[0], &rubble[0],
              &jungle[1], &rubble[0],  &jungle[0], &water[0],  &desert[0],
              &mountain,  &mountain,   &rubble[2], &water[0],  &water[0],
              &mountain,  &desert[0],  &rubble[0], &rubble[0], &jungle[2],
              &mountain,  &jungle[0],  &desert[0], &jungle[0], &jungle[1],
              &jungle[0], &jungle[1],  &desert[0], &jungle[0], &rubble[0],
              &jungle[0], &basecamp[0]},
             largepiece_coords, Difficulty::HARD, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Fpiece
    MapPiece({&rubble[0], &rubble[0],   &jungle[0], &basecamp[0], &rubble[0],
              &desert[0], &desert[0],   &jungle[2], &jungle[1],   &jungle[1],
              &jungle[0], &desert[1],   &rubble[1], &jungle[0],   &rubble[0],
              &jungle[0], &jungle[0],   &water[1],  &mountain,    &jungle[0],
              &water[1],  &basecamp[1], &mountain,  &mountain,    &water[2],
              &jungle[1], &jungle[0],   &water[1],  &mountain,    &water[0],
              &water[0],  &jungle[0],   &rubble[0], &water[0],    &water[0],
              &rubble[0], &rubble[0]},
             largepiece_coords, Difficulty::EASY, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Gpiece
    MapPiece({&jungle[0], &jungle[0],  &jungle[0], &rubble[0], &rubble[0],
              &water[0],  &mountain,   &jungle[1], &jungle[0], &rubble[0],
              &jungle[0], &rubble[0],  &jungle[0], &water[0],  &desert[0],
              &mountain,  &mountain,   &rubble[2], &water[0],  &water[0],
              &mountain,  &desert[0],  &rubble[0], &rubble[0], &jungle[2],
              &mountain,  &jungle[0],  &desert[0], &jungle[0], &jungle[1],
              &jungle[0], &jungle[1],  &desert[0], &jungle[0], &rubble[0],
              &jungle[0], &basecamp[0]},
             largepiece_coords, Difficulty::HARD, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Hpiece
    MapPiece({&jungle[1], &jungle[1], &jungle[1], &jungle[0], &jungle[1],
              &jungle[0], &jungle[0], &jungle[0], &water[1],  &jungle[1],
              &jungle[0], &desert[0], &desert[0], &water[0],  &water[1],
              &jungle[0], &jungle[0], &desert[0], &desert[1], &desert[0],
              &water[0],  &water[1],  &desert[0], &desert[1], &desert[1],
              &desert[0], &water[0],  &water[1],  &desert[1], &mountain,
              &desert[1], &water[0],  &water[1],  &desert[2], &desert[1],
              &desert[0], &water[0]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Ipiece
    MapPiece({&jungle[1], &jungle[1], &jungle[1], &jungle[0], &jungle[1],
              &jungle[0], &jungle[0], &jungle[0], &water[1],  &jungle[1],
              &jungle[0], &desert[0], &desert[0], &water[0],  &water[1],
              &jungle[0], &jungle[0], &desert[0], &desert[1], &desert[0],
              &water[0],  &water[1],  &desert[0], &desert[1], &desert[1],
              &desert[0], &water[0],  &water[1],  &desert[1], &mountain,
              &desert[1], &water[0],  &water[1],  &desert[2], &desert[1],
              &desert[0], &water[0]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Jpiece
    MapPiece({&desert[0], &desert[0], &desert[0], &rubble[1],   &desert[0],
              &desert[1], &desert[1], &mountain,  &rubble[0],   &desert[0],
              &desert[1], &jungle[0], &jungle[0], &rubble[1],   &rubble[0],
              &desert[0], &desert[0], &jungle[2], &basecamp[0], &jungle[0],
              &rubble[1], &rubble[0], &water[0],  &water[1],    &jungle[0],
              &jungle[1], &rubble[1], &rubble[0], &water[0],    &mountain,
              &water[1],  &water[0],  &rubble[1], &water[0],    &water[0],
              &water[0],  &water[0]},
             largepiece_coords, Difficulty::EASY, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Kpiece
    MapPiece({&jungle[1],   &jungle[1],   &jungle[1], &jungle[0], &jungle[0],
              &jungle[0],   &jungle[0],   &jungle[0], &jungle[1], &jungle[0],
              &jungle[1],   &jungle[2],   &jungle[2], &water[2],  &jungle[1],
              &basecamp[0], &jungle[0],   &jungle[0], &jungle[0], &jungle[0],
              &jungle[0],   &basecamp[0], &jungle[1], &desert[3], &jungle[2],
              &jungle[2],   &jungle[1],   &jungle[0], &jungle[1], &jungle[0],
              &jungle[0],   &jungle[0],   &jungle[0], &jungle[0], &jungle[1],
              &jungle[1],   &jungle[1]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Lpiece
    MapPiece({&jungle[1], &jungle[1], &jungle[0], &jungle[2],   &jungle[0],
              &jungle[0], &jungle[0], &jungle[2], &basecamp[0], &jungle[0],
              &jungle[1], &mountain,  &jungle[2], &water[0],    &basecamp[0],
              &mountain,  &jungle[0], &jungle[0], &jungle[0],   &jungle[0],
              &water[0],  &water[0],  &jungle[0], &desert[1],   &jungle[1],
              &mountain,  &jungle[0], &jungle[0], &jungle[1],   &basecamp[0],
              &jungle[1], &jungle[0], &jungle[1], &jungle[1],   &desert[1],
              &jungle[0], &jungle[1]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Mpiece
    MapPiece({&basecamp[0], &jungle[0], &jungle[0], &jungle[0], &water[3],
              &mountain,    &mountain,  &desert[3], &jungle[0], &mountain,
              &water[0],    &jungle[0], &jungle[0], &desert[1], &jungle[0],
              &mountain,    &jungle[0], &jungle[0], &rubble[1], &jungle[0],
              &jungle[0],   &mountain,  &jungle[0], &rubble[1], &mountain,
              &mountain,    &mountain,  &mountain,  &jungle[0], &rubble[1],
              &jungle[0],   &jungle[0], &jungle[0], &jungle[0], &jungle[0],
              &water[0],    &water[0]},
             largepiece_coords, Difficulty::HARD, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Npiece
    MapPiece({&desert[0], &water[0],  &water[0],  &jungle[0], &desert[0],
              &desert[1], &water[0],  &jungle[0], &jungle[0], &jungle[0],
              &desert[1], &desert[2], &water[0],  &jungle[1], &jungle[0],
              &jungle[0], &jungle[0], &jungle[0], &desert[3], &jungle[0],
              &jungle[0], &jungle[0], &jungle[0], &jungle[1], &water[0],
              &desert[2], &desert[1], &desert[0], &jungle[0], &jungle[0],
              &water[0],  &water[0],  &desert[0], &jungle[0], &jungle[0],
              &water[0],  &water[0]},
             largepiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::LARGE),
    // Opiece
    MapPiece({&desert[1], &jungle[1], &desert[0], &desert[0], &desert[1],
              &desert[0], &mountain, &mountain, &water[3], &mountain,
              &desert[0], &desert[0], &jungle[0], &jungle[1], &jungle[0],
              &desert[0]},
             smallpiece_coords, Difficulty::HARD, PieceType::TRAVEL,
             PieceSize::SMALL),
    // Ppiece
    MapPiece({&water[2], &water[1], &water[0], &water[1], &water[2], &jungle[0],
              &water[0], &water[0], &water[0], &water[0], &rubble[0], &water[0],
              &rubble[1], &water[2], &jungle[1], &water[0]},
             smallpiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::SMALL),
    // Qpiece
    MapPiece({&jungle[0], &rubble[2], &jungle[0], &jungle[0], &water[1],
              &jungle[1], &rubble[0], &jungle[1], &desert[2], &water[0],
              &jungle[1], &rubble[0], &desert[0], &desert[0], &water[0],
              &jungle[2]},
             smallpiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::SMALL),
    // Rpiece
    MapPiece({&jungle[0], &jungle[0], &mountain, &desert[0], &desert[0],
              &jungle[0], &jungle[2], &mountain, &desert[0], &basecamp[0],
              &desert[0], &jungle[0], &jungle[0], &mountain, &desert[0],
              &desert[0]},
             smallpiece_coords, Difficulty::MEDIUM, PieceType::TRAVEL,
             PieceSize::SMALL),
};

std::array<MapPiece, 2> end_pieces = {
    // Endpiece 1
    MapPiece({&end_hexes[0], &end_hexes[0], &end_hexes[0]}, endpiece_coords,
             Difficulty::EASY, PieceType::ENDING, PieceSize::TRIPLE_CURVED),
    // Endpiece 2
    MapPiece({&end_hexes[1], &end_hexes[1], &end_hexes[1]}, endpiece_coords,
             Difficulty::EASY, PieceType::ENDING, PieceSize::TRIPLE_CURVED),
};

void Map::generate(u_char n_pieces, Difficulty difficulty, int failures,
                   int max_failures, std::default_random_engine rng) {
  if (failures >= max_failures) {
    throw generate_map_failure(
        "Failed to generate map in specified maximum number of attempts");
  }

  u_char n_start_pieces = start_pieces.size();
  MapPiece *start = &start_pieces[std::uniform_int_distribution<size_t>(
      0, n_start_pieces - 1)(rng)];
  add_piece(start, point{0, 0}, 0);
  std::vector<size_t> valid_indices;
  valid_indices.reserve(travel_pieces.size());
  for (size_t i = 0; i < travel_pieces.size(); i++) {
    if ((travel_pieces[i].get_difficulty() <= difficulty)) {
      valid_indices.push_back(i);
    };
  }
  for (int i = 0; i < n_pieces; i++) {
    size_t valid_idx;
    if (valid_indices.size()) {
      valid_idx = std::uniform_int_distribution<size_t>(
          0, valid_indices.size() - 1)(rng);
      size_t piece_idx = valid_indices[valid_idx];
      if (add_random_piece(&travel_pieces[piece_idx], rng)) {
        std::swap(valid_indices[valid_idx], valid_indices.back());
        valid_indices.pop_back();
      } else {
        // try again
        reset();
        return generate(n_pieces, difficulty, failures + 1, max_failures, rng);
      }
    } else {
      // no valid pieces, invalid configuration
      throw generate_map_failure(
          "Trying to generate a map with more pieces than available for "
          "current difficulty! Either increase map difficulty or generate a "
          "smaller map.");
    }
  }
  MapPiece &end = end_pieces[std::uniform_int_distribution<size_t>(
      0, end_pieces.size() - 1)(rng)];
  bool success = add_random_piece(&end, rng);
  if (not success) {
    // try again
    reset();
    return generate(n_pieces, difficulty, failures + 1, max_failures, rng);
  }
  finalize();
};

void Map::reset() {
  pieces.resize(0);
  min_xy = {0, 0};
  max_xy = {0, 0};
  hexes.clear();
  xy.clear();
  hex_array.clear();
  hex_index.clear();
};

const std::vector<const Hex *> &Map::get_hexes() const { return hexes; };

const std::vector<point> &Map::get_xy() const { return xy; };
