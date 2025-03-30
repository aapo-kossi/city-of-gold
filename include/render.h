#pragma once

#include "SDL3/SDL.h"
#include "config.h"
#include "constants.h"
#include "environment.h"
#include "geometry.h"
#include <cmath>
#include <limits>
#include <string_view>
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

/*constexpr size_t WINDOW_WIDTH = 1280;*/
/*constexpr size_t WINDOW_HEIGHT = 960;*/
constexpr size_t HEX_RENDERSIZE = 128;

struct RGBColor {
  float r, g, b;
};

constexpr std::array<RGBColor, MAX_N_PLAYERS> player_colors = {{
    {1.0, 1.0, 1.0}, // White
    {0.0, 0.0, 1.0}, // Blue
    {1.0, 0.0, 0.0}, // Red
    {0.0, 1.0, 0.0}, // Green
}};

enum class HexSprite { Jungle, Water, Desert, Rubble, Basecamp, COUNT };
enum class ReqSprite { Machete, Paddle, Coin, Discard, Remove, COUNT };
enum class ObjectSprite { Player, Card, COUNT };

constexpr std::array<std::string_view, static_cast<size_t>(HexSprite::COUNT)>
    hex_sprite_files = {"hexes/jungle.bmp", "hexes/water.bmp",
                        "hexes/desert.bmp", "hexes/rubble.bmp",
                        "hexes/basecamp.bmp"};

constexpr std::array<std::string_view, static_cast<size_t>(ReqSprite::COUNT)>
    req_sprite_files = {"requires/machete.bmp", "requires/paddle.bmp",
                        "requires/coin.bmp", "requires/discard.bmp",
                        "requires/remove.bmp"};

constexpr std::array<std::string_view, static_cast<size_t>(ObjectSprite::COUNT)>
    obj_sprite_files = {"player.bmp", "card.bmp"};

constexpr float half_root_3 = 0.866025403784f;

struct coord {
  float x, y;

  constexpr coord() = default; // uninitialized!
  constexpr coord(float x_, float y_) : x(x_), y(y_) {}
  explicit constexpr coord(const point &p)
      : x{p.x + p.y * 0.5f}, y{p.y * half_root_3} {}
  constexpr static coord from_hexpoint(const point &p) { return coord{p}; }

  constexpr coord operator+(const coord &other) const {
    return {x + other.x, y + other.y};
  }
  constexpr coord operator-(const coord &other) const {
    return {x - other.x, y - other.y};
  }
  constexpr coord operator-() const { return coord{0.0f, 0.0f} - *this; }
  constexpr coord operator*(float scalar) const {
    return {x * scalar, y * scalar};
  }
  constexpr coord operator*(const coord &other) const {
    return {x * other.x, y * other.y};
  }
  constexpr coord operator/(float scalar) const {
    return {x / scalar, y / scalar};
  }
  constexpr coord operator/(const coord &other) const {
    return {x / other.x, y / other.y};
  }

  constexpr coord &operator+=(const coord &other) {
    x += other.x;
    y += other.y;
    return *this;
  }
  constexpr coord &operator-=(const coord &other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }
  constexpr coord &operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }
  constexpr coord &operator/=(float scalar) {
    x /= scalar;
    y /= scalar;
    return *this;
  }

  constexpr float dot(const coord &other) const {
    return x * other.x + y * other.y;
  }
  constexpr float sqnorm() const { return x * x + y * y; }
  float norm() const { return std::sqrt(sqnorm()); }

  // requirement: len > 0
  coord normalized() const {
    float len = norm();
    return *this / len;
  }
};

// distances in lattice coordinates from cell center to edge and corner
constexpr float middle_to_point = 0.612372435696f;
constexpr float middle_to_edge = 0.5f;

// define rendering basis for the hexagonal grid
constexpr coord e_x = coord::from_hexpoint({1.0, 0.0});
constexpr coord e_y = coord::from_hexpoint({1.0, -1.0});

std::string get_asset_path();

class cog_renderer {
public:
  cog_renderer(cog_env const *const env_) : env{env_}, state{SDL_APP_CONTINUE} {
    SDL_SetAppMetadata("city-of-gold window", PROJECT_VER, "com.cog_env");

    bool should_load_assets = false;
    if (!sdl_init_counter) {

      if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        state = SDL_APP_FAILURE;
      }
      should_load_assets = true;
      sdl_init_counter++;
    }

    if (!SDL_CreateWindowAndRenderer(
            "examples/renderer/clear", static_cast<int>(display_size.x),
            static_cast<int>(display_size.y), 0, &window, &renderer)) {
      SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
      state = SDL_APP_FAILURE;
    }
    if (should_load_assets)
      load_assets();
  }

  void render() {

    SDL_SetRenderDrawColorFloat(renderer, 0.0, 0.0, 0.0, 1.0);
    SDL_RenderClear(renderer);

    const Map &m = env->get_map();
    const std::vector<point> &hex_xy = m.get_xy();
    const std::vector<const Hex *> &hexes = m.get_hexes();

    // Draw the base map
    for (size_t i = 0; i < hex_xy.size(); ++i) {
      const point &loc = hex_xy[i];
      coord center_unnormed = coord::from_hexpoint(loc);
      coord center = xy_to_screenspace(center_unnormed);
      auto hex_dest = get_hex_rect(center);
      SDL_RenderTexture(renderer, fetch_hex_basetex(hexes[i]), NULL, &hex_dest);
      coord shift = req_offsets[static_cast<size_t>(hexes[i]->requirement)];
      coord offset =
          -shift * (static_cast<float>(hexes[i]->n_required) - 1.0f) * 0.5f;
      for (size_t j = 0; j < hexes[i]->n_required; ++j, offset += shift) {
        coord x = xy_to_screenspace(center_unnormed + offset);
        auto dest = get_req_rect(x, hexes[i]->requirement);
        SDL_RenderTexture(renderer,
                          fetch_hex_resourcetex(hexes[i]->requirement), NULL,
                          &dest);
      }
    }

    // Draw the players
    size_t i_player_tex = static_cast<size_t>(ObjectSprite::Player);
    SDL_Texture *p_tex = obj_texs[i_player_tex];
    const auto &p_idx = m.get_player_locations();
    for (size_t j = 0; j < p_idx.size(); ++j) {
      coord center = xy_to_screenspace(coord::from_hexpoint(p_idx[j]));
      auto dest = get_player_rect(center);
      const RGBColor &c = player_colors[j];
      SDL_SetTextureColorModFloat(p_tex, c.r, c.g, c.b);
      SDL_RenderTexture(renderer, p_tex, NULL, &dest);
    }

    SDL_RenderPresent(renderer);
  }

  const SDL_AppResult &get_state() { return state; }

  void set_map_size() {
    _set_map_properties();
    display_size = hex_displaysize * (xy_max - xy_min + with_margin({}) * 2);
    SDL_SetWindowSize(window, static_cast<int>(display_size.x),
                      static_cast<int>(display_size.y));
  };

  ~cog_renderer() = default;

private:
  cog_env const *const env;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_AppResult state;
  coord xy_max{std::numeric_limits<float>::min(),
               std::numeric_limits<float>::min()};
  coord xy_min{std::numeric_limits<float>::max(),
               std::numeric_limits<float>::max()};
  coord display_size{};
  coord hex_displaysize = {middle_to_edge * HEX_RENDERSIZE,
                           middle_to_point *HEX_RENDERSIZE};

  const std::string asset_path = get_asset_path();

  static std::array<SDL_Texture *, static_cast<size_t>(HexSprite::COUNT)>
      hex_texs;
  static std::array<SDL_Texture *, static_cast<size_t>(ReqSprite::COUNT)>
      req_texs;
  static std::array<SDL_Texture *, static_cast<size_t>(ObjectSprite::COUNT)>
      obj_texs;
  static std::array<coord, static_cast<size_t>(Requirement::NULL_REQUIREMENT)>
      req_texshapes;
  static std::array<coord, static_cast<size_t>(Requirement::NULL_REQUIREMENT)>
      req_offsets;
  static int sdl_init_counter;

  coord get_xy_size() const { return xy_max - xy_min; }

  void load_assets() {

    for (size_t i = 0; i < static_cast<size_t>(HexSprite::COUNT); ++i) {
      hex_texs[i] = load_tex(i, hex_sprite_files);
    }
    for (size_t i = 0; i < static_cast<size_t>(ReqSprite::COUNT); ++i) {
      req_texs[i] = load_tex(i, req_sprite_files);
      if (req_texs[i]) {
        req_texshapes[i] = coord{static_cast<float>(req_texs[i]->w),
                                 static_cast<float>(req_texs[i]->h)};
      }
    }
    for (size_t i = 0; i < static_cast<size_t>(ObjectSprite::COUNT); ++i) {
      obj_texs[i] = load_tex(i, obj_sprite_files);
    }
  }

  template <class S> SDL_Texture *load_tex(S sprite, const auto &filenames) {
    const std::string_view name = filenames[static_cast<size_t>(sprite)];
    std::string full_name = asset_path + static_cast<std::string>(name);
    SDL_Surface *surf = SDL_LoadBMP(full_name.c_str());
    if (!surf) {
      /*SDL_Log("Failed to load texture %s: %s", full_name.c_str(),*/
      /*        SDL_GetError());*/
      return nullptr;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
      /*SDL_Log("Failed to create texture %s: %s", full_name.c_str(),*/
      /*        SDL_GetError());*/
      return nullptr;
    }
    SDL_DestroySurface(surf);
    return tex;
  }

  SDL_Texture *fetch_hex_basetex(const Hex *h) {
    const Requirement hex_req = h->requirement;
    SDL_Texture *tex = NULL;
    if (h->requirement != Requirement::NULL_REQUIREMENT) {
      tex = hex_texs[static_cast<size_t>(hex_req)];
    } else if (h->player_start) {
      tex = hex_texs[0]; // use jungle texture for start locations
    }

    /*if (!tex) {*/
    /*  SDL_Log("Texture for req %zu is null", static_cast<size_t>(hex_req));*/
    /*}*/
    return tex;
  }

  SDL_Texture *fetch_hex_resourcetex(Requirement hex_req) {
    SDL_Texture *tex = req_texs[static_cast<size_t>(hex_req)];
    /*if (!tex) {*/
    /*  SDL_Log("Texture for req %zu is null", static_cast<size_t>(hex_req));*/
    /*}*/
    return tex;
  }

  SDL_FRect get_hex_rect(coord c) {
    coord size = scale_to_screenhex({2 * middle_to_edge, 2 * middle_to_point});
    /*float w = static_cast<float>(WINDOW_WIDTH) / map_size.x;*/
    /*float h = static_cast<float>(WINDOW_HEIGHT) / map_size.y;*/
    SDL_FRect dest = get_centered_rect(c, size.x, size.y);
    return dest;
  }

  SDL_FRect get_req_rect(coord c, Requirement r) {
    coord hex_frac = req_texshapes[static_cast<size_t>(r)];
    hex_frac /= std::max(hex_frac.x, hex_frac.y) * 2.0f;
    coord size = scale_to_screenhex(hex_frac);
    /*float w = static_cast<float>(WINDOW_WIDTH) * hex_frac.x / map_size.x;*/
    /*float h = static_cast<float>(WINDOW_HEIGHT) * hex_frac.y / map_size.y;*/
    SDL_FRect dest = get_centered_rect(c, size.x, size.y);
    return dest;
  }

  SDL_FRect get_player_rect(coord c) {
    float hex_frac = 0.7f;
    coord size = scale_to_screenhex(coord(hex_frac, hex_frac));
    /*float w = static_cast<float>(WINDOW_WIDTH) * hex_frac / map_size.x;*/
    /*float h = static_cast<float>(WINDOW_HEIGHT) * hex_frac / map_size.y;*/
    SDL_FRect dest = get_centered_rect(c, size.x, size.y);
    return dest;
  }

  SDL_FRect get_centered_rect(coord c, float w, float h) {
    SDL_FRect dest;
    dest.x = c.x - w / 2;
    dest.y = c.y - h / 2;
    dest.w = w;
    dest.h = h;
    /*SDL_Log("Rendering at x: %f, y: %f, w: %f, h: %f", dest.x, dest.y,
     * dest.w,*/
    /*        dest.h);*/
    return dest;
  }

  void _set_map_properties() {
    xy_max = {std::numeric_limits<float>::min(),
              std::numeric_limits<float>::min()};
    xy_min = {std::numeric_limits<float>::max(),
              std::numeric_limits<float>::max()};
    const auto &hex_xy = env->get_map().get_xy();
    xy_min.x = coord::from_hexpoint(hex_xy[0]).x;

    for (const auto &p : hex_xy) {
      const coord xy = coord::from_hexpoint(p);
      xy_max.x = std::max(xy_max.x, xy.x);
      xy_max.y = std::max(xy_max.y, xy.y);
      xy_min.x = std::min(xy_min.x, xy.x);
      xy_min.y = std::min(xy_min.y, xy.y);
    }
  }

  coord xy_to_screenspace(coord xy) {
    coord map_size_with_margins =
        get_xy_size() + coord{middle_to_edge, middle_to_point} * 2;
    coord xy_rightsideup{xy.x, -xy.y};
    coord inverted_offsets{xy_min.x, -xy_max.y};
    coord xy_normalized =
        with_margin(xy_rightsideup - inverted_offsets) / map_size_with_margins;
    return xy_normalized * display_size;
  }

  coord with_margin(coord xy) {
    return xy + coord{middle_to_edge, middle_to_point};
  }

  /*scale a fraction wh of a hex to its displayed size*/
  coord scale_to_screenhex(coord wh) {
    return wh * display_size /
           (get_xy_size() + coord{middle_to_edge, middle_to_point} * 2);
  }
};
