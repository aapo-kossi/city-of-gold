#include "render.h"

int cog_renderer::sdl_init_counter = 0;
std::array<SDL_Texture *, static_cast<size_t>(HexSprite::COUNT)>
    cog_renderer::hex_texs;
std::array<SDL_Texture *, static_cast<size_t>(ReqSprite::COUNT)>
    cog_renderer::req_texs;
std::array<coord, static_cast<size_t>(ReqSprite::COUNT)>
    cog_renderer::req_texshapes;
std::array<SDL_Texture *, static_cast<size_t>(ObjectSprite::COUNT)>
    cog_renderer::obj_texs;
std::array<coord, static_cast<size_t>(Requirement::NULL_REQUIREMENT)>
    cog_renderer::req_offsets = {
        {{.0f, .1f}, {.0f, .1f}, {.1f, .1f}, {.1f, .0f}, {.1f, .0f}}};

std::string get_asset_path() {
  std::string p;
#if defined(__linux__) || defined(__APPLE__)
  Dl_info dl_info;
  if (dladdr((void *)get_asset_path, &dl_info)) {
    p = dl_info.dli_fname;
  }
#elif defined(_WIN32)
  char path[MAX_PATH];
  HMODULE hModule = nullptr;
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCTSTR)get_asset_path, &hModule)) {
    GetModuleFileName(hModule, path, MAX_PATH);
    p = path;
  }
#else
  p = "";
#endif
  size_t pos = p.find_last_of("/\\");
  std::string base_dir = (pos != std::string::npos) ? p.substr(0, pos) : ".";
  return base_dir + "/assets/";
}
